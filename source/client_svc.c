/**
 * client_svc.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * An implementation of Message Transport Layer client service. Can be accessed
 * through routines and types defined in client_svc.h
 *
 * Version: 0.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "message.h"
#include "message_generator.h"
#include "linked_list.h"
#include "client_svc.h"


int
_start_sending_messages(client_svc_t *svc);
int
_stop_sending_messages(client_svc_t *svc);
void *
_send_messages(void *args);
int
_start_receiving_messages(client_svc_t *svc);
int
_stop_receiving_messages(client_svc_t *svc);
void *
_receive_messages(void *args);
int
_send_message(client_svc_t *svc, message_t *m);
void
_handle_nacked_message(client_svc_t *svc, message_t *m);
void
_modify_send_rate(client_svc_t *svc, double multiplier);
void
_wait_for_next_send(client_svc_t *svc);


client_svc_t *
client_svc_create()
{
    int rc;

    client_svc_t *svc = (client_svc_t *) malloc(sizeof(client_svc_t));
    if (!svc) goto error;

    svc->out_messages = linked_list_create();
    svc->nacked_out_messages = linked_list_create();
    svc->out_messages_mutex =
        (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    svc->out_messages_exist =
        (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    svc->out_messages_not_full =
        (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    if (!svc->out_messages || !svc->nacked_out_messages ||
        !svc->out_messages_mutex || !svc->out_messages_exist ||
        !svc->out_messages_not_full) goto error;

    rc = pthread_mutex_init(svc->out_messages_mutex, NULL);
    rc |= pthread_cond_init(svc->out_messages_exist, NULL);
    rc |= pthread_cond_init(svc->out_messages_not_full, NULL);
    if (rc) goto error;

    svc->handle_incoming = NULL;
    svc->counter = 0;
    svc->sender_unit_run = 0;
    svc->flow_balance = 0;

    // Default wait interval should be set to 100us.
    svc->flow_delay.tv_sec = 0;
    svc->flow_delay.tv_nsec = 100 * 1000;  // 100 us

    return svc;

error:
    perror("Failed to create client service.");
    client_svc_destroy(svc);
    return NULL;
}


void
client_svc_destroy(client_svc_t *svc)
{
    if (svc) {
        if (svc->out_messages) linked_list_destroy(svc->out_messages);
        if (svc->nacked_out_messages)
            linked_list_destroy(svc->nacked_out_messages);
        if (svc->out_messages_mutex) {
            pthread_mutex_destroy(svc->out_messages_mutex);
            free(svc->out_messages_mutex);
        }
        if (svc->out_messages_exist) {
            pthread_cond_destroy(svc->out_messages_exist);
            free(svc->out_messages_exist);
        }
        if (svc->out_messages_not_full) {
            pthread_cond_destroy(svc->out_messages_not_full);
            free(svc->out_messages_not_full);
        }
        free(svc);
    }
}


int
client_svc_connect(client_svc_t *svc, struct client_svc_cfg *options)
{
    if (!svc) {
        fprintf(stderr, "ERROR: Cannot connect to a NULL service.\n");
        return -1;
    }
    if (!options) {
        fprintf(stderr, "ERROR: No options provided for service connection.\n");
        return -1;
    }

    int rc;

    // Open an IPv4 TCP socket.
    svc->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svc->socket_fd < 0) {
        perror("ERROR opening socket");
        return -1;
    }

    // Bind new socket to provided service port.
    struct sockaddr_in svc_addr;
    svc_addr.sin_family = AF_INET;
    svc_addr.sin_port = htons(options->local_port);
    svc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    rc = bind(svc->socket_fd, (struct sockaddr *) &svc_addr,
              sizeof(struct sockaddr_in));
    if (rc) {
        perror("ERROR binding to provided service port");
        return -1;
    }

    // Resolve address of remote service.
    struct addrinfo *server_info;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_next = NULL;
    rc = getaddrinfo(options->hostname, NULL, &hints, &server_info);
    if (rc) {
        perror("ERROR resolving provided hostname");
        return -1;
    }

    // Extract and fill returned server address with provided remote svc port.
    struct sockaddr_in *server_addr =
            (struct sockaddr_in *) server_info->ai_addr;
    socklen_t addr_len = server_info->ai_addrlen;
    server_addr->sin_port = htons(options->server_port);

    // Connect to server.
    rc = connect(svc->socket_fd, (struct sockaddr *) server_addr, addr_len);
    if (rc < 0) {
        perror("ERROR connecting to remote server");
        return -1;
    }

    // Cleanup temp resources.
    freeaddrinfo(server_info);

    return 0;
}


int
client_svc_start(client_svc_t *svc)
{
    int rc = 0;
    rc |= _start_sending_messages(svc);
    rc |= _start_receiving_messages(svc);
    return rc;
}


int
client_svc_stop(client_svc_t *svc)
{
    // TODO: Disable scheduling of new messages.

    // Wait untill both out buffers are empty.
    for (int i = 0; i < 2; i++) {  // Double check in case of NACKed received.
        pthread_mutex_lock(svc->out_messages_mutex);
        while (linked_list_size(svc->out_messages) > 0 ||
               linked_list_size(svc->nacked_out_messages) > 0)
            pthread_cond_wait(svc->out_messages_not_full, svc->out_messages_mutex);
        pthread_mutex_unlock(svc->out_messages_mutex);
        sleep(1);
    }

    // Ask socket for shutdown.
    shutdown(svc->socket_fd, SHUT_RDWR);

    _stop_receiving_messages(svc);
    _stop_sending_messages(svc);

    // Finally, close the socket.
    close(svc->socket_fd);

    return 0;
}


void
client_svc_schedule_out_message(client_svc_t *svc, message_t *m)
{
    // TODO: Fill these properties in a different place.
    m->src_addr = 0;
    m->src_port = 0;
    m->flags = 0;
    m->len = MESSAGE_DATA_LENGTH;

    pthread_mutex_lock(svc->out_messages_mutex);
    while ((linked_list_size(svc->out_messages) +
            linked_list_size(svc->nacked_out_messages)) >=
                    MAX_OUT_MESSAGES_BUFFER) {
        // DEBUG
        // printf("Waiting to schedule...\n");
        pthread_cond_wait(svc->out_messages_not_full, svc->out_messages_mutex);
        // DEBUG
        // printf("Scheduled. %d remaining\n",
        //        linked_list_size(svc->out_messages));
    }

    m->count = svc->counter;
    svc->counter++;
    linked_list_append(svc->out_messages, m);
    pthread_cond_signal(svc->out_messages_exist);
    pthread_mutex_unlock(svc->out_messages_mutex);
}


void
client_svc_set_incoming_mes_listener(
        client_svc_t *svc,
        void (*callback) (client_svc_t *, message_t *, void *),
        void *arg)
{
    svc->handle_incoming = callback;
    svc->callback_arg = arg;
}


int
_start_sending_messages(client_svc_t *svc)
{
    svc->sender_unit_run = 1;
    return pthread_create(
        &svc->sender_tid, NULL, _send_messages, (void *) svc);
}


int
_stop_sending_messages(client_svc_t *svc)
{
    svc->sender_unit_run = 0;
    pthread_cond_signal(svc->out_messages_exist);
    return pthread_join(svc->sender_tid, NULL);
}


void *
_send_messages(void *args)
{
    client_svc_t *svc = (client_svc_t *) args;

    uint16_t prev_counter = 0;
    int first_message = 1;

    while (svc->sender_unit_run) {
        message_t *m;

        pthread_mutex_lock(svc->out_messages_mutex);

        // DEBUG
        // printf("Acquired mutex for sending message.\n");

        // Pause sender when there are no outgoing messages.
        while (svc->sender_unit_run &&
               linked_list_size(svc->out_messages) == 0 &&
               linked_list_size(svc->nacked_out_messages) == 0)
            pthread_cond_wait(svc->out_messages_exist, svc->out_messages_mutex);
        if (!svc->sender_unit_run) {  // Valid when sender should terminate.
            pthread_mutex_unlock(svc->out_messages_mutex);
            break;
        }
        // DEBUG
        // printf("Found messages in some list.\n");
        // printf("Counter before check %d\n", prev_counter);

        // Pause sender when a NACKed message has been resent, but more messages
        // had been previously sent from the normal stream. Server will NACK
        // all these messages since their order is invalid. Sender should wait
        // until all these messages have been received back and be send again,
        // before it can continue with the normal stream, i.e. when prev_counter
        // is a direct decrement from the next pending message.
        while(1) {
            // If there is a message in the list of pending NACKed messages,
            // it should be send.
            if (linked_list_size(svc->nacked_out_messages)) {
                m = (message_t *) linked_list_pop(svc->nacked_out_messages);
                // DEBUG
                // printf("Picking from NACKed: %d\n", m->count);
                break;
            }

            // If NACKed messages list is empty, we can proceed with normal
            // messages stream. Though, if counter of previous message send is
            // not a direct decrement from current message, we should wait
            // until all intermediate messages are sent back by the server.
            m = (message_t *) linked_list_get_first(svc->out_messages);
            if (svc->sender_unit_run && !first_message &&
                (prev_counter+1)%(MESSAGE_COUNT_MAX+1) != m->count) {

                // DEBUG
                // printf("Waiting for correct order...\n");
                // printf("Message has: %d - Expected: %d\n",
                //        m->count, (prev_counter+1)%(MESSAGE_COUNT_MAX+1));
                // printf("Pending normal messages: %d\n",
                //        linked_list_size(svc->out_messages));
                // printf("Pending nacked messages: %d\n",
                //        linked_list_size(svc->nacked_out_messages));

                pthread_cond_wait(
                    svc->out_messages_exist, svc->out_messages_mutex);
            } else {
                m = linked_list_pop(svc->out_messages);
                // DEBUG
                // printf("Picking from Normal: %d\n", m->count);
                break;
            }
        }
        if (!svc->sender_unit_run) {  // Valid when sender should terminate.
            pthread_mutex_unlock(svc->out_messages_mutex);
            break;
        }

        svc->flow_balance++;
        if (svc->flow_balance >= INCREASE_RATE_AT_CORRECT_NUM) {
            svc->flow_balance = 0;
            _modify_send_rate(svc, RATE_AT_CORRECT);
        }

        pthread_cond_signal(svc->out_messages_not_full);
        // DEBUG
        // printf("Acquired messages from list.\n");
        pthread_mutex_unlock(svc->out_messages_mutex);

        // DEBUG
        // printf("Sending message: %d\n", (prev_counter+1)%MESSAGE_COUNT_MAX);
        _send_message(svc, m);
        // DEBUG
        // printf("Sent message: %d\n", m->count);
        prev_counter = m->count;
        first_message = 0;
        // printf("Updated prev_counter to %d\n", prev_counter);

        message_destroy(m);
        _wait_for_next_send(svc);
    }

    pthread_exit(0);
}


/**
 * Starts messages receiving unit from given socket.
 *
 * Parameters:
 *  -socket_fd: File descriptor of a connected socket to read messages from.
 */
