/**
 * Assignment 7 Submission
 * Name: Aritra Maji
 * Roll number: 22CS300011
 * 
 * Custom Lightweight Discovery Protocol (CLDP) Server Implementation
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
#include <sys/sysinfo.h>
#include <net/if.h>
#include <sys/ioctl.h>
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
void send_hello_message(int sock, struct sockaddr_in *broadcast_addr, uint8_t server_id);
void process_query_message(int sock, struct sockaddr_in *client_addr, void *buffer, uint8_t server_id);
void handle_system_uptime_query(char *response);
void handle_memory_usage_query(char *response);
void handle_network_interface_query(char *response);

int main() {
    int sock, on = 1;
    struct sockaddr_in server_addr, client_addr, broadcast_addr;
    socklen_t client_len = sizeof(client_addr);
    fd_set readfds;
    struct timeval now, last, timeout;
    char buffer[BUFFER_SIZE];
    unsigned short counter = 1;
    uint8_t server_id;
    srand(time(NULL));
    server_id = (uint8_t)(rand() % 255 + 1);
    printf("CLDP Server starting with server_id: %d\n", server_id);
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
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(0);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) < 0) {
        perror("Failed to set socket to non-blocking mode");
        close(sock);
        exit(EXIT_FAILURE);
    }
    gettimeofday(&last, NULL);
    send_hello_message(sock, &broadcast_addr, server_id);
    printf("CLDP Server running...\n");
    while (1) {
        gettimeofday(&now, NULL);
        if ((now.tv_sec - last.tv_sec) >= 10) {
            memcpy(&last, &now, sizeof(struct timeval));
            send_hello_message(sock, &broadcast_addr, server_id);
        }
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        int activity = select(sock + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0 && errno != EINTR) {
            perror("select() error");
            continue;
        }
        if (FD_ISSET(sock, &readfds)) {
            int received_bytes = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                                          (struct sockaddr *)&client_addr, &client_len);
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
            printf("Received message - Type: 0x%02x, Transaction ID: %u, from client ID: %d\n", 
                   cldp_header->type, ntohl(cldp_header->transaction_id), cldp_header->sender_id);
            switch (cldp_header->type) {
                case HELLO:
                    break;
                case QUERY:
                    process_query_message(sock, &client_addr, buffer, server_id);
                    break;
                case RESPONSE:
                    break;
                default:
                    printf("Warning: Unknown message type: 0x%02x\n", cldp_header->type);
                    break;
            }
        }
    }
    close(sock);
    return 0;
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

void send_hello_message(int sock, struct sockaddr_in *broadcast_addr, uint8_t server_id) {
    static unsigned short counter = 1;
    char packet[sizeof(struct iphdr) + sizeof(struct cldp_header_t) + 20];
    memset(packet, 0, sizeof(packet));
    char hostname[20];
    gethostname(hostname, sizeof(hostname));
    int payload_len = strlen(hostname) + 1;
    struct iphdr *ip_header = (struct iphdr *)packet;
    ip_header->version = 4;
    ip_header->ihl = 5;
    ip_header->tos = 0;
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct cldp_header_t) + payload_len);
    ip_header->id = htons(counter++);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = PROTOCOL;
    ip_header->saddr = htonl(INADDR_ANY);
    ip_header->daddr = htonl(INADDR_BROADCAST);
    ip_header->check = 0;
    ip_header->check = checksum((unsigned short *)ip_header, sizeof(struct iphdr));
    struct cldp_header_t *cldp_header = (struct cldp_header_t *)(packet + sizeof(struct iphdr));
    cldp_header->type = HELLO;
    cldp_header->transaction_id = htonl(rand());
    cldp_header->payload_len = htonl(payload_len);
    cldp_header->reserved = 0;
    cldp_header->sender_id = server_id;
    cldp_header->receiver_id = 0;
    char *payload = packet + sizeof(struct iphdr) + sizeof(struct cldp_header_t);
    memcpy(payload, hostname, payload_len);
    if (sendto(sock, packet, sizeof(struct iphdr) + sizeof(struct cldp_header_t) + payload_len, 0,
               (struct sockaddr *)broadcast_addr, sizeof(*broadcast_addr)) < 0) {
        perror("sendto() failed for HELLO message");
    } else {
        printf("HELLO message sent (Transaction ID: %u)\n", ntohl(cldp_header->transaction_id));
    }
}

void process_query_message(int sock, struct sockaddr_in *client_addr, void *buffer, uint8_t server_id) {
    struct iphdr *ip_header = (struct iphdr *)buffer;
    int ip_header_len = ip_header->ihl * 4;
    struct cldp_header_t *cldp_header = (struct cldp_header_t *)((char *)buffer + ip_header_len);
    char *query_payload = (char *)buffer + ip_header_len + sizeof(struct cldp_header_t);
    uint32_t payload_len = ntohl(cldp_header->payload_len);
    uint32_t transaction_id = ntohl(cldp_header->transaction_id);
    uint8_t query_type = 0;
    if (strncmp(query_payload, "Query : ", 8) == 0 && payload_len >= 9) {
        query_type = query_payload[8] - '0';
    } else {
        printf("Warning: Malformed query payload\n");
        return;
    }
    char response_buffer[1024] = {0};
    switch (query_type) {
        case 1:
            handle_system_uptime_query(response_buffer);
            break;
        case 2:
            handle_memory_usage_query(response_buffer);
            break;
        case 3:
            handle_network_interface_query(response_buffer);
            break;
        default:
            sprintf(response_buffer, "Unknown query type: %d", query_type);
            break;
    }
    int response_payload_len = strlen(response_buffer) + 1;
    int packet_size = sizeof(struct iphdr) + sizeof(struct cldp_header_t) + response_payload_len;
    char *packet = malloc(packet_size);
    if (!packet) {
        perror("Memory allocation failed");
        return;
    }
    memset(packet, 0, packet_size);
    struct iphdr *resp_ip_header = (struct iphdr *)packet;
    resp_ip_header->version = 4;
    resp_ip_header->ihl = 5;
    resp_ip_header->tos = 0;
    resp_ip_header->tot_len = htons(packet_size);
    resp_ip_header->id = htons(rand());
    resp_ip_header->frag_off = 0;
    resp_ip_header->ttl = 64;
    resp_ip_header->protocol = PROTOCOL;
    resp_ip_header->saddr = htonl(INADDR_ANY);
    resp_ip_header->daddr = htonl(INADDR_BROADCAST);
    resp_ip_header->check = 0;
    resp_ip_header->check = checksum((unsigned short *)resp_ip_header, sizeof(struct iphdr));
    struct cldp_header_t *resp_cldp_header = (struct cldp_header_t *)(packet + sizeof(struct iphdr));
    resp_cldp_header->type = RESPONSE;
    resp_cldp_header->transaction_id = htonl(transaction_id);
    resp_cldp_header->payload_len = htonl(response_payload_len);
    resp_cldp_header->reserved = 0;
    resp_cldp_header->sender_id = server_id;
    resp_cldp_header->receiver_id = cldp_header->sender_id;
    char *response_payload = packet + sizeof(struct iphdr) + sizeof(struct cldp_header_t);
    memcpy(response_payload, response_buffer, response_payload_len);
    if (sendto(sock, packet, packet_size, 0,
               (struct sockaddr *)client_addr, sizeof(*client_addr)) < 0) {
        perror("sendto() failed for RESPONSE message");
    } else {
        printf("RESPONSE message sent (Transaction ID: %u) to client ID: %d\n", 
               transaction_id, resp_cldp_header->receiver_id);
    }
    free(packet);
}

void handle_system_uptime_query(char *response) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        sprintf(response, "Error retrieving system uptime");
        return;
    }
    long days = info.uptime / (60*60*24);
    long hours = (info.uptime / (60*60)) % 24;
    long minutes = (info.uptime / 60) % 60;
    long seconds = info.uptime % 60;
    sprintf(response, "System Uptime: %ld days, %ld hours, %ld minutes, %ld seconds", 
            days, hours, minutes, seconds);
}

void handle_memory_usage_query(char *response) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        sprintf(response, "Error retrieving memory information");
        return;
    }
    unsigned long total_ram = info.totalram / (1024 * 1024);
    unsigned long free_ram = info.freeram / (1024 * 1024);
    unsigned long used_ram = total_ram - free_ram;
    float usage_percent = (float)used_ram / total_ram * 100;
    sprintf(response, "Memory Usage: %.2f%% (Used: %lu MB, Free: %lu MB, Total: %lu MB)",
            usage_percent, used_ram, free_ram, total_ram);
}

void handle_network_interface_query(char *response) {
    char hostname[256];
    struct timeval tv;
    
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        sprintf(response, "Error retrieving hostname: %s", strerror(errno));
        return;
    }
    
    if (gettimeofday(&tv, NULL) != 0) {
        sprintf(response, "Error retrieving system time: %s", strerror(errno));
        return;
    }
    
    // Format time
    time_t now = tv.tv_sec;
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Construct response using hostname and time data
    sprintf(response, 
            "System and Hostname Information:\n"
            "- Hostname: %s\n"
            "- Current Time: %s\n"
            "- Microseconds: %ld\n"
            "- Timezone: %s",
            hostname, time_str, tv.tv_usec,
            tm_info->tm_zone ? tm_info->tm_zone : "Unknown");
}
