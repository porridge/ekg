CC = gcc -O2 -Wall -ggdb -I../ ../libgg.o ../libgg_search.o

all:
	$(CC) search.c -o search-sync
	$(CC) -DASYNC search.c -o search-async
	$(CC) send.c -o send

