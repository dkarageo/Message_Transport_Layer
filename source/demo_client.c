/**
* demo_client.c
*
* Created by Dimitrios Karageorgiou,
*  for course "Embedded And Realtime Systems".
*  Electrical and Computers Engineering Department, AuTh, GR - 2017-20
*
* A demo client that provides various testing modes for MTL. It can be run
* on interactive mode and testing mode.
*
* Usage: ./exec_name <server_hostname> <server_port> -mode=<mode>
*                    [...mode_specific_args...]
*   where:
*      -server_hostname : IPv4 address in dot format or hostname of server.
*      -server_port : Port number on server where MTL service is running.
*      -mode : 'i' for interactive mode, 't' for testing mode
*      -mode_specific_args:
*           +interactive mode: ... <port>
*               where:
*                   -port : Port which will be used by demo client.
*           +testing mode: .... <clients_num> <send_mode> <messages_num> <if_ip>
*               where:
*                   -clients_num : Number of clients to be started.
*                   -send_mode : 'all' for send to all, 'random' for send to random
*                   -messages_num : Number of messages to be send by a client
*                           to each of its targets.
*                   -if_ip : IP assigned to the interface which will be used
*                           for communicating with the server. It should be
*                           the IP visible to the server. If device is behind
*                           a NAT, the public IP of the NAT should be provided.
*
* Version: 0.1
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
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
    // Messaging service of the client.
    client_svc_t *svc;
    // Generator for sending dummy messages.
    message_generator_t *gen;
    // IPv4 of the network interface used for communicating with remote
    // messaging service.
    uint32_t ip;
    // Local port used by this client instance.
    uint16_t port;

    // The lowest port number contained in targets list, used for hashing.
    int start_port;
    // A list of references to all the clients this client will receive
    // messages from.
    struct test_client **targets;  // TODO: Use it
    long *prev_counters;

    // Total number of messages received.
    long received;
    // Total number of messages expected to be received.
    long expected;
    // Indicates whether an error detected on order of received messages.
    int error;
    // Indicates whether this testing client has finished all expected
    // communications successfully (sending/receiving all test messages).
    int finished;
};


struct test_client *clients;     // List of active clients.
pthread_mutex_t *clients_mutex;
pthread_cond_t *client_finished;

client_svc_t *msg_svc;  // Messaging service (valid on interactive mode).


message_t *
get_user_message();
void
error(const char *msg);
void
parse_received_message(client_svc_t *svc, message_t *m, void *arg);
void
send_dump_message(message_t *m, void *arg);
void
interactive_mode(char *host, int server_port, int svc_port);
void
test_mode(char *hostname, int server_port,
          char *if_ip, int lp_start, int clients_num,
          int send_mode, long messages_num);
int
_rand_lim(int limit, struct random_data *state);
double
get_elapsed_time(struct timespec start, struct timespec stop);


int
main(int argc, char *argv[])
{
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
        interactive_mode(host, server_port, svc_port);
        break;

    case TEST_MODE:
        if (argc < 8) {
            fprintf(
                stderr,
                "Please provide num_of_clients, send_mode, num_messages interface_ip\n");
        }

        int num_clients = atoi(argv[4]);
        int send_mode;
        if (strcmp(argv[5], "all") == 0) send_mode = SEND_TO_ALL;
        else if (strcmp(argv[5], "random") == 0) send_mode = SEND_TO_RANDOM;
        else {
            fprintf(stderr, "%s : Invalid sending mode.\n", argv[5]);
            exit(-1);
        }
        int messages_num = atoi(argv[6]);
        char *if_ip = argv[7];
        test_mode(host, server_port, if_ip, 48000,
                  num_clients, send_mode, messages_num);
        break;
    }

    return 0;
}


void
interactive_mode(char *host, int server_port, int svc_port)
{
    // TODO: Make use of testing mode structures, for uniformity of both modes.
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
            break;
        }
        client_svc_schedule_out_message(svc, m);
    }

    client_svc_stop(svc);
    client_svc_destroy(svc);

    printf("Client quitting...\n");
}


void
test_mode(char *hostname, int server_port,
          char *if_ip, int lp_start, int clients_num,
          int send_mode, long messages_num)
{
    int rc;
    struct timespec start, stop;

    // Init global resources.
    clients = (struct test_client *) calloc(
         clients_num, sizeof(struct test_client));
    if (!clients) error("Failed to allocate memory for given number of clients");

    clients_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    client_finished = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    if (!clients_mutex || !client_finished)
        error("Failed to allocate memory for sync structures");
    if (pthread_mutex_init(clients_mutex, NULL) ||
        pthread_cond_init(client_finished, NULL))
        error("Failed initializing sync structures");

    // Convert given interface IPv4 to binary.
    uint32_t if_ip_bin;
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    rc = getaddrinfo(if_ip, NULL, &hints, &res);
    if (rc) error("Could not resolve hostname");
    if_ip_bin = ntohl(((struct sockaddr_in *) res->ai_addr)->sin_addr.s_addr);
    freeaddrinfo(res);

    // Init a PRNG.
    struct random_data *prng_state =
        (struct random_data *) malloc(sizeof(struct random_data));
    char *prng_buf = (char *) malloc(sizeof(char) * 128);
    prng_state->state = NULL;
    initstate_r((unsigned int) time(NULL), prng_buf, 128, prng_state);
    setstate_r(prng_buf, prng_state);
    srandom_r((unsigned int) time(NULL), prng_state);

    // Init each test client struct.
    for (int i = 0; i < clients_num; i++) {
        // In any case allocate clients_num targets for easy hashing.
        clients[i].targets = (struct test_client **) malloc(
            sizeof(struct test_client *) * clients_num);
        clients[i].prev_counters = (long *) malloc(clients_num * sizeof(long));
        if (!clients[i].targets || !clients[i].prev_counters)
            error("Failed initializing test clients");

        for (int j = 0; j < clients_num; j++) clients[i].prev_counters[j] = -1;
        clients[i].received = 0;
        clients[i].finished = 0;
        clients[i].error = 0;
    }

    // Try to find clients_num successive ports available, starting
    // from lp_start port and until spanning a range of 1024 ports.
    int r = 0;
    while ((r * clients_num) < 1024) {
        int range_start = lp_start + clients_num * r;
        for (int i = 0; i < clients_num; i++) {

            struct client_svc_cfg options;
            options.hostname = hostname;
            options.server_port = server_port;
            options.local_port = range_start + i;

            client_svc_t *svc = client_svc_create();
            if (!svc) error("Could not initialize service");
            rc = client_svc_connect(svc, &options);
            if (rc) break;
            client_svc_start(svc);
            if (rc) error("Could not start service");

            clients[i].svc = svc;
            clients[i].ip = if_ip_bin;
            clients[i].port = range_start + i;
            clients[i].start_port = range_start;

            client_svc_set_incoming_mes_listener(
                svc, parse_received_message, clients+i);
        }
        if (!rc) break;  // Successive ports found.
        r++;
    }
    if (rc) error("Failed to find available ports");

    // Create a generator for each client.
    for (int i = 0; i < clients_num; i++) {
        // On random mode, number of messages each client will received is not
        // known, so start with 0 and increment. Also consider the service
        // finished, and enable it when the first sender to it is added.
        clients[i].expected =
            send_mode == SEND_TO_ALL ? (clients_num-1)*messages_num : 0;
        if (send_mode == SEND_TO_RANDOM) clients[i].finished = 1;

        clients[i].gen = message_generator_create();
        if (!clients[i].gen) error("Failed to create message generator");

        message_generator_set_message_listener(
            clients[i].gen, send_dump_message, clients+i);

        // Hashing based on port num.
        int target_i = clients[i].port - clients[i].start_port;

        if (send_mode == SEND_TO_ALL) {
            // On SEND_TO_ALL mode, just add the addresses of all created
            // clients (except itself) as destination addresses.
            for (int d = 0; d < clients_num; d++) {
                if (d == i) continue;  // Don't add self as destination.

                message_generator_add_dest_address(
                    clients[i].gen, clients[d].ip, clients[d].port);
                clients[d].targets[target_i] = clients + i;
            }
        } else if (send_mode == SEND_TO_RANDOM) {
            // On SEND_TO_RANDOM mode, pick a destination randomly out of all
            // created clients (except itself).
            int rand_i;
            do {
                rand_i = _rand_lim(clients_num-1, prng_state);
            } while(rand_i == i);

            message_generator_add_dest_address(
                clients[i].gen, clients[rand_i].ip, clients[rand_i].port);
            clients[rand_i].targets[target_i] = clients + i;
            clients[rand_i].expected += messages_num;
            clients[rand_i].finished = 0;
        }
    }

    sleep(1);

    clock_gettime(CLOCK_MONOTONIC, &start);

    // Start generators.
    for (int i = 0; i < clients_num; i++) {
        struct message_generator_cfg gen_cfg;
        gen_cfg.stop_count = messages_num;
        int rc = message_generator_start(clients[i].gen, &gen_cfg);
        if (rc) error("Could not start generator");
    }

    // Wait for messages to be exchanged.
    pthread_mutex_lock(clients_mutex);
    for (int i = 0; i < clients_num; i++) {
        // No need for specific check of which client finished (and signaled).
        // Just make sure to signal with mutex acquired.
        while(!clients[i].finished)
            pthread_cond_wait(client_finished, clients_mutex);
    }
    pthread_mutex_unlock(clients_mutex);

    clock_gettime(CLOCK_MONOTONIC, &stop);

    // Check if messages exchanged successfully.
    int error = 0;
    long exchanged = 0;
    for (int i = 0; i < clients_num; i++) {
        error |= clients[i].error;
        exchanged += clients[i].received;
    }
    printf("TEST %s\n", !error ? "PASSED" : "FAILED");
    printf("SEND MODE: %s\n", send_mode == SEND_TO_ALL ? "TO_ALL" : "TO_RANDOM");

    double elapsed = get_elapsed_time(start, stop);
    double mes_rate = (double) exchanged / elapsed;
    double data_rate = mes_rate * MESSAGE_DATA_LENGTH / 1024 / 1024;
    printf("%ld messages exchanged\n", exchanged);
    printf("Elapsed time: %.2f secs\n", elapsed);
    printf("Rate: %.2f messages/sec\n", mes_rate);
    printf("Data Rate: %.2f MB/s\n", data_rate);

    // Free resources.
    for (int i = 0; i < clients_num; i++) {
        message_generator_destroy(clients[i].gen);
        client_svc_stop(clients[i].svc);
        client_svc_destroy(clients[i].svc);
        free(clients[i].targets);
        free(clients[i].prev_counters);
    }
    pthread_mutex_destroy(clients_mutex);
    pthread_cond_destroy(client_finished);
    free(prng_state);
    free(prng_buf);
    free(clients_mutex);
    free(client_finished);
    free(clients);
}


void
error(const char *msg)
{
    perror(msg);
    exit(0);
}


/**
 * Reads a message in "ip:port message" format from stdin.
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


/**
 * Callback routine for received messages.
 */
