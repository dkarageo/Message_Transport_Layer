/**
 * message.h
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A header file defining the structure of a message used by Message Transport
 * Layer and providing routines for managing its lifetime.
 *
 * Routines defines in message.h:
 *  -message_t *
 *   message_create()
 *  -void
 *   message_destroy(message_t *m)
 *  -char *
 *   message_host_to_net(message_t *m)
 *  -message_t *
 *   message_net_to_host(char *m)
 *
 * Version: 0.1
 */

#ifndef __message_h__
#define __message_h__


#include <stdint.h>

#define MESSAGE_DATA_LENGTH 256

#define ERR_BUFFER_FULL 1    // Error when a message buffer is full.
#define ERR_INVALID_ORDER 2  // Error when received messages are out of order.
#define ERR_TARGET_DOWN 4    // Error when message destination is not active.


typedef struct {
    uint32_t src_addr;   // IPv4 address of message's source.
    uint16_t src_port;   // Port number of message's source.
    uint32_t dest_addr;  // IPv4 address of message's destination.
    uint16_t dest_port;  // Port number on which message should be delivered.
    uint8_t flags;      // Error flags.
    uint8_t count;      // Mod8 counter that indicates correct order of messages.
    uint16_t len;        // Length of data array in bytes.
    // A byte array containing data of the message.
    char data[MESSAGE_DATA_LENGTH];
} message_t;


/**
 * Constructs a new message object.
 *
 * Returns:
 *  A newly created message object.
 */
message_t *
message_create();

/**
 * Destroys a message object.
 *
 * After destruction message object can no longer be dereferenced again.
 *
 * Parameters:
 *  -m : Message object to be destroyed.
 */
void
message_destroy(message_t *m);

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


#endif
