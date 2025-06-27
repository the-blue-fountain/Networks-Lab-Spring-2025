/*===========================================
 Assignment 4: Emulating End-to-End Reliable Flow Control
 Name: Aritra Maji
 Roll number: 22CS30011
============================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include <pthread.h>
#include "ksocket.h"

// Local helper function prototypes 
static void initialize_ipc_resources(void);
static void cleanup_ipc_resources(void);
static void encode_sequence(char *buffer, int seq_num);
static void encode_window_size(char *buffer, int window_size);
static int extract_sequence(const char *buffer);
static int extract_data_length(const char *buffer);
static int extract_window_size(const char *buffer);
static void send_ack_message(int sock_id, int seq, int window_size, struct sockaddr_in *addr);
static void process_data_message(int sock_index, char *buffer, int msg_len, struct sockaddr_in *addr);
static void process_ack_message(int sock_index, char *buffer);
static void retransmit_packets(int sock_index);
static void transmit_new_packets(int sock_index);

// Global thread variables to properly terminate threads
pthread_t receiver_thread, sender_thread, gc_thread;
volatile sig_atomic_t terminate_flag = 0;

// Function to create and bind UDP sockets as requested by k_socket and k_bind
void create_and_bind() {
    printf("Starting socket handler thread\n");
    
    while(1) {
        // Wait for a socket request from KTP library
        P(semid_init);
        P(semid_net_socket);
        
        // Check if this is a socket creation or a bind request
        if (net_socket->sock_id == 0 && net_socket->port == 0) {
            // Create new UDP socket
            printf("Creating new UDP socket\n");
            int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (udp_sock < 0) {
                fprintf(stderr, "Failed to create UDP socket: %s\n", strerror(errno));
                net_socket->sock_id = -1;
                net_socket->err_code = errno;
            } else {
                net_socket->sock_id = udp_sock;
                printf("Created UDP socket with ID: %d\n", udp_sock);
            }
        } else {
            // Bind existing socket to address
            printf("Binding socket %d to %s:%d\n", net_socket->sock_id, 
                   net_socket->ip_addr, net_socket->port);
                   
            // Prepare address structure
            struct sockaddr_in bind_addr;
            memset(&bind_addr, 0, sizeof(bind_addr));
            bind_addr.sin_family = AF_INET;
            bind_addr.sin_port = htons(net_socket->port);
            
            // Convert IP string to network format
            if (inet_pton(AF_INET, net_socket->ip_addr, &(bind_addr.sin_addr)) <= 0) {
                fprintf(stderr, "Invalid IP address format: %s\n", net_socket->ip_addr);
                net_socket->sock_id = -1;
                net_socket->err_code = EINVAL;
                V(semid_net_socket);
                V(semid_ktp);
                continue;
            }
            
            // Perform the bind operation
            if (bind(net_socket->sock_id, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
                fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
                net_socket->sock_id = -1;
                net_socket->err_code = errno;
            } else {
                printf("Successfully bound socket %d to %s:%d\n", 
                       net_socket->sock_id, net_socket->ip_addr, net_socket->port);
            }
        }
        
        V(semid_net_socket);
        V(semid_ktp);
    }
}

// Garbage collector thread function
void *GC() {
    printf("Starting garbage collector thread\n");
    
    while(1) {
        // Run garbage collection periodically
        sleep(T);
        
        P(semid_shared_mem);
        for (int socket_idx = 0; socket_idx < N; socket_idx++) {
            if (shared_mem[socket_idx].sock_info.free == 0) {
                // Check if the process that created this socket still exists
                if (kill(shared_mem[socket_idx].sock_info.pid, 0) == -1 && errno == ESRCH) {
                    // Process doesn't exist anymore, free the socket resources
                    printf("GC: Process %d not found, freeing socket %d\n", 
                           shared_mem[socket_idx].sock_info.pid, socket_idx);
                    shared_mem[socket_idx].sock_info.free = 1;
                    
                    // Close the UDP socket if it's open
                    if (shared_mem[socket_idx].sock_info.udp_sockid > 0) {
                        close(shared_mem[socket_idx].sock_info.udp_sockid);
                        printf("GC: Closed UDP socket %d\n", shared_mem[socket_idx].sock_info.udp_sockid);
                    }
                }
            }
        }
        V(semid_shared_mem);
    }
    
    return NULL;
}

// Helper function to encode sequence number in binary format
static void encode_sequence(char *buffer, int seq_num) {
    // Convert sequence number to binary (8 bits)
    for (int i = 0; i < 8; i++) {
        buffer[8 - i - 1] = '0' + ((seq_num >> i) & 1);
    }
}

// Helper function to encode window size in binary format
static void encode_window_size(char *buffer, int window_size) {
    // Convert window size to binary (4 bits)
    for (int i = 0; i < 4; i++) {
        buffer[4 - i - 1] = '0' + ((window_size >> i) & 1);
    }
}

// Helper function to extract sequence number from binary format
static int extract_sequence(const char *buffer) {
    int seq = 0;
    for (int i = 1; i <= 8; i++) {
        seq = (seq << 1) | (buffer[i] - '0');
    }
    return seq;
}

// Helper function to extract data length from binary format
static int extract_data_length(const char *buffer) {
    int len = 0;
    for (int i = 9; i <= 18; i++) {
        len = (len << 1) | (buffer[i] - '0');
    }
    return len;
}

// Helper function to extract window size from binary format
static int extract_window_size(const char *buffer) {
    int size = 0;
    for (int i = 9; i <= 12; i++) {
        size = (size << 1) | (buffer[i] - '0');
    }
    return size;
}

// Helper function to send an ACK message
static void send_ack_message(int sock_id, int seq, int window_size, struct sockaddr_in *addr) {
    char ack[13]; // ACK message is 13 bytes long
    
    // Format the ACK message
    ack[0] = ACK_MSG;
    
    // Add sequence number in binary format
    encode_sequence(ack + 1, seq);
    
    // Add window size in binary format
    encode_window_size(ack + 9, window_size);
    
    // Send the ACK message
    sendto(sock_id, ack, sizeof(ack), 0, (struct sockaddr*)addr, sizeof(*addr));
    printf("R: Sent ACK seq=%d rwnd=%d\n", seq, window_size);
}

// Process a received data message
static void process_data_message(int sock_index, char *buffer, int msg_len, struct sockaddr_in *addr) {
    // Extract sequence number and data length
    int seq_num = extract_sequence(buffer);
    int data_len = extract_data_length(buffer);
    
    printf("R: Received DATA seq=%d len=%d for socket %d\n", seq_num, data_len, sock_index);
    
    // Handle in-order message
    if (seq_num == shared_mem[sock_index].rwnd.start) {
        // Get corresponding buffer slot for this sequence number
        int buffer_idx = shared_mem[sock_index].rwnd.slots[seq_num];
        
        if (buffer_idx >= 0) {
            // Copy data to the buffer slot
            memcpy(shared_mem[sock_index].recv_info.buffer[buffer_idx], buffer + 19, data_len);
            shared_mem[sock_index].recv_info.active[buffer_idx] = 1;
            shared_mem[sock_index].recv_info.lengths[buffer_idx] = data_len;
            shared_mem[sock_index].rwnd.size--;
            
            // Slide window forward for consecutive received packets
            int next_seq = seq_num;
            do {
                next_seq = (next_seq + 1) % MAX_SEQ_NUM;
                shared_mem[sock_index].rwnd.start = next_seq;
            } while (shared_mem[sock_index].rwnd.slots[next_seq] >= 0 && 
                    shared_mem[sock_index].recv_info.active[shared_mem[sock_index].rwnd.slots[next_seq]] && 
                    next_seq != seq_num);
        }
    } 
    // Handle out-of-order message within receive window
    else {
        int rel_seq = (seq_num - shared_mem[sock_index].rwnd.start + MAX_SEQ_NUM) % MAX_SEQ_NUM;
        
        if (rel_seq < BUFFER_SIZE) {
            int buffer_idx = shared_mem[sock_index].rwnd.slots[seq_num];
            
            if (buffer_idx >= 0 && !shared_mem[sock_index].recv_info.active[buffer_idx]) {
                // Store out-of-order packet
                memcpy(shared_mem[sock_index].recv_info.buffer[buffer_idx], buffer + 19, data_len);
                shared_mem[sock_index].recv_info.active[buffer_idx] = 1;
                shared_mem[sock_index].recv_info.lengths[buffer_idx] = data_len;
                shared_mem[sock_index].rwnd.size--;
            }
        }
    }
    
    // Check if buffer is now full
    if (shared_mem[sock_index].rwnd.size == 0) {
        shared_mem[sock_index].buffer_full = 1;
        printf("R: Buffer is now full for socket %d\n", sock_index);
    }
    
    // Send ACK for the highest consecutive received packet
    int last_ack = (shared_mem[sock_index].rwnd.start - 1 + MAX_SEQ_NUM) % MAX_SEQ_NUM;
    send_ack_message(shared_mem[sock_index].sock_info.udp_sockid, last_ack, shared_mem[sock_index].rwnd.size, addr);
}

// Process a received ACK message
static void process_ack_message(int sock_index, char *buffer) {
    // Extract ACK sequence number and remote window size
    int ack_seq = extract_sequence(buffer);
    int remote_window = extract_window_size(buffer);
    
    printf("R: Received ACK seq=%d rwnd=%d for socket %d\n", ack_seq, remote_window, sock_index);
    
    // Check if this ACK acknowledges messages in our window
    int start_seq = shared_mem[sock_index].swnd.start;
    int distance = (ack_seq - start_seq + MAX_SEQ_NUM) % MAX_SEQ_NUM;

    // Only process if this ACK is for a packet we sent (within our current window)
    if (distance < shared_mem[sock_index].swnd.size) {
        // Slide window to acknowledge all packets up to this ACK
        int current_seq = start_seq;
        
        while (current_seq != (ack_seq + 1) % MAX_SEQ_NUM) {
            if (shared_mem[sock_index].swnd.slots[current_seq] >= 0) {
                // Free buffer slot
                shared_mem[sock_index].send_info.free_slots++;
                shared_mem[sock_index].swnd.slots[current_seq] = -1;
                printf("S: Freeing buffer slot for seq=%d, free_slots now=%d\n", 
                       current_seq, shared_mem[sock_index].send_info.free_slots);
            }
            
            // Clear send timestamp
            shared_mem[sock_index].send_info.timestamps[current_seq] = -1;
            
            // Move to next sequence number
            current_seq = (current_seq + 1) % MAX_SEQ_NUM;
        }
        
        // Update window start
        shared_mem[sock_index].swnd.start = (ack_seq + 1) % MAX_SEQ_NUM;
    }
    
    // Always update send window size based on receiver's capacity
    shared_mem[sock_index].swnd.size = remote_window;
    printf("S: Updated window for socket %d: start=%d size=%d\n", 
           sock_index, shared_mem[sock_index].swnd.start, shared_mem[sock_index].swnd.size);
}

// Retransmit packets in case of timeout
static void retransmit_packets(int sock_index) {
    // Setup destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(shared_mem[sock_index].sock_info.port);
    inet_pton(AF_INET, shared_mem[sock_index].sock_info.ip_addr, &(dest_addr.sin_addr));
    
    // Start from window base
    int seq_num = shared_mem[sock_index].swnd.start;
    
    printf("S: Retransmitting packets for socket %d starting at seq %d\n", sock_index, seq_num);
    
    // Iterate through the send window
    while (seq_num != (shared_mem[sock_index].swnd.start + shared_mem[sock_index].swnd.size) % MAX_SEQ_NUM) {
        if (shared_mem[sock_index].swnd.slots[seq_num] >= 0) {
            // Get buffer index for this sequence number
            int buffer_idx = shared_mem[sock_index].swnd.slots[seq_num];
            int data_len = shared_mem[sock_index].send_info.lengths[buffer_idx];
            
            // Prepare message with header
            char *packet_buffer = malloc(19 + data_len);
            if (!packet_buffer) {
                perror("Memory allocation failed");
                return;
            }
            
            // Set message type (DATA)
            packet_buffer[0] = DATA_MSG;
            
            // Add sequence number
            encode_sequence(packet_buffer + 1, seq_num);
            
            // Add data length
            for (int bit_idx = 0; bit_idx < 10; bit_idx++) {
                packet_buffer[18 - bit_idx] = ((data_len >> bit_idx) & 1) + '0';
            }
            
            // Copy data from send buffer
            memcpy(packet_buffer + 19, shared_mem[sock_index].send_info.buffer[buffer_idx], data_len);
            
            // Send the packet
            if (sendto(shared_mem[sock_index].sock_info.udp_sockid, packet_buffer, 19 + data_len, 0,
                     (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                perror("Failed to retransmit packet");
            } else {
                // Update timestamp
                shared_mem[sock_index].send_info.timestamps[seq_num] = time(NULL);
                printf("S: Retransmitted packet seq=%d for socket %d\n", seq_num, sock_index);
            }
            
            free(packet_buffer);
        }
        
        // Move to next sequence number
        seq_num = (seq_num + 1) % MAX_SEQ_NUM;
    }
}

// Send new packets that haven't been transmitted yet
static void transmit_new_packets(int sock_index) {
    // Setup destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(shared_mem[sock_index].sock_info.port);
    inet_pton(AF_INET, shared_mem[sock_index].sock_info.ip_addr, &(dest_addr.sin_addr));
    
    // Iterate through the sending window
    int seq_num = shared_mem[sock_index].swnd.start;
    while (seq_num != (shared_mem[sock_index].swnd.start + shared_mem[sock_index].swnd.size) % MAX_SEQ_NUM) {
        // Check if this sequence number has data but hasn't been sent yet
        if (shared_mem[sock_index].swnd.slots[seq_num] >= 0 && shared_mem[sock_index].send_info.timestamps[seq_num] == -1) {
            // Get buffer index for this sequence number
            int buffer_idx = shared_mem[sock_index].swnd.slots[seq_num];
            int data_len = shared_mem[sock_index].send_info.lengths[buffer_idx];
            
            // Prepare message with header
            char *packet_buffer = malloc(19 + data_len);
            if (!packet_buffer) {
                perror("Memory allocation failed");
                return;
            }
            
            // Set message type (DATA)
            packet_buffer[0] = DATA_MSG;
            
            // Add sequence number
            encode_sequence(packet_buffer + 1, seq_num);
            
            // Add data length
            for (int bit_idx = 0; bit_idx < 10; bit_idx++) {
                packet_buffer[18 - bit_idx] = ((data_len >> bit_idx) & 1) + '0';
            }
            
            // Copy data from send buffer
            memcpy(packet_buffer + 19, shared_mem[sock_index].send_info.buffer[buffer_idx], data_len);
            
            // Send the packet
            if (sendto(shared_mem[sock_index].sock_info.udp_sockid, packet_buffer, 19 + data_len, 0,
                     (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                perror("Failed to send new packet");
            } else {
                // Update timestamp
                shared_mem[sock_index].send_info.timestamps[seq_num] = time(NULL);
                printf("S: Sent new packet seq=%d for socket %d\n", seq_num, sock_index);
            }
            
            free(packet_buffer);
        }
        
        // Move to next sequence number
        seq_num = (seq_num + 1) % MAX_SEQ_NUM;
    }
}

// Receiver thread function (R)
void *R() {
    printf("Starting receiver thread\n");
    fd_set read_fds;
    int max_fd = 0;
    
    FD_ZERO(&read_fds);
    
    while(1) {
        // Prepare file descriptor set for select
        fd_set temp_fds = read_fds;
        struct timeval timeout;
        timeout.tv_sec = T / 2;  // Use half the timeout period for responsiveness
        timeout.tv_usec = 0;
        
        // Wait for incoming messages or timeout
        int select_result = select(max_fd + 1, &temp_fds, NULL, NULL, &timeout);
        
        if (select_result < 0) {
            if (errno != EINTR) {
                perror("select() error");
            }
            continue;
        }
        
        // Update socket set and send window updates if needed
        FD_ZERO(&read_fds);
        max_fd = 0;
        
        P(semid_shared_mem);
        for (int socket_idx = 0; socket_idx < N; socket_idx++) {
            if (!shared_mem[socket_idx].sock_info.free) {
                // Add socket to read set
                FD_SET(shared_mem[socket_idx].sock_info.udp_sockid, &read_fds);
                if (shared_mem[socket_idx].sock_info.udp_sockid > max_fd) {
                    max_fd = shared_mem[socket_idx].sock_info.udp_sockid;
                }
                
                // Check if we need to send a window update
                if (shared_mem[socket_idx].buffer_full == 1 && shared_mem[socket_idx].rwnd.size > 0) {
                    // Buffer was full but now has space
                    shared_mem[socket_idx].buffer_full = 0;
                    
                    // Send a window update (duplicate ACK)
                    struct sockaddr_in dest_addr;
                    memset(&dest_addr, 0, sizeof(dest_addr));
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(shared_mem[socket_idx].sock_info.port);
                    inet_pton(AF_INET, shared_mem[socket_idx].sock_info.ip_addr, &(dest_addr.sin_addr));
                    
                    // Last acknowledged sequence number
                    int last_ack = (shared_mem[socket_idx].rwnd.start - 1 + MAX_SEQ_NUM) % MAX_SEQ_NUM;
                    
                    // Send the window update
                    printf("R: Sending window update for socket %d: ACK=%d rwnd=%d\n", 
                           socket_idx, last_ack, shared_mem[socket_idx].rwnd.size);
                    send_ack_message(shared_mem[socket_idx].sock_info.udp_sockid, last_ack, 
                                   shared_mem[socket_idx].rwnd.size, &dest_addr);
                }
            }
        }
        
        // Process any incoming messages
        if (select_result > 0) {
            for (int socket_idx = 0; socket_idx < N; socket_idx++) {
                if (!shared_mem[socket_idx].sock_info.free && 
                    FD_ISSET(shared_mem[socket_idx].sock_info.udp_sockid, &temp_fds)) {
                    // Buffer for incoming message
                    char *message_buffer = malloc(MAX_MSG_SIZE + 20);
                    if (!message_buffer) {
                        perror("Memory allocation failed");
                        continue;
                    }
                    
                    // Receive message
                    struct sockaddr_in src_addr;
                    socklen_t addr_len = sizeof(src_addr);
                    int bytes_received = recvfrom(shared_mem[socket_idx].sock_info.udp_sockid, 
                                             message_buffer, MAX_MSG_SIZE + 20, 0,
                                             (struct sockaddr*)&src_addr, &addr_len);
                    
                    if (bytes_received <= 0) {
                        perror("recvfrom() error");
                        free(message_buffer);
                        continue;
                    }
                    
                    // Simulate message loss
                    if (dropMessage(DROP_PROB)) {
                        printf("R: Dropped message for socket %d\n", socket_idx);
                        free(message_buffer);
                        continue;
                    }
                    
                    // Process based on message type
                    if (bytes_received > 0) {
                        if (message_buffer[0] == DATA_MSG) {
                            process_data_message(socket_idx, message_buffer, bytes_received, &src_addr);
                        } else if (message_buffer[0] == ACK_MSG) {
                            process_ack_message(socket_idx, message_buffer);
                        } else {
                            printf("R: Received unknown message type: %c\n", message_buffer[0]);
                        }
                    }
                    
                    free(message_buffer);
                }
            }
        }
        
        V(semid_shared_mem);
    }
    
    return NULL;
}

// Sender thread function (S)
void *S() {
    printf("Starting sender thread\n");
    
    while(1) {
        // Periodically check for timeouts and send new messages
        sleep(T/2);
        
        P(semid_shared_mem);
        
        // Check each active socket
        for (int socket_idx = 0; socket_idx < N; socket_idx++) {
            if (!shared_mem[socket_idx].sock_info.free) {
                // Check for timeouts
                int timeout_detected = 0;
                time_t current_time = time(NULL);
                
                // Iterate through send window
                for (int win_idx = 0; win_idx < shared_mem[socket_idx].swnd.size; win_idx++) {
                    int seq_num = (shared_mem[socket_idx].swnd.start + win_idx) % MAX_SEQ_NUM;
                    
                    // Check if this packet was sent and timed out
                    if (shared_mem[socket_idx].send_info.timestamps[seq_num] > 0 && 
                        (current_time - shared_mem[socket_idx].send_info.timestamps[seq_num] >= T)) {
                        timeout_detected = 1;
                        break;
                    }
                }
                
                if (timeout_detected) {
                    // Retransmit all unacknowledged packets
                    printf("S: Timeout detected for socket %d\n", socket_idx);
                    retransmit_packets(socket_idx);
                } else {
                    // Send any new packets that are in the window but not yet sent
                    transmit_new_packets(socket_idx);
                }
            }
        }
        
        V(semid_shared_mem);
    }
    
    return NULL;
}

// Initialize IPC resources
static void initialize_ipc_resources() {
    // Generate unique keys for IPC resources
    key_t ipc_keys[6];
    ipc_keys[0] = ftok("/etc/hosts", 'A');  
    ipc_keys[1] = ftok("/etc/hosts", 'B');
    ipc_keys[2] = ftok("/etc/hosts", 'C');
    ipc_keys[3] = ftok("/etc/hosts", 'D');
    ipc_keys[4] = ftok("/etc/hosts", 'E');
    ipc_keys[5] = ftok("/etc/hosts", 'F');
    
    // Create shared memory segments
    shmid_net_socket = shmget(ipc_keys[0], sizeof(NET_SOCKET), 0666 | IPC_CREAT);
    shmid_shared_mem = shmget(ipc_keys[2], sizeof(SHARED_MEMORY) * N, 0666 | IPC_CREAT);
    
    if (shmid_net_socket < 0 || shmid_shared_mem < 0) {
        fprintf(stderr, "Failed to create shared memory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Create semaphores
    semid_net_socket = semget(ipc_keys[1], 1, 0666 | IPC_CREAT);
    semid_shared_mem = semget(ipc_keys[3], 1, 0666 | IPC_CREAT);
    semid_init = semget(ipc_keys[4], 1, 0666 | IPC_CREAT);
    semid_ktp = semget(ipc_keys[5], 1, 0666 | IPC_CREAT);
    
    if (semid_net_socket < 0 || semid_shared_mem < 0 || 
        semid_init < 0 || semid_ktp < 0) {
        fprintf(stderr, "Failed to create semaphores: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Initialize semaphores
    semctl(semid_net_socket, 0, SETVAL, 1);
    semctl(semid_shared_mem, 0, SETVAL, 1);
    semctl(semid_init, 0, SETVAL, 0);
    semctl(semid_ktp, 0, SETVAL, 0);
    
    // Attach to shared memory
    shared_mem = (SHARED_MEMORY *)shmat(shmid_shared_mem, NULL, 0);
    net_socket = (NET_SOCKET *)shmat(shmid_net_socket, NULL, 0);
    
    if (shared_mem == (void *)-1 || net_socket == (void *)-1) {
        fprintf(stderr, "Failed to attach shared memory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Initialize shared memory
    memset(net_socket, 0, sizeof(NET_SOCKET));
    
    for (int socket_idx = 0; socket_idx < N; socket_idx++) {
        shared_mem[socket_idx].sock_info.free = 1;
    }
    
    printf("IPC resources initialized successfully\n");
}

// Enhanced cleanup function to properly release all resources
static void cleanup_ipc_resources() {
    printf("\nCleaning up IPC resources...\n");
    
    // First, close all open UDP sockets
    if (shared_mem != NULL) {
        printf("Checking for open UDP sockets...\n");
        for (int socket_idx = 0; socket_idx < N; socket_idx++) {
            if (!shared_mem[socket_idx].sock_info.free && shared_mem[socket_idx].sock_info.udp_sockid > 0) {
                close(shared_mem[socket_idx].sock_info.udp_sockid);
                printf("Closed UDP socket %d\n", shared_mem[socket_idx].sock_info.udp_sockid);
            }
        }
    }
    
    // Detach from shared memory
    printf("Detaching from shared memory...\n");
    if (shared_mem != NULL) {
        if (shmdt(shared_mem) == -1) {
            perror("Failed to detach from shared memory (shared_mem)");
        }
        shared_mem = NULL;
    }
    
    if (net_socket != NULL) {
        if (shmdt(net_socket) == -1) {
            perror("Failed to detach from shared memory (net_socket)");
        }
        net_socket = NULL;
    }
    
    // Remove shared memory segments
    printf("Removing shared memory segments...\n");
    if (shmid_shared_mem >= 0) {
        if (shmctl(shmid_shared_mem, IPC_RMID, NULL) == -1) {
            perror("Failed to remove shared memory segment (shared_mem)");
        }
        shmid_shared_mem = -1;
    }
    
    if (shmid_net_socket >= 0) {
        if (shmctl(shmid_net_socket, IPC_RMID, NULL) == -1) {
            perror("Failed to remove shared memory segment (net_socket)");
        }
        shmid_net_socket = -1;
    }
    
    // Remove semaphores
    printf("Removing semaphores...\n");
    if (semid_shared_mem >= 0) {
        if (semctl(semid_shared_mem, 0, IPC_RMID) == -1) {
            perror("Failed to remove semaphore (shared_mem)");
        }
        semid_shared_mem = -1;
    }
    
    if (semid_net_socket >= 0) {
        if (semctl(semid_net_socket, 0, IPC_RMID) == -1) {
            perror("Failed to remove semaphore (net_socket)");
        }
        semid_net_socket = -1;
    }
    
    if (semid_init >= 0) {
        if (semctl(semid_init, 0, IPC_RMID) == -1) {
            perror("Failed to remove semaphore (init)");
        }
        semid_init = -1;
    }
    
    if (semid_ktp >= 0) {
        if (semctl(semid_ktp, 0, IPC_RMID) == -1) {
            perror("Failed to remove semaphore (ktp)");
        }
        semid_ktp = -1;
    }
    
    printf("IPC resources cleaned up successfully\n");
}

// Improved signal handler for more reliable cleanup
void sigHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d (Ctrl+C). Initiating graceful shutdown...\n", sig);
        
        // Set termination flag (for future thread support)
        terminate_flag = 1;
        
        // Cancel threads first to prevent them from using resources while we're cleaning up
        pthread_cancel(receiver_thread);
        pthread_cancel(sender_thread);
        pthread_cancel(gc_thread);
        
        // Clean up resources
        cleanup_ipc_resources();
        
        printf("Shutdown complete.\n");
        exit(0);
    }
}

// Main function to initialize KTP socket system
int main() {
    // Set up signal handler
    signal(SIGINT, sigHandler);
    
    // Seed random number generator for dropout simulation
    srand(time(NULL) ^ getpid());
    
    // Initialize semaphore operations
    sem_decrement.sem_num = 0;
    sem_decrement.sem_op = -1;
    sem_decrement.sem_flg = 0;
    
    sem_increment.sem_num = 0;
    sem_increment.sem_op = 1;
    sem_increment.sem_flg = 0;
    
    // Initialize IPC resources
    initialize_ipc_resources();
    
    // Create threads
    pthread_attr_t attr;
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    // Create receiver thread
    if (pthread_create(&receiver_thread, &attr, R, NULL) != 0) {
        perror("Failed to create receiver thread");
        cleanup_ipc_resources();
        exit(EXIT_FAILURE);
    }
    
    // Create sender thread
    if (pthread_create(&sender_thread, &attr, S, NULL) != 0) {
        perror("Failed to create sender thread");
        cleanup_ipc_resources();
        exit(EXIT_FAILURE);
    }
    
    // Create garbage collector thread
    if (pthread_create(&gc_thread, &attr, GC, NULL) != 0) {
        perror("Failed to create garbage collector thread");
        cleanup_ipc_resources();
        exit(EXIT_FAILURE);
    }
    
    pthread_attr_destroy(&attr);
    
    printf("KTP initialization complete. All threads started.\n");
    printf("Press Ctrl+C to terminate the program.\n");
    
    // Main thread handles socket creation and binding
    create_and_bind();
    
    // Should never reach here
    return 0;
}
