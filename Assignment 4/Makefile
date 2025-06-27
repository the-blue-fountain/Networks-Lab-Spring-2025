CC = gcc
CFLAGS = -Wall -g
LIBRARY = libksocket.a

all: $(LIBRARY) initksocket user1 user2

# Create the static library
$(LIBRARY): ksocket.o
	ar rcs $(LIBRARY) ksocket.o

# Compile the KTP socket library
ksocket.o: ksocket.c ksocket.h
	$(CC) $(CFLAGS) -c ksocket.c

# Compile and link the initialization process
initksocket: initksocket.o $(LIBRARY)
	$(CC) $(CFLAGS) -o initksocket initksocket.o -L. -lksocket -pthread

initksocket.o: initksocket.c ksocket.h
	$(CC) $(CFLAGS) -c initksocket.c

# Compile and link the sender application
user1: user1.o $(LIBRARY)
	$(CC) $(CFLAGS) -o user1 user1.o -L. -lksocket

user1.o: user1.c ksocket.h
	$(CC) $(CFLAGS) -c user1.c

# Compile and link the receiver application
user2: user2.o $(LIBRARY)
	$(CC) $(CFLAGS) -o user2 user2.o -L. -lksocket

user2.o: user2.c ksocket.h
	$(CC) $(CFLAGS) -c user2.c

# Run commands for testing
run_init:
	./initksocket

# Run receiver (should be started before sender)
run_recv:
	./user2 127.0.0.1 5076 127.0.0.1 8081

# Run sender
run_send:
	./user1 127.0.0.1 8081 127.0.0.1 5076

# For testing multiple simultaneous connections
run_recv2:
	./user2 127.0.0.1 5077 127.0.0.1 8082

run_send2:
	./user1 127.0.0.1 8082 127.0.0.1 5077

clean:
	rm -f *.o user1 user2 initksocket $(LIBRARY) received_file_*.txt
