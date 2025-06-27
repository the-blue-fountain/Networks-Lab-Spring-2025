/*===========================================
 Assignment 4: Emulating End-to-End Reliable Flow Control
 Name: Aritra Maji
 Roll number: 22CS30011
============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "ksocket.h"

#define BUFSIZE MAX_MSG_SIZE

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <src_ip> <src_port> <dest_ip> <dest_port>\n", argv[0]);
        return 1;
    }
    
    // Create KTP socket
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("Socket created successfully with ID: %d\n", sockfd);
    
    // Parse command line arguments
    char src_ip[INET_ADDRSTRLEN], dest_ip[INET_ADDRSTRLEN];
    strcpy(src_ip, argv[1]);
    strcpy(dest_ip, argv[3]);
    uint16_t src_port = atoi(argv[2]);
    uint16_t dest_port = atoi(argv[4]);
    
    // Bind the socket
    if (k_bind(src_ip, src_port, dest_ip, dest_port) < 0) {
        perror("Bind operation failed");
        exit(1);
    }
    printf("Socket bound successfully\n");
    
    // Create output filename based on port number
    char filename[100];
    sprintf(filename, "received_file_%d.txt", src_port);
    
    // Open file to write
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Error opening output file");
        exit(1);
    }
    printf("Output file '%s' created. Waiting for data...\n", filename);
    
    // Prepare source address structure
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    
    // Receive and write file
    char buffer[BUFSIZE];
    int packet_count = 0;
    int total_bytes = 0;
    int recv_bytes;
    
    while (1) {
        // Try to receive data
        while ((recv_bytes = k_recvfrom(sockfd, buffer, BUFSIZE, 0,
                            (struct sockaddr *)&src_addr, &addrlen)) < 0) {
            if (errno == ENOMESSAGE) {
                // No message available, wait and try again
                usleep(100000); // 100ms
                continue;
            }
            perror("Error in receiving data");
            exit(1);
        }
        
        printf("Received %d bytes in packet #%d\n", recv_bytes, ++packet_count);
        
        // Check for EOF
        if (recv_bytes == 1 && buffer[0] == '#') {
            printf("Received EOF marker. File transfer complete!\n");
            break;
        }
        
        // Write to file
        if (write(fd, buffer, recv_bytes) != recv_bytes) {
            perror("Error writing to file");
            exit(1);
        }
        
        total_bytes += recv_bytes;
        printf("Wrote %d bytes to file. Total bytes received: %d\n", recv_bytes, total_bytes);
    }
    
    // Close file
    close(fd);
    printf("Received a total of %d bytes in %d packets\n", total_bytes, packet_count - 1); // Subtract 1 for EOF packet
    
    // Close socket
    if (k_close(sockfd) < 0) {
        perror("Error closing socket");
        exit(1);
    }
    printf("Socket closed successfully\n");
    
    return 0;
}
