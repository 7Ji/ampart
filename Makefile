BINARY = ampart
DIR_INCLUDE = include
DIR_SOURCE = src
DIR_OBJECT = obj
CC ?= gcc
STRIP ?= strip
LDFLAGS = -lz
CFLAGS = -I$(DIR_INCLUDE) -Wall -Wextra
STATIC ?= 0
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CFLAGS += -g
endif
ifeq ($(STATIC), 1)
	LDFLAGS += -static
endif

INCLUDES = $(wildcard $(DIR_INCLUDE)/*.h)

_OBJECTS = $(wildcard $(DIR_SOURCE)/*.c)
OBJECTS = $(patsubst $(DIR_SOURCE)/%.c,$(DIR_OBJECT)/%.o,$(_OBJECTS))

ifndef VERSION
	VERSION:=$(shell bash scripts/build-only-version.sh)
endif

$(BINARY): $(OBJECTS) | version
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
ifneq ($(DEBUG), 1)
	$(STRIP) $(BINARY)
endif

ifdef VERSION
$(DIR_OBJECT)/version.o: $(DIR_SOURCE)/version.c $(INCLUDES) version | prepare
	$(CC) -c -o $@ $< $(CFLAGS) -DVERSION=\"$(VERSION)\"
endif

$(DIR_OBJECT)/%.o: $(DIR_SOURCE)/%.c $(INCLUDES) | prepare
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: version clean prepare fresh

clean:
	rm -rf $(DIR_OBJECT) $(BINARY)

prepare:
	mkdir -p $(DIR_OBJECT)

fresh: clean $(BINARY)
