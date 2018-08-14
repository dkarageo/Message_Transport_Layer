/**
 * client.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A simple TCP client.
 *
 * Usage: exec_name <service_port> <service_hostname> <server_port>
 *   where:
 *      -service_port : The port on which client will run.
 *      -server_hostname : IPv4 address or hostname of server.
 *      -server_port : Port number on server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "message.h"
#include "message_generator.h"
#include "linked_list.h"


message_t *get_user_message();
int send_message(int sock_fd, message_t *m);
void error(const char *msg);
void start_receiving_messages(int socket_fd);
void *receive_messages(void *args);
void add_new_out_message(message_t *m);
void handle_nacked_message(int socket_fd, message_t *m);
void start_sending_messages(int socket_fd);
void stop_sending_messages();
void *send_messages(void *args);


pthread_t receiver_tid;  // Thread id of receiver unit.
pthread_t sender_tid;    // Thread id of sender unit.
int sender_unit_run;
uint8_t counter = 0;     // Counter for outgoing messages.
linked_list_t *out_messages;  // List of pending messages to be send.
pthread_mutex_t *out_messages_mutex;
pthread_cond_t *out_messages_exist;


int
main(int argc, char *argv[])
{
    int sockfd, svc_port, server_port;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int rc;

    if (argc < 4) {
       fprintf(stderr,"usage %s port server_hostname server_port\n", argv[0]);
       exit(0);
    }
    svc_port = atoi(argv[1]);
    server_port = atoi(argv[3]);

    out_messages = linked_list_create();
    out_messages_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    out_messages_exist = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    rc = pthread_mutex_init(out_messages_mutex, NULL);
    rc |= pthread_cond_init(out_messages_exist, NULL);
    if (!out_messages || !out_messages_mutex || !out_messages_exist || rc)
        error("Failed to initialize client.");

    // Open an IPv4 TCP socket.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    // Bind new socket to provided service port.
    struct sockaddr_in svc_addr;
    svc_addr.sin_family = AF_INET;
    svc_addr.sin_port = htons(svc_port);
    svc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    rc = bind(sockfd, (struct sockaddr *) &svc_addr, sizeof(struct sockaddr_in));
    if (rc) error("ERROR binding to provided service port");

    // Get a hostent struct with the server's address.
    server = gethostbyname(argv[2]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(server_port);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    start_sending_messages(sockfd);
    start_receiving_messages(sockfd);

    printf("Enter an invalid message to terminate.\n");
    printf("Please enter your messages {ip:port text}:\n");

    // message_generator_t *generator = message_generator_create();
    // message_generator_add_dest_address(generator, )

    while (1) {
        message_t *m = get_user_message();
        if (!m) break;
        add_new_out_message(m);
        message_destroy(m);
    }

    // Ask socket for shutdown.
    shutdown(sockfd, SHUT_RDWR);
    // bzero(buffer, 256);
    // while ((n = write(sockfd, buffer, 1)) > 0) {
    //     printf("waiting...\n");
    // }  // Wait until it can be closed.
    printf("after shutdown\n");

    // Wait for receiver unit to terminate before closing the socket.
    pthread_join(receiver_tid, NULL);
    printf("Terminating client...\n");
    stop_sending_messages();

    // Finally, close the socket.
    close(sockfd);

    pthread_cond_destroy(out_messages_exist);
    pthread_mutex_destroy(out_messages_mutex);
    free(out_messages_exist);
    free(out_messages_mutex);
    linked_list_destroy(out_messages);

    return 0;
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

    printf("ip: %s port: %s data: %s\n", buffer, port, data);

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
 * Schedules given message for sending.
 *
 * Parameters:
 *  -m: Message to be scheduled for sending.
 */
void
add_new_out_message(message_t *m)
{
    // TODO: Fill these properties in a different place.
    m->src_addr = 0;
    m->src_port = 0;
    m->count = counter;
    m->flags = 0;
    m->len = MESSAGE_DATA_LENGTH;

    pthread_mutex_lock(out_messages_mutex);
    linked_list_append(out_messages, m);
    pthread_cond_signal(out_messages_exist);
    pthread_mutex_unlock(out_messages_mutex);
}


int
send_message(int sock_fd, message_t *m)
{
    int rc;

    char *serial_message = message_host_to_net(m);

    int n = write(sock_fd, serial_message, sizeof(message_t));
    if (n < (int) sizeof(message_t)) {
        fprintf(stderr, "Failed to send message\n");
        rc = -1;
    } else rc = 0;

    free(serial_message);
    counter++;

    return rc;
}


