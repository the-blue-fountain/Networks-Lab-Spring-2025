/*=====================================
Assignment 2 Submission
Name: Aritra Maji
Roll number: 22CS30011
Link of the pcap file: https://drive.google.com/file/d/1mFBiTE64x02oR3Cl3fKv5SUedlcmAVl6/view?usp=sharing
=====================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to server address
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    printf("Server running...\n");

    while (1) {
        addr_size = sizeof(client_addr);
        
        // Receive filename from client
        memset(buffer, 0, BUFFER_SIZE);
        recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_size);
        printf("Received file request: %s\n", buffer);

        // Open requested file
        FILE *file = fopen(buffer, "r");
        if (file == NULL) {
            sprintf(buffer, "NOTFOUND %s", buffer);
            sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_size);
            continue;
        }

        // Send HELLO
        char word[BUFFER_SIZE];
        fgets(word, BUFFER_SIZE, file);
        word[strcspn(word, "\n")] = 0; // Remove newline
        sendto(sockfd, word, strlen(word), 0, (struct sockaddr*)&client_addr, addr_size);

        // Wait for WORD1 request
        memset(buffer, 0, BUFFER_SIZE);
        recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_size);

        // Send words one by one
        while (fgets(word, BUFFER_SIZE, file)) {
            word[strcspn(word, "\n")] = 0; // Remove newline
            sendto(sockfd, word, strlen(word), 0, (struct sockaddr*)&client_addr, addr_size);
            
            if (strcmp(word, "FINISH") == 0) {
                break;
            }

            // Wait for next WORD request
            memset(buffer, 0, BUFFER_SIZE);
            recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &addr_size);
        }

        fclose(file);
        printf("File transfer completed\n");
        break;
    }

    close(sockfd);
    return 0;
}