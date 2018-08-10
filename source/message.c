#include <arpa/inet.h>
#include "message.h"


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
    memcpy(host->data, mc->data, MESSAGE_DATA_LENGTH)

    return host;
}
