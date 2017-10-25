#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <ascl.h>

//int named_pipes_fds [2];

/*TECHNIQUE_ID techniques_array [TECHNIQUES_COUNT] = {
	CODE_SPLITTING,
	CODE_MOBILITY,
	WBS,
	MTC_CRYPTO_SERVER,
	CG_HASH_RANDOMIZATION,
	CG_HASH_VERIFICATION,
	CFGT_REMOVE_VERIFIER,
	AC_DECISION_LOGIC,
	RA_REACTION_MANAGER,
	RA_VERIFIER,
	TEST,
	RENEWABILITY
};*/

static struct per_session_data__accl* sessions[1024];
int sessions_count = 0;

#ifdef ASCL_INTERNAL_DISPATCHER
static int asclWebSocketDispatcherMessage(
		TECHNIQUE_ID	technique_id,
		char			application_id[1024],
		int 			message_id,
		size_t			length,
		void*			payload,
		size_t*			response_length,
		void*			response) {

	/* indentify the appropriate r/w named pipes */
	int w_name_pipe_fd = named_pipes_fds[W_PIPE_ID],
			r_name_pipe_fd = named_pipes_fds[R_PIPE_ID];

	ssize_t	bytes_overall = 0,
			bytes_trans = 0;

	if (w_name_pipe_fd == -1)
		return ASCL_ERROR;

	lwsl_notice("ASCL - asclWebSocketDispatcherMessage() enter\n");

	/* write the message id */
	if (write(w_name_pipe_fd, (void*)&message_id, sizeof(int)) != sizeof(int))
		return ASCL_ERROR;

	/* write the application id */
	if (write(w_name_pipe_fd, (void*)application_id, 32) != 32)
		return ASCL_ERROR;

	/* write the payload length */
	if (write(w_name_pipe_fd, (void*)&length, sizeof(size_t)) != sizeof(size_t))
		return ASCL_ERROR;

	/* write the payload */
	while (bytes_overall < length) {
		bytes_trans = write(w_name_pipe_fd, payload, length);

		if (bytes_trans == -1)
			return ASCL_ERROR;

		bytes_overall = bytes_overall + bytes_trans;
	}

	/* the exchange message foresee a response from backend service */
	if (message_id == ASCL_WS_MESSAGE_EXCHANGE) {
		ssize_t read_val, buffer_read = 0;
		void* buffer = NULL;

		if (w_name_pipe_fd == -1)
			return ASCL_ERROR;

		/* read an integer from the pipe: this will be the response length */
		read_val = read (r_name_pipe_fd, (void*)response_length, sizeof(int));

		if (read_val != sizeof(int)) {
			return ASCL_ERROR;
		} else {
			if (*response_length > ASCL_WS_MAX_BUFFER_SIZE)
				return ASCL_ERROR;

			/* return buffer allocation */
			buffer = malloc(*response_length);

			if ((NULL == buffer) && (*response_length > 0)) {
				return ASCL_ERROR;
			}

			/* response reading */
			while (buffer_read < *response_length) {
				read_val = read (r_name_pipe_fd, buffer + buffer_read,
								 *response_length - buffer_read);

				buffer_read += read_val;

				if (-1 == read_val) {
					free(buffer);
					return ASCL_ERROR;
				}
			}
		}

		memcpy (response, response, *response_length);
	}

	return ASCL_SUCCESS;
}
#endif

struct per_session_data__accl* session_by_application_id (char* application_id, int technique_id) {
	int i;

	for (i = 0; i < sessions_count; i++) {
		if (sessions[i] != NULL)
			if (sessions[i]->technique_id == technique_id)
				if (strcmp(sessions[i]->application_id, application_id) == 0)
					return sessions[i];
	}

	return NULL;
}

int session_index_by_application_id (char* application_id, int technique_id) {
	int i;

	for (i = 0; i < sessions_count; i++) {
		if (sessions[i] != NULL)
			if (sessions[i]->technique_id == technique_id)
				if (strcmp(sessions[i]->application_id, application_id) == 0)
					return i;
	}

	return -1;
}

/*
 * Exchange response helper
 */
