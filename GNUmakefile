CC = gcc -std=gnu99
override CFLAGS += -Wall -Wshadow -Werror -Wextra -Wformat=2 -Wpointer-arith -Wtype-limits -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wno-sign-conversion -Wlogical-op -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes
override CPPFLAGS += -D_FILE_OFFSET_BITS=64 -I$(shell pg_config --includedir)
LIBS = $(shell pkg-config fuse --libs) -lpq

OBJS = postgresqlfs.o path.o strlcpy.o query.o

all: postgresqlfs

postgresqlfs: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f $(OBJS) postgresqlfs
