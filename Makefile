CC=gcc
CFLAGS=-march=armv8-a -std=gnu11 -Iinclude -Ibinutils/include -Wall -Wextra -Og -g
LDLIBS=
DEFINES=-D_FILE_OFFSET_BITS=64

ifeq ($(USE_CAPSTONE),TRUE)
LDLIBS+=-lcapstone
DEFINES+=-DUSE_CAPSTONE
endif

SRCS=$(wildcard src/*.c)
OBJS=$(notdir $(SRCS:.c=.o))

ifeq ($(SHARED_LIBOPCODES),TRUE)
LDLIBS+=-lopcodes
else
SRCS+=$(wildcard binutils/opcodes/*.c)
endif

all: fuzzer

fuzzer: $(OBJS)
	$(CC) -o $@ $(LDLIBS) $(OBJS)

%.o: src/%.c
	$(CC) $(CFLAGS) $(DEFINES) -c $<

%.o: binutils/opcodes/%.c
	$(CC) $(CFLAGS) -Wno-unused -DHAVE_STRING_H -DARCH_arm -c $<

clean:
	$(RM) $(OBJS) fuzzer
