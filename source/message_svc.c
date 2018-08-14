/**
 * message_svc.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * An implementation of server-side module of Message Transport Layer. It is
 * implemented as a service of a TCP server.
 *
 * Version: 0.1
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <error.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "message_svc.h"

#define CLIENT_BUF_LEN 3  // Number of incoming messages to be buffered for
                          // each client.


// A list containing all connected clients.
linked_list_t *clients;
pthread_mutex_t *clients_mutex;  // clients list corresponding mutex

// A list of clients that have pending messages.
linked_list_t *active_clients;
pthread_mutex_t *active_clients_mutex;  // active clients corresponding mutex

// Defines whether sending unit should keep running or terminate.
int sending_unit_run;
// Thread id of thread that runs sending unit.
pthread_t sending_unit_tid;
// Condition for blocking sending unit when no outgoing messages are available.
pthread_cond_t *messages_exist_cond;
pthread_mutex_t *messages_exist_mutex;


void
define_sender(message_t *m, client_t *client);


void
init_svc()
{
    int rc;

    // Initialize list for keeping all connected clients.
    clients = linked_list_create();
    clients_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!clients || !clients_mutex) goto error;
    if (pthread_mutex_init(clients_mutex, NULL)) goto error;

    // Initialize list for keeping clients with pending outgoing messages.
    active_clients = linked_list_create();
    active_clients_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!active_clients || !active_clients_mutex) goto error;
    if (pthread_mutex_init(active_clients_mutex, NULL)) goto error;

    // Initialize tools for signaling sender unit that outgoing messages exist.
    messages_exist_cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    messages_exist_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!messages_exist_cond || !messages_exist_mutex) goto error;
    if (pthread_cond_init(messages_exist_cond, NULL)) goto error;
    if (pthread_mutex_init(messages_exist_mutex, NULL)) goto error;

    // Start sending unit.
    sending_unit_run = 1;
    rc = pthread_create(
        &sending_unit_tid, NULL, start_sending_unit, (void *) NULL);
    if (rc) goto error;

    return;

error:
    perror("Failed to initialize messaging service.");
    exit(-1);
}


void
stop_svc()
{
    // Ask sender unit to terminate and wait until it terminates.
    sending_unit_run = 0;
    pthread_cond_signal(messages_exist_cond);  // Cause it to stop waiting for messages.
    pthread_join(sending_unit_tid, NULL);

    // Clean-up global allocated resources.
    pthread_mutex_destroy(clients_mutex);
    pthread_mutex_destroy(active_clients_mutex);
    pthread_mutex_destroy(messages_exist_mutex);
    pthread_cond_destroy(messages_exist_cond);
    free(clients_mutex);
    free(active_clients_mutex);
    free(messages_exist_mutex);
    free(messages_exist_cond);
    linked_list_destroy(active_clients);
}


void
handle_client(int socket_fd)
{
    client_t *c = client_create(socket_fd);

    // Add new client to list of connected clients.
    pthread_mutex_lock(clients_mutex);
    node_t *c_ref = linked_list_append(clients, c);
    pthread_mutex_unlock(clients_mutex);

    // Buffer for raw network data.
    char *in = (char *) malloc(sizeof(message_t));
    int n;
    uint8_t counter = 0;  // Local counter that should match incoming messages.
    uint8_t first_message = 1;  // Flag for first message to ignore counter.

    // Keep reading incoming messages until the connection is dead.
    while ((n = recv(socket_fd, in, sizeof(message_t), 0)) > 0) {
        message_t *message = message_net_to_host(in);
        define_sender(message, c);  // fill sender fields of message

        uint8_t error_code = 0;  // clear error flags

        // A message is not valid unless its 'count' field is a direct increment
        // from the previous successfully received message.
        if (message->count == (counter+1)%256 || first_message) {
            first_message = 0;
            counter = message->count;
        } else error_code |= ERR_INVALID_ORDER;

        // If buffer is full, discard message.
        // --- Save a few CPU cycles by not synchronizing check of linked list
        // length. It's better to just NACK a message in the edge case, instead
        // of constantly locking. ---
        if (linked_list_size(c->out_messages) >= CLIENT_BUF_LEN)
            error_code |= ERR_BUFFER_FULL;

        if (!error_code) {
            // If no error, push message to pending outgoing messages of this
            // client.
            pthread_mutex_lock(c->out_mutex);
            int had_messages = linked_list_size(c->out_messages);
            linked_list_append(c->out_messages, message);

            // If there were no messages in out_messages list, then this client
            // had been removed from active clients list by the sending unit.
            // So, add it again and signal sender unit.
            if (!had_messages) {
                pthread_mutex_lock(active_clients_mutex);
                linked_list_append(active_clients, c);
                pthread_mutex_unlock(active_clients_mutex);
                pthread_cond_signal(messages_exist_cond);
            }

            pthread_mutex_unlock(c->out_mutex);

        } else NACK_message(message, error_code);
    }

    // Remove client from list of connected clients.
    pthread_mutex_lock(clients_mutex);
    linked_list_remove(clients, c_ref);
    pthread_mutex_unlock(clients_mutex);

    free(in);
    client_destroy(c);
}


void *
start_sending_unit(void *args)
{
    while(sending_unit_run) {
        message_t *message;  // Next message to be send.

        pthread_mutex_lock(active_clients_mutex);
        while (linked_list_size(active_clients) < 1 && sending_unit_run)
            pthread_cond_wait(messages_exist_cond, active_clients_mutex);
        if (!sending_unit_run) { // Required for termination request.
            pthread_mutex_unlock(active_clients_mutex);
            break;
        }

        // Select the first client with a pending outgoing message.
        client_t *selected = (client_t *) linked_list_pop(active_clients);
        pthread_mutex_lock(selected->out_mutex);
        message = (message_t *) linked_list_pop(selected->out_messages);

        // If there are pending messages from this client, push it to the back.
        // A simple one-message round-robin scheduling is used.
        if (linked_list_size(selected->out_messages) > 0)
            linked_list_append(active_clients, (void *) selected);

        pthread_mutex_unlock(selected->out_mutex);
        pthread_mutex_unlock(active_clients_mutex);

        send_message(message);
        message_destroy(message);  // Once handled, message is no longer needed.
    }

    return 0;
}


void
NACK_message(message_t *m, uint8_t error_code)
{
    printf("NACK message\n");

    m->flags |= error_code;

    client_t *src = NULL;

    // Acquire the source of the message.
    iterator_t * it = linked_list_iterator(clients);
    while(iterator_has_next(it)) {
        client_t *c = iterator_next(it);
        if (m->src_addr == c->address && m->src_port == c->port) {
            src = c;
            break;
        }
    }

    // If the source has gone offline, it's impossible to NACK the message.
    if (src) {
        char *out_buffer = message_host_to_net(m);
        pthread_mutex_lock(src->sock_wr_mutex);
        int n = write(src->socket_fd, out_buffer, sizeof(message_t));
        pthread_mutex_unlock(src->sock_wr_mutex);
        if (n < (int) sizeof(message_t))
            fprintf(stderr, "Failed to sent NACK message.\n");
        free(out_buffer);
    }
}


void
send_message(message_t *m)
{
    client_t *dest = NULL;

    // Search all connected clients for the requested destination.
    iterator_t * it = linked_list_iterator(clients);
    while(iterator_has_next(it)) {
        client_t *c = iterator_next(it);
        if (m->dest_addr == c->address && m->dest_port == c->port) {
            dest = c;
            break;
        }
    }

    // If there is a connected client that matches destination ip and port of
    // message, send it the message. Otherwise, NACK it.
    if (dest) {
        char *out_buffer = message_host_to_net(m);
        pthread_mutex_lock(dest->sock_wr_mutex);
        int n = write(dest->socket_fd, out_buffer, sizeof(message_t));
        pthread_mutex_unlock(dest->sock_wr_mutex);
        if (n < (int) sizeof(message_t))
            fprintf(stderr, "Failed to sent message.\n");
        free(out_buffer);

    } else NACK_message(m, ERR_TARGET_DOWN);

    // DEBUG code.
    char src_ip[INET_ADDRSTRLEN];
    char dest_ip[INET_ADDRSTRLEN];

    struct in_addr src_addr;
    struct in_addr dest_addr;
    src_addr.s_addr = htonl(m->src_addr);
    dest_addr.s_addr = htonl(m->dest_addr);
    inet_ntop(AF_INET, &src_addr, src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &dest_addr, dest_ip, INET_ADDRSTRLEN);

    printf("Sending message from %s:%d to %s:%d\n",
           src_ip, m->src_port,
           dest_ip, m->dest_port);
    // DEBUG end.
}


void
define_sender(message_t *m, client_t *client)
{
    m->src_addr = client->address;
    m->src_port = client->port;
}


client_t *
client_create(int socket_fd)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int rc;

    // Acquire a sockaddr_in object to get client's IPv4 address.
    rc = getpeername(socket_fd, (struct sockaddr *) &addr, &addr_len);
    if (rc) {
        perror("client_create() failed");
        return NULL;
    }
    if (addr_len > sizeof(struct sockaddr_in)) {
        printf("Invalid protocol.\n");
        return NULL;
    }

    client_t *client = (client_t *) malloc(sizeof(client_t));
    if (!client) {
        perror("client_create() failed");
        return NULL;
    }
    client->socket_fd = socket_fd;
    client->address = ntohl(addr.sin_addr.s_addr);
    client->port = ntohs(addr.sin_port);
    client->out_messages = linked_list_create();
    client->out_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    client->sock_wr_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!client->out_mutex || !client->sock_wr_mutex) {
        perror("client_create() failed");
        return NULL;
    }

    rc = pthread_mutex_init(client->out_mutex, NULL);
    rc |= pthread_mutex_init(client->sock_wr_mutex, NULL);
    if (rc) {
        perror("client_create() failed to init a mutex");
        client_destroy(client);  // Clean all allocated memory before return.
        return NULL;
    }

    return client;
}


void
client_destroy(client_t *client)
{
    if (!client) return;

    int rc;

    if (client->out_mutex) {
        rc = pthread_mutex_destroy(client->out_mutex);
        if (rc) perror("Failed to destroy mutex.");
        free(client->out_mutex);
    }
    if (client->sock_wr_mutex) {
        rc = pthread_mutex_destroy(client->sock_wr_mutex);
        if (rc) perror("Failed to destroy mutex.");
        free(client->sock_wr_mutex);
    }
    if (client->out_messages) linked_list_destroy(client->out_messages);
    free(client);
}
