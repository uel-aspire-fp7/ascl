/*
	ASPIRE Server-side Communication Logic

	ascl.h - Prototypes and defines
*/

#ifndef ASCL__
#define ASCL__
#endif

#ifndef ASCL_EXTERN
#define ASCL_EXTERN
#endif

#include <string.h>
#include <pthread.h>
#include <libwebsockets.h>

/* ASCL Return values */
#define ASCL_SUCCESS	1
#define ASCL_ERROR		0

#define ASCL_WS_LISTENING_PORT	8081

/* 
	WebSocket Service Dispatcher communication named pipe 

	NB there will be one named pipe per protection technique
*/
#define ASCL_WS_SD_NAMED_PIPE_IN 		"/tmp/ascl_sd_%d_in"
#define ASCL_WS_SD_NAMED_PIPE_OUT 		"/tmp/ascl_sd_%d_out"

/*
	The following are the MESSAGE IDs sent by ASCL to Service Dispatchers
		in response to events on the channel
*/
#define ASCL_WS_MESSAGE_OPEN			0
#define ASCL_WS_MESSAGE_SEND			1
#define ASCL_WS_MESSAGE_EXCHANGE		2
#define ASCL_WS_MESSAGE_CLOSE			3

#define ASCL_WS_MAX_BUFFER_SIZE			16384

#define TECHNIQUES_COUNT				12

/* Technique Unique Identifiers */
typedef enum {
	CODE_SPLITTING			= 10,
	CODE_MOBILITY			= 20,
	WBS						= 30,
	MTC_CRYPTO_SERVER		= 40,
	CG_HASH_RANDOMIZATION	= 50,
	CG_HASH_VERIFICATION	= 55,
	CFGT_REMOVE_VERIFIER	= 60,
	AC_DECISION_LOGIC		= 70,
	AC_STATUS_LOGIC			= 75,
	RA_REACTION_MANAGER 	= 80,
	RA_VERIFIER		 		= 90,		/* NOT USED */
	RN_RENEWABILITY	 		= 500,		/* NB not defined in D1.04 */
	RA_ATTESTATOR_0			= 9000,
	RA_ATTESTATOR_1			= 9001,
	RA_ATTESTATOR_2			= 9002,
	RA_ATTESTATOR_3			= 9003,
	RA_ATTESTATOR_4			= 9004,
	RA_ATTESTATOR_5			= 9005,
	RA_ATTESTATOR_6			= 9006,
	RA_ATTESTATOR_7			= 9007,
	RA_ATTESTATOR_8			= 9008,
	RA_ATTESTATOR_9			= 9009,
	TEST					= 9999
} TECHNIQUE_ID;

#define R_PIPE_ID	0
#define W_PIPE_ID	1

typedef enum {
	ASCL_IDLE,
	ASCL_S2C_SEND,
	ASCL_S2C_EXCHANGE,
	ASCL_C2S_SEND,
	ASCL_C2S_EXCHANGE,
} ascl_context_status_enum;

/* ASCL data sending logic */
struct ascl_context_buffer {
	void* buffer_ptr;
	size_t buffer_size;
	char application_id[1024];
	TECHNIQUE_ID technique_id;
	int wait_for_response;
	int connection_initialized;
	int send_in_progress;
	ascl_context_status_enum context_status;
};

//static pthread_mutex_t mutex_channel_busy;
//static TECHNIQUE_ID current_technique_id;

/* the ASCL implements only one communication protocol */
enum ascl_protocols {
	PROTOCOL_ACCL_COMMUNICATION,

	/* always last */
	ASCL_PROTOCOL_COUNT
};

/* an instance of this struct is mantained for each connection
 * as session storage, it is used to identify the client */
struct per_session_data__accl {
	char application_id[1024];
	TECHNIQUE_ID technique_id;
	int outbound_exchange_pending;
	int outbound_send_pending;
	int inbound_exchange_pending;
	int inbound_send_pending;
	void* pending_operation_buffer;
	int pending_operation_buffer_size;
	struct libwebsocket *wsi;
};

LWS_EXTERN char* lws_hdr_simple_ptr(struct libwebsocket *wsi, enum lws_token_indexes h);

/* ASCL API prototypes */
ASCL_EXTERN struct libwebsocket_context* asclWebSocketInit(TECHNIQUE_ID);
ASCL_EXTERN int asclWebSocketSend(struct libwebsocket_context* context, char* application_id, int technique_id, void* buffer, size_t buffer_length);
ASCL_EXTERN int asclWebSocketExchange(struct libwebsocket_context* context, char* application_id, int technique_id, void* buffer, size_t buffer_length, void*, size_t*);
ASCL_EXTERN int asclWebSocketShutdown(struct libwebsocket_context*);

#ifdef ASCL_INTERNAL_DISPATCHER
	static int asclWebSocketDispatcherMessage(TECHNIQUE_ID, char[1024], int, size_t, const char*, size_t*, const*);
#else
	extern int asclWebSocketDispatcherMessage(TECHNIQUE_ID, char[1024], int, size_t, const char*, size_t*, char*);
#endif