int _asclWebSocketResponseToExchange(struct lws_context* context, char* application_id, int technique_id, void* buffer, size_t buffer_length, void* response, size_t* response_length) {

	// prepare user context for sending callback
	struct ascl_context_buffer* user_context = (struct ascl_context_buffer*)lws_context_user(context);
	struct per_session_data__accl* session;

	// buffer size check
	if (buffer_length > ASCL_WS_MAX_BUFFER_SIZE) {
		lwsl_err("ASCL - _asclWebSocketCommunicate() maximum buffer size of %d bytes exceeded\n", ASCL_WS_MAX_BUFFER_SIZE);
		return ASCL_ERROR;
	}

	if (NULL != user_context) {
		session = session_by_application_id(application_id, technique_id);

		if (NULL != session) {
			user_context->buffer_ptr = buffer;
			user_context->buffer_size = buffer_length;
			user_context->wait_for_response = 0;
			user_context->send_in_progress = 1;
			user_context->technique_id = technique_id;
			strcpy((char*)&user_context->application_id, application_id);

			// request a write session to libwebsocket for ACCL communication to a specific client
			lws_callback_on_writable(session->wsi);

			while (1 == user_context->send_in_progress)
				lws_service(context, 50);

		} else {
			lwsl_err("ASCL - _asclWebSocketCommunicate() invalid AID %s\n", application_id);
			//pthread_mutex_unlock(&mutex_channel_busy);
			return ASCL_ERROR;
		}
	} else {
		lwsl_err("ASCL - _asclWebSocketCommunicate() invalid user context\n");
		//pthread_mutex_unlock(&mutex_channel_busy);
		return ASCL_ERROR;
	}

	lwsl_notice("ASCL - _asclWebSocketCommunicate(%s) terminated\n", application_id);

	return ASCL_SUCCESS;
}

