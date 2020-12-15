CC = gcc
LIBS = 
CFLAGS = -w -O2
SRC = $(wildcard *.c)

boulder: $(SRC)
	$(CC) -s -o $@ $^ $(CFLAGS) $(LIBS)
