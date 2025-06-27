/**
 * Assignment 7 Submission
 * Name: Aritra Maji
 * Roll number: 22CS300011
 * 
 * Custom Lightweight Discovery Protocol (CLDP) Client Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define PROTOCOL 253
#define HELLO 0x01
#define QUERY 0x02
#define RESPONSE 0x03
#define BUFFER_SIZE 4096

#pragma pack(push, 1)
struct cldp_header_t {
    uint8_t type;
    uint32_t transaction_id;
    uint32_t payload_len;
    uint8_t reserved;
    uint8_t sender_id;
    uint8_t receiver_id;
};
#pragma pack(pop)

unsigned short checksum(unsigned short *buf, int bufsz);
void send_query_message(int sock, struct sockaddr_in *broadcast_addr, uint8_t client_id, int query_type);
void process_hello_message(void *buffer);
void process_response_message(void *buffer);
void print_usage();

int main() {
    int sock, on = 1;
    struct sockaddr_in client_addr, broadcast_addr, server_addr;
    socklen_t server_len = sizeof(server_addr);
    fd_set readfds;
    struct timeval timeout;
    char buffer[BUFFER_SIZE];
    unsigned short counter = 1;
    uint8_t client_id;
    srand(time(NULL));
    client_id = (uint8_t)(rand() % 255 + 1);
    printf("CLDP Client starting with client_id: %d\n", client_id);
    printf("Available commands:\n");
    print_usage();
    sock = socket(AF_INET, SOCK_RAW, PROTOCOL);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        perror("setsockopt IP_HDRINCL failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
        perror("setsockopt SO_BROADCAST failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(0);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    if (bind(sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
        perror("Failed to set socket to non-blocking mode");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK) < 0) {
        perror("Failed to set stdin to non-blocking mode");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("CLDP Client running...\n");
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;
        int activity = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (activity > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            char cmd[20];
            if (fgets(cmd, sizeof(cmd), stdin) != NULL) {
                cmd[strcspn(cmd, "\n")] = 0;
                if (strcmp(cmd, "q1") == 0) {
                    printf("Sending system uptime query...\n");
                    send_query_message(sock, &broadcast_addr, client_id, 1);
                }
                else if (strcmp(cmd, "q2") == 0) {
                    printf("Sending memory usage query...\n");
                    send_query_message(sock, &broadcast_addr, client_id, 2);
                }
                else if (strcmp(cmd, "q3") == 0) {
                    printf("Sending system and hostname info query...\n");
                    send_query_message(sock, &broadcast_addr, client_id, 3);
                }
                else if (strcmp(cmd, "help") == 0) {
                    print_usage();
                }
                else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
                    printf("Exiting...\n");
                    break;
                }
                else {
                    printf("Unknown command. Type 'help' to see available commands.\n");
                }
            }
        }
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;
        activity = select(sock + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0 && errno != EINTR) {
            perror("select() error");
            continue;
        }
        if (FD_ISSET(sock, &readfds)) {
            int received_bytes = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                                         (struct sockaddr *)&server_addr, &server_len);
            if (received_bytes < 0) {
                perror("recvfrom() error");
                continue;
            }
            struct iphdr *ip_header = (struct iphdr *)buffer;
            int ip_header_len = ip_header->ihl * 4;
            if (ip_header->protocol != PROTOCOL || 
                received_bytes < (ip_header_len + sizeof(struct cldp_header_t))) {
                continue;
            }
            unsigned short original_checksum = ip_header->check;
            ip_header->check = 0;
            if (original_checksum != checksum((unsigned short *)ip_header, ip_header->ihl * 4)) {
                printf("Warning: Invalid IP checksum, ignoring packet\n");
                continue;
            }
            ip_header->check = original_checksum;
            struct cldp_header_t *cldp_header = (struct cldp_header_t *)(buffer + ip_header_len);
            if (cldp_header->receiver_id != 0 && cldp_header->receiver_id != client_id) {
                continue;
            }
            switch (cldp_header->type) {
                case HELLO:
                    process_hello_message(buffer);
                    break;
                case QUERY:
                    break;
                case RESPONSE:
                    process_response_message(buffer);
                    break;
                default:
                    printf("Warning: Unknown message type: 0x%02x\n", cldp_header->type);
                    break;
            }
        }
        usleep(1000);
    }
    close(sock);
    return 0;
}

void print_usage() {
    printf("  q1     - Send query for system uptime\n");
    printf("  q2     - Send query for memory usage\n");
    printf("  q3     - Send query for system and hostname info\n");
    printf("  help   - Show this help message\n");
    printf("  exit   - Exit the program\n");
}

unsigned short checksum(unsigned short *buf, int bufsz) {
    unsigned long sum = 0;
    while (bufsz > 1) {
        sum += *buf++;
        bufsz -= 2;
    }
    if (bufsz == 1) {
        sum += *(unsigned char *)buf;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

void send_query_message(int sock, struct sockaddr_in *broadcast_addr, uint8_t client_id, int query_type) {
    static unsigned short counter = 1;
    char query_text[20];
    sprintf(query_text, "Query : %d", query_type);
    int payload_len = strlen(query_text) + 1;
    int packet_size = sizeof(struct iphdr) + sizeof(struct cldp_header_t) + payload_len;
    char *packet = (char *)malloc(packet_size);
    if (!packet) {
        perror("Memory allocation failed");
        return;
    }
    memset(packet, 0, packet_size);
    struct iphdr *ip_header = (struct iphdr *)packet;
    ip_header->version = 4;
    ip_header->ihl = 5;
    ip_header->tos = 0;
    ip_header->tot_len = htons(packet_size);
    ip_header->id = htons(counter++);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = PROTOCOL;
    ip_header->saddr = htonl(INADDR_ANY);
    ip_header->daddr = htonl(INADDR_BROADCAST);
    ip_header->check = 0;
    ip_header->check = checksum((unsigned short *)ip_header, sizeof(struct iphdr));
    struct cldp_header_t *cldp_header = (struct cldp_header_t *)(packet + sizeof(struct iphdr));
    cldp_header->type = QUERY;
    uint32_t transaction_id = rand();
    cldp_header->transaction_id = htonl(transaction_id);
    cldp_header->payload_len = htonl(payload_len);
    cldp_header->reserved = 0;
    cldp_header->sender_id = client_id;
    cldp_header->receiver_id = 0;
    char *payload = packet + sizeof(struct iphdr) + sizeof(struct cldp_header_t);
    memcpy(payload, query_text, payload_len);
    if (sendto(sock, packet, packet_size, 0,
               (struct sockaddr *)broadcast_addr, sizeof(*broadcast_addr)) < 0) {
        perror("sendto() failed for QUERY message");
    } else {
        printf("QUERY message sent (Transaction ID: %u)\n", transaction_id);
    }
    free(packet);
}

void process_hello_message(void *buffer) {
    struct iphdr *ip_header = (struct iphdr *)buffer;
    int ip_header_len = ip_header->ihl * 4;
    struct cldp_header_t *cldp_header = (struct cldp_header_t *)((char *)buffer + ip_header_len);
    char *hostname = (char *)buffer + ip_header_len + sizeof(struct cldp_header_t);
    uint32_t transaction_id = ntohl(cldp_header->transaction_id);
    struct in_addr src_addr;
    src_addr.s_addr = ip_header->saddr;
    printf("Server HELLO received - Server ID: %d, Hostname: %s, IP: %s (Transaction ID: %u)\n",
           cldp_header->sender_id, hostname, inet_ntoa(src_addr), transaction_id);
}

void process_response_message(void *buffer) {
    struct iphdr *ip_header = (struct iphdr *)buffer;
    int ip_header_len = ip_header->ihl * 4;
    struct cldp_header_t *cldp_header = (struct cldp_header_t *)((char *)buffer + ip_header_len);
    char *response_data = (char *)buffer + ip_header_len + sizeof(struct cldp_header_t);
    uint32_t transaction_id = ntohl(cldp_header->transaction_id);
    struct in_addr src_addr;
    src_addr.s_addr = ip_header->saddr;
    printf("\n===== RESPONSE from Server ID: %d (Transaction ID: %u) =====\n", 
           cldp_header->sender_id, transaction_id);
    printf("IP: %s\n", inet_ntoa(src_addr));
    printf("%s\n", response_data);
    printf("=====================================================\n\n");
}