void
parse_received_message(client_svc_t *svc, message_t *m, void *arg)
{
    struct test_client *c = (struct test_client *) arg;

    if (c) {  // Valid on testing modes.
        int sender_i = m->src_port - c->start_port;

        // Verify integrity of message parameters.
        int cur_error = 0;
        cur_error |= m->dest_addr != c->ip;
        cur_error |= m->dest_port != c->port;
        cur_error |= m->src_addr != c->targets[sender_i]->ip;
        cur_error |= m->src_port != c->targets[sender_i]->port;
        if (cur_error) {
            fprintf(stderr, "FAILED: Could not verify incoming message parameters.\n");
            c->error = 1;
        }

        // Verify that message arrived in correct order.
        char *saveptr;
        char *counter_txt = strtok_r(m->data, ":", &saveptr);
        long counter = atol(counter_txt);
        if (counter == c->prev_counters[sender_i]+1) {
            c->prev_counters[sender_i] = counter;
        } else {
            fprintf(stderr, "FAILED: Incoming message arrived in wrong order.\n");
            c->error = 1;
        }

        // If expected number of messages received or an error occured,
        // then indicate that this client is no longer needed and signal
        // all waiting threads.
        c->received++;
        if (c->expected == c->received || c->error) {
            int rc = pthread_mutex_lock(clients_mutex);
            if (rc) error("Failed to acquire clients testing mutex");
            c->finished = 1;
            pthread_cond_signal(client_finished);
            pthread_mutex_unlock(clients_mutex);
        }

    } else {  // If no client struct provided, just print incoming message.
              // Valid on interactive mode.

        char src_ip[INET_ADDRSTRLEN];
        // char dest_ip[INET_ADDRSTRLEN];

        struct in_addr src_addr;
        // struct in_addr dest_addr;
        src_addr.s_addr = htonl(m->src_addr);
        // dest_addr.s_addr = htonl(m->dest_addr);
        inet_ntop(AF_INET, &src_addr, src_ip, INET_ADDRSTRLEN);
        // inet_ntop(AF_INET, &dest_addr, dest_ip, INET_ADDRSTRLEN);

        char data_txt[MESSAGE_DATA_LENGTH+1];
        memcpy(data_txt, m->data, MESSAGE_DATA_LENGTH);

        printf("Receiving from %s:%d --> %s\n", src_ip, m->src_port, data_txt);
    }

    message_destroy(m);
}


