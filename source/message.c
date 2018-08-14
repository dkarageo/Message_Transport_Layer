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


char *
message_host_to_net(message_t *m)
{
    message_t *net = (message_t *) malloc(sizeof(message_t));

    net->src_addr = htonl(m->src_addr);
    net->src_port = htons(m->src_port);
    net->dest_addr = htonl(m->dest_addr);
    net->dest_port = htons(m->dest_port);
    net->flags = m->flags;
    net->count = m->count;
    net->len = htons(m->len);
    memcpy(net->data, m->data, MESSAGE_DATA_LENGTH);

    return (char *) net;
}


message_t *
message_net_to_host(char *m)
{
    message_t *host = (message_t *) malloc(sizeof(message_t));

    message_t *mc = (message_t *) m;

    host->src_addr = ntohl(mc->src_addr);
    host->src_port = ntohs(mc->src_port);
    host->dest_addr = ntohl(mc->dest_addr);
    host->dest_port = ntohs(mc->dest_port);
    host->flags = mc->flags;
    host->count = mc->count;
    host->len = ntohs(mc->len);
    memcpy(host->data, mc->data, MESSAGE_DATA_LENGTH);

    return host;
}
