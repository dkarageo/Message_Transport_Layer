/**
* message.c
*
* Created by Dimitrios Karageorgiou,
*  for course "Embedded And Realtime Systems".
*  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
*
* An implementation of routines defined in message.h.
*
* Version: 0.1
*/

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "message.h"


message_t *
message_create()
{
    message_t *m = (message_t *) malloc(sizeof(message_t));
    return m;
}


void
message_destroy(message_t *m)
{
    free(m);
}


void *
message_host_to_net_buf(message_t *m, void *buffer)
{
    message_t *net = (message_t *) buffer;

    net->src_addr = htonl(m->src_addr);
    net->src_port = htons(m->src_port);
    net->dest_addr = htonl(m->dest_addr);
    net->dest_port = htons(m->dest_port);
    net->flags = m->flags;
    net->count = htons(m->count);
    net->len = htons(m->len);
    memcpy(net->data, m->data, MESSAGE_DATA_LENGTH);

    return buffer;
}


void *
message_host_to_net(message_t *m)
{
    void *serial = calloc(1, sizeof(message_t));
    return message_host_to_net_buf(m, serial);
}


message_t *
message_net_to_host_buf(void *m, message_t *dest)
{
    message_t *mc = (message_t *) m;

    dest->src_addr = ntohl(mc->src_addr);
    dest->src_port = ntohs(mc->src_port);
    dest->dest_addr = ntohl(mc->dest_addr);
    dest->dest_port = ntohs(mc->dest_port);
    dest->flags = mc->flags;
    dest->count = ntohs(mc->count);
    dest->len = ntohs(mc->len);
    memcpy(dest->data, mc->data, MESSAGE_DATA_LENGTH);

    return dest;
}


message_t *
message_net_to_host(void *m)
{
    message_t *host = (message_t *) malloc(sizeof(message_t));
    return message_net_to_host_buf(m, host);
}