int static callback_accl_communication(
		struct lws *wsi,
		enum lws_callback_reasons reason,
		void *user, 		// <-- user data
		void *in, 			// <-- reveived buffer
		size_t len) {
	int m, i;

  struct lws_context *context = lws_get_context(wsi);
	struct ascl_context_buffer* user_context = (struct ascl_context_buffer*)lws_context_user(context);

	// output buffer has to be pre and post padded
	// [ ... PRE-PADDING ... | ACTUAL BUFFER CONTENT | ... POST-PADDING ... ]
	unsigned char write_buffer[LWS_SEND_BUFFER_PRE_PADDING + ASCL_WS_MAX_BUFFER_SIZE + LWS_SEND_BUFFER_POST_PADDING];

	// pointer to output buffer actual start offset
	unsigned char *write_buffer_pointer = &write_buffer[LWS_SEND_BUFFER_PRE_PADDING];

	// user session data (T_ID, A_ID, ecc...)
	struct per_session_data__accl* session_data = (struct per_session_data__accl*)user;

  char request_uri[256];
  char* buffer = request_uri;
  char *token;

	//if (reason != LWS_CALLBACK_GET_THREAD_ID)
	//	lwsl_notice("ASCL - callback_accl_communication %d\n", reason);

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:			// initializing
			if (sizeof(request_uri) <= lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
				break;

			lws_hdr_copy(wsi, buffer, sizeof(request_uri), WSI_TOKEN_GET_URI);

			lwsl_notice("ASCL - callback_accl_communication: LWS_CALLBACK_ESTABLISHED (URI: %s)\n", request_uri);

			if (0 == lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
				break;

			for(i = 0; (token = strsep(&buffer, "/")) != NULL; i++) {
				switch (i) {
					case 0:	// first item is empty by design
						break;
					case 1: // second item is technique_id
						session_data->technique_id = atoi(token);
						break;
					case 2: // third item is application_id
						strcpy (session_data->application_id, token);
						break;
					default:
						lwsl_err("unexpected param\n");
						break;
				}
			}

			session_data->wsi = wsi;
			user_context->connection_initialized = 1;

			sessions[sessions_count++] = session_data;

			asclWebSocketDispatcherMessage (session_data->technique_id, session_data->application_id, ASCL_WS_MESSAGE_OPEN, strlen(session_data->application_id), (void*)session_data->application_id, 0, NULL);

			break;
		case LWS_CALLBACK_CLOSED:				// connection closed
			lwsl_notice("ASCL - LWS_CALLBACK_CLOSED (AID: %s)\n", session_data->application_id);

			int s_index = session_index_by_application_id(session_data->application_id, session_data->technique_id);

			if (s_index != -1)
				sessions[s_index] = NULL;

			asclWebSocketDispatcherMessage (session_data->technique_id, session_data->application_id, ASCL_WS_MESSAGE_CLOSE, strlen(session_data->application_id), (void*)session_data->application_id, 0, NULL);

			break;
		case LWS_CALLBACK_CLIENT_WRITEABLE:		// sending data
			lwsl_err("HERE?\n");
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:		// sending data
			lwsl_notice("ASCL - callback_accl_communication: LWS_CALLBACK_SERVER_WRITEABLE %s: %d bytes\n", user_context->application_id, user_context->buffer_size);

			/**
			 * Outgoing data can be:
			 * - response to client initiated Exchange
			 * - server initiated Exchange
			 * - server initiated Send
			 */

			// fill in the output buffer
			memcpy (write_buffer_pointer, user_context->buffer_ptr, user_context->buffer_size);

			// send data though the channel
			m = lws_write(wsi, write_buffer_pointer, user_context->buffer_size, LWS_WRITE_BINARY);

			if (m < user_context->buffer_size) {
				lwsl_err("ASCL - Writing error: expected %d bytes, %d actually\n", user_context->buffer_size, m);
				// TODO signal error?
			}

			user_context->send_in_progress = 0;

			break;
		case LWS_CALLBACK_RECEIVE:				// data received
			lwsl_notice("ASCL - callback_accl_communication: LWS_CALLBACK_RECEIVE [T_ID: %d, A_ID: %s]\n", session_data->technique_id, session_data->application_id);

			/**
			 * Incoming data can be:
			 * - response to server initiated Exchange
			 * - client initiated Exchange
			 * - client initiated Send
			 */

			// client initiated

			if (0x0 == ((char*) in)[0]) {
				// dispatch data to the backend

				asclWebSocketDispatcherMessage (session_data->technique_id, session_data->application_id,  ASCL_WS_MESSAGE_SEND, len - 1, in + 1, 0, NULL);
			} else {
				char* response = (char*)malloc(ASCL_WS_MAX_BUFFER_SIZE);
				size_t response_length = 0;

				// dispatch data to the backend
				asclWebSocketDispatcherMessage (session_data->technique_id, session_data->application_id, ASCL_WS_MESSAGE_EXCHANGE, len - 1, in + 1, &response_length, response);

				// send a response to the client
				_asclWebSocketResponseToExchange (context, session_data->application_id, session_data->technique_id, response, response_length, 0, NULL);

				lwsl_notice("ASCL - exchange response sent\n");
			}


			// server to client exchange response

			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			lwsl_notice("ASCL - callback_accl_communication: LWS_CALLBACK_CLIENT_CONNECTION_ERROR");

			user_context->connection_initialized = 2;

			break;
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
			lwsl_info("ASCL - callback_accl_communication: LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION\n");
			/* return non-zero here and kill the connection */
			break;
		default:
			//lwsl_notice("ASCL - DEFAULT CALLBACK HANDLER %d\n", reason);
			break;
	}

	return 0;
}

/* list of supported protocols and callbacks */
static struct lws_protocols protocols[] = {
	{
		"accl-communication-protocol",
		callback_accl_communication,
		sizeof(struct per_session_data__accl),
		ASCL_WS_MAX_BUFFER_SIZE,
    0,
    NULL
	},
	{ NULL, NULL, 0, 0, 0, NULL } /* terminator */
};

/*
	ASCL shutdown
*/
int asclWebSocketShutdown(struct lws_context* context) {

	if (NULL != context) {
		lws_cancel_service(context);
		lws_context_destroy(context);
	}
	
	lwsl_notice("ASCL - WebSockets Shutdown\n");

	return ASCL_SUCCESS;
}

int acclGetWebSocketPort(int technique_id) {
	int port;

	switch (technique_id) {
		case CODE_SPLITTING:
			port = 8082;
			break;
		case RA_REACTION_MANAGER:
			port = 8083;
			break;
		case RA_VERIFIER:
			port = 8084;
			break;
		case RA_ATTESTATOR_0:
		case RA_ATTESTATOR_1:
		case RA_ATTESTATOR_2:
		case RA_ATTESTATOR_3:
		case RA_ATTESTATOR_4:
		case RA_ATTESTATOR_5:
		case RA_ATTESTATOR_6:
		case RA_ATTESTATOR_7:
		case RA_ATTESTATOR_8:
		case RA_ATTESTATOR_9:
			port = technique_id - 9000 + 8090;
			break;
		case RN_RENEWABILITY:
			port = 18001;
			break;
		default:
			port = ASCL_WS_LISTENING_PORT;
			break;
	}

	return port;
}

/*
	ASCL initialization
*/
struct lws_context* asclWebSocketInit(TECHNIQUE_ID technique_id) {
	struct lws_context_creation_info info;
	struct ascl_context_buffer* user_context;
	struct lws_context *context;
	struct lws_protocols *context_protocols;

    // buffer information
    user_context = (struct ascl_context_buffer*)malloc(sizeof(struct ascl_context_buffer));

    if (NULL == user_context)
    	return NULL;

    user_context->buffer_ptr = malloc(ASCL_WS_MAX_BUFFER_SIZE);
    user_context->buffer_size = 0;
	user_context->send_in_progress = 0;
	user_context->connection_initialized = 0;

    if (NULL == user_context->buffer_ptr)
    	return NULL;

	// structure initialization
	memset(&info, 0, sizeof info);

	lwsl_notice("ASPIRE Server Communication Logic - WebSockets Initialization (technique id: %d)\n", technique_id);

	context_protocols = (struct lws_protocols*)malloc(sizeof(struct lws_protocols) * 2);
	memcpy(context_protocols, protocols, sizeof(struct lws_protocols) * 2);

	info.port = acclGetWebSocketPort(technique_id);
	info.iface = NULL;
	info.protocols = context_protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;
	info.options = 0;
	info.user = (void*)user_context;

	// context creation
	context = lws_create_context(&info);

	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return NULL;
	}

	return context;
}

