/**
 * message.h
 *
 * A header file defining the structure of a message used by Message Transport
 * Layer.
 *
 * Version: 0.1
 */
#include <stdint.h>

#define MESSAGE_DATA_LENGTH 256

#define ERR_BUFFER_FULL 1    // Error when a message buffer is full.
#define ERR_INVALID_ORDER 2  // Error when received messages are out of order.
#define ERR_TARGET_DOWN 4    // Error when message destination is not active.


typedef struct {
    int32_t src_addr;   // IPv4 address of message's source.
    int16_t src_port;   // Port number of message's source.
    int32_t dest_addr;  // IPv4 address of message's destination.
    int16_t dest_port;  // Port number on which message should be delivered.
    uint8_t flags;      // Error flags.
    uint8_t count;      // Mod8 counter that indicates correct order of messages.
    int16_t len;        // Length of data array in bytes.
    // A byte array containing data of the message.
    char data[MESSAGE_DATA_LENGTH];
} message_t;


/**
 * Converts a message from host byte order to network byte order.
 *
 * Parameters:
 *  -m : A message object in host byte order.
 *
 * Returns:
 *  A serialized message object represented in network byte order. It can
 *  be send as is through the network.
 */
char *
message_host_to_net(message_t *m);

/**
 * Converts a message from network byte order to host byte order.
 *
 * Parameters:
 *  -m : A serialized message object as received through the network.
 *
 * Returns:
 *  A message object represented in byte order of host machine. 
 */
message_t *
message_net_to_host(char *m);
