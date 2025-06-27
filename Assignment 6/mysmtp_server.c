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
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10 
#define MAX_RECIPIENTS 5
#define PORT 2525
int mutex_id;
int writeaccess_id;
int readcount = 0;
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
void handle_client(int client_socket);
void create_directory(const char *path);
void send_response(int client_socket, const char *message);
int process_command(int client_socket, char *command, int *state, char *sender_email, char recipient_emails[][BUFFER_SIZE], int *recipient_count);
void handle_helo(int client_socket, char *args, int *state);
void handle_mail_from(int client_socket, char *args, int *state, char *sender_email, int *recipient_count);
void handle_rcpt_to(int client_socket, char *args, int *state, char recipient_emails[][BUFFER_SIZE], int *recipient_count, char *sender_email);
void handle_data(int client_socket, int *state, char *sender_email, char recipient_emails[][BUFFER_SIZE], int recipient_count);
void handle_list(int client_socket, char *args);
void handle_get_mail(int client_socket, char *args);
void handle_quit(int client_socket);
void start_read(void);
void end_read(void);
void start_write(void);
void end_write(void);
FILE* ensure_mailbox_file(const char *email);
int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pid_t child_pid;
    int port = PORT;
    key_t mutex_key = ftok("/tmp", 'a');
    key_t writeaccess_key = ftok("/tmp", 'b');
    mutex_id = semget(mutex_key, 1, IPC_CREAT | 0666);
    writeaccess_id = semget(writeaccess_key, 1, IPC_CREAT | 0666);
    if (mutex_id == -1 || writeaccess_id == -1) {
        perror("Error creating semaphores");
        exit(EXIT_FAILURE);
    }
    union semun arg;
    arg.val = 1;
    if (semctl(mutex_id, 0, SETVAL, arg) == -1) {
        perror("Error initializing mutex semaphore");
        exit(EXIT_FAILURE);
    }
    if (semctl(writeaccess_id, 0, SETVAL, arg) == -1) {
        perror("Error initializing writeaccess semaphore");
        exit(EXIT_FAILURE);
    }
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error setting socket options");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Error listening on socket");
        exit(EXIT_FAILURE);
    }
    printf("My_SMTP Server started. Listening on port %d...\n", port);
    create_directory("mailbox");
    signal(SIGPIPE, SIG_IGN);
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting connection");
            continue;
        }
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
        child_pid = fork();
        if (child_pid < 0) {
            perror("Error forking process");
            close(client_socket);
            continue;
        } else if (child_pid == 0) {
            close(server_socket);
            handle_client(client_socket);
            close(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }
    close(server_socket);
    semctl(mutex_id, 0, IPC_RMID);
    semctl(writeaccess_id, 0, IPC_RMID);
    return 0;
}
void start_read(void) {
    struct sembuf ops[2];
    ops[0].sem_num = 0;
    ops[0].sem_op = -1;
    ops[0].sem_flg = 0;
    if (semop(mutex_id, &ops[0], 1) == -1) {
        perror("semop: start_read [1]");
        exit(EXIT_FAILURE);
    }
    readcount++;
    if (readcount == 1) {
        ops[0].sem_num = 0;
        ops[0].sem_op = -1;
        ops[0].sem_flg = 0;
        if (semop(writeaccess_id, &ops[0], 1) == -1) {
            perror("semop: start_read [2]");
            exit(EXIT_FAILURE);
        }
    }
    ops[0].sem_num = 0;
    ops[0].sem_op = 1;
    ops[0].sem_flg = 0;
    if (semop(mutex_id, &ops[0], 1) == -1) {
        perror("semop: start_read [3]");
        exit(EXIT_FAILURE);
    }
}
void end_read(void) {
    struct sembuf ops[1];
    ops[0].sem_num = 0;
    ops[0].sem_op = -1;
    ops[0].sem_flg = 0;
    if (semop(mutex_id, &ops[0], 1) == -1) {
        perror("semop: end_read [1]");
        exit(EXIT_FAILURE);
    }
    readcount--;
    if (readcount == 0) {
        ops[0].sem_num = 0;
        ops[0].sem_op = 1;
        ops[0].sem_flg = 0;
        if (semop(writeaccess_id, &ops[0], 1) == -1) {
            perror("semop: end_read [2]");
            exit(EXIT_FAILURE);
        }
    }
    ops[0].sem_num = 0;
    ops[0].sem_op = 1;
    ops[0].sem_flg = 0;
    if (semop(mutex_id, &ops[0], 1) == -1) {
        perror("semop: end_read [3]");
        exit(EXIT_FAILURE);
    }
}
void start_write(void) {
    struct sembuf ops[1];
    ops[0].sem_num = 0;
    ops[0].sem_op = -1;
    ops[0].sem_flg = 0;
    if (semop(writeaccess_id, &ops[0], 1) == -1) {
        perror("semop: start_write");
        exit(EXIT_FAILURE);
    }
}
void end_write(void) {
    struct sembuf ops[1];
    ops[0].sem_num = 0;
    ops[0].sem_op = 1;
    ops[0].sem_flg = 0;
    if (semop(writeaccess_id, &ops[0], 1) == -1) {
        perror("semop: end_write");
        exit(EXIT_FAILURE);
    }
}
FILE* ensure_mailbox_file(const char *email) {
    char filepath[BUFFER_SIZE];
    FILE *file;
    snprintf(filepath, BUFFER_SIZE, "mailbox/%s.txt", email);
    if (access(filepath, F_OK) != 0) {
        file = fopen(filepath, "w");
        if (file) {
            fprintf(file, "_\n");
            fclose(file);
        } else {
            perror("Error creating mailbox file");
            return NULL;
        }
    }
    return fopen(filepath, "a+");
}
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int state = 1;
    char sender_email[BUFFER_SIZE] = "";
    char recipient_emails[MAX_RECIPIENTS + 1][BUFFER_SIZE];
    int recipient_count = 0;
    for (int i = 0; i <= MAX_RECIPIENTS; i++) {
        recipient_emails[i][0] = '\0';
    }
    send_response(client_socket, "220 My_SMTP Server Ready");
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("%s", "Client disconnected\n");
            break;
        }
        buffer[bytes_received] = '\0';
        if (buffer[bytes_received - 1] == '\n') {
            buffer[bytes_received - 1] = '\0';
        }
        if (buffer[bytes_received - 2] == '\r') {
            buffer[bytes_received - 2] = '\0';
        }
        if (process_command(client_socket, buffer, &state, sender_email, recipient_emails, &recipient_count) == -1) {
            break;
        }
    }
}
int process_command(int client_socket, char *command, int *state, char *sender_email, char recipient_emails[][BUFFER_SIZE], int *recipient_count) {
    char cmd[BUFFER_SIZE];
    char args[BUFFER_SIZE];
    int num_args = sscanf(command, "%s %[^\n]", cmd, args);
    if (num_args < 1) {
        send_response(client_socket, "400 ERR Invalid command syntax");
        return 0;
    }
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] >= 'a' && cmd[i] <= 'z') {
            cmd[i] = cmd[i] - 'a' + 'A';
        }
    }
    if (strcmp(cmd, "HELO") == 0) {
        handle_helo(client_socket, num_args > 1 ? args : "", state);
    } else if (strcmp(cmd, "MAIL") == 0) {
        if (strncmp(command, "MAIL FROM:", 10) == 0) {
            char *email_start = strchr(command + 10, '<');
            char *email_end = strchr(command + 10, '>');
            if (email_start && email_end && email_start < email_end) {
                *email_end = '\0';
                handle_mail_from(client_socket, email_start + 1, state, sender_email, recipient_count);
            } else {
                send_response(client_socket, "400 ERR Invalid MAIL FROM syntax");
            }
        } else {
            send_response(client_socket, "400 ERR Invalid MAIL FROM syntax");
        }
    } else if (strcmp(cmd, "RCPT") == 0) {
        if (strncmp(command, "RCPT TO:", 8) == 0) {
            char *email_start = strchr(command + 8, '<');
            char *email_end = strchr(command + 8, '>');
            if (email_start && email_end && email_start < email_end) {
                *email_end = '\0';
                handle_rcpt_to(client_socket, email_start + 1, state, recipient_emails, recipient_count, sender_email);
            } else {
                send_response(client_socket, "400 ERR Invalid RCPT TO syntax");
            }
        } else {
            send_response(client_socket, "400 ERR Invalid RCPT TO syntax");
        }
    } else if (strcmp(cmd, "DATA") == 0) {
        if (*state == 3) {
            handle_data(client_socket, state, sender_email, recipient_emails, *recipient_count);
        } else {
            send_response(client_socket, "403 FORBIDDEN Action not permitted");
        }
    } else if (strcmp(cmd, "LIST") == 0) {
        if (*state != 1) {
            send_response(client_socket, "403 FORBIDDEN LIST can only be used in initial state");
            return 0;
        }
        if (num_args > 1) {
            handle_list(client_socket, args);
        } else {
            send_response(client_socket, "400 ERR Invalid LIST syntax");
        }
    } else if (strcmp(cmd, "GET_MAIL") == 0) {
        if (*state != 1) {
            send_response(client_socket, "403 FORBIDDEN GET_MAIL can only be used in initial state");
            return 0;
        }
        if (num_args > 1) {
            handle_get_mail(client_socket, args);
        } else {
            send_response(client_socket, "400 ERR Invalid GET_MAIL syntax");
        }
    } else if (strcmp(cmd, "QUIT") == 0) {
        handle_quit(client_socket);
        return -1;
    } else {
        send_response(client_socket, "400 ERR Unknown command");
    }
    return 0;
}
void handle_helo(int client_socket, char *args, int *state) {
    printf("HELO received from %s\n", args);
    *state = 1;
    send_response(client_socket, "200 OK");
}
void handle_mail_from(int client_socket, char *args, int *state, char *sender_email, int *recipient_count) {
    if (*state != 1) {
        send_response(client_socket, "403 FORBIDDEN Action not permitted");
        return;
    }
    strcpy(sender_email, args);
    *recipient_count = 0;
    printf("MAIL FROM: %s\n", sender_email);
    *state = 2;
    send_response(client_socket, "200 OK");
}
void handle_rcpt_to(int client_socket, char *args, int *state, char recipient_emails[][BUFFER_SIZE], int *recipient_count, char *sender_email) {
    if (*state != 2 && *state != 3) {
        send_response(client_socket, "403 FORBIDDEN Action not permitted");
        return;
    }
    if (*recipient_count >= MAX_RECIPIENTS) {
        send_response(client_socket, "452 Too many recipients");
        return;
    }
    if (strcasecmp(args, sender_email) == 0) {
        send_response(client_socket, "403 FORBIDDEN Cannot send email to yourself");
        return;
    }
    strcpy(recipient_emails[*recipient_count], args);
    (*recipient_count)++;
    recipient_emails[*recipient_count][0] = '\0';
    printf("RCPT TO: %s (Recipient %d of %d)\n", args, *recipient_count, MAX_RECIPIENTS);
    *state = 3;
    send_response(client_socket, "200 OK");
}
void handle_data(int client_socket, int *state, char *sender_email, char recipient_emails[][BUFFER_SIZE], int recipient_count) {
    char buffer[BUFFER_SIZE];
    FILE *email_file;
    time_t current_time;
    struct tm *time_info;
    char time_str[80];
    if (recipient_count == 0) {
        send_response(client_socket, "503 No valid recipients");
        return;
    }
    time(&current_time);
    time_info = localtime(&current_time);
    strftime(time_str, sizeof(time_str), "%d-%m-%Y %H:%M:%S", time_info);
    send_response(client_socket, "354 Start mail input; end with <CRLF>.<CRLF>");
    char message_body[BUFFER_SIZE * 50] = "";
    int message_len = 0;
    int done = 0;
    while (!done) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            return;
        }
        buffer[bytes_received] = '\0';
        if (strcmp(buffer, ".\r\n") == 0 || strcmp(buffer, ".\n") == 0 || strcmp(buffer, ".") == 0) {
            done = 1;
        } else {
            int buffer_len = strlen(buffer);
            if (message_len + buffer_len < BUFFER_SIZE * 50) {
                strcat(message_body, buffer);
                message_len += buffer_len;
            }
        }
    }
    for (int i = 0; i < recipient_count; i++) {
        start_write();
        email_file = ensure_mailbox_file(recipient_emails[i]);
        if (!email_file) {
            end_write();
            continue;
        }
        fprintf(email_file, "sender: %s\n", sender_email);
        fprintf(email_file, "datetime: %s\n", time_str);
        fprintf(email_file, "data: %s\n", message_body);
        fprintf(email_file, "_\n");
        fclose(email_file);
        end_write();
        printf("Email stored for recipient %s\n", recipient_emails[i]);
    }
    printf("DATA received, message stored for %d recipients.\n", recipient_count);
    *state = 1;
    send_response(client_socket, "200 OK Message stored successfully");
}
void handle_list(int client_socket, char *email) {
    char file_path[BUFFER_SIZE];
    FILE *file;
    char line[BUFFER_SIZE];
    char response[BUFFER_SIZE * 10] = "200 OK\n";
    char clean_email[BUFFER_SIZE];
    char sender[BUFFER_SIZE] = "";
    char datetime[BUFFER_SIZE] = "";
    int found = 0;
    int id = 0;
    int in_message = 0;
    if (email[0] == '<' && strchr(email, '>') != NULL) {
        char *end = strchr(email, '>');
        strncpy(clean_email, email + 1, end - email - 1);
        clean_email[end - email - 1] = '\0';
    } else {
        sscanf(email, "%s", clean_email);
    }
    printf("Processing LIST command for email: '%s'\n", clean_email);
    snprintf(file_path, BUFFER_SIZE, "mailbox/%s.txt", clean_email);
    start_read();
    file = fopen(file_path, "r");
    if (!file) {
        end_read();
        send_response(client_socket, "200 OK\nNo emails found.");
        printf("File not found: %s\n", file_path);
        return;
    }
    while (fgets(line, BUFFER_SIZE, file)) {
        if (strncmp(line, "_", 1) == 0) {
            if (in_message) {
                char temp_buffer[BUFFER_SIZE];
                snprintf(temp_buffer, BUFFER_SIZE, "%d: Email from %s (%s)\n", id, sender, datetime);
                strcat(response, temp_buffer);
                found = 1;
                sender[0] = '\0';
                datetime[0] = '\0';
            }
            in_message = 1;
            id++;
        } 
        else if (in_message) {
            if (strncmp(line, "sender:", 7) == 0) {
                sscanf(line, "sender: %[^\n]", sender);
            } 
            else if (strncmp(line, "datetime:", 9) == 0) {
                sscanf(line, "datetime: %[^\n]", datetime);
            }
        }
    }
    fclose(file);
    end_read();
    if (!found) {
        send_response(client_socket, "200 OK\nNo emails found.");
    } else {
        send_response(client_socket, response);
    }
    printf("LIST %s\nEmails retrieved; list sent.\n", clean_email);
}
void handle_get_mail(int client_socket, char *args) {
    char email[BUFFER_SIZE];
    char args_copy[BUFFER_SIZE];
    int target_id;
    char file_path[BUFFER_SIZE];
    FILE *file;
    char line[BUFFER_SIZE];
    char response[BUFFER_SIZE * 100] = "200 OK\n";
    int found = 0;
    int id = 0;
    int in_target_message = 0;
    strncpy(args_copy, args, BUFFER_SIZE - 1);
    args_copy[BUFFER_SIZE - 1] = '\0';
    char *email_start = args_copy;
    if (args_copy[0] == '<') {
        email_start = args_copy + 1;
        char *email_end = strchr(email_start, '>');
        if (email_end) {
            *email_end = '\0';
            char *id_str = email_end + 1;
            while (*id_str && (*id_str == ' ' || *id_str == '\t')) {
                id_str++;
            }
            if (sscanf(id_str, "%d", &target_id) != 1) {
                send_response(client_socket, "400 ERR Invalid GET_MAIL syntax");
                return;
            }
        } else {
            send_response(client_socket, "400 ERR Invalid GET_MAIL syntax");
            return;
        }
        strcpy(email, email_start);
    } else {
        if (sscanf(args_copy, "%s %d", email, &target_id) != 2) {
            send_response(client_socket, "400 ERR Invalid GET_MAIL syntax");
            return;
        }
    }
    printf("Processing GET_MAIL command for email: '%s', id: %d\n", email, target_id);
    snprintf(file_path, BUFFER_SIZE, "mailbox/%s.txt", email);
    start_read();
    file = fopen(file_path, "r");
    if (!file) {
        end_read();
        send_response(client_socket, "401 NOT FOUND Requested email does not exist");
        printf("File not found: %s\n", file_path);
        return;
    }
    while (fgets(line, BUFFER_SIZE, file)) {
        if (strncmp(line, "_", 1) == 0) {
            if (in_target_message) {
                found = 1;
                break;
            }
            id++;
            if (id == target_id) {
                in_target_message = 1;
            }
        } 
        else if (in_target_message) {
            strcat(response, line);
        }
    }
    fclose(file);
    end_read();
    if (!found) {
        send_response(client_socket, "401 NOT FOUND Requested email does not exist");
    } else {
        send_response(client_socket, response);
    }
    printf("GET_MAIL %s %d\nEmail with id %d sent.\n", email, target_id, target_id);
}
void handle_quit(int client_socket) {
    send_response(client_socket, "200 Goodbye");
    printf("Client disconnected.\n");
}
void create_directory(const char *path) {
    char tmp[BUFFER_SIZE];
    char *p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}
void send_response(int client_socket, const char *message) {
    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "%s\r\n", message);
    send(client_socket, response, strlen(response), 0);
}
