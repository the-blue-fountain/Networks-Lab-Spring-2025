/*===========================================
 Assignment 4: Emulating End-to-End Reliable Flow Control
 Name: Aritra Maji
 Roll number: 22CS30011
============================================*/

#include "ksocket.h"

// Global variable definitions - visible to all files including ksocket.h
SHARED_MEMORY *shared_mem = NULL;
NET_SOCKET *net_socket = NULL;
int semid_shared_mem = -1, semid_net_socket = -1;
int shmid_shared_mem = -1, shmid_net_socket = -1;
int semid_init = -1, semid_ktp = -1;
struct sembuf sem_decrement, sem_increment;

// Local helper function prototypes
static int find_free_socket_slot(void);
static int find_process_socket(void);
static int find_free_buffer_slot(int sockfd);
static int check_destination_match(int sockfd, const char* dest_ip, uint16_t dest_port);

// Connect to shared memory segments and semaphores
void retrieve_SHARED_MEMORY() {
    // Generate unique keys for IPC objects using different paths for uniqueness
    key_t ipc_keys[6];
    ipc_keys[0] = ftok("/etc/hosts", 'A');  
    ipc_keys[1] = ftok("/etc/hosts", 'B');
    ipc_keys[2] = ftok("/etc/hosts", 'C');
    ipc_keys[3] = ftok("/etc/hosts", 'D');
    ipc_keys[4] = ftok("/etc/hosts", 'E');
    ipc_keys[5] = ftok("/etc/hosts", 'F');
    
    // Get existing IPC identifiers
    shmid_net_socket = shmget(ipc_keys[0], sizeof(NET_SOCKET), 0666);
    semid_net_socket = semget(ipc_keys[1], 1, 0666);
    shmid_shared_mem = shmget(ipc_keys[2], sizeof(SHARED_MEMORY) * N, 0666);
    semid_shared_mem = semget(ipc_keys[3], 1, 0666);
    semid_init = semget(ipc_keys[4], 1, 0666);
    semid_ktp = semget(ipc_keys[5], 1, 0666);
    
    // If any resources not available, print helpful error and exit
    if (shmid_net_socket < 0 || semid_net_socket < 0 || 
        shmid_shared_mem < 0 || semid_shared_mem < 0 || 
        semid_init < 0 || semid_ktp < 0) {
        fprintf(stderr, "KTP initialization service not running. Please start initksocket first.\n");
        exit(EXIT_FAILURE);
    }
    
    // Attach to shared memory segments
    shared_mem = (SHARED_MEMORY *)shmat(shmid_shared_mem, NULL, 0);
    net_socket = (NET_SOCKET *)shmat(shmid_net_socket, NULL, 0);
    
    if (shared_mem == (void*)-1 || net_socket == (void*)-1) {
        perror("Failed to attach to shared memory");
        exit(EXIT_FAILURE);
    }
}

// Setup semaphore operation structures
void init_sembuf() {
    // Initialize "P" operation (decrement)
    sem_decrement.sem_num = 0;
    sem_decrement.sem_op = -1;
    sem_decrement.sem_flg = 0;
    
    // Initialize "V" operation (increment)
    sem_increment.sem_num = 0;
    sem_increment.sem_op = 1;
    sem_increment.sem_flg = 0;
}

// Find a free socket slot in the shared memory
static int find_free_socket_slot(void) {
    for (int slot_idx = 0; slot_idx < N; slot_idx++) {
        if (shared_mem[slot_idx].sock_info.free == 1) {
            return slot_idx;  // Found a free slot
        }
    }
    return -1;  // No free slots available
}

// Find the socket associated with the current process
static int find_process_socket(void) {
    pid_t current_pid = getpid();
    
    for (int slot_idx = 0; slot_idx < N; slot_idx++) {
        if (!shared_mem[slot_idx].sock_info.free && shared_mem[slot_idx].sock_info.pid == current_pid) {
            return slot_idx;  // Found the socket for this process
        }
    }
    return -1;  // No socket found for this process
}

