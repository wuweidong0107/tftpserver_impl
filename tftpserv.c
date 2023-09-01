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
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


#define RECV_TIMEOUT 5
#define RECV_RETRIES 5

enum opcode {
    RRQ=1,
    WRQ,
    DATA,
    ACK,
    ERROR
};

enum mode {
    NETASCII=1,
    OCTET
};

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
} tftp_message;

static char* base_directory;

ssize_t tftp_send_error(int s, int error_code, char* error_string,
            struct sockaddr_in* sock, socklen_t slen)
{
    tftp_message m;
    ssize_t c;

    if (strlen(error_string) >= 512) {
        fprintf(stderr, "server: tftp_send_error(): string too long\n");
        return -1;
    }
    m.opcode = htons(ERROR),
    m.error.error_code = error_code;
    strcpy((char *)m.error.error_string, error_string);

    if ((c = sendto(s, &m, 4 + strlen(error_string) + 1, 0,
            (struct sockaddr *)sock, slen)) < 0) {
        perror("server: sendto()");
    }

    return c;
}

static ssize_t tftp_recv_message(int s, tftp_message* m, struct sockaddr_in* sock, socklen_t* slen)
{
    ssize_t c;

    if ((c = recvfrom(s, m, sizeof(*m), 0, (struct sockaddr *)sock, slen)) < 0 
        && errno != EAGAIN) {
        perror("server: recvfrom()");
    }
    return c;
}

static void tftp_handle_request(tftp_message* m, ssize_t len,
                struct sockaddr_in* client_sock, socklen_t slen)
{
    int s;
    struct timeval tv;
    char *filename, *end, *mode_s;
    uint16_t opcode;
    FILE *fd;

    printf("tftp_handle_request\n");

    /* open new socket, to handle client request */
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("server: socket()");
        exit(1);
    }

    tv.tv_sec = RECV_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("server: setsockopt()");
        exit(1);
    }

    /* parse client request */
    filename = (char *)m->request.filename_and_mode;
    end = &filename[len - 2 - 1];

    if (*end != '\0') {
        printf("%s.%u: invalid filename or mode\n",
            inet_ntoa(client_sock->sin_addr), ntohs(client_sock->sin_port));
        tftp_send_error(s, 0, "invalid filename or mode", client_sock, slen);
        exit(1);
    }

    mode_s = strchr(filename, '\0') + 1;

    if (mode_s > end) {
        printf("%s.%u: transfer mode not specified\n", 
            inet_ntoa(client_sock->sin_addr), ntohs(client_sock->sin_port));
        tftp_send_error(s, 0, "transfer mode not specified", client_sock, slen);
    }

    opcode = ntohs(m->opcode);
    printf("file: %s\n", filename);
    printf("mode: %s\n", mode_s);
}

static void cld_handler(int sig)
{
    int status;
    wait(&status);
    printf("got child exit signal\n");
}

int main(int argc, char* argv[])
{
    int s;
    struct sockaddr_in server_sock;
    uint16_t port = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <directory> [port]\n", argv[0]);
        exit(1);
    }

    base_directory = argv[1];

    if (argc > 2) {
        if (sscanf(argv[2], "%hu", &port)) {
            printf("xxxx: %d\n", port);
            printf("yyyy: %d\n", port);
        } else {
            fprintf(stderr, "error: invalid port\n");
            exit(1);
        }
    } else {
        port = 69;
    }

    if (chdir(base_directory) < 0) {
        perror("server: chdir()");
        exit(1);
    }

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    server_sock.sin_family = AF_INET;
    server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&server_sock, sizeof(server_sock)) == -1) {
        perror("server: bind()");
        close(s);
        exit(1);
    }

    signal(SIGCLD, (void *)cld_handler);
    printf("tftp server: listening on %d\n", ntohs(server_sock.sin_port));
    while(1) {
        struct sockaddr_in client_sock;
        socklen_t slen = sizeof(client_sock);
        ssize_t len;
        tftp_message message;
        uint16_t opcode;
        
        /* receive tftp message */
        if ((len = tftp_recv_message(s, &message, &client_sock, &slen)) < 0) {
            continue;
        }
        
        if (len < 4) {
            printf("%s.%u: request with invalid size received\n",
                inet_ntoa(client_sock.sin_addr), ntohs(client_sock.sin_port));
            tftp_send_error(s, 0, "invalid request size", &client_sock, slen);
            continue;
        }

        opcode = ntohs(message.opcode);
        if (opcode == RRQ || opcode == WRQ) {
            if (fork() == 0) {
                tftp_handle_request(&message, len, &client_sock, slen);
                exit(0);
            }
        } else {
            printf("%s.%u: invalid request received: opcode\n",
                inet_ntoa(client_sock.sin_addr), ntohs(client_sock.sin_port));
            tftp_send_error(s, 0, "invalid opcode", &client_sock, slen);
        }
    }

    close(s);
    return 0;
}