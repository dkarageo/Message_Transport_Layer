/**
 * message_generator.h
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * Header file that provides access to routines for creating and managing a
 * message generator.
 *
 * It's suitable for realtime generation of messages for testing
 * capabilities and limits of communication services.
 *
 * Types defined in message_generator.h:
 *  -message_generator_t
 *
 * Routines defined in message_generator.h:
 *  -message_generator_t *
 *   message_generator_create()
 *  -void
 *   message_generator_destroy(message_generator_t *generator)
 *  -void
 *   message_generator_set_message_listener(message_generator_t *generator,
 *                                          void (*callback) (message_t *))
 *  -void
 *   message_generator_add_dest_address(message_generator_t *generator,
 *                                      uint32_t address, uint16_t port)
 *  -int
 *   messsage_generator_start(message_generator_t *generator)
 *  -int
 *   message_generator_stop(message_generator_t *generator)
 *
 * Version: 0.1
 */

#ifndef __message_generator_h__
#define __message_generator_h__

#include <stdint.h>
#include <pthread.h>
#include "message.h"


typedef struct {
    // Callback routine for generates messages.
    void (*handle_message) (message_t *);
    // Thread id running this generator instance.
    pthread_t tid;
    // Boolean flag for defining the state of generator.
    int running;
    // List of IPv4 addresses in binary host byte order format.
    uint32_t *dest_addresses;
    uint16_t *dest_ports;
    // Number of addresses contained in dest_addresses/ports.
    int dest_count;
    int dest_space;  // total allocated space of array
    // State of the PRNG.
    struct random_data *prng_state;
    char *prng_buf;
} message_generator_t;


/**
 * Creates a new message generator object.
 *
 * Returns:
 *  A new message genertor object that can be immediatelly started.
 */
message_generator_t *
message_generator_create();

/**
 * Destroys a message generator object.
 *
 * If given message generator was running, it's terminated before destruction.
 * A destroyed message generator object should never be referenced again.
 *
 * Parameters:
 *  -generator: Message generator object to destroy.
 */
void
message_generator_destroy(message_generator_t *generator);

/**
 * Sets a callback function to be called upon generation of each new message.
 *
 * Parameters:
 *  -generator: Message generator whose callback function will be set.
 *  -callback: A reference to a callback function to be called upon generation
 *          of each new message. A pointer to the new message object is passed
 *          to the callback function. The callback function is responsible for
 *          further managing the message, INCLUDING DESTROYING IT.
 */
void
message_generator_set_message_listener(message_generator_t *generator,
                                       void (*callback) (message_t *));

/**
 * Adds a new destination address to the list of addresses of given message
 * generator.
 *
 * Added destination addresses are used upon message creation. The destination
 * address of each message is randomly picked from the list of addresses. In
 * order to generate messages with fixed destination, just add only one address
 * to the generator.
 *
 * Parameters:
 *  -generator: Message generator on which a new destination address will be
 *          added.
 *  -address: IPv4 address in host byte order.
 *  -port: Port number in host byte order.
 */
void
message_generator_add_dest_address(message_generator_t *generator,
                                   uint32_t address, uint16_t port);

/**
 * Starts the given message generator.
 *
 * Parameters:
 *  -generator: Message generator to be started.
 *
 * Returns:
 *  0 on success, otherwise a non-zero integer.
 */
int
message_generator_start(message_generator_t *generator);

/**
 * Stops the given message generator.
 *
 * Parameters:
 *  -generator: Message generator to be stopped.
 *
 * Returns:
 *  0 on success, otherwise a non-zero integer.
 */
int
message_generator_stop(message_generator_t *generator);


#endif
