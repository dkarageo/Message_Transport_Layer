/**
 * client.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A simple TCP client.
 *
 * Usage: exec_name <host> <port>
 *   where:
 *      -host : IPv4 address or hostname of server.
 *      -port : Port number on server.
 *
 * Credits:
 *  This file includes public code from Rensselaer Polytechnic Institute (RPI).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);

    // Open an IPv4 TCP socket.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    // Get a hostent struct with the server's address.
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    printf("Type 'quit' to terminate.\n");
    printf("Please enter your messages:\n");

    // Use 'quit' string as termination indicator.
    while (1) {
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);

        if (strcmp(buffer, "quit\n") == 0) break;

        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0) error("ERROR: Writing to socket failed");
    }

    // Ask socket for shutdown.
    shutdown(sockfd, SHUT_RDWR);
    bzero(buffer, 256);
    while ((n = write(sockfd, buffer, 1)) > 0);  // Wait until it can be closed.

    // Finally, close the socket.
    close(sockfd);

    return 0;
}
