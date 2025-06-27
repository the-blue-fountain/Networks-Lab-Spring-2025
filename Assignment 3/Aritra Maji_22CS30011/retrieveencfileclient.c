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
#include <netinet/tcp.h> // Add this header for TCP_NODELAY
#include <sys/time.h>

#define MAX_BUFFER 100
#define PORT 8080
#define SERVER_IP "127.0.0.1"

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    // Set TCP_NODELAY option
    int flag = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        printf("Failed to set TCP_NODELAY option\n");
        close(sock);
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IP address to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/Address not supported\n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return -1;
    }

    while (1) {
        FILE *fp = NULL;
        char filename[MAX_BUFFER];
        char key[27];

        // Get filename and check if file exists
        do {
            printf("Enter filename to encrypt: ");
            scanf("%99s", filename);
            clear_input_buffer();

            fp = fopen(filename, "rb");
            if (!fp) {
                printf("NOTFOUND %s\n", filename);
            }
        } while (!fp);

        // Get encryption key
        do {
            printf("Enter 26-character encryption key: ");
            scanf("%26s", key);
            clear_input_buffer();
            
            if (strlen(key) != 26) {
                printf("Error: Key must be exactly 26 characters long\n");
            }
        } while (strlen(key) != 26);
        
        
        // Send key to server
        if (write(sock, key, 26) != 26) {
            printf("Error sending key\n");
            fclose(fp);
            close(sock);
            return -1;
        }
        struct timeval start, end;
        gettimeofday(&start, NULL);
        // Send file contents in chunks
        while (1) {
            size_t bytes_read = fread(buffer, 1, MAX_BUFFER, fp);
            if (bytes_read == 0) break;

            if (write(sock, buffer, bytes_read) != (ssize_t)bytes_read) {
                printf("Error sending file data\n");
                fclose(fp);
                close(sock);
                return -1;
            }
        }

        // Send EOF marker
        if (write(sock, "EOF\n", 4) != 4) {
            printf("Error sending EOF marker\n");
            fclose(fp);
            close(sock);
            return -1;
        }

        fclose(fp);

        // Receive encrypted file
        char enc_filename[MAX_BUFFER];
        snprintf(enc_filename, sizeof(enc_filename), "%s.enc", filename);
        
        FILE *fp_out = fopen(enc_filename, "wb");
        if (!fp_out) {
            printf("Error creating output file\n");
            close(sock);
            return -1;
        }

        // Receive and write encrypted data
        int eof_found = 0;
        while (!eof_found) {
            ssize_t bytes_received = read(sock, buffer, MAX_BUFFER);
            if (bytes_received <= 0) break;

            // Check for EOF marker
            for (int i = 0; i < bytes_received - 3; i++) {
                if (memcmp(&buffer[i], "EOF\n", 4) == 0) {
                    fwrite(buffer, 1, i, fp_out);
                    eof_found = 1;
                    break;
                }
            }
            
            if (!eof_found) {
                fwrite(buffer, 1, bytes_received, fp_out);
            }
        }
        gettimeofday(&end, NULL);
        fclose(fp_out);

        // Print success message
        printf("File encrypted. Original: %s, Encrypted: %s\n", filename, enc_filename);
        // printf("Time taken: %ld microseconds\n", (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec);

        // Ask if user wants to encrypt another file
        char choice[4];
        printf("Do you want to encrypt another file? (Yes/No): ");
        scanf("%3s", choice);
        clear_input_buffer();

        if (strcasecmp(choice, "No") == 0) {
            break;
        }
    }

    close(sock);
    return 0;
}