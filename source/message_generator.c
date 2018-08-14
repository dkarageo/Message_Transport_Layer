/**
 * message_generator.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * An implementation of routines defined in message_generator.h.
 *
 * Version: 0.1
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "message_generator.h"


void *
_message_generator_enter(void *args);
int
_rand_lim(int limit, struct random_data *state);


// Content of newly created messages.
const char message_content[] = "This is a fixed testing message.";


message_generator_t *
message_generator_create()
{
    message_generator_t *g =
        (message_generator_t *) malloc(sizeof(message_generator_t));
    g->dest_addresses = (uint32_t *) malloc(sizeof(uint32_t));
    g->dest_ports = (uint16_t *) malloc(sizeof(uint16_t));
    g->dest_space = 1;
    g->dest_count = 0;

    // Init PRNG.
    g->prng_state = (struct random_data *) malloc(sizeof(struct random_data));
    g->prng_buf = (char *) malloc(sizeof(char) * 128);
    g->prng_state->state = NULL;
    initstate_r((unsigned int) time(NULL), g->prng_buf, 128, g->prng_state);
    setstate_r(g->prng_buf, g->prng_state);
    srandom_r((unsigned int) time(NULL), g->prng_state);

    return g;
}

void
message_generator_destroy(message_generator_t *generator)
{
    message_generator_stop(generator);

    if (generator->dest_addresses) free(generator->dest_addresses);
    if (generator->dest_ports) free(generator->dest_ports);
    if (generator->prng_state) free(generator->prng_state);
    if (generator->prng_buf) free(generator->prng_buf);
    if (generator) free(generator);
}

void
message_generator_set_message_listener(message_generator_t *generator,
                                       void (*callback) (message_t *))
{
    generator->handle_message = callback;
}

void
message_generator_add_dest_address(message_generator_t *generator,
                                   uint32_t address, uint16_t port)
{
    // If list is full, increase its allocated size.
    if (generator->dest_space == generator->dest_count) {
        generator->dest_addresses = (uint32_t *) realloc(
            generator->dest_addresses, generator->dest_space*2*sizeof(uint32_t)
        );
        generator->dest_ports = (uint16_t *) realloc(
            generator->dest_ports, generator->dest_space*2*sizeof(uint16_t)
        );
        generator->dest_space *= 2;
    }

    generator->dest_addresses[generator->dest_count] = address;
    generator->dest_ports[generator->dest_count] = port;
    generator->dest_count++;
}

int
message_generator_start(message_generator_t *generator)
{
    generator->running = 1;
    return pthread_create(
            &generator->tid, NULL,
            _message_generator_enter, (void *) generator);
}

int
message_generator_stop(message_generator_t *generator)
{
    generator->running = 0;
    return pthread_join(generator->tid, NULL);
}

void *
_message_generator_enter(void *args)
{
    message_generator_t *g = (message_generator_t *) args;

    while (g->running) {
        if (!g->handle_message) {
            fprintf(
                stderr,
                "ERROR: Message generator found no target for new message.\n");
            sleep(1);
            continue;
        }

        message_t *m = message_create();

        // Pick a random destination.
        int i = _rand_lim(g->dest_count-1, g->prng_state);
        m->dest_addr = g->dest_addresses[i];
        m->dest_port = g->dest_ports[i];
        // Fill the data field with prefixed data.
        memset(m->data, 0, MESSAGE_DATA_LENGTH);
        memcpy(m->data, message_content, sizeof(message_content));

        g->handle_message(m);
    }

    return 0;
}

int
_rand_lim(int limit, struct random_data *state)
{
    int divisor = RAND_MAX / (limit+1);
    int32_t retval;

    do {
        random_r(state, &retval);
        retval = retval / divisor;
    } while (retval > limit);

    return retval;
}
