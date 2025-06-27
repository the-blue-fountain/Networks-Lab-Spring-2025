/*
Name : Aritra Maji
Roll : 22CS30011
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define BUFFER_SIZE 1024
#define DEFAULT_PORT 2525
void print_response(const char *response);
void handle_data_command(int sock);
void get_user_input(char *buffer, int size);
void display_help(void);
void display_declaration(void);

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char server_response[BUFFER_SIZE * 10];
    char *server_ip;
    int port;
    display_declaration();
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_ip> [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    server_ip = argv[1];
    port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Connected to My_SMTP server at %s:%d\n", server_ip, port);
    display_help();
    memset(server_response, 0, BUFFER_SIZE * 10);
    if (recv(sock, server_response, BUFFER_SIZE * 10 - 1, 0) <= 0) {
        perror("Error receiving server welcome message");
        close(sock);
        exit(EXIT_FAILURE);
    }
    print_response(server_response);
    while (1) {
        printf("::=> ");
        get_user_input(buffer, BUFFER_SIZE);
        if (strcasecmp(buffer, "HELP") == 0) {
            display_help();
            continue;
        }
        if (strcasecmp(buffer, "QUIT") == 0) {
            strcat(buffer, "\r\n");
            send(sock, buffer, strlen(buffer), 0);
            memset(server_response, 0, BUFFER_SIZE * 10);
            if (recv(sock, server_response, BUFFER_SIZE * 10 - 1, 0) > 0) {
                print_response(server_response);
            }
            break;
        }
        if (strcasecmp(buffer, "DATA") == 0) {
            strcat(buffer, "\r\n");
            send(sock, buffer, strlen(buffer), 0);
            memset(server_response, 0, BUFFER_SIZE * 10);
            if (recv(sock, server_response, BUFFER_SIZE * 10 - 1, 0) <= 0) {
                perror("Error receiving response");
                break;
            }
            print_response(server_response);
            if (strncmp(server_response, "354", 3) == 0) {
                handle_data_command(sock);
            }
            continue;
        }
        strcat(buffer, "\r\n");
        send(sock, buffer, strlen(buffer), 0);
        memset(server_response, 0, BUFFER_SIZE * 10);
        if (recv(sock, server_response, BUFFER_SIZE * 10 - 1, 0) <= 0) {
            perror("Error receiving response");
            break;
        }
        print_response(server_response);
    }
    close(sock);
    printf("Connection closed\n");
    return 0;
}

void display_declaration(void) {
    printf("\n======= IMPORTANT DECLARATION =======\n");
    printf("You are not allowed to send a mail to more than 5 recipients at a time.\n");
    printf("If you need to send to more recipients, you must modify the MAX_RECIPIENTS\n");
    printf("value in the server code and recompile.\n");
    printf("====================================\n\n");
}

void display_help(void) {
    printf("\n===== My_SMTP Client Help =====\n");
    printf("Available commands:\n");
    printf("  HELO <client_id>              - Initiate communication with server\n");
    printf("  MAIL FROM: <email>            - Specify sender's email address\n");
    printf("  RCPT TO: <email>              - Specify recipient's email address\n");
    printf("  DATA                          - Start email content (end with a single '.' on a new line)\n");
    printf("  LIST <email>                  - List all emails for the given recipient\n");
    printf("  LIST email                    - Alternative form without angle brackets\n");
    printf("  GET_MAIL <email> <id>         - Retrieve a specific email by its ID\n");
    printf("  GET_MAIL email id             - Alternative form without angle brackets\n");
    printf("  HELP                          - Display this help message\n");
    printf("  QUIT                          - End the session\n");
    printf("\nTypical email sending sequence: HELO -> MAIL FROM: <email> -> RCPT TO: <email> -> DATA\n");
    printf("================================\n\n");
}

void print_response(const char *response) {
    char *line, *tmp, *resp_copy;
    int is_get_mail_response = 0;
    int header_line = 0;
    resp_copy = strdup(response);
    if (!resp_copy) {
        perror("Memory allocation failed");
        return;
    }
    if (strstr(resp_copy, "\nFrom:") != NULL && strstr(resp_copy, "\nDate:") != NULL) {
        is_get_mail_response = 1;
    }
    tmp = resp_copy;
    while ((line = strsep(&tmp, "\r\n")) != NULL) {
        if (strlen(line) > 0) {
            if (header_line == 0) {
                if (strncmp(line, "200", 3) == 0 || strncmp(line, "220", 3) == 0 || 
                    strncmp(line, "250", 3) == 0 || strncmp(line, "354", 3) == 0) {
                    printf("[+] %s\n", line);
                } else if (strncmp(line, "4", 1) == 0 || strncmp(line, "5", 1) == 0) {
                    printf("[-] %s\n", line);
                } else {
                    printf("[+] %s\n", line);
                }
                header_line = 1;
            } 
            else if (is_get_mail_response) {
                printf("[=] %s\n", line);
            } 
            else {
                printf("[=] %s\n", line);
            }
        }
    }
    free(resp_copy);
}

void handle_data_command(int sock) {
    char buffer[BUFFER_SIZE];
    char server_response[BUFFER_SIZE * 10];
    printf("Enter your message (end with a single dot '.' on a new line):\n");
    while (1) {
        get_user_input(buffer, BUFFER_SIZE);
        strcat(buffer, "\r\n");
        send(sock, buffer, strlen(buffer), 0);
        if (strcmp(buffer, ".\r\n") == 0) {
            break;
        }
    }
    memset(server_response, 0, BUFFER_SIZE * 10);
    if (recv(sock, server_response, BUFFER_SIZE * 10 - 1, 0) > 0) {
        print_response(server_response);
    }
}

void get_user_input(char *buffer, int size) {
    if (fgets(buffer, size, stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    } else {
        buffer[0] = '\0';
    }
}
