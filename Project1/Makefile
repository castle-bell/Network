CC=gcc

all : client server

client: client.o
	$(CC) -o client $^

server: server.o
	$(CC) -o server $^

client.o: client.c
	$(CC) -c -o client.o $^

server.o: server.c
	$(CC) -c -o server.o $^

clean:
	rm *.o client server