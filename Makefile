CC=gcc

all: server client

server:
	$(CC) source/server.c \
		  source/linked_list.c \
		  source/message_svc.c \
		  source/message.c \
		  -o server \
		  -O3 -Wall -Wextra -lpthread -g

client:
	$(CC) source/client.c \
		  source/message.c \
	 	  -o client \
		  -O3 -Wall -Wextra -g

clean:
	rm client server
