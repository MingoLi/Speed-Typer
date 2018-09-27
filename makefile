CC=gcc
CFLAGS = -Wall -lpthread
#DEPS = hellomake.h

# %.o: %.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)

# hellomake: hellomake.o hellofunc.o 
# 	$(CC) -o hellomake hellomake.o hellofunc.o 



all: client server

client: client.c
	$(CC) -o $@ $< $(CFLAGS)
server: server.c
	$(CC) -o $@ $< $(CFLAGS)

# clean:
#     -rm -f *.o
#     -rm -f $(TARGET)