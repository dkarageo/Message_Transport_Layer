/**
 * server.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A TCP server able to handle multiple connections in a thread based model.
 *
 * Usage: exec_name <port>
 *  where:
 *      -port : Port number on which to start the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "linked_list.h"
#include "message_svc.h"


typedef struct {
    int socket_fd;
    struct sockaddr_in addr;
	node_t *list_entry;
} handler_args_t;

typedef struct {
    pthread_t tid;
    int fd;
} handler_t;


int init_listener(int port);
void start_listener(int socket_fd);
void destroy_listener(int socket_fd);
void handle_client(int client_fd, struct sockaddr_in client_addr);
void *start_handler(void *args);
void error(const char *msg);
void terminate_server(int signum);


const int TERM_SIGNAL = SIGINT;  // Signal for requesting server termination.

linked_list_t *handler_fds;   // Storage for info of active handlers.
int listener_fd;              // Socket descriptor of listener.
pthread_mutex_t *list_mutex;  // A mutex used for list operations.
pthread_cond_t *list_size_cond;  // Condition to be used for tracking handlers num.


int main(int argc, char *argv[])
{
    // Listening port should be provided by caller.
    if (argc < 2) {
        fprintf(stderr, "ERROR: No listening port provided.\n");
        fprintf(stdout, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Initialize globals.
    handler_fds = linked_list_create();
    list_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(list_mutex, NULL);
	list_size_cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
	pthread_cond_init(list_size_cond, NULL);

    int port = atoi(argv[1]); // Listening port.
    listener_fd = init_listener(port);

    // Init Message Transport Layer service.
    init_svc();

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = terminate_server;
    sigaction(TERM_SIGNAL, &act, NULL);
    printf("Use CTRL+C to terminate.\n");

    // Use current thread for the listener.
    start_listener(listener_fd);

    printf("\nServer terminating...\n");

    // Ask active handlers to terminate.
    pthread_mutex_lock(list_mutex);
    iterator_t * iter = linked_list_iterator(handler_fds);
    while(iterator_has_next(iter)) {
        handler_t *handler = (handler_t *) iterator_next(iter);
        shutdown(handler->fd, SHUT_RDWR);
    }
    iterator_destroy(iter);
    pthread_mutex_unlock(list_mutex);

    // Wait for previous active handlers to terminate (maybe already done so).
    pthread_mutex_lock(list_mutex);
	while (linked_list_size(handler_fds) > 0) {
		pthread_cond_wait(list_size_cond, list_mutex);
	}
	pthread_mutex_unlock(list_mutex);

    // Terminate Message Transport Layer service.
    stop_svc();

    // Clean up resources.
    pthread_mutex_destroy(list_mutex);
    free(list_mutex);
	pthread_cond_destroy(list_size_cond);
	free(list_size_cond);
    linked_list_destroy(handler_fds);

    return 0;
}

/**
 * Prints a message about currently set errno and terminates process.
 */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/**
 * Ask server to terminate normally completing any critical unhandled task.
 *
 * This is a signal handler, that should be connected to a terminating signal.
 */
void terminate_server(int signum)
{
    if (signum == TERM_SIGNAL) destroy_listener(listener_fd);
}

/**
 * Initialize a listener on the given port.
 */
int init_listener(int port)
{
    int socket_fd;                 // Listener's file descriptor.
    struct sockaddr_in serv_addr;  // Server's local address.

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);  // IPv4 TCP socket.
    if (socket_fd < 0) error("ERROR: Opening of socket failed");

    // Create a sockaddr object with local IP and listening port.
    memset((void *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // Bind to the listening port at localhost.
    if (bind(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR: Binding failed");
    }

    return socket_fd;
}

/**
 * Converts current thread into a listener on given socket.
 */
void start_listener(int socket_fd)
{
    int rc = listen(socket_fd, 8);  // Mark socket as listener.
    if (rc < 0) error("ERROR: Failed to listen on given socket");

    int in_fd;  // File descriptor for incoming connection.
    struct sockaddr_in client_addr;  // Address object of the client.
    socklen_t sock_size = sizeof(client_addr);

    while ((in_fd = accept(socket_fd,
                           (struct sockaddr *) &client_addr,
                           &sock_size)) > -1)
    {
        handle_client(in_fd, client_addr);
    }
}

/**
 * Properly terminates a listener on the given socket.
 */
void destroy_listener(int socket_fd)
{
    close(socket_fd);
}

/**
 * Create a handler on a new thread for the given client connection.
 */
void handle_client(int client_fd, struct sockaddr_in client_addr)
{
    handler_args_t *args = (handler_args_t *) malloc(sizeof(handler_args_t));
    args->socket_fd = client_fd;
    args->addr = client_addr;

    // New thread should be detached, since it's not gonna be joined.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_mutex_lock(list_mutex);

	// Add new handler to the list of active handlers just to get a
	// node reference.
    handler_t *handler = malloc(sizeof(handler_t));
	node_t *node = linked_list_append(handler_fds, (void *) handler);

	args->list_entry = node;  // Pass node reference to new thread.

	// Create new handler thread.
    pthread_t tid;
    pthread_create(&tid, &attr, start_handler, (void *) args);

	// Actually fill the handler object with tid value.
    handler->tid = tid;
    handler->fd = client_fd;

	pthread_mutex_unlock(list_mutex);

    pthread_attr_destroy(&attr);
}

/**
 * Entry point for new handler's thread.
 */
void *start_handler(void *args)
{
    handler_args_t *h_args = (handler_args_t *) args;
    // printf("New connection accepted. FD: %d\n", h_args->socket_fd);

    // Pass to Message Transport Layer for handling.
    handle_client(h_args->socket_fd);

    // Remove handler from list before terminating.
    pthread_mutex_lock(list_mutex);
    handler_t *handler = linked_list_remove(handler_fds, h_args->list_entry);
    free(handler);  // Also keep in mutex, to properly handle server termination.
	pthread_cond_signal(list_size_cond);
	pthread_mutex_unlock(list_mutex);

    // Finally, close the connection to the client.
    close(h_args->socket_fd);

    // Free local resources.
    free(args);

    // printf("Connection closed.\n");
    pthread_exit(0);
}