// Initialize the sending and receiving windows for a new socket
static void initialize_windows(int socket_idx) {
    // Initialize each element of the windows
    for (int seq_idx = 0; seq_idx < MAX_SEQ_NUM; seq_idx++) {
        // Initialize sending window slots to empty
        shared_mem[socket_idx].swnd.slots[seq_idx] = -1;
        shared_mem[socket_idx].send_info.timestamps[seq_idx] = -1;
        
        // Initialize receiving window
        if (seq_idx < BUFFER_SIZE) {
            // Map sequence numbers to buffer slots
            shared_mem[socket_idx].rwnd.slots[seq_idx] = seq_idx;
        } else {
            // Mark other slots as unused
            shared_mem[socket_idx].rwnd.slots[seq_idx] = -1;
        }
    }
    
    // Set initial window parameters
    shared_mem[socket_idx].swnd.size = BUFFER_SIZE;  // Start with full sending capacity
    shared_mem[socket_idx].rwnd.size = BUFFER_SIZE;  // Start with full receiving capacity
    shared_mem[socket_idx].swnd.start = 0;           // Start at sequence 0
    shared_mem[socket_idx].rwnd.start = 0;           // Expect sequence 0 first
    
    // Initialize buffer management
    shared_mem[socket_idx].send_info.free_slots = BUFFER_SIZE;  // All send slots available
    shared_mem[socket_idx].recv_info.base_idx = 0;            // Start receiving at slot 0
    shared_mem[socket_idx].buffer_full = 0;                  // Buffer has space initially
    
    // Mark all receive buffer slots as empty
    for (int buf_idx = 0; buf_idx < BUFFER_SIZE; buf_idx++) {
        shared_mem[socket_idx].recv_info.active[buf_idx] = 0;
    }
}

// Find an available buffer slot for sending data
static int find_free_buffer_slot(int sockfd) {
    for (int buf_idx = 0; buf_idx < BUFFER_SIZE; buf_idx++) {
        int slot_occupied = 0;
        // Check if this buffer slot is already assigned to any sequence number
        for (int seq_idx = 0; seq_idx < MAX_SEQ_NUM; seq_idx++) {
            if (shared_mem[sockfd].swnd.slots[seq_idx] == buf_idx) {
                slot_occupied = 1;
                break;
            }
        }
        // Return the first unoccupied slot
        if (!slot_occupied) {
            return buf_idx;
        }
    }
    return -1;  // No free slots available
}

// Check if destination matches the bound address
static int check_destination_match(int sockfd, const char* dest_ip, uint16_t dest_port) {
    return (strcmp(shared_mem[sockfd].sock_info.ip_addr, dest_ip) == 0 && 
            shared_mem[sockfd].sock_info.port == dest_port);
}

// Create a new KTP socket
int k_socket(int domain, int type, int protocol) {
    // Connect to IPC resources
    retrieve_SHARED_MEMORY();
    init_sembuf();
    
    // Validate socket parameters
    if (domain != AF_INET || type != SOCK_KTP) {
        errno = EINVAL;
        return -1;
    }
    
    // Atomic operation to find and allocate a free socket slot
    P(semid_shared_mem);
    int socket_idx = find_free_socket_slot();
    // If we found a slot, mark it as used
    if (socket_idx >= 0) {
        shared_mem[socket_idx].sock_info.free = 0;
        shared_mem[socket_idx].sock_info.pid = getpid();
    }
    V(semid_shared_mem);
    
    // Handle no available slots
    if (socket_idx < 0) {
        errno = ENOSPACE;
        return -1;
    }
    
    // Request UDP socket creation from initksocket
    P(semid_net_socket);
    memset(net_socket, 0, sizeof(NET_SOCKET));  // Clear any previous data
    V(semid_net_socket);
    
    // Signal init process and wait for response
    V(semid_init);
    P(semid_ktp);
    
    // Check UDP socket creation status
    P(semid_net_socket);
    if (net_socket->sock_id < 0) {
        // Socket creation failed, mark KTP socket as free again
        errno = net_socket->err_code;
        P(semid_shared_mem);
        shared_mem[socket_idx].sock_info.free = 1;
        V(semid_shared_mem);
        V(semid_net_socket);
        return -1;
    }
    
    // Associate UDP socket with KTP socket
    shared_mem[socket_idx].sock_info.udp_sockid = net_socket->sock_id;
    memset(net_socket, 0, sizeof(NET_SOCKET));
    V(semid_net_socket);
    
    // Initialize windows and buffers
    P(semid_shared_mem);
    initialize_windows(socket_idx);
    V(semid_shared_mem);
    
    return socket_idx;
}

