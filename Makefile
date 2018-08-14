CC=gcc
CFLAGS=-O3 -Wall -Wextra -std=gnu11 -g
LDLIBS=-lpthread
BINDIR=bin
OBJDIR=obj

vpath %.c source
vpath %.h source


server_objects=$(addprefix $(OBJDIR)/, \
									server.o \
									linked_list.o \
									message_svc.o \
									message.o )

client_objects=$(addprefix $(OBJDIR)/, \
									client.o \
									message.o \
									message_generator.o \
									linked_list.o )

all: server client

server: $(server_objects) | $(BINDIR)
	$(CC) $(server_objects) -o $(BINDIR)/server $(LDLIBS) $(CFLAGS)

client: $(client_objects) | $(BINDIR)
	$(CC) $(client_objects) -o $(BINDIR)/client $(LDLIBS) $(CFLAGS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $< -c -o $@ $(LDLIBS) $(CFLAGS)

$(BINDIR):
	mkdir $(BINDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

clean:
	rm -f $(server_objects) $(client_objects)

purge:
	rm -r $(OBJDIR)
	rm -r $(BINDIR)
