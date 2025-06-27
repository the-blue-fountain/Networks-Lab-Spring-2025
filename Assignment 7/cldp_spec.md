# Custom Lightweight Discovery Protocol (CLDP) Specification

## 1. Protocol Overview
CLDP is a lightweight protocol designed for node discovery and system metadata querying in local networks. It operates directly over IP (protocol number 253) using raw sockets.

## 2. Packet Format

### 2.1 IP Header
Uses standard IPv4 header with:
- Protocol Number: 253
- Header Length: 20 bytes (no options)
- TTL: 64 (suitable for local network)
- Checksum: Calculated as per IP standard

### 2.2 CLDP Header
Total Size: 12 bytes
Fields:
- Type (8 bits): Message type    
- Transaction ID (32 bits): Unique identifier for message pairs    
- Payload Length (32 bits): Length of payload data    
- Reserved (8 bits): Reserved for future use    
- Sender ID (8 bits): Unique identifier of sending node   
- Receiver ID (8 bits): Identifier of intended recipient (0 for broadcast)    


## 3. Message Types

### 3.1 HELLO (0x01)
- Purpose: Node announces its presence   
- Direction: Server → All   
- Payload: Hostname string   
- Frequency: Every 10 seconds    
- Receiver ID: NULL (broadcast)    

### 3.2 QUERY (0x02)
- Purpose: Request system metadata     
- Direction: Client → All Servers     
- Payload Format: "Query : N" where N is query type      
- Receiver ID: NULL (broadcast)     
- Query Types:     
  1. System uptime
  2. Memory usage    
  3. Network interface status   
  
### 3.3 RESPONSE (0x03)
- Purpose: Reply to query with requested data      
- Direction: Server → Specific Client     
- Payload: Formatted query result string     
- Receiver ID: Original querying client's ID     
- Transaction ID: Matches original query     

## 4. Operation Flow

### 4.1 Server Operation
1. Generate random server ID (1-255)     
2. Send HELLO messages every 10 seconds     
3. Process incoming QUERY messages     
4. Send RESPONSE messages to specific clients     

### 4.2 Client Operation
1. Generate random client ID (1-255)    
2. Listen for HELLO messages      
3. Send QUERY messages on user command      
4. Process RESPONSE messages from servers     

## 5. Query Types and Responses

### 5.1 System Uptime (Query Type 1)
Response format: "System Uptime: X days, Y hours, Z minutes, W seconds"

### 5.2 Memory Usage (Query Type 2)
Response format: "Memory Usage: P% (Used: X MB, Free: Y MB, Total: Z MB)"

### 5.3 Network Interface Status (Query Type 3)
Response format:
```
Network Interface Status:
eth0: [UP/DOWN] [, RUNNING]
wlan0: [UP/DOWN] [, RUNNING]
lo: [UP/DOWN] [, RUNNING]
```

## 6. Error Handling
- Malformed packets are silently dropped    
- Invalid checksums trigger warning messages    
- Unrecognized query types return error message
- Socket errors are logged with error description     
- Messages with wrong receiver ID are ignored     
