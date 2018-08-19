#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "message_generator.h"
#include "message.h"


void handle_message(message_t *m);


int count;


int main(int argc, char *argv[])
{
    char *address_txt = "127.0.0.1";
    int32_t address;
    int16_t port = 48000;
    struct in_addr address_net;

    inet_pton(AF_INET, address_txt, &address_net);
    address = ntohl(address_net.s_addr);

    count = 0;
    message_generator_t *generator = message_generator_create();
    message_generator_add_dest_address(generator, address, port);
    message_generator_set_message_listener(generator, handle_message);
    message_generator_start(generator);

    while(count < 10) sleep(1);

    message_generator_stop(generator);
    message_generator_destroy(generator);

    return 0;
}

void handle_message(message_t *m)
{
    char address_txt[INET_ADDRSTRLEN];

    struct in_addr address_net;
    address_net.s_addr = ntohl(m->dest_addr);
    inet_ntop(AF_INET, (void *) &address_net, address_txt, INET_ADDRSTRLEN);
    printf("To %s:%d : %s\n", address_txt, m->dest_port, m->data);

    count++;
}
