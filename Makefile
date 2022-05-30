all: ampart
ampart: ampart.c
	$(CC) $(CFLAGS) $(LDFLAGS) -Wall -Wextra ampart.c -lz -o ampart