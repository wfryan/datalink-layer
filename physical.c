#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "project_headers.h"

static double errorRate = DEFAULT_ERROR_RATE;

int connect_to_server(const char *server_ip, double errRate)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    errorRate = errRate;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int setup_server(int *serverfd)
{
    int new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((*serverfd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(*serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(*serverfd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(*serverfd, 3) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    if ((new_socket = accept(*serverfd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    printf("%d\n", *serverfd);

    return new_socket;
}

int physicalSend(int sock, Frame *frame)
{
    double randVal = (double)rand() / RAND_MAX;

    if (randVal < errorRate)
    {
        //printf("Frame dropped due to simulated error (type: %d, seq_num: %d).\n", frame->type, frame->seq_num);
        return -1;
    }
    else if (send(sock, frame, sizeof(Frame), 0) < 0)
    {
        perror("Failed to send frame");
        return -1;
    }
    return 0;
}

int physicalRecv(int sock, Frame *frame)
{
    if (recv(sock, frame, sizeof(Frame), 0) < 0)
    {
        return -1;
    }
    return 0;
}
