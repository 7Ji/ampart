GIT_VERSION := "$(shell git log --format="%H" -n 1)"

all: ampart
ampart: ampart.c
	$(CC) -DVERSION=\"git-$(GIT_VERSION)\" $(CFLAGS) $(LDFLAGS) -Wall -Wextra -lz -o ampart ampart.c
getSub: getSub.c
	$(CC) $(CFLAGS) $(LDFLAGS) -Wall -Wextra -lz -o getSub getSub.c 