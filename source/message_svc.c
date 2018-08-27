/**
 * message_svc.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * An implementation of server-side module of Message Transport Layer. It is
 * implemented as a service of a TCP server.
 *
 * Version: 0.1
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <error.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include "message_svc.h"

#define CLIENT_BUF_LEN 4 // Number of incoming messages to be buffered for
                         // each client.


// A list containing all connected clients.
linked_list_t **clients;
int connected_clients;  // Total numbwe
pthread_mutex_t *clients_mutex;  // clients list corresponding mutex

// A list of clients that have pending messages.
linked_list_t *active_clients;
pthread_mutex_t *active_clients_mutex;  // active clients corresponding mutex

// Defines whether sending unit should keep running or terminate.
int sending_unit_run;
// Thread id of thread that runs sending unit.
pthread_t sending_unit_tid;
// Condition for blocking sending unit when no outgoing messages are available.
pthread_cond_t *messages_exist_cond;
pthread_mutex_t *messages_exist_mutex;
uint32_t total_messages_sent;
struct timespec message_sending_period;


// ---- Definitions of logger  ----
FILE *log_file;      // File where log data will
pthread_t log_tid;   // Thread if of logger.
int logger_run;

struct log_data {
    struct timespec timestamp;  // Timestamp of sample.
    unsigned long messages;     // Number of messages sent at this sample.
    unsigned long total_cpu;    // Total system-wide CPU usage at this sample.
    unsigned long utime;  // Total CPU time consumed by server in user mode.
    unsigned long stime;  // Total CPU time consumed by server in kernel mode.
};

int
_start_logger(char *logger_fn);
int
_stop_logger();
void *
_logger_work(void *arg);
void
_log(FILE *log_file, struct log_data *prev);


// ---- Definitions of speed limiter ----
int speed_limiter_run;
pthread_t limiter_tid;

struct limiter_data {
    long period;
    long max_rate;
    long min_rate;
    long rate_step;
};

int
_start_speed_limiter(long period, long max_rate, long min_rate, long step);
int
_stop_speed_limiter();
void *
_speed_limiter_worker(void *arg);


// ---- Definitions of util routines ----
void
define_sender(message_t *m, client_t *client);
long
get_elapsed_time_millis(struct timespec start, struct timespec stop);
void
timespec_add(struct timespec *res, struct timespec *a, struct timespec *b);
void
timespec_subtract(struct timespec *res, struct timespec *a, struct timespec *b);


void
init_svc(struct svc_cfg *options)
{
    int rc;

    // Initialize list for keeping all connected clients.
    clients = (linked_list_t **) malloc(sizeof(linked_list_t *) * 256);
    // clients = linked_list_create();
    clients_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!clients || !clients_mutex) goto error;
    if (pthread_mutex_init(clients_mutex, NULL)) goto error;

    // Initialize list for keeping clients with pending outgoing messages.
    active_clients = linked_list_create();
    active_clients_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!active_clients || !active_clients_mutex) goto error;
    if (pthread_mutex_init(active_clients_mutex, NULL)) goto error;

    // Initialize tools for signaling sender unit that outgoing messages exist.
    messages_exist_cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    messages_exist_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (!messages_exist_cond || !messages_exist_mutex) goto error;
    if (pthread_cond_init(messages_exist_cond, NULL)) goto error;
    if (pthread_mutex_init(messages_exist_mutex, NULL)) goto error;

    total_messages_sent = 0;
    connected_clients = 0;

    // Start sending unit.
    sending_unit_run = 1;
    rc = pthread_create(
        &sending_unit_tid, NULL, start_sending_unit, (void *) NULL);
    if (rc) goto error;

    // Initialize logger.
    if (options && options->enable_logger) {
        rc = _start_logger(options->log_fn);
        if (rc) goto error;
    }

    // Initialize speed limiter.
    if (options && options->enable_speed_limiter) {
        rc = _start_speed_limiter(options->time_of_step, options->max_rate,
                                  options->min_rate, options->rate_step);
        if (rc) goto error;
    }

    return;

error:
    perror("Failed to initialize messaging service.");
    exit(-1);
}


void
stop_svc()
{
    if (logger_run) _stop_logger();
    if (speed_limiter_run) _stop_speed_limiter();

    // Ask sender unit to terminate and wait until it terminates.
    sending_unit_run = 0;
    pthread_cond_signal(messages_exist_cond);  // Cause it to stop waiting for messages.
    pthread_join(sending_unit_tid, NULL);

    // Clean-up global allocated resources.
    pthread_mutex_destroy(clients_mutex);
    pthread_mutex_destroy(active_clients_mutex);
    pthread_mutex_destroy(messages_exist_mutex);
    pthread_cond_destroy(messages_exist_cond);
    free(clients_mutex);
    free(active_clients_mutex);
    free(messages_exist_mutex);
    free(messages_exist_cond);
    linked_list_destroy(active_clients);
    // linked_list_destroy(clients);
    free(clients);
}


void
handle_client(int socket_fd)
{
    client_t *c = NULL;
    node_t *c_ref = NULL;
    char *in = NULL;
    message_t *mspace = NULL;
    int rc;

    c = client_create(socket_fd);
    if (!c) goto error;

    // Add new client to list of connected clients.
    pthread_mutex_lock(clients_mutex);
    int index = (c->address + c->port) & 0xFF;
    if (!clients[index]) clients[index] = linked_list_create();
    if (!clients[index]) {
        pthread_mutex_unlock(clients_mutex);
        goto error;
    }
    c_ref = linked_list_append(clients[index], c);
    if (!c_ref) {
        pthread_mutex_unlock(clients_mutex);
        goto error;
    }
    connected_clients++;
    pthread_mutex_unlock(clients_mutex);

    // Allocate buffer for incoming data.
    in = (char *) malloc(sizeof(message_t));
    if (!in) goto error;
    int n;
    uint16_t counter = 0;  // Local counter that should match incoming messages.
    uint8_t first_message = 1;  // Flag for first message to ignore counter.

    // Workspace memory for storing outgoing messages in order to avoid
    // malloc at each receive. We need CLIENT_BUF_LEN for pending messages
    // plus 1 more slot for the currently receiving message plus 1 more slot
    // for the currently sending message (on the sender unit).
    mspace = (message_t *) malloc (sizeof(message_t) * (CLIENT_BUF_LEN + 2));
    if (!mspace) goto error;
    int mspace_i = 0;  // Mod(CLIENT_BUF_LEN+1) counter for circular usage
                       // of mspace.

    // DEBUG
    // int biggest_error = 0;

    // Keep reading incoming messages until the connection is dead.
    while ((n = recv(socket_fd, in, sizeof(message_t), MSG_WAITALL)) > 0) {
        if (n < (int) sizeof(message_t)) {
            fprintf(stderr, "Failed to completely read incoming messages.\n");
            // first_message = 1;
            continue;
        }
        message_t *message = message_net_to_host_buf(in, mspace+mspace_i);
        define_sender(message, c);  // fill sender fields of message

        message->flags = 0;
        uint8_t error_code = 0;  // clear error flags

        // A message is not valid unless its 'count' field is a direct increment
        // from the previous successfully received message.
        if (message->count != (counter+1)%(MESSAGE_COUNT_MAX+1) && !first_message) {
            error_code |= ERR_INVALID_ORDER;
            // DEBUG
            // printf("NACKing because of INVALID_ODER\n");
            // printf("Message had: %d - Expected: %d\n", message->count, (counter+1)%256);

            // DEBUG
            // int m_c = message->count;
            // int expect = (counter+1)%(MESSAGE_COUNT_MAX+1);
            // if (expect > m_c) {
            //     if (biggest_error < (m_c+(MESSAGE_COUNT_MAX+1)-expect))
            //         biggest_error = m_c+(MESSAGE_COUNT_MAX+1)-expect;
            // } else {
            //     if (biggest_error < (m_c-expect)) biggest_error = m_c-expect;
            // }
            // printf("Biggest error: %d\n", biggest_error);
        }

        // If buffer is full, discard message.
        // --- Save a few CPU cycles by not synchronizing check of linked list
        // length. It's better to just NACK a message in the edge case, instead
        // of constantly locking. ---
        // if (linked_list_size(c->out_messages) >= CLIENT_BUF_LEN) {
        //     error_code |= ERR_BUFFER_FULL;
        //     // DEBUG
        //     // printf("NACKing because of BUFFER FULL\n");
        // }
        // printf("Received incoming. Error code: %d\n", error_code);

        if (!error_code) {
            first_message = 0;
            counter = message->count;

            // If no error, push message to pending outgoing messages of this
            // client.
            rc = pthread_mutex_lock(c->out_mutex);
            if (rc) perror("Failed to acquire client out mutex.\n");

            while (linked_list_size(c->out_messages) >= CLIENT_BUF_LEN)
                pthread_cond_wait(c->out_message_removed, c->out_mutex);

            int had_messages = linked_list_size(c->out_messages);
            linked_list_append(c->out_messages, message);

            // If there were no messages in out_messages list, then this client
            // had been removed from active clients list by the sending unit.
            // So, add it again and signal sender unit.
            if (!had_messages) {
                rc = pthread_mutex_lock(active_clients_mutex);
                if (rc) perror("Failed to acquire global out mutex.\n");
                linked_list_append(active_clients, c);
                pthread_cond_signal(messages_exist_cond);
                pthread_mutex_unlock(active_clients_mutex);
            }

            pthread_mutex_unlock(c->out_mutex);
            mspace_i = (mspace_i + 1) % (CLIENT_BUF_LEN + 2);

        } else NACK_message(message, error_code);
    }
    goto cleanup;

error:
    perror("Could not handle new client");

cleanup:
    // Remove client from connected clients.
    if (clients[index]) {
        pthread_mutex_lock(clients_mutex);
        if (c_ref) {
            linked_list_remove(clients[index], c_ref);
            connected_clients--;
        }
        if (linked_list_size(clients[index]) == 0) {
            linked_list_destroy(clients[index]);
            clients[index] = NULL;
        }
        pthread_mutex_unlock(clients_mutex);
    }

    // Wait until sender has handled all pending outgoing messages.
    if (c) {
        pthread_mutex_lock(c->out_mutex);
        while(linked_list_size(c->out_messages) > 0)
            pthread_cond_wait(c->out_message_removed, c->out_mutex);
        pthread_mutex_unlock(c->out_mutex);
        client_destroy(c);
    }

    if (in) free(in);
    if (mspace) free(mspace);
}


void *
start_sending_unit(void *args)
{
    int rc;

    struct timespec target;     // Target time for next timeout.
    struct timespec cur_time;   // Current time.
    struct timespec diff;       // Difference between current time and target.

    // Set first target to current time plus a period.
    clock_gettime(CLOCK_MONOTONIC, &target);
    timespec_add(&target, &target, &message_sending_period);

    while (sending_unit_run) {
        if (speed_limiter_run) {
            // Calculate the difference between current time and target.
            clock_gettime(CLOCK_MONOTONIC, &cur_time);
            timespec_subtract(&diff, &target, &cur_time);

            if (diff.tv_sec >= 0 && diff.tv_nsec >= 0) {
                // Sleep for calculated diff time.
                int rc = nanosleep(&diff, NULL);
                if (rc != 0) {
                    perror("Nanosleep not completed properly.\n");
                }
            }
        }

        rc = pthread_mutex_lock(active_clients_mutex);
        if (rc) perror("Failed to acquire mutex of active clients\n");

        while (linked_list_size(active_clients) < 1 && sending_unit_run)
            pthread_cond_wait(messages_exist_cond, active_clients_mutex);
        if (!sending_unit_run) { // Required for termination request.
            pthread_mutex_unlock(active_clients_mutex);
            break;
        }

        // Select the first client with a pending outgoing message.
        client_t *selected = (client_t *) linked_list_pop(active_clients);
        rc = pthread_mutex_lock(selected->out_mutex);
        if (rc) perror("Failed to acquire mutex of selected client\n");

        message_t *message = (message_t *) linked_list_pop(selected->out_messages);
        pthread_cond_signal(selected->out_message_removed);

        // If there are pending messages from this client, push it to the back.
        // A simple one-message round-robin scheduling is used.
        if (linked_list_size(selected->out_messages) > 0)
            linked_list_append(active_clients, (void *) selected);

        pthread_mutex_unlock(selected->out_mutex);
        pthread_mutex_unlock(active_clients_mutex);

        send_message(message);

        if (speed_limiter_run) {
            // Set next target. Everything is integral, so no error accumulation.
            timespec_add(&target, &target, &message_sending_period);
        }
    }

    return 0;
}


void
NACK_message(message_t *m, uint8_t error_code)
{
    int rc;
    // printf("NACK message\n");

    m->flags = error_code;

    client_t *src = NULL;

    pthread_mutex_lock(clients_mutex);

    // // Acquire the source of the message.
    // iterator_t * it = linked_list_iterator(clients);
    // while(iterator_has_next(it)) {
    //     client_t *c = iterator_next(it);
    //     if (m->src_addr == c->address && m->src_port == c->port) {
    //         src = c;
    //         break;
    //     }
    // }
    // iterator_destroy(it);

    int index = (m->src_addr + m->src_port) & 0xFF;
    linked_list_t *hashed_list = clients[index];
    if (hashed_list && linked_list_size(hashed_list) == 1) {
        src = linked_list_get_first(hashed_list);
    } else if (hashed_list && linked_list_size(hashed_list) > 1) {
        iterator_t *it = linked_list_iterator(hashed_list);
        while(iterator_has_next(it)) {
            client_t *c = iterator_next(it);
            if (m->src_addr == c->address && m->src_port == c->port) {
                src = c;
                break;
            }
        }
        iterator_destroy(it);
    }

    // If the source has gone offline, it's impossible to NACK the message.
    if (src) {
        // DEBUG
        // char txt_addr[INET_ADDRSTRLEN];
        // struct in_addr bin_addr;
        // bin_addr.s_addr = htonl(src->address);
        // inet_ntop(AF_INET, &bin_addr, txt_addr, INET_ADDRSTRLEN);
        // printf("NACKing to: %s:%d\n", txt_addr, src->port);

        char *out_buffer = message_host_to_net(m);
        rc = pthread_mutex_lock(src->sock_wr_mutex);
        if (rc) perror("Failed to acquire socket writing mutex.\n");
        int socket_fd = src->socket_fd;
        int n = send(socket_fd, out_buffer, sizeof(message_t), MSG_NOSIGNAL);
        pthread_mutex_unlock(src->sock_wr_mutex);
        if (n < (int) sizeof(message_t))
            fprintf(stderr, "Failed to sent NACK message.\n");
        free(out_buffer);
    }
    pthread_mutex_unlock(clients_mutex);
}


void
send_message(message_t *m)
{
    // Static buffer to be used for network byte-order messages.
    static char out_buffer[sizeof(message_t)];

    client_t *dest = NULL;
    int rc;

    pthread_mutex_lock(clients_mutex);
    // // Search all connected clients for the requested destination.
    // iterator_t * it = linked_list_iterator(clients);
    // while(iterator_has_next(it)) {
    //     client_t *c = iterator_next(it);
    //     if (m->dest_addr == c->address && m->dest_port == c->port) {
    //         dest = c;
    //         break;
    //     }
    // }
    // iterator_destroy(it);

    int index = (m->dest_addr + m->dest_port) & 0xFF;
    linked_list_t *hashed_list = clients[index];
    if (hashed_list && linked_list_size(hashed_list) == 1) {
        dest = linked_list_get_first(hashed_list);
    } else if (hashed_list && linked_list_size(hashed_list) > 1) {
        iterator_t *it = linked_list_iterator(hashed_list);
        while(iterator_has_next(it)) {
            client_t *c = iterator_next(it);
            if (m->dest_addr == c->address && m->dest_port == c->port) {
                dest = c;
                break;
            }
        }
        iterator_destroy(it);
    }

    // If there is a connected client that matches destination ip and port of
    // message, send it the message. Otherwise, NACK it.
    if (dest) {
        message_host_to_net_buf(m, out_buffer);
        rc = pthread_mutex_lock(dest->sock_wr_mutex);
        if (rc) perror("Failed to acquire socket writing mutex.\n");
        int n = send(
            dest->socket_fd, out_buffer, sizeof(message_t), MSG_NOSIGNAL);
        pthread_mutex_unlock(dest->sock_wr_mutex);
        if (n < (int) sizeof(message_t))
            fprintf(stderr, "Failed to sent message.\n");

    } else {
        pthread_mutex_unlock(clients_mutex);
        NACK_message(m, ERR_TARGET_DOWN);
    }

    pthread_mutex_unlock(clients_mutex);

    total_messages_sent++;

    // DEBUG code.
    // char src_ip[INET_ADDRSTRLEN];
    // char dest_ip[INET_ADDRSTRLEN];
    //
    // struct in_addr src_addr;
    // struct in_addr dest_addr;
    // src_addr.s_addr = htonl(m->src_addr);
    // dest_addr.s_addr = htonl(m->dest_addr);
    // inet_ntop(AF_INET, &src_addr, src_ip, INET_ADDRSTRLEN);
    // inet_ntop(AF_INET, &dest_addr, dest_ip, INET_ADDRSTRLEN);
    //
    // printf("Sending message from %s:%d to %s:%d\n",
    //        src_ip, m->src_port,
    //        dest_ip, m->dest_port);
    // DEBUG end.
}


void
define_sender(message_t *m, client_t *client)
{
    m->src_addr = client->address;
    m->src_port = client->port;
}


client_t *
client_create(int socket_fd)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int rc;

    // Acquire a sockaddr_in object to get client's IPv4 address.
    rc = getpeername(socket_fd, (struct sockaddr *) &addr, &addr_len);
    if (rc) {
        perror("client_create() failed");
        return NULL;
    }
    if (addr_len > sizeof(struct sockaddr_in)) {
        printf("Invalid protocol.\n");
        return NULL;
    }

    client_t *client = (client_t *) malloc(sizeof(client_t));
    if (!client) {
        perror("client_create() failed");
        return NULL;
    }
    client->socket_fd = socket_fd;
    client->address = ntohl(addr.sin_addr.s_addr);
    client->port = ntohs(addr.sin_port);
    client->out_messages = linked_list_create();
    client->out_mutex =
        (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    client->sock_wr_mutex =
        (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    client->out_message_removed =
        (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    if (!client->out_mutex || !client->sock_wr_mutex ||
        !client->out_message_removed) {
        perror("client_create() failed");
        return NULL;
    }

    rc = pthread_mutex_init(client->out_mutex, NULL);
    rc |= pthread_mutex_init(client->sock_wr_mutex, NULL);
    rc |= pthread_cond_init(client->out_message_removed, NULL);
    if (rc) {
        perror("client_create() failed to init a mutex");
        client_destroy(client);  // Clean all allocated memory before return.
        return NULL;
    }

    return client;
}


void
client_destroy(client_t *client)
{
    if (!client) return;

    int rc;

    if (client->out_mutex) {
        rc = pthread_mutex_destroy(client->out_mutex);
        if (rc) perror("Failed to destroy mutex");
        free(client->out_mutex);
    }
    if (client->sock_wr_mutex) {
        rc = pthread_mutex_destroy(client->sock_wr_mutex);
        if (rc) perror("Failed to destroy mutex");
        free(client->sock_wr_mutex);
    }
    if (client->out_message_removed) {
        rc = pthread_cond_destroy(client->out_message_removed);
        if (rc) perror("Failed to destroy condition");
        free(client->out_message_removed);
    }
    if (client->out_messages) linked_list_destroy(client->out_messages);
    free(client);
}


int
_start_logger(char *logger_fn)
{
    log_file = fopen(logger_fn, "w");
    if (!log_file) return -1;

    logger_run = 1;
    return pthread_create(&log_tid, NULL, _logger_work, log_file);
}


int
_stop_logger()
{
    logger_run = 0;
    if (pthread_join(log_tid, NULL)) return -1;
    return fclose(log_file);
}


void *
_logger_work(void *arg)
{
    // Write message and data size to the beggining of logfile.
    int message_size = sizeof(message_t);
    int data_size = MESSAGE_DATA_LENGTH;
    int rc = fprintf(log_file, "%d %d\n", message_size, data_size);
    if (rc < 1) {
        fprintf(stderr, "Failed to write header data to log file.\n");
        pthread_exit((void *) -1);
    }
    fflush(log_file);

    struct log_data previous;
    memset(&previous, 0, sizeof(struct log_data));

    struct timespec period_spec;
    period_spec.tv_sec = 1;  // 1 sec period
    period_spec.tv_nsec = 0;

    struct timespec target;     // Target time for next timeout.
    struct timespec cur_time;   // Current time.
    struct timespec diff;       // Difference between current time and target.

    // Set first target to current time plus a period.
    clock_gettime(CLOCK_MONOTONIC, &target);
    timespec_add(&target, &target, &period_spec);

    while (logger_run) {
        // Calculate the difference between current time and target.
        clock_gettime(CLOCK_MONOTONIC, &cur_time);
        timespec_subtract(&diff, &target, &cur_time);

        if (diff.tv_sec >= 0 && diff.tv_nsec >= 0) {
            // Sleep for calculated diff time.
            int rc = nanosleep(&diff, NULL);
            if (rc != 0) {
                perror("Nanosleep not completed properly.\n");
            }
        }

        _log((FILE *) arg, &previous);

        // Set next target. Everything is integral, so no error accumulation.
        timespec_add(&target, &target, &period_spec);
    }

    pthread_exit(0);
}


void
_log(FILE *log_file, struct log_data *prev)
{
    struct log_data current;

    // Get utime and stime.
    pid_t pid = getpid();
    char pid_path[128];
    sprintf(pid_path, "/proc/%d/stat", pid);
    FILE *pid_proc = fopen(pid_path, "r");
    if (!pid_proc) {
        fprintf(stderr, "ERROR: Failed to open %s\n", pid_path);
        return;
    }
    fscanf(pid_proc,
           "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu",
           &current.utime, &current.stime);
    fclose(pid_proc);

    // Get total cpu usage.
    char stat_buf[256];
    FILE *proc = fopen("/proc/stat", "r");
    if (!proc) {
        fprintf(stderr, "ERROR: Failed to open /proc/stat\n");
        return;
    }
    if (!fgets(stat_buf, 256, proc)) {
        fprintf(stderr, "ERROR: Failed to read /proc/stat\n");
        fclose(proc);
        return;
    }
    current.total_cpu = 0;
    char *saveptr;
    char *next_field = strtok_r(stat_buf, " ", &saveptr);
    while(next_field) {
        current.total_cpu += atol(next_field);
        next_field = strtok_r(NULL, " ", &saveptr);
    }
    fclose(proc);

    // Get current timestamp.
    clock_gettime(CLOCK_MONOTONIC, &current.timestamp);

    // Get messages number.
    current.messages = total_messages_sent;

    unsigned long out_timestamp = 0;
    unsigned long out_messages = 0;
    float out_cpu_usage = 0;

    // There is nothing to calculate on initial sample.
    if (prev->timestamp.tv_sec != 0 && prev->timestamp.tv_nsec != 0) {

        out_timestamp = get_elapsed_time_millis(
            prev->timestamp, current.timestamp);

        if (current.messages < prev->messages)
            out_messages = UINT32_MAX - prev->messages + 1 + current.messages;
        else out_messages = current.messages - prev->messages;

        out_cpu_usage =
            (float) (current.utime - prev->utime + current.stime - prev->stime) /
            (float) (current.total_cpu - prev->total_cpu);
    }

    // Append to log file.
    int rc = fprintf(log_file, "%lu %lu %.6f %d\n",
                     out_timestamp, out_messages, out_cpu_usage,
                     connected_clients);
    if (rc < 1) fprintf(stderr, "Failed to write log file\n");
    fflush(log_file);

    // Current sample is from now on the previous one.
    memcpy(prev, &current, sizeof(struct log_data));
}


int
_start_speed_limiter(long period, long max_rate, long min_rate, long step)
{
    struct limiter_data *specs =
        (struct limiter_data *) malloc(sizeof(struct limiter_data));
    specs->period = period;
    specs->max_rate = max_rate;
    specs->min_rate = min_rate;
    specs->rate_step = step;

    // Set message_sending_period for max_rate.
    message_sending_period.tv_sec = 1 / max_rate;
    message_sending_period.tv_nsec = (1000000000 / max_rate) % 1000000000;

    speed_limiter_run = 1;
    return pthread_create(&limiter_tid, NULL, _speed_limiter_worker, specs);
}
int
_stop_speed_limiter()
{
    speed_limiter_run = 0;
    pthread_cancel(limiter_tid);
    return pthread_join(limiter_tid, NULL);
}

void *
_speed_limiter_worker(void *arg)
{
    struct limiter_data *arg_specs = (struct limiter_data *) arg;
    struct limiter_data local_specs;
    struct limiter_data *specs = &local_specs;  // Bored to change all references.
    memcpy(specs, arg_specs, sizeof(struct limiter_data));
    free(arg_specs);  // Struct in stack and free is needed for thread cancellation.

    struct timespec period_spec;
    period_spec.tv_sec = specs->period / 1000;
    period_spec.tv_nsec = (specs->period % 1000) * 1000000;

    struct timespec target;     // Target time for next timeout.
    struct timespec cur_time;   // Current time.
    struct timespec diff;       // Difference between current time and target.

    long cur_rate = specs->max_rate;

    // struct timespec incr_step;  // Step of increment for message sending period.
    // struct timespec max_period; // Period of minimum rate.
    // struct timespec min_period; // Period of maximum rate.
    // incr_step.tv_sec = 1 / specs->rate_step;
    // incr_step.tv_nsec = (1000000000 / specs->rate_step) % 1000000000;
    // max_period.tv_sec = 1 / specs->min_rate;
    // max_period.tv_nsec = (1000000000 / specs->min_rate) % 1000000000;
    // min_period.tv_sec = 1 / specs->max_rate;
    // min_period.tv_nsec = (1000000000 / specs->max_rate) % 1000000000;

    // DEBUG
    // printf("Max period: %ld\n", max_period.tv_nsec);
    // printf("Min period: %ld\n", min_period.tv_nsec);
    // printf("Step: %ld\n", incr_step.tv_nsec);

    // Set first target to current time plus a period.
    clock_gettime(CLOCK_MONOTONIC, &target);
    timespec_add(&target, &target, &period_spec);

    while (speed_limiter_run) {
        // Calculate the difference between current time and target.
        clock_gettime(CLOCK_MONOTONIC, &cur_time);
        timespec_subtract(&diff, &target, &cur_time);

        if (diff.tv_sec >= 0 && diff.tv_nsec >= 0) {
            // Sleep for calculated diff time.
            int rc = nanosleep(&diff, NULL);
            if (rc != 0) {
                perror("Nanosleep not completed properly.\n");
            }
        }

        cur_rate -= specs->rate_step;
        if (cur_rate < specs->min_rate) cur_rate = specs->max_rate;
        message_sending_period.tv_sec = 1 / cur_rate;
        message_sending_period.tv_nsec = (1000000000 / cur_rate) % 1000000000;

        // DEBUG
        // struct timespec new_period;
        // timespec_add(&new_period, &message_sending_period, &incr_step);
        // if (new_period.tv_sec > max_period.tv_sec ||
        //         (new_period.tv_sec == max_period.tv_sec &&
        //          new_period.tv_nsec > max_period.tv_nsec)) {
        //     // When new sending rate is lower than min limit, reset to
        //     // max rate and start decreasing it again.
        //     message_sending_period.tv_sec = min_period.tv_sec;
        //     message_sending_period.tv_nsec = min_period.tv_nsec;
        // } else {
        //     message_sending_period.tv_sec = new_period.tv_sec;
        //     message_sending_period.tv_nsec = new_period.tv_nsec;
        // }

        // printf("New time in nanos: %ld\n", message_sending_period.tv_nsec);

        // Set next target. Everything is integral, so no error accumulation.
        timespec_add(&target, &target, &period_spec);
    }

    pthread_exit(0);
}


long
get_elapsed_time_millis(struct timespec start, struct timespec stop)
{
    long elapsed_time = (stop.tv_sec - start.tv_sec) * 1000;
    elapsed_time += (stop.tv_nsec - start.tv_nsec) / 1000000;
    return elapsed_time;
}


/**
 * Adds two timespec structs.
 *
 * Operation: res = a + b
 *
 * Parameters:
 *  -a : First operand.
 *  -b : Second operand.
 *  -res : Target in which the result will be written.
 */
void
timespec_add(struct timespec *res, struct timespec *a, struct timespec *b)
{
    res->tv_nsec = a->tv_nsec + b->tv_nsec;
    res->tv_sec = a->tv_sec + b->tv_sec;
    if (res->tv_nsec >= 1000000000) {
        res->tv_nsec -= 1000000000;
        res->tv_sec += 1;
    }
}


/**
 * Subtracts two timespec structs.
 *
 * Operation: res = a - b
 *
 * Parameters:
 *  -a : First operand.
 *  -b : Second operand.
 *  -res : Target in which the result will be written.
 */
void
timespec_subtract(struct timespec *res, struct timespec *a, struct timespec *b)
{
    if (a->tv_nsec < b->tv_nsec) {
        res->tv_nsec = a->tv_nsec - b->tv_nsec + 1000000000;
        res->tv_sec = a->tv_sec - b->tv_sec - 1;
    }
    else {
        res->tv_nsec = a->tv_nsec - b->tv_nsec;
        res->tv_sec = a->tv_sec - b->tv_sec;
    }
}
