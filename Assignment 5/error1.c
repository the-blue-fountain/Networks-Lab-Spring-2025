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
    printf("Error1 client: This client repeatedly sends GET_TASK without sending results\n");
    int sock = 0, bytes;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER] = {0};
    int delay = 1;
    int port = DEFAULT_PORT;
    int client_id = getpid();
    
    if (argc >= 2) {
        delay = atoi(argv[1]);
        if (delay < 0) {
            delay = 1;
        }
    }
    
    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            port = DEFAULT_PORT;
        }
    }
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        return -1;
    }
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    while (1) {
        // Send GET_TASK
        memset(buffer, 0, MAX_BUFFER);
        strcpy(buffer, "GET_TASK");
        send(sock, buffer, strlen(buffer), 0);
        printf("Sent: %s\n", buffer);
        
        // Receive response
        memset(buffer, 0, MAX_BUFFER);
        int received = 0;
        while (!received) {
            bytes = recv(sock, buffer, MAX_BUFFER - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                received = 1;
            } else if (bytes == 0) {
                close(sock);
                return 0;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                close(sock);
                return -1;
            }
            usleep(50000);
        }
        
        printf("Received: %s\n", buffer);
        
        if (strcmp(buffer, "No tasks available") == 0) {
            break;
        }
        
        // Instead of processing, just sleep and ask for another task
        sleep(delay);
    }
    
    close(sock);
    printf("Cleaning up and exiting...\n");
    return 0;
}