int
_start_receiving_messages(client_svc_t *svc)
{
    return pthread_create(
        &svc->receiver_tid, NULL, _receive_messages, (void *) svc);
}

int
_stop_receiving_messages(client_svc_t *svc)
{
    // Wait for receiver unit to terminate before closing the socket.
    return pthread_join(svc->receiver_tid, NULL);
}

/**
 * Entry point for messages receiving unit.
 */
void *
_receive_messages(void *args)
{
    client_svc_t *svc = (client_svc_t *) args;

    char *in_buffer = (char *) malloc(sizeof(message_t));
    int n;

    while ((n = recv(svc->socket_fd, in_buffer,
                     sizeof(message_t), MSG_WAITALL)) > 0) {
        // If for any reason a message was only partially read, discard it.
        if (n < (int) sizeof(message_t)) {
            fprintf(stderr, "Failed to completely read incoming message.\n");
            continue;
        }

        message_t *message = message_net_to_host(in_buffer);

        if (message->flags) {
            // DEBUG
            // fprintf(stderr, "NACKed message received.\n");
            _handle_nacked_message(svc, message);
        } else if (svc->handle_incoming)
            svc->handle_incoming(svc, message, svc->callback_arg);
    }

    free(in_buffer);
    pthread_exit(0);
}


int
_send_message(client_svc_t *svc, message_t *m)
{
    int rc;

    char *serial_message = message_host_to_net(m);

    ssize_t n = send(
        svc->socket_fd, serial_message, sizeof(message_t), MSG_NOSIGNAL);
    if (n < (ssize_t) sizeof(message_t)) {
        fprintf(stderr, "Failed to send message\n");
        rc = -1;
    } else rc = 0;

    free(serial_message);

    return rc;
}


