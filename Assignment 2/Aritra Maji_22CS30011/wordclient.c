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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Send filename to server
    sendto(sockfd, argv[1], strlen(argv[1]), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Receive first response
    addr_size = sizeof(server_addr);
    memset(buffer, 0, BUFFER_SIZE);
    recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &addr_size);

    // Check if file not found
    if (strncmp(buffer, "NOTFOUND", 8) == 0) {
        printf("FILE NOT FOUND\n");
        close(sockfd);
        return 1;
    }

    // Create output file
    char output_filename[BUFFER_SIZE];
    sprintf(output_filename, "received_%s", argv[1]);
    FILE *file = fopen(output_filename, "w");
    if (file == NULL) {
        perror("Failed to create output file");
        close(sockfd);
        return 1;
    }

    // Write HELLO to file
    fprintf(file, "%s\n", buffer);

    // Request words one by one
    int word_count = 1;
    while (1) {
        // Send WORD request
        sprintf(buffer, "WORD%d", word_count);
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

        // Receive word
        memset(buffer, 0, BUFFER_SIZE);
        recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &addr_size);

        // Write word to file
        fprintf(file, "%s\n", buffer);

        // Check if this was the last word (FINISH)
        if (strcmp(buffer, "FINISH") == 0) {
            break;
        }

        word_count++;
    }

    fclose(file);
    close(sockfd);
    printf("File transfer completed. Output saved to %s\n", output_filename);
    return 0;
}