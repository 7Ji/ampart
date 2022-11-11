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

ifdef VERSION_CUSTOM
	CLI_VERSION := $(VERSION_CUSTOM)
else
	VERSION_GIT_TAG := $(shell git describe --abbrev=0 --tags ${TAG_COMMIT} 2>/dev/null || true)
	VERSION_GIT_TAG_NO_V := $(VERSION_GIT_TAG:v%=%)
	VERSION_GIT_COMMIT := $(shell git rev-list --abbrev-commit --tags --max-count=1)
	VERSION_GIT_DATE := $(shell git log -1 --format=%cd --date=format:"%Y%m%d")
	CLI_VERSION := $(VERSION_GIT_TAG_NO_V)-$(VERSION_GIT_COMMIT)-$(VERSION_GIT_DATE)
	ifeq ($(CLI_VERSION),--)
		CLI_VERSION := unknown
	endif
endif

ifeq ($(CLI_VERSION),)
	CLI_VERSION := unknown
endif

$(BINARY): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(DIR_OBJECT)/cli.o: $(DIR_SOURCE)/cli.c $(INCLUDES)
	$(CC) -c -o $@ $< $(CFLAGS) -DCLI_VERSION=\"$(CLI_VERSION)\"


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