/**
 * Callback routine for generators.
 */
void
send_dump_message(message_t *m, void *arg)
{
    struct test_client *c = (struct test_client *) arg;
    client_svc_t *svc;

    if (!c) svc = msg_svc;  // Compatibility with interactive mode.
    else svc = c->svc;

    client_svc_schedule_out_message(svc, m);

    struct timespec time_to_wait;
    time_to_wait.tv_sec = 0;
    time_to_wait.tv_nsec = 10 * 1000;  // 10μs
    nanosleep(&time_to_wait, NULL);
}


/**
 * Generate a random number in range [0, limit].
 */
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


/**
 * Get elapsed time in seconds between two timestamps.
 */
double
get_elapsed_time(struct timespec start, struct timespec stop)
{
    double elapsed_time = (stop.tv_sec - start.tv_sec) * 1.0;
    elapsed_time += (stop.tv_nsec - start.tv_nsec) / 1000000000.0;
    return elapsed_time;
}


// struct address *
// get_address()
// {
//     struct address *addr = (struct address *) malloc(sizeof(struct address));
//     int rc;
//
//     char buffer[512];
//     memset(buffer, 0, sizeof(buffer));
//     fgets(buffer, sizeof(buffer), stdin);
//
//     char *saveptr;
//     char *ip = strtok_r(buffer, ":", &saveptr);
//     if (!ip) goto error;
//     char *port = strtok_r(NULL, "\n", &saveptr);
//     if (!port) goto error;
//
//     struct in_addr dest_ip;
//     rc = inet_pton(AF_INET, ip, &dest_ip);
//     if (!rc) goto error;
//
//     addr->ip = ntohl(dest_ip.s_addr);
//     addr->port = atoi(port);
//
//     return addr;
//
// error:
//     free(addr);
//     return NULL;
// }
