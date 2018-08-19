/**
* demo_client.c
*
* Created by Dimitrios Karageorgiou,
*  for course "Embedded And Realtime Systems".
*  Electrical and Computers Engineering Department, AuTh, GR - 2017-20
*
* Usage: exec_name <service_port> <service_hostname> <server_port>
*   where:
*      -service_port : The port on which client will run.
*      -server_hostname : IPv4 address or hostname of server.
*      -server_port : Port number on server.
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "message.h"
#include "client_svc.h"
#include "message_generator.h"


#define INTERACTIVE_MODE 1
#define TEST_MODE 2
#define SEND_TO_ALL 1
#define SEND_TO_RANDOM 2


struct address {
    int32_t ip;
    int16_t port;
};

struct test_client {
    client_svc_t *svc;
    long *received;
}

struct test_client *clients;
int clients_c;
int clients_alloc;

client_svc_t *msg_svc;
long int messages_sent;
long int messages_received;


message_t *
get_user_message();
void
error(const char *msg);
void
parse_received_message(client_svc_t *svc, message_t *m, void *arg);
void
send_dump_message(message_t *m);
struct address *
get_address();
void
start_interactive_mode(char *host, int server_port, int svc_port);
void
start_test_mode(char *host, int server_port, int clients_num,
                int send_mode, long messsages_num);


int
main(int argc, char *argv[])
{
    int rc;

    if (argc < 4) {
       fprintf(
           stderr,
           "Usage: %s server_hostname server_port mode [mode_specific_args]\n",
           argv[0]
       );
       exit(-1);
    }

    char *host = argv[1];
    int server_port = atoi(argv[2]);
    int mode;

    // Load mode of operation.
    if (strcmp(argv[3], "-mode=i") == 0) mode = INTERACTIVE_MODE;
    else if (strcmp(argv[3], "-mode=t") == 0) mode = TEST_MODE;
    else {
        fprintf(stderr, "%s : Invalid mode of operation.\n", argv[3]);
        exit(-1);
    }

    switch (mode) {
    case INTERACTIVE_MODE:
        if (argc < 5) {
            fprintf(stderr, "Please provide service port.\n");
            exit(-1);
        }
        int svc_port = atoi(argv[4]);
        start_interactive_mode(host, server_port, svc_port);
        break;

    case TEST_MODE:
        if (argc < 7) {
            fprintf(
                stderr,
                "Please provide num_of_clients, send_mode, num_messages\n");
        }

        int num_clients = atoi(argv[4]);
        int send_mode;
        if (strcmp(argv[5], "all") == 0) send_mode = SEND_TO_ALL;
        else if (strcmp(argv[5], "random") == 0) send_mode = SEND_TO_RANDOM;
        else {
            fprintf(stderr, "%s : Invalid sending mode.\n", argv[5]);
            exit(-1);
        }
        int messages_num = atoi(argv[7]);
        start_test_mode(
            host, server_port, num_clients, send_mode, messages_num);
        break;
    }

    // int svc_port = atoi(argv[1]);
    //
    // struct client_svc_cfg options;
    // options.hostname = argv[2];
    // options.server_port = server_port;
    // options.local_port = svc_port;
    //
    // client_svc_t *svc = client_svc_create();
    // if (!svc) error("Could not initialize service.");
    // rc = client_svc_connect(svc, &options);
    // if (rc) error("Could not connect to service.");
    // client_svc_start(svc);
    // if (rc) error("Could not start service.");
    // client_svc_set_incoming_mes_listener(svc, parse_received_message);
    // msg_svc = svc;
    //
    // printf("Enter an invalid message to terminate.\n");
    // printf("Please enter a destination address {ip:port}:\n");
    //
    // struct address *addr = get_address();
    // if (!addr) {
    //     printf("No address provided. Terminating...\n");
    //     exit(-1);
    // }
    //
    // messages_sent = 0;
    // messages_received = 0;
    // message_generator_t *generator = message_generator_create();
    // message_generator_set_message_listener(generator, send_dump_message);
    // message_generator_add_dest_address(generator, addr->ip, addr->port);
    // message_generator_start(generator);
    //
    // while (1) {
    //     message_t *m = get_user_message();
    //     if (!m) {
    //         // DEBUG
    //         printf("Terminating due to invalid message.\n");
    //         break;
    //     }
    //     client_svc_schedule_out_message(svc, m);
    // }
    //
    // free(addr);
    // message_generator_stop(generator);
    // message_generator_destroy(generator);
    // client_svc_stop(svc);
    // client_svc_destroy(svc);
    //
    // printf("Sent %ld messages.\n", messages_sent);
    // printf("Received %ld messages.\n", messages_received);
    //
    // // DEBUG
    // printf("Service terminated. Program quitting.\n");

    return 0;
}


void
start_interactive_mode(char *host, int server_port, int svc_port)
{
    int rc;

    struct client_svc_cfg options;
    options.hostname = host;
    options.server_port = server_port;
    options.local_port = svc_port;

    client_svc_t *svc = client_svc_create();
    if (!svc) error("Could not initialize service");
    rc = client_svc_connect(svc, &options);
    if (rc) error("Could not connect to service");
    client_svc_start(svc);
    if (rc) error("Could not start service");
    client_svc_set_incoming_mes_listener(svc, parse_received_message, NULL);
    msg_svc = svc;

    printf("Enter an invalid message to terminate.\n");
    printf("Please enter your messages {ip:port message}:\n");

    while (1) {
        message_t *m = get_user_message();
        if (!m) {
            // DEBUG
            printf("Terminating due to invalid message.\n");
            break;
        }
        client_svc_schedule_out_message(svc, m);
    }

    client_svc_stop(svc);
    client_svc_destroy(svc);

    // DEBUG
    printf("Service terminated. Program quitting.\n");
}


void
start_test_mode(char *host, int server_port, int clients_num,
                int send_mode, long messsages_num)
{
    // Find a range of local ports.

    // Create clients_num clients.

    // Connect clients_num clients to callback routine.

    // If send_mode == SEND_TO_RANDOM 
}


void
error(const char *msg)
{
    perror(msg);
    exit(0);
}


/**
 *
 */
