CFLAGS = -Wall -Wextra -std=c11
release: CFLAGS += -s -O3
debug:   CFLAGS += -g -O1
LDFLAGS =

SRCS = $(wildcard driver/*.c)
OBJS = $(SRCS:%.c=%.o)
TARGET_SRCS = $(wildcard *.c)
TARGETS = $(TARGET_SRCS:%.c=%)

.PHONY: all release debug clean

all: release

release: $(TARGETS)
debug: $(TARGETS)

$(TARGETS): %: %.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGETS)