void
_handle_nacked_message(client_svc_t *svc, message_t *m)
{
    if (m->flags & ERR_TARGET_DOWN)
        fprintf(stderr, "Failed to send message. Destination is offline.\n");

    else if (m->flags & ERR_BUFFER_FULL || m->flags & ERR_INVALID_ORDER) {
        pthread_mutex_lock(svc->out_messages_mutex);
        // NACKed message will be the first to be send.
        linked_list_append(svc->nacked_out_messages, m);
        pthread_cond_signal(svc->out_messages_exist);

        // If the defined number of NACKed messages is reached, then decrease
        // rate. By clearing any positive count, it requires that no NACKed
        // messages have been received in the defined row, in order to increase
        // rate, thus trying to keep NACKed messaged at 0.
        if (svc->flow_balance > 0) svc->flow_balance = 0;
        svc->flow_balance--;
        if (svc->flow_balance <= -DECREASE_RATE_AT_NACKED_NUM) {
            _modify_send_rate(svc, RATE_AT_NACKED);
            svc->flow_balance = 0;
        }

        pthread_mutex_unlock(svc->out_messages_mutex);
    }
}


void
_modify_send_rate(client_svc_t *svc, double multiplier)
{
    // long new_rate = (long) ((double) svc->flow_delay.tv_nsec / multiplier);
    //
    // // Only allow values in the range [1us, 1s].
    // if (new_rate > 999999999) svc->flow_delay.tv_nsec = 999999999;
    // else if (new_rate < 1000) svc->flow_delay.tv_nsec = 1000;
    // else svc->flow_delay.tv_nsec = new_rate;
    //
    // // DEBUG
    // if (multiplier < 1)
    //     printf("Increasing delay to: %ld us\n", svc->flow_delay.tv_nsec / 1000);
    // else
    //     printf("Decreasing delay to: %ld us\n", svc->flow_delay.tv_nsec / 1000);
}

void
_wait_for_next_send(client_svc_t *svc)
{
    // struct timespec rem;
    // rem.tv_sec = 0;
    // rem.tv_nsec = 0;
    // do {
    //     nanosleep(&svc->flow_delay, &rem);
    // } while (rem.tv_sec != 0 || rem.tv_nsec != 0);
}
