CFLAGS = -Idriver -Wall -Wextra -std=c11
CXXFLAGS = -Idriver -Wall -Wextra -std=c++20
release: CFLAGS += -s -O3
release: CXXFLAGS += -s -O3
debug:   CFLAGS += -g -O1
debug:   CXXFLAGS += -g -O1
LDFLAGS = -lm

SRCS = $(wildcard driver/*.c)
OBJS = $(filter-out driver/baremetal.o,$(SRCS:%.c=%.o))
SRCS_CXX = $(wildcard lib/*.cpp)
OBJS_CXX = $(SRCS_CXX:%.cpp=%.o)
TARGET_SRCS_C = $(wildcard *.c)
TARGET_SRCS_CXX = $(wildcard *.cpp)
TARGETS_C = $(TARGET_SRCS_C:%.c=%)
TARGETS_CXX = $(TARGET_SRCS_CXX:%.cpp=%)
TARGETS = $(TARGETS_C) $(TARGETS_CXX)

.PHONY: all release debug clean

all: release

release: $(TARGETS)
debug: $(TARGETS)

$(TARGETS_C): %: %.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGETS_CXX): %: %.cpp $(OBJS) $(OBJS_CXX)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGETS)
