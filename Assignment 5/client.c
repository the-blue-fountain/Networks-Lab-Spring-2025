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
double evaluate(const char* expression) {
    double a, b, result;
    char op = '\0';
    int i;
    for (i = 0; i < strlen(expression); i++) {
        if (expression[i] == '+' || expression[i] == '-' || 
            expression[i] == '*' || expression[i] == '/' || 
            expression[i] == '^') {
            op = expression[i];
            break;
        }
    }
    if (op == '\0') {
        return 0;
    }
    char left[MAX_BUFFER] = {0}, right[MAX_BUFFER] = {0};
    strncpy(left, expression, i);
    strcpy(right, expression + i + 1);
    a = atof(left);
    b = atof(right);
    switch (op) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*': result = a * b; break;
        case '/': 
            if (b == 0) return 0;
            result = a / b; 
            break;
        case '^': result = pow(a, b); break;
        default: result = 0;
    }    
    return result;
}

int main(int argc, char *argv[]) {
    printf("The first command line argument to client is the additional time, apart from the computation time, that I will take to evaluate a task. This is necessary because otherwise you cannot test for multiple client processes.\n");
    int sock = 0, bytes;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER] = {0};
    char task[MAX_BUFFER] = {0};
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
    int tasks_completed = 0;
    memset(buffer, 0, MAX_BUFFER);
    strcpy(buffer, "GET_TASK");
    send(sock, buffer, strlen(buffer), 0);
    while (1) {        
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
        if (strcmp(buffer, "OK") == 0) {
            memset(buffer, 0, MAX_BUFFER);
            strcpy(buffer, "GET_TASK");
            send(sock, buffer, strlen(buffer), 0);
            continue;
        }
        if (strcmp(buffer, "No tasks available") == 0) {
            break;
        }
        if (strncmp(buffer, "Error:", 6) == 0) {
            sleep(delay);
            memset(buffer, 0, MAX_BUFFER);
            strcpy(buffer, "GET_TASK");
            send(sock, buffer, strlen(buffer), 0);
            continue;
        }
        if (strncmp(buffer, "Task:", 5) == 0) {
            strcpy(task, buffer + 6);
            double result = evaluate(task);
            printf("task: %s\n", task);
            sleep(delay);
            memset(buffer, 0, MAX_BUFFER);
            sprintf(buffer, "RESULT %.6f", result);
            send(sock, buffer, strlen(buffer), 0);
            printf("result: %.6f\n", result);
            tasks_completed++;
        }
    }
    close(sock);
    printf("Cleaning up and exiting...\n");
    return 0;
}
