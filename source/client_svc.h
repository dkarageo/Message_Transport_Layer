/**
 * client_svc.h
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * Header defining routines and types for creating and accessing a client
 * service for Message Transport Layer. It provides a complete interface
 * for creating a service ready to send and receive messages from a MTL
 * server.
 *
 * Normal usage of client_svc is to start an instance using the following
 * sequence of calls:
 *  1. client_svc_create()
 *  2. client_svc_connect()
 *  3. client_svc_start()
 * Termination is expected to be done with the following sequence:
 *  1. client_svc_stop()
 *  2. client_svc_destroy()
 *
 * Types defined in client_svc.h:
 *  -client_svc_t
 *  -struct client_svc_cfg
 *
 * Routines defined in client_svc.h:
 *  -client_svc_t *
 *   client_svc_create()
 *  -void
 *   client_svc_destroy(client_svc_t *svc)
 *  -int
 *   client_svc_connect(client_svc_t *svc, struct client_svc_cfg *options)
 *  -int
 *   client_svc_start(client_svc_t *svc)
 *  -int
 *   client_svc_stop(client_svc_t *svc)
 *  -void
 *   client_svc_schedule_out_message(client_svc_t *svc, message_t *m)
 *
 * Version: 0.1
 */

#ifndef __client_svc_h__
#define __client_svc_h__

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "linked_list.h"


// Maximum number of pending messages to be send.
#define MAX_OUT_MESSAGES_BUFFER 128

#define DECREASE_RATE_AT_NACKED_NUM 256
#define RATE_AT_NACKED 0.9
#define INCREASE_RATE_AT_CORRECT_NUM 512
#define RATE_AT_CORRECT 1.1


typedef struct Client_Svc client_svc_t;
struct Client_Svc {
    // File descriptor of the socket used for communicating with MTL server.
    int socket_fd;
    pthread_t receiver_tid;  // Thread id of receiver unit.
    pthread_t sender_tid;    // Thread id of sender unit.
    int sender_unit_run;     // Indicates whether sender unit should keep running.
    uint16_t counter;     // Counter for outgoing messages.
    // +SEND/-NACK Indicator for controlling flow rate.
    long int flow_balance;
    // Time for waiting between each successive send.
    struct timespec flow_delay;
    // Callback function for incoming messages.
    void (*handle_incoming) (client_svc_t *svc, message_t *m, void *arg);
    void *callback_arg;
    // List of pending outgoing messages.
    linked_list_t *out_messages;
    linked_list_t *nacked_out_messages;
    pthread_mutex_t *out_messages_mutex;
    pthread_cond_t *out_messages_exist;
    pthread_cond_t *out_messages_not_full;
};

struct client_svc_cfg {
    // Hostname of the remote server on which MTL server is running. It should
    // be a pointer to a NULL terminated string.
    char *hostname;
    // Port on the remote server where MTL service is running in host byte
    // order.
    int16_t server_port;
    // Port on local host to be used for running MTL client service in host
    // byte order.
    int16_t local_port;
};


client_svc_t *
client_svc_create();

void
client_svc_destroy(client_svc_t *svc);

int
client_svc_connect(client_svc_t *svc, struct client_svc_cfg *options);

int
client_svc_start(client_svc_t *svc);

int
client_svc_stop(client_svc_t *svc);

/**
 * Schedules given message for sending.
 *
 * Parameters:
 *  -m: Message to be scheduled for sending.
 */
void
client_svc_schedule_out_message(client_svc_t *svc, message_t *m);

void
client_svc_set_incoming_mes_listener(
        client_svc_t *svc,
        void (*callback) (client_svc_t *, message_t *, void *),
        void *arg);

#endif
