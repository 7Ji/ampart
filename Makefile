BINARY = ampart
DIR_INCLUDE = include
DIR_SOURCE = src
DIR_OBJECT = obj
CC ?= gcc
STRIP ?= strip
CFLAGS = -I$(DIR_INCLUDE) -Wall -Wextra

LDFLAGS = -lz

INCLUDES = $(wildcard $(DIR_INCLUDE)/*.h)

# _OBJECTS = alloc.o drive.o escape.o logging.o main.o sort.o systemd.o
_OBJECTS = $(wildcard $(DIR_SOURCE)/*.c)
OBJECTS = $(patsubst $(DIR_SOURCE)/%.c,$(DIR_OBJECT)/%.o,$(_OBJECTS))

$(BINARY): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(DIR_OBJECT)/%.o: $(DIR_SOURCE)/%.c $(INCLUDES)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(DIR_OBJECT)/*.o $(BINARY)

# ifeq ($(PREFIX),)
#     PREFIX := /usr/sbin
# endif

# install: $(BINARY)
# 	install -d $(DESTDIR)$(PREFIX)
# 	install -m 755 $< $(DESTDIR)$(PREFIX)/
# 	$(STRIP) $(DESTDIR)$(PREFIX)/$<
