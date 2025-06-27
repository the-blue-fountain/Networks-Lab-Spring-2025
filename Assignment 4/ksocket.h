/*===========================================
 Assignment 4: Emulating End-to-End Reliable Flow Control
 Name: Aritra Maji
 Roll number: 22CS30011
============================================*/

#ifndef KSOCKET_H
#define KSOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

// Configuration parameters
#define T 5             // Timeout period in seconds
#define DROP_PROB 0.05  // Message drop probability
#define SOCK_KTP 3      // Socket type for KTP
#define N 10            // Maximum number of KTP sockets
#define MAX_MSG_SIZE 512 // Fixed message size
#define MAX_SEQ_NUM 256 // Maximum sequence number (8 bits)
#define BUFFER_SIZE 10  // Size of buffer (in number of messages)

// Semaphore macros
#define P(s) semop(s, &sem_decrement, 1)
#define V(s) semop(s, &sem_increment, 1)

// Custom error codes
#define ENOTBOUND 200   // Not bound to destination
#define ENOSPACE 201    // No space available
#define ENOMESSAGE 202  // No message available

// Message types
#define DATA_MSG '1'
#define ACK_MSG '0'

// Flow control window structure
typedef struct window {
    int slots[MAX_SEQ_NUM];  // Window entries (buffer indices or -1 if not used)
    int size;              // Current window size
    int start;             // Start position of the window
} window;

// Socket information structure
typedef struct net_socket {
    int sock_id;           // UDP socket ID
    char ip_addr[INET_ADDRSTRLEN]; // IP address
    uint16_t port;         // Port number
    int err_code;          // Error code
} NET_SOCKET;

struct sock_info {
    int free;              // 1 if socket is free, 0 if allocated
    pid_t pid;             // Process ID
    int udp_sockid;        // Associated UDP socket ID
    char ip_addr[INET_ADDRSTRLEN];  // Destination IP address
    uint16_t port;         // Destination port
};

struct send_info{
    // Send buffer
    char buffer[BUFFER_SIZE][MAX_MSG_SIZE];
    int free_slots;       // Available space in send buffer
    int lengths[BUFFER_SIZE];  // Actual data length for each buffer slot
    time_t timestamps[MAX_SEQ_NUM];  // Timestamp of last send for each sequence
};

struct receive_info{
    // Receive buffer
    char buffer[BUFFER_SIZE][MAX_MSG_SIZE];
    int active[BUFFER_SIZE];   // 1 if slot contains valid data, 0 otherwise
    int lengths[BUFFER_SIZE];  // Length of received data
    int base_idx;              // Base index of the receive buffer
};

// Shared memory structure for each KTP socket
typedef struct shared_memory {    
    struct sock_info sock_info;  // Socket information
    struct send_info send_info;  // Send buffer information
    struct receive_info recv_info;  // Receive buffer information     
    window swnd;           // Sending window
    window rwnd;           // Receiving window
    int buffer_full;       // Flag to indicate no space in receive buffer
} SHARED_MEMORY;

// External variables
extern SHARED_MEMORY *shared_mem;
extern NET_SOCKET *net_socket;
extern struct sembuf sem_decrement, sem_increment;
extern int semid_shared_mem, semid_net_socket;
extern int shmid_shared_mem, shmid_net_socket;
extern int semid_init, semid_ktp;

// Function prototypes
int k_socket(int domain, int type, int protocol);
int k_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port);
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int k_close(int sockfd);
int dropMessage(float prob);

#endif // KSOCKET_H
