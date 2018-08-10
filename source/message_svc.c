#include <arpa/inet.h>
#include <sys/socket.h>
#include "message_svc.h"

#define CLIENT_BUF_LEN 3  // Number of incoming messages to be buffered for
                          // each client.


linked_list_t *clients;          // A list of clients that have pending messages.
pthread_mutex_t *clients_mutex;  // Mutex for synchronizing clients list.
int sending_unit_run;
pthread_t sending_unit_tid;

void
init_svc()
{
    clients = linked_list_create();
    clients_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!clients || !clients_mutex) {
        perror("Failed to initialize messaging service.");
        exit(-1);
    }
    int rc = pthread_mutex_init(clients_mutex);
    if (rc) {
        perror("Failed to initialized messaging service.");
        exit(-1);
    }

    sending_unit_run = 1;
    int rc = pthread_create(
        &sending_unit_tid, NULL, start_sending_unit, (void *) NULL);
    if (rc) {
        perror("Failed to initialize messaging service.");
        exit(-1);
    }
}


void
stop_svc()
{
    sending_unit_run = 0;
    pthread_join(sending_unit_tid);
    pthread_mutex_destroy(clients_mutex);
    free(clients_mutex);
    linked_list_destroy(clients);
}


void
handle_client(int socket_fd)
{
    // Add client to list of active clients.
    client_t *c = client_create(socket_fd);

    // Buffer for raw network data.
    char *in = (char *) malloc(sizeof(message_t));
    int n;
    uint8_t counter = -1;

    // Keep reading incoming messages until the connection is dead.
    while ((n = recv(socket_fd, in, sizeof(message_t), 0)) > 0) {
        message_t *message = message_net_to_host(in);

        uint8_t error_code = 0;  // clear error flags

        // A message is not valid unless its 'count' field is a direct increment
        // from the previous successfully received message.
        if (message->count == (counter+1)%256 || counter == -1) {
            counter = message->count;
        } else error_code |= ERR_INVALID_ORDER

        // If buffer is full, discard message.
        // --- Save a few CPU cycles by not synchronizing check of linked list
        // length. It's better to just NACK a message in the edge case, instead
        // of constantly locking. ---
        if (linked_list_size(c->out_messages) >= CLIENT_BUF_LEN) {
            error_code |= ERR_BUFFER_FULL;
        }

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
                pthread_mutex_lock(clients_mutex);
                linked_list_append(clients, c);
                pthread_mutex_unlock(clients_mutex);
            }

            pthread_mutex_unlock(c->out_mutex);

        } else NACK_message(message, error_code);
    }

    free(in);
    client_destroy(c);
}


void
NACK_message(message_t *m, uint8_t error_code)
{

}


void *
start_sending_unit(void *args)
{
    while(sending_unit_run) {

    }
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
    client->address = ntohl(addr->sin_addr.s_addr);
    client->port = ntohs(addr->sin_port);
    client->out_messages = linked_list_create();
    client->out_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    rc = pthread_mutex_init(client->out_mutex, NULL);
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

    if (mutex) {
        rc = pthread_mutex_destroy(client->out_mutex);
        if (rc) perror("Failed to destroy mutex.");
    }
    if (linked_list) linked_list_destroy(client->out_messages);
    free(client);
}