void
handle_nacked_message(int socket_fd, message_t *m)
{
    if (m->flags & ERR_TARGET_DOWN) {
        fprintf(stderr, "Failed to send message. Destination is offline.\n");
    }
    else if (m->flags & ERR_BUFFER_FULL || m->flags & ERR_INVALID_ORDER) {
        pthread_mutex_lock(out_messages_mutex);
        // NACKed message will be the first to be send.
        linked_list_push(out_messages, m);
        pthread_mutex_unlock(out_messages_mutex);
        pthread_cond_signal(out_messages_exist);
    }
}


void
start_sending_messages(int socket_fd)
{
    sender_unit_run = 1;
    int rc = pthread_create(
        &sender_tid, NULL, send_messages, (void *) socket_fd);
    if (rc) error("ERROR in starting messages sender.");
}


void
stop_sending_messages()
{
    sender_unit_run = 0;
    pthread_cond_signal(out_messages_exist);
    pthread_join(receiver_tid, NULL);
}


void *
send_messages(void *args)
{
    int socket_fd = (int) args;  // Socket to write messages.
    uint8_t prev_counter;
    uint8_t first_message = 1;

    while (sender_unit_run) {
        message_t *m;

        pthread_mutex_lock(out_messages_mutex);

        // Pause sender when outgoing messages list is empty.
        while (sender_unit_run && linked_list_size(out_messages) == 0)
            pthread_cond_wait(out_messages_exist, out_messages_mutex);
        if (!sender_unit_run) {  // Valid when sender should terminate.
            pthread_mutex_unlock(out_messages_mutex);
            break;
        }

        // Pause sender when a NACKed message has been resent, but more messages
        // had been previously sent from the normal stream. Server will NACK
        // all these messages since their order is invalid. Sender should wait
        // until all these messages have been received back and sent again,
        // before it can continue with the normal stream, i.e. when prev_counter
        // is a direct decrement from the next pending message.
        m = (message_t *) linked_list_get_first(out_messages);
        while (sender_unit_run && !first_message &&
               (prev_counter+1)%256 != m->count) {
            pthread_cond_wait(out_messages_exist, out_messages_mutex);
            m = (message_t *) linked_list_get_first(out_messages);
        }
        if (!sender_unit_run) {  // Valid when sender should terminate.
            pthread_mutex_unlock(out_messages_mutex);
            break;
        }

        m = linked_list_pop(out_messages);

        pthread_mutex_unlock(out_messages_mutex);

        send_message(socket_fd, m);
        prev_counter = m->count;
        first_message = 0;
    }

    return 0;
}


/**
 * Starts messages receiving unit from given socket.
 *
 * Parameters:
 *  -socket_fd: File descriptor of a connected socket to read messages from.
 */
void
start_receiving_messages(int socket_fd)
{
    int rc = pthread_create(
        &receiver_tid, NULL, receive_messages, (void *) socket_fd);
    if (rc) error("ERROR in starting message receiver.");
}


/**
 * Entry point for messages receiving unit.
 */
void *
receive_messages(void *args)
{
    char *in_buffer = (char *) malloc(sizeof(message_t));
    int socket_fd = (int) args;
    int n;

    while ((n = read(socket_fd, in_buffer, sizeof(message_t))) > 0) {
        // If for any reason a message was only partially read, discard it.
        if (n < sizeof(message_t)) {
            fprintf(stderr, "Failed to completely read incoming message.\n");
            continue;
        }

        message_t *message = message_net_to_host(in_buffer);

        if (message->flags)
            fprintf(stderr, "NACKed message received.\n");


        // DEBUG code.
        char src_ip[INET_ADDRSTRLEN];
        char dest_ip[INET_ADDRSTRLEN];

        struct in_addr src_addr;
        struct in_addr dest_addr;
        src_addr.s_addr = htonl(message->src_addr);
        dest_addr.s_addr = htonl(message->dest_addr);
        inet_ntop(AF_INET, &src_addr, src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &dest_addr, dest_ip, INET_ADDRSTRLEN);

        printf("Receiving message at %s:%d from %s:%d\n",
               dest_ip, message->dest_port,
               src_ip, message->src_port);
        char print_buffer[MESSAGE_DATA_LENGTH+1];
        memcpy(print_buffer, message->data, MESSAGE_DATA_LENGTH);
        printf("%s\n", print_buffer);
        // DEBUG code end.

        message_destroy(message);
    }

    return 0;
}