message_t *
get_user_message()
{
    int rc;

    message_t *m = message_create();

    char buffer[512];
    memset(buffer, 0, 512);
    fgets(buffer, 511, stdin);

    char *saveptr;
    char *ip = strtok_r(buffer, ":", &saveptr);
    if (!ip) goto cleanup;
    char *port = strtok_r(NULL, " ", &saveptr);
    if (!port) goto cleanup;
    char *data = strtok_r(NULL, "\n", &saveptr);
    if (!data) goto cleanup;

    // printf("ip: %s port: %s data: %s\n", buffer, port, data);

    struct in_addr dest_ip;
    int16_t dest_port;

    rc = inet_pton(AF_INET, buffer, &dest_ip);
    if (!rc) goto cleanup;
    dest_port = atoi(port);

    m->dest_addr = ntohl(dest_ip.s_addr);
    m->dest_port = dest_port;
    memcpy(m->data, data, MESSAGE_DATA_LENGTH);

    return m;

cleanup:
    message_destroy(m);
    return NULL;
}


struct address *get_address()
{
    struct address *addr = (struct address *) malloc(sizeof(struct address));
    int rc;

    char buffer[512];
    memset(buffer, 0, sizeof(buffer));
    fgets(buffer, sizeof(buffer), stdin);

    char *saveptr;
    char *ip = strtok_r(buffer, ":", &saveptr);
    if (!ip) goto error;
    char *port = strtok_r(NULL, "\n", &saveptr);
    if (!port) goto error;

    struct in_addr dest_ip;
    rc = inet_pton(AF_INET, ip, &dest_ip);
    if (!rc) goto error;

    addr->ip = ntohl(dest_ip.s_addr);
    addr->port = atoi(port);

    return addr;

error:
    free(addr);
    return NULL;
}

void
parse_received_message(client_svc_t *svc, message_t *m, void *arg)
{
    // DEBUG
    char src_ip[INET_ADDRSTRLEN];
    char dest_ip[INET_ADDRSTRLEN];

    struct in_addr src_addr;
    struct in_addr dest_addr;
    src_addr.s_addr = htonl(m->src_addr);
    dest_addr.s_addr = htonl(m->dest_addr);
    inet_ntop(AF_INET, &src_addr, src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &dest_addr, dest_ip, INET_ADDRSTRLEN);

    char print_buffer[MESSAGE_DATA_LENGTH+1];
    memcpy(print_buffer, m->data, MESSAGE_DATA_LENGTH);

    // printf("Receiving from %s:%d --> %s\n", src_ip, m->src_port, print_buffer);

    message_destroy(m);

    messages_received++;
    if ((messages_received % 1000) == 0)
        printf("%ld messages received\n", messages_received);
}


void
send_dump_message(message_t *m)
{
    client_svc_schedule_out_message(msg_svc, m);

    struct timespec time_to_wait;
    time_to_wait.tv_sec = 0;
    time_to_wait.tv_nsec = 10 * 1000;  // 10μs
    nanosleep(&time_to_wait, NULL);

    messages_sent++;
    if ((messages_sent % 1000) == 0)
        printf("%ld messages sent\n", messages_sent);
}
