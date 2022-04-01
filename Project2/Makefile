CC=gcc

all : proxy

proxy: proxy.o
	$(CC) -o proxy $^

proxy.o: proxy.c
	$(CC) -c -o proxy.o $^

clean:
	rm *.o proxy