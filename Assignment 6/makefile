# Assignment 6 Submission
# Name: Aritra Maji
# Roll number: 22CS30011

CC = gcc
CFLAGS = -Wall -Wextra
LIBS = 

all: mysmtp_server mysmtp_client

mysmtp_server: mysmtp_server.c
	$(CC) $(CFLAGS) -o server mysmtp_server.c $(LIBS)

mysmtp_client: mysmtp_client.c
	$(CC) $(CFLAGS) -o client mysmtp_client.c $(LIBS)

clean:
	rm -f server client
