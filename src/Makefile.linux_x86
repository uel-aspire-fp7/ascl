CC=gcc

PLATFORM=linux
THIRD_PARTY=/opt/3rd_party

CFLAGS=	-I. \
		-I$(THIRD_PARTY)/libwebsockets/$(PLATFORM)/include \
		-Wall \
		-g

all:
	$(CC) $(CFLAGS) -fpic -c ascl.c -o ascl.o

clean:
	rm *.o -f