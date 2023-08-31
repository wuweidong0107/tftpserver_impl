#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

typedef union {
    uint16_t opcode;
    struct {
        uint16_t opcode; /* RRQ or WRQ */
        uint8_t filename_and_mode[514];
    } request;

    struct {
        uint16_t opcode; /* DATA */
        uint16_t block_number;
        uint8_t data[512];
    } data;

    struct {
        uint16_t opcode; /* ACK */
        uint16_t block_number;
    } ack;

    struct {
        uint16_t opcode; /* ERROR */
        uint16_t error_code;
        uint8_t error_string[512];
    } error;
};

static char* base_directory;

int main(int argc, char* argv[])
{
    int s;
    struct sockaddr_in server_sock;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <directory> [port]\n", argv[0]);
    }

    base_directory = argv[1];

    if (chdir(base_directory) < 0) {
        perror("server: chdir()");
        exit(1);
    }

    s = socket(AF_INET, SOCK_DGRAM, );

    server_sock.sin_family = ...;
    server_sock.sin_addr.s_addr = ...;
    server_sock.sin_port = ...;
    bind(s, (struct sockaddr *)&server_sock, );

    printf("tftp server: listening on %d\n", ntohs(server_sock.sin_port));

    while(1) {
        /* receive tftp message */

        if (fork() == 0) {
            /* handle request */
        }
    }

    close(s);
    return 0;
}