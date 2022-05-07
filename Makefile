.PHONY: clean

CFLAGS = -g -Wall -DDEBUG=1 -lpthread

all: om_socket udp_client

om_socket: om_socket.o

udp_client: udp_client.o

clean:
	-rm -f *.o om_socket udp_client