// Bind a KTP socket to source and destination addresses
int k_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port) {
    retrieve_SHARED_MEMORY();
    init_sembuf();
    
    // Find the socket for this process
    P(semid_shared_mem);
    int socket_idx = find_process_socket();
    if (socket_idx < 0) {
        V(semid_shared_mem);
        errno = EINVAL;
        return -1;
    }
    V(semid_shared_mem);
    
    // Set up bind request for initksocket
    P(semid_net_socket);
    net_socket->sock_id = shared_mem[socket_idx].sock_info.udp_sockid;
    strncpy(net_socket->ip_addr, src_ip, INET_ADDRSTRLEN);
    net_socket->ip_addr[INET_ADDRSTRLEN-1] = '\0';
    net_socket->port = src_port;
    V(semid_net_socket);
    
    // Signal init process and wait for response
    V(semid_init);
    P(semid_ktp);
    
    // Check bind status
    P(semid_net_socket);
    if (net_socket->sock_id < 0) {
        // Bind failed
        errno = net_socket->err_code;
        memset(net_socket, 0, sizeof(NET_SOCKET));
        V(semid_net_socket);
        return -1;
    }
    memset(net_socket, 0, sizeof(NET_SOCKET));
    V(semid_net_socket);
    
    // Store destination address for future checks
    P(semid_shared_mem);
    strncpy(shared_mem[socket_idx].sock_info.ip_addr, dest_ip, INET_ADDRSTRLEN);
    shared_mem[socket_idx].sock_info.ip_addr[INET_ADDRSTRLEN-1] = '\0';
    shared_mem[socket_idx].sock_info.port = dest_port;
    V(semid_shared_mem);
    
    return 0;
}

// Send data through a KTP socket
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, 
                const struct sockaddr *dest_addr, socklen_t addrlen) {
    retrieve_SHARED_MEMORY();
    init_sembuf();
    
    // Basic validation
    if (sockfd < 0 || sockfd >= N) {
        errno = EINVAL;
        return -1;
    }
    
    P(semid_shared_mem);
    // Check if socket is allocated
    if (shared_mem[sockfd].sock_info.free) {
        V(semid_shared_mem);
        errno = EINVAL;
        return -1;
    }
    V(semid_shared_mem);
    
    // Extract destination address
    char dest_ip[INET_ADDRSTRLEN];
    const struct sockaddr_in *addr_in = (const struct sockaddr_in *)dest_addr;
    if (inet_ntop(AF_INET, &(addr_in->sin_addr), dest_ip, INET_ADDRSTRLEN) == NULL) {
        errno = EINVAL;
        return -1;
    }
    uint16_t dest_port = ntohs(addr_in->sin_port);
    
    P(semid_shared_mem);
    
    // Verify destination matches bound address
    if (!check_destination_match(sockfd, dest_ip, dest_port)) {
        V(semid_shared_mem);
        errno = ENOTBOUND;
        return -1;
    }
    
    // Check for buffer space
    if (shared_mem[sockfd].send_info.free_slots <= 0) {
        V(semid_shared_mem);
        errno = ENOSPACE;
        return -1;
    }
    
    // Find next available sequence number
    int seq_num = shared_mem[sockfd].swnd.start;
    int seq_checked_count = 0;
    while (shared_mem[sockfd].swnd.slots[seq_num] != -1) {
        seq_num = (seq_num + 1) % MAX_SEQ_NUM;
        seq_checked_count++;
        
        // Prevent infinite loop - checked all possible slots
        if (seq_checked_count >= MAX_SEQ_NUM) {
            V(semid_shared_mem);
            errno = ENOSPACE;
            return -1;
        }
    }
    
    // Find available send buffer slot
    int buffer_idx = find_free_buffer_slot(sockfd);
    if (buffer_idx < 0) {
        V(semid_shared_mem);
        errno = ENOSPACE;
        return -1;
    }
    
    // Store data and metadata
    shared_mem[sockfd].swnd.slots[seq_num] = buffer_idx;
    memcpy(shared_mem[sockfd].send_info.buffer[buffer_idx], buf, len);
    shared_mem[sockfd].send_info.lengths[buffer_idx] = len;
    shared_mem[sockfd].send_info.timestamps[seq_num] = -1;  // Not sent yet
    shared_mem[sockfd].send_info.free_slots--;
    
    V(semid_shared_mem);
    return len;
}

