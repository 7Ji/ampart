all: ampart
ampart: ampart.c
	gcc -Wall -Wextra -o ampart ampart.c