/**
 * Send / Exchange helper
 */
int _asclWebSocketCommunicate(int wait_for_response, struct lws_context* context, char* application_id, int technique_id, void* buffer, size_t buffer_length, void* response, size_t* response_length) {
	// prepare user context for sending callback
	struct ascl_context_buffer* user_context = (struct ascl_context_buffer*)lws_context_user(context);
	struct per_session_data__accl* session;

	// buffer size check
	if (buffer_length > ASCL_WS_MAX_BUFFER_SIZE) {
		lwsl_err("ASCL - _asclWebSocketCommunicate() maximum buffer size of %d bytes exceeded\n", ASCL_WS_MAX_BUFFER_SIZE);
		return ASCL_ERROR;
	}

	if (NULL != user_context) {
		session = session_by_application_id(application_id, technique_id);

		if (NULL != session) {
			user_context->buffer_ptr = buffer;
			user_context->buffer_size = buffer_length;
			user_context->wait_for_response = wait_for_response;
			user_context->technique_id = technique_id;
			user_context->send_in_progress = 1;
			strcpy(user_context->application_id, application_id);

			// request a write session to libwebsocket for ACCL communication to a specific client
			lws_callback_on_writable(session->wsi);

			//lwsl_notice("ASCL - _asclWebSocketCommunicate(%s) write on channel requested\n", user_context->application_id);

			while (1 == user_context->send_in_progress) {
				lws_service(context, 50);
			}

			//lwsl_notice("ASCL - _asclWebSocketCommunicate(%s) write on channel completed\n", user_context->application_id);

			while (1 == user_context->wait_for_response) {
				lws_service(context, 50);
			}

			//lwsl_notice("ASCL - _asclWebSocketCommunicate(%s) wait for response passed\n", user_context->application_id);
		} else {
			lwsl_err("ASCL - _asclWebSocketCommunicate(%s, %d) invalid AID\n", application_id, technique_id);

			return ASCL_ERROR;
		}
	} else {
		lwsl_err("ASCL - _asclWebSocketCommunicate() invalid user context\n");

		return ASCL_ERROR;
	}

	//lwsl_notice("ASCL - _asclWebSocketCommunicate(%s) terminated\n", application_id);

	return ASCL_SUCCESS;
}

/*
	ASCL WebSockets data send function

	[in] struct libwebsocket_context*	ASCL context
	[in] void* buffer 					Data to be sent to client
	[in] size_t buffer_length 			Buffer length

	Returns	ASCL_SUCCESS -> OK
			ASCL_ERROR -> KO
*/
int asclWebSocketSend(struct lws_context* context, char* application_id, int technique_id, void* buffer, size_t buffer_length) {
	if (NULL == context)
		return ASCL_ERROR;

	//lwsl_notice("ASCL - asclWebSocketSend(%s, %d) entered\n", application_id, technique_id);

	return _asclWebSocketCommunicate(0, context, application_id, technique_id, buffer, buffer_length, NULL, 0);
}

int asclWebSocketExchange(struct lws_context* context, char* application_id, int technique_id, void* buffer, size_t buffer_length, void* response, size_t* response_length) {
	if (NULL == context)
		return ASCL_ERROR;

	//lwsl_notice("ASCL - asclWebSocketExchange(%s, %d) entered\n", application_id, technique_id);

	return _asclWebSocketCommunicate(1, context, application_id, technique_id, buffer, buffer_length, response, response_length);
}
