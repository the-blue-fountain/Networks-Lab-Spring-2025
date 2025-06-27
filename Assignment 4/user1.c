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
        perror("Error in socket creation");
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
        perror("Error in binding socket");
        exit(1);
    }
    printf("Socket bound successfully\n");
    
    // Prepare destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &(dest_addr.sin_addr));
    
    // Open file to send
    int fd = open("file.txt", O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        exit(1);
    }
    printf("File opened successfully. Starting transfer...\n");
    
    // Send file content
    char buffer[BUFSIZE];
    int packet_count = 0;
    int read_bytes, sent_bytes;
    
    while ((read_bytes = read(fd, buffer, BUFSIZE)) > 0) {
        printf("Sending %d bytes in packet #%d...\n", read_bytes, packet_count + 1);
        
        // Try to send until successful
        while ((sent_bytes = k_sendto(sockfd, buffer, read_bytes, 0, 
                           (struct sockaddr *)&dest_addr, sizeof(dest_addr))) < 0) {
            if (errno == ENOSPACE) {
                printf("Send buffer full, waiting for space...\n");
                sleep(1);
                continue;
            }
            perror("Error in sending data");
            exit(1);
        }
        
        printf("Sent %d bytes successfully! Packet #%d complete.\n", sent_bytes, ++packet_count);
    }
    
    if (read_bytes < 0) {
        perror("Error reading from file");
        exit(1);
    }
    
    // Send EOF marker
    buffer[0] = '#';  // Using '#' as EOF marker
    while ((sent_bytes = k_sendto(sockfd, buffer, 1, 0, 
                       (struct sockaddr *)&dest_addr, sizeof(dest_addr))) < 0) {
        if (errno == ENOSPACE) {
            printf("Send buffer full, waiting for space to send EOF...\n");
            sleep(1);
            continue;
        }
        perror("Error in sending EOF");
        exit(1);
    }
    
    printf("Sent EOF marker. File transfer complete!\n");
    printf("Total packets sent: %d\n", packet_count);
    
    // Close file
    close(fd);
    
    // Give some time for the last packet to be processed
    printf("Waiting for final acknowledgments...\n");
    sleep(10);
    
    // Close socket
    if (k_close(sockfd) < 0) {
        perror("Error closing socket");
        exit(1);
    }
    printf("Socket closed successfully\n");
    
    return 0;
}
