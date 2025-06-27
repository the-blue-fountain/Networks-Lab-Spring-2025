/*
Assignment 3 Submission
Name: Aritra Maji
Roll number: 22CS30011
Link of the pcap file: https://drive.google.com/drive/folders/1FZaiyOtUO3sHLY4UMG1YrD2fDEUJwc5A?usp=drive_link
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/tcp.h>

#define MAX_BUFFER 100
#define PORT 8080

void encrypt_file(FILE *input, FILE *output, const char *key) {
    int c;
    while ((c = fgetc(input)) != EOF) {
        if (isalpha(c)) {
            char base = isupper(c) ? 'A' : 'a';
            int index = c - base;
            c = key[index];
            if (isupper(base)) {
                c = toupper(c);
            } else {
                c = tolower(c);
            }
        }
        fputc(c, output);
    }
}

int main() {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Add TCP_NODELAY option
    if (setsockopt(server_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))) {
        perror("setsockopt TCP_NODELAY failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept connection
    if ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    // Set TCP_NODELAY for client socket
    if (setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt))) {
        perror("setsockopt TCP_NODELAY failed for client socket");
        // Continue anyway, not a critical error
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(address.sin_port);

    while (1) {
        // Receive encryption key
        char key[27];
        ssize_t key_bytes = read(client_sock, key, 26);
        if (key_bytes <= 0) break;
        key[26] = '\0';

        // Create input filename with IP.port format
        char input_filename[256];
        snprintf(input_filename, sizeof(input_filename), "%s.%d.txt", 
                 client_ip, client_port);
        
        // Create encrypted filename
        char encrypted_filename[256];
        snprintf(encrypted_filename, sizeof(encrypted_filename), "%s.enc", 
                 input_filename);

        // Open input file for writing received data
        FILE *input_file = fopen(input_filename, "wb");
        if (!input_file) {
            perror("Failed to create input file");
            continue;
        }

        // Receive and write file data
        int eof_marker_found = 0;
        while (!eof_marker_found) {
            ssize_t bytes_read = read(client_sock, buffer, MAX_BUFFER);
            if (bytes_read <= 0) break;

            // Check for EOF marker
            for (int i = 0; i < bytes_read - 3; i++) {
                if (memcmp(&buffer[i], "EOF\n", 4) == 0) {
                    fwrite(buffer, 1, i, input_file);
                    eof_marker_found = 1;
                    break;
                }
            }
            
            if (!eof_marker_found) {
                fwrite(buffer, 1, bytes_read, input_file);
            }
        }
        fclose(input_file);

        // Open files for encryption
        input_file = fopen(input_filename, "rb");
        FILE *output_file = fopen(encrypted_filename, "wb");
        
        if (!input_file || !output_file) {
            perror("Error opening files for encryption");
            if (input_file) fclose(input_file);
            if (output_file) fclose(output_file);
            continue;
        }

        // Perform encryption
        encrypt_file(input_file, output_file, key);
        
        fclose(input_file);
        fclose(output_file);

        // Send encrypted file back to client
        FILE *send_file = fopen(encrypted_filename, "rb");
        if (!send_file) {
            perror("Cannot open encrypted file for sending");
            continue;
        }

        while (1) {
            size_t bytes_read = fread(buffer, 1, MAX_BUFFER, send_file);
            if (bytes_read == 0) break;
            
            if (write(client_sock, buffer, bytes_read) != (ssize_t)bytes_read) {
                perror("Error sending encrypted data");
                break;
            }
        }

        // Send EOF marker
        write(client_sock, "EOF\n", 4);
        fclose(send_file);
    }

    close(client_sock);
    close(server_fd);
    return 0;
}