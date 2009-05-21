CC = gcc
override CFLAGS += -Wall -Wshadow -Werror
override CPPFLAGS += -D_FILE_OFFSET_BITS=64 -I$(shell pg_config --includedir)
LIBS = $(shell pkg-config fuse --libs) -lpq

OBJS = postgresqlfs.o path.o strlcpy.o query.o

all: postgresqlfs

postgresqlfs: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f $(OBJS) postgresqlfs
