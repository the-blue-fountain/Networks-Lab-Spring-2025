# Lightweight Custom Discovery Protocol (CLDP) Implementation

A custom protocol implementation for node discovery and system metadata querying in local networks, operating directly over IP using raw sockets.

## Build Instructions

### Prerequisites
- Linux-based operating system
- GCC compiler
- Root privileges (for raw sockets)

### Compilation
```bash
gcc -o cldp_server server.c
gcc -o cldp_client client.c
```

## Running the Applications

### Server
```bash
sudo ./cldp_server
```
The server will:
- Generate a random server ID
- Broadcast HELLO messages every 10 seconds
- Listen for and respond to queries from clients

### Client
```bash
sudo ./cldp_client
```
The client supports the following commands:
- `q1`: Query system uptime
- `q2`: Query memory usage
- `q3`: Query network interface status
- `help`: Show available commands
- `exit`: Exit the program

## Implementation Details

### Network Protocol
- Uses raw sockets with custom protocol number 253
- Operates directly over IP without transport layer
- Implements custom checksum calculation
- Supports broadcast messaging

### Security Considerations
- Requires root privileges due to raw socket usage
- Validates packet integrity through IP checksum
- Implements basic message filtering
- Validates sender and receiver IDs

### Limitations
- Works only on IPv4 networks
- Requires root/sudo privileges
- Limited to local network segment (TTL=64)
- Maximum hostname length: 20 bytes
- Maximum response size: 1024 bytes

### Error Handling
- Validates all incoming packets
- Checks message integrity
- Reports socket and system call errors
- Handles malformed messages gracefully
- Logs protocol-specific warnings

