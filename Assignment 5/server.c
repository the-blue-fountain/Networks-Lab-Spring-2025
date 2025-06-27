/*
Assignment 5 Submission
Name: Aritra Maji
Roll number: 22CS30011
*/
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#define MAX_TASK_LEN 40
#define MAX_BUFFER 1024
#define MAX_TASKS 100
#define PORT 8080
#define ENDMARK "end"
typedef struct {
    char tasks[MAX_TASKS][MAX_TASK_LEN];
    int out;
    int numTasks;
} SharedData;
int shm_id, sem_access_id, sem_print_id;
SharedData* shared_data;
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};

void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void getTasks() {
    int fd;
    char buffer[MAX_TASK_LEN];
    ssize_t bytes_read;
    int task_count = 0;
    shared_data->numTasks = 0;
    fd = open("tasks.txt", O_RDONLY);
    if (fd == -1) {
        exit(EXIT_FAILURE);
    }
    char ch;
    int idx = 0;
    memset(buffer, 0, MAX_TASK_LEN);
    while ((bytes_read = read(fd, &ch, 1)) > 0) {
        if (ch == '\n' || idx >= MAX_TASK_LEN - 1) {
            buffer[idx] = '\0';
            if (idx > 0) {
                if (strcmp(buffer, ENDMARK) == 0) {
                    break;
                }
                if (task_count < MAX_TASKS) {
                    strcpy(shared_data->tasks[task_count], buffer);
                    task_count++;
                }
            }
            idx = 0;
            memset(buffer, 0, MAX_TASK_LEN);
        } else {
            buffer[idx++] = ch;
        }
    }
    if (idx > 0) {
        buffer[idx] = '\0';
        if (strcmp(buffer, ENDMARK) != 0 && task_count < MAX_TASKS) {
            strcpy(shared_data->tasks[task_count], buffer);
            task_count++;
        }
    }
    close(fd);
    if (task_count == 0) {
        exit(EXIT_FAILURE);
    }
    shared_data->numTasks = task_count;
    shared_data->out = 0;
}

void wait_sem(int sem_id) {
    struct sembuf sb = {0, -1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        exit(EXIT_FAILURE);
    }
}

void signal_sem(int sem_id) {
    struct sembuf sb = {0, 1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        exit(EXIT_FAILURE);
    }
}

void handle_client(int client_socket) {
    char buffer[MAX_BUFFER];
    char task_str[MAX_TASK_LEN];
    int bytes;
    int client_id = getpid();
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    char client_ip[INET_ADDRSTRLEN];
    getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_size);
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
    printf("Connected with client %s:%d\n", client_ip, ntohs(client_addr.sin_port));
    while (1) {
        memset(buffer, 0, MAX_BUFFER);
        while(1){
            while (1) {
                bytes = recv(client_socket, buffer, MAX_BUFFER - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    break;
                } else if (bytes == 0) {
                    close(client_socket);
                    printf("Connection closed with client %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                    exit(EXIT_SUCCESS);
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    close(client_socket);
                    exit(EXIT_FAILURE);
                }
                usleep(50000);
            }
            if(strcmp(buffer, "GET_TASK")==0){
                break;
            } else {
                printf("Error : Unexpected message from client %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                strcpy(buffer, "Error: Unexpected message");
                send(client_socket, buffer, strlen(buffer), 0);
                memset(buffer, 0, MAX_BUFFER);
            }
        }
        wait_sem(sem_access_id);
        if (shared_data->out < shared_data->numTasks) {
            strcpy(task_str, shared_data->tasks[shared_data->out]);
            shared_data->out++;
            signal_sem(sem_access_id);
            snprintf(buffer, MAX_BUFFER, "Task: %s", task_str);
            send(client_socket, buffer, strlen(buffer), 0);
        } else {
            signal_sem(sem_access_id);
            strcpy(buffer, "No tasks available");
            send(client_socket, buffer, strlen(buffer), 0);
            break;
        }
        memset(buffer, 0, MAX_BUFFER);
        while(1){
            while (1) {
                bytes = recv(client_socket, buffer, MAX_BUFFER - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    break;
                } else if (bytes == 0) {
                    close(client_socket);
                    printf("Connection closed with client %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                    exit(EXIT_SUCCESS);
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    close(client_socket);
                    exit(EXIT_FAILURE);
                }
                usleep(50000);
            }
            if(strncmp(buffer, "RESULT", 6) == 0){
                break;
            } else {
                printf("Error : Unexpected message from client %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                strcpy(buffer, "Error: Unexpected message");
                send(client_socket, buffer, strlen(buffer), 0);
                memset(buffer, 0, MAX_BUFFER);
            }
        }
        double result = atof(buffer + 7);
        printf("task: %s\n", task_str);
        printf("result: %.6f\n", result);
        strcpy(buffer, "OK");
        send(client_socket, buffer, strlen(buffer), 0);
    }
    close(client_socket);
    printf("Connection closed with client %s:%d\n", client_ip, ntohs(client_addr.sin_port));
    exit(EXIT_SUCCESS);
}

int server_fd;

void cleanup_and_exit(int sig) {
    printf("Cleaning up and exiting...\n");
    if (server_fd > 0) {
        close(server_fd);
    }
    if (shared_data != NULL) {
        shmdt(shared_data);
    }
    if (shm_id > 0) {
        shmctl(shm_id, IPC_RMID, NULL);
    }
    if (sem_access_id > 0) {
        semctl(sem_access_id, 0, IPC_RMID);
    }
    if (sem_print_id > 0) {
        semctl(sem_print_id, 0, IPC_RMID);
    }
    exit(EXIT_SUCCESS);
}

int main() {
    int client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    pid_t pid;
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        exit(EXIT_FAILURE);
    }
    struct sigaction sa_int;
    sa_int.sa_handler = &cleanup_and_exit;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa_int, NULL) == -1) {
        exit(EXIT_FAILURE);
    }
    shm_id = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        exit(EXIT_FAILURE);
    }
    shared_data = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared_data == (SharedData *)-1) {
        exit(EXIT_FAILURE);
    }
    sem_access_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    sem_print_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_access_id == -1 || sem_print_id == -1) {
        exit(EXIT_FAILURE);
    }
    union semun arg;
    arg.val = 1;
    if (semctl(sem_access_id, 0, SETVAL, arg) == -1) {
        exit(EXIT_FAILURE);
    }
    if (semctl(sem_print_id, 0, SETVAL, arg) == -1) {
        exit(EXIT_FAILURE);
    }
    wait_sem(sem_access_id);
    getTasks();
    signal_sem(sem_access_id);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) == -1) {
        exit(EXIT_FAILURE);
    }
    int client_counter = 0;
    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_socket == -1) {
            if (errno != EINTR) {
            }
            continue;
        }
        client_counter++;
        pid = fork();
        if (pid < 0) {
            close(client_socket);
        } else if (pid == 0) {
            close(server_fd);
            handle_client(client_socket);
        } else {
            close(client_socket);
        }
    }  
    return 0;
}
