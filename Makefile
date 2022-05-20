all: ampart
ampart: ampart.c
	$(CC) $(CFLAGS) $(LDFLAGS) -Wall -Wextra -o ampart ampart.c