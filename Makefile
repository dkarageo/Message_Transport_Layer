CC=gcc
CFLAGS=-O3 -Wall -Wextra -std=gnu11
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
									demo_client.o \
									client_svc.o \
									message.o \
									message_generator.o \
									linked_list.o )

test_objects=$(addprefix $(OBJDIR)/, \
									test_message_generator.o \
									message_generator.o \
									message.o )

all: server client

server: $(server_objects) | $(BINDIR)
	$(CC) $(server_objects) -o $(BINDIR)/server $(LDLIBS) $(CFLAGS)

client: $(client_objects) | $(BINDIR)
	$(CC) $(client_objects) -o $(BINDIR)/demo_client $(LDLIBS) $(CFLAGS)

test: $(test_objects) | $(BINDIR)
	$(CC) $(test_objects) -o $(BINDIR)/test_message_generator $(LDLIBS) $(CFLAGS)
	./$(BINDIR)/test_message_generator

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
