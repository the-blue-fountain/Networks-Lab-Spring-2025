Here is the **full written version** of the assignment, reproduced exactly as it appears in the PDF:

---

### **CS39006: Networks Laboratory**  
**Semester-Spring 2024-25**  

---

### **Assignment 3: TCP Sockets**  
**Deadline**: February 3, 2025, EOD  

---

#### **SUBMISSION INSTRUCTIONS**  
Prepare a detailed observation and analysis report along with appropriate screenshots from Wireshark and the Terminal for listed questions. Submit your report in **PDF format** with the filename `<NAME>_<ROLL NO>_Report.pdf`. The `.pcap` file should be uploaded to a public **Google Drive** folder, and the link needs to be included in the code. Zip all these files, including the source code of the two programs, into a single zip file `<NAME>_<ROLL NO>.zip` and submit it on **MS Teams**.  

**NOTES**:  
1. Mention your name (as per ERP), roll number, and assignment number within the comment line at the beginning of each program. A sample header should look like this:  
   ```
   =====================================
   Assignment 3 Submission
   Name: <Your_Name>
   Roll number: <Your_Roll_Number>
   Link of the pcap file: <Google_Drive_Link_of_the_pcap_File>
   =====================================
   ```  
2. Proper documentation and indentation are mandatory. Unreadable code will not be evaluated.  
3. Ensure that your code executes successfully on lab machines. Errors during compilation or runtime will result in mark deductions.  
4. Any form of plagiarism will incur severe penalties. You should type the code yourself, even if you consult online sources or your friends. We may ask you to explain your code, and failure to do so will result in a zero.  

---

#### **Objective**  
The objective of this assignment is to get familiar with **TCP sockets using POSIX C programming** and to analyze the behavior of the communication between client and server using **Wireshark**. The target is to establish communication between two processes (client and server) using a TCP socket and observe the network traffic using Wireshark.

---

#### **Problem Statement**  
Consider a simple **Substitution Cipher encryption scheme** in which each letter of a plaintext file is replaced by a fixed letter in the same set as described by the key. Here is the sample key:  

| **Plain Text Letter** | A  | B  | C  | D  | E  | F  | G  | H  | I  | J  | K  | L  | M  | N  | O  | P  | Q  | R  | S  | T  | U  | V  | W  | X  | Y  | Z  |
|------------------------|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| **Mapped Cipher Text** | D  | E  | F  | P  | R  | T  | V  | W  | L  | M  | Z  | A  | Y  | G  | H  | Q  | S  | I  | U  | J  | X  | K  | B  | C  | N  | O  |

The same mapping applies to **lowercase letters**.  

In this assignment, a **client** will send a plaintext file and a key (e.g., `DEFPRTVWLMZAYGHQSIUJXKBCNO`) to an **encryption server** using a TCP socket. The server will encrypt the file using the **Substitution Cipher scheme** and send it back to the client using the same TCP socket.  

The file is a text file of arbitrary size (> 0 bytes). Assume the file contains only **alphabets (uppercase and lowercase)**, spaces, and newline characters. Only letters are encrypted; spaces and newlines remain unchanged.  

Here is the content of a **sample file**:  
```
HELLO  
ABC  
CSE  
IIT Kharagpur  
```  

The encrypted version of the above file using the key `DEFPRTVWLMZAYGHQSIUJXKBCNO` is as follows:  
```
WRAAH  
DEF  
FUR  
LLJ Zwdidvqxi  
```  

The file transfer works using the following **communication protocol**:  
1. **Connection Establishment**: The client establishes a connection to the server.  
2. **Filename Input**: The client reads the filename from the user. If the file does not exist, it prints:  
   ```
   NOTFOUND <FILENAME>
   ```  
   The user is then prompted to enter another filename.  
3. **Key Input**: If the file exists, the user is prompted to enter the encryption key. If the key is less than 26 characters long, the client throws an error and prompts for the key again.  
4. **Send Key and File**: The client sends the key first, followed by the file contents, to the server in chunks (not in a single send call).  
5. **Server Handling**: The server:  
   - Saves the file as `<IP.port>.txt`, where `IP` and `port` are the client’s details. For example:  
     ```
     192.168.0.20.5000.txt
     ```  
   - Encrypts the file and saves it as `<IP.port>.txt.enc`.  
6. **Server Response**: The server sends the encrypted file back to the client in chunks.  
7. **Client Handling**: The client stores the encrypted file in the same directory as the original file with the name `<original_filename>.enc`. For example:  
   ```
   dummy.txt.enc
   ```  
8. **Success Message**: The client prints a message:  
   ```
   File encrypted. Original: <original_filename>, Encrypted: <encrypted_filename>
   ```  
9. **Repeat or Exit**: The client prompts the user to encrypt additional files. If the user enters "No", the client initiates a connection closure procedure. Otherwise, steps 2–9 are repeated.  

---

#### **Part-1: Socket Programming**  
Write two programs:  
1. **`doencfileserver.c`** (Server Program):  
   The server listens for multiple file encryption requests from the client, encrypts them, and sends them back.  
   **Marks: 20**  
2. **`retrieveencfileclient.c`** (Client Program):  
   The client sends plaintext files to the server and receives the encrypted versions.  
   **Marks: 15**  

**Constraints**:  
- No buffer larger than 100 bytes can be used in either the client or the server.  
- Neither the client nor the server can make more than one pass over the file or determine the file size beforehand.  

---

#### **Part-2: Wireshark Analysis**  
Use Wireshark to capture and analyze communication between the client and server. Perform the following tasks:  

1. Identify the **source and destination IP addresses and ports**. Include screenshots. **(Marks: 2)**  
2. Capture packets exchanged during the **three-way handshake**. Include screenshots. **(Marks: 4)**  
3. Capture packets exchanged during the **connection closure**. Include screenshots. **(Marks: 4)**  
4. Count the number of packets exchanged during file transfer and plot a graph:  
   ```
   File Size vs Number of Packets
   ```  
   **(Marks: 2)**  
5. Measure the total time for file transfer, encryption, and return. Plot a graph:  
   ```
   File Size vs Time
   ```  
   Include necessary screenshots. **(Marks: 3)**  
6. Calculate the average packet size during data communication. Use the plotted graph for reference. **(Marks: 2)**  

