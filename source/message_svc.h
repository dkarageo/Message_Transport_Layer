/**
 * message_svc.h
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A header defining types and routines that implement the Message Transport
 * Layer as a service of TCP server.
 *
 * Types defined in message_svc.h:
 *  -client_t
 *
 * Routines defined in message_svc.h:
 *  -client_t *
 *   client_create(int socket_fd)
 *  -void
 *   client_destroy(client_t *client)
 *  -void
 *   init_svc()
 *  -void
 *   stop_svc()
 *  -void
 *   handle_client(int socket_fd)
 *  -void *
 *   start_sending_unit(void *args)
 *  -void
 *   NACK_message(message_t *m, uint8_t error_code)
 *  -void
 *   send_message(message_t *m)
 *
 * Version: 0.1
 */

#ifndef __message_svc_h__
#define __message_svc_h__


#include <stdint.h>
#include <sys/types.h>
#include "message.h"
#include "linked_list.h"


typedef struct {
    int socket_fd;                // File descriptor of the connected socket to client.
    uint32_t address;             // IPv4 address of the client.
    uint16_t port;                // Port number of the client to send messages.
    linked_list_t *out_messages;  // A list with pending messages to be sent.
    pthread_mutex_t *out_mutex;   // A mutex for out_messages list synchronization.
    // Condition for signaling removal of out message.
    pthread_cond_t *out_message_removed;
    pthread_mutex_t *sock_wr_mutex;  // Mutex for synchronizing socket writing.
} client_t;


struct svc_cfg {
    int enable_logger;  // Boolean flag for enabling logging.
    char *log_fn;       // Path to logfile (valid only if enable_logger == 1).
    int enable_speed_limiter;  // Boolean flag for enabling speed limiter.
    long time_of_step;  // Step in ms to reduce message rate by rate_step
    long max_rate;   // Max messagge rate allowed (messages/sec).
    long min_rate;   // Min message rate allowed (messages/sec).
    long rate_step;  // Step of rate reduction (messages/sec).
};


/**
 * Creates a client object for the client connected to the given socket.
 *
 * Connection should be TCP IPv4 based.
 *
 * Parameters:
 *  -socket_fd : The socket file descriptor of a connected socket to the client.
 *
 * Returns:
 *  On success, a new and fully initialized client object that can be used for
 *  exchanging messages. Upon failure, it returns NULL.
 */
client_t *
client_create(int socket_fd);

/**
 * Destroys a given client object, by releasing all its resources.
 *
 * A destroyed client object should never be used again.
 *
 * Parameters:
 *  -client : A client object to destroy.
 */
void
client_destroy(client_t *client);

/**
* Initializes messaging service.
*/
void
init_svc(struct svc_cfg *options);

/**
 * Entry point for handling new connections (clients).
 *
 * Parameters:
 *  -socket_fd : File descriptor for a connected TCP socket to the client to be
 *          handled.
 */
void
handle_client(int socket_fd);

/**
 * Stops messaging service.
 */
void
stop_svc();

/**
 * Starts message sending unit.
 */
void *
start_sending_unit(void *args);

/**
 * Not ACKnowledges (NACKs) given message back to its source.
 *
 * NACKing a message involves sending the whole message back to its sender.
 * Provided error code should indicate the reason(s) message rejected and
 * is included into the message that is sent back to the sender.
 *
 * Parameters:
 *  -m: Message to be NACKed.
 *  -error_code: Code that indicates the reason(s) message got rejected. It
 *          is consisted of flags described in message.h.
 */
void
NACK_message(message_t *m, uint8_t error_code);

/**
 * Sends message to its recepient.
 *
 * If recepient of the message is offine, the message is automatically NACked
 * to its sender.
 *
 * Parameters:
 *  -m: Message to be send.
 */
void
send_message(message_t *m);


#endif