// Receive data from a KTP socket
ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, 
                  struct sockaddr *src_addr, socklen_t *addrlen) {
    retrieve_SHARED_MEMORY();
    init_sembuf();
    
    P(semid_shared_mem);
    
    // Validate socket
    if (sockfd < 0 || sockfd >= N || shared_mem[sockfd].sock_info.free) {
        V(semid_shared_mem);
        errno = EINVAL;
        return -1;
    }
    
    // Check if any data is available in the receive buffer
    int base_idx = shared_mem[sockfd].recv_info.base_idx;
    if (shared_mem[sockfd].recv_info.active[base_idx]) {
        // Get message length and copy appropriate amount of data
        int data_len = shared_mem[sockfd].recv_info.lengths[base_idx];
        int copy_len = (data_len < len) ? data_len : len;
        
        memcpy(buf, shared_mem[sockfd].recv_info.buffer[base_idx], copy_len);
        shared_mem[sockfd].recv_info.active[base_idx] = 0;  // Mark slot as free
        
        // Update window management
        int found_seq = -1;
        
        // Find which sequence number maps to this buffer slot
        for (int seq_idx = 0; seq_idx < MAX_SEQ_NUM; seq_idx++) {
            if (shared_mem[sockfd].rwnd.slots[seq_idx] == base_idx) {
                found_seq = seq_idx;
                break;
            }
        }
        
        if (found_seq >= 0) {
            // Mark current sequence slot as unused
            shared_mem[sockfd].rwnd.slots[found_seq] = -1;
            
            // Allocate a future sequence number to this buffer slot
            int new_seq = (found_seq + BUFFER_SIZE) % MAX_SEQ_NUM;
            shared_mem[sockfd].rwnd.slots[new_seq] = base_idx;
            
            // Advance base pointer to next slot
            shared_mem[sockfd].recv_info.base_idx = (base_idx + 1) % BUFFER_SIZE;
            
            // Update receiver window size
            if (shared_mem[sockfd].rwnd.size < BUFFER_SIZE) {
                shared_mem[sockfd].rwnd.size++;
                
                // If we transitioned from full to having space, set flag for window update
                if (shared_mem[sockfd].rwnd.size == 1) {
                    shared_mem[sockfd].buffer_full = 1;
                }
            }
        }
        
        // Set source address if requested
        if (src_addr && addrlen) {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)src_addr;
            addr_in->sin_family = AF_INET;
            addr_in->sin_port = htons(shared_mem[sockfd].sock_info.port);
            if (inet_pton(AF_INET, shared_mem[sockfd].sock_info.ip_addr, &(addr_in->sin_addr)) <= 0) {
                // Should never happen but just in case
                memset(&(addr_in->sin_addr), 0, sizeof(addr_in->sin_addr));
            }
            *addrlen = sizeof(struct sockaddr_in);
        }
        
        V(semid_shared_mem);
        return copy_len;
    }
    
    // No data available
    V(semid_shared_mem);
    errno = ENOMESSAGE;
    return -1;
}

// Close a KTP socket
int k_close(int sockfd) {
    retrieve_SHARED_MEMORY();
    init_sembuf();
    
    P(semid_shared_mem);
    
    // Validate socket
    if (sockfd < 0 || sockfd >= N || shared_mem[sockfd].sock_info.free) {
        V(semid_shared_mem);
        errno = EINVAL;
        return -1;
    }
    
    // Close socket and mark as free
    shared_mem[sockfd].sock_info.free = 1;
    
    V(semid_shared_mem);
    return 0;
}

// Simulate packet loss
int dropMessage(float prob) {
    // Generate random number between 0 and 1
    double rand_val = (double)rand() / (double)RAND_MAX;
    
    // Return 1 (drop) if random value is below threshold
    return (rand_val < prob) ? 1 : 0;
}
