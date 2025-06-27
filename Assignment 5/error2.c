/*
Assignment 5 Submission
Name: Aritra Maji
Roll number: 22CS30011
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#define MAX_BUFFER 1024
#define DEFAULT_PORT 8080
#define SERVER_IP "127.0.0.1"

int main(int argc, char *argv[]) {
    printf("Error2 client: This client connects, sends GET_TASK, and immediately closes the connection\n");
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER] = {0};
    int port = DEFAULT_PORT;
    
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            port = DEFAULT_PORT;
        }
    }
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address");
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }
    
    // Send GET_TASK and then immediately close
    memset(buffer, 0, MAX_BUFFER);
    strcpy(buffer, "GET_TASK");
    send(sock, buffer, strlen(buffer), 0);
    printf("Sent: %s\n", buffer);
    
    printf("Immediately closing connection...\n");
    close(sock);
    
    return 0;
}
