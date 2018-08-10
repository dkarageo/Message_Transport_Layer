/**
 * message_svc.h
 *
 * A header defining types and routines that implement the Message Transport
 * Layer on top of a tcp server.
 *
 * Version: 0.1
 */

#include <stdint.h>
#include <sys/types.h>


typedef struct {
    uint32_t address;             // IPv4 address of the client.
    uint16_t port;                // Port number of the client to send messages.
    linked_list_t *out_messages;  // A list with pending messages to be sent.
    pthread_mutex_t *out_mutex;   // A mutex for out_messages list synchronization.
} client_t;


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
client_create(int socket_fd)

/**
 * Destroys a given client object, by releasing all its resources.
 *
 * A destroyed client object should never be used again.
 *
 * Parameters:
 *  -client : A client object to destroy.
 */
void
client_destroy(client_t *client)

/**
* Initializes messaging service.
*/
void
init_svc();

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
 * Starts message sending unit.
 */
void
start_sending_unit();

/**
 * Stops messaging service.
 */
void
stop_svc();

/**
 * 
 */
void
NACK_message(message_t *m, uint8_t error_code);
