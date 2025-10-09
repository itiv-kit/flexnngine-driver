CVA6_REPO_DIR := $(abspath ../../../../modules/cva6)
# RV_TOOL_PREFIX := riscv64-unknown-elf-
# CXX := $(RV_TOOL_PREFIX)gcc
# RISCV_AR := $(RV_TOOL_PREFIX)ar
# RISCV_OBJDUMP := $(RV_TOOL_PREFIX)objdump
# RISCV_OBJCOPY := $(RV_TOOL_PREFIX)objcopy

# CVA6 cflags and defs copied from tests/integration/testlist/hello_world
ROOT_DIR := $(shell dirname $(abspath $(lastword $(MAKEFILE_LIST))))
include ../../../common.mk
# include ../../../rtl.mk

SYS_DIR = ../../sw/sys
LIB_DIR = ../../libs
INC_DIR = ../../sw/include
INCLUDES = $(foreach d, $(INC_DIR), -I$d)
INCLUDES += $(foreach d, $(SYS_DIR), -I$d)

CVA6_CFLAGS := \
	$(INCLUDES) \
	-DPREALLOCATE=1 \
	-ffast-math \
	-T $(SYS_DIR)/linker.ld \
	-march=rv64imafdc_zifencei -mabi=lp64d -mcmodel=medany \
	-fno-common \
	-fno-builtin-printf \
	-falign-jumps=32 -falign-functions=32 \
	-fno-tree-loop-distribute-patterns \
	-fvisibility=hidden \
	-fno-zero-initialized-in-bss \
	-funroll-all-loops
# 	-ffunction-sections -fdata-sections
CVA6_LDFLAGS := \
	-nostartfiles \
	-static \
	-Xlinker \
	-lgcc \
	-Wl,--gc-sections
# 	-nostdlib
CVA6_LIBS := $(SYS_DIR)/crt.S $(LIB_DIR)/libintegr.a syscalls.o

# riscv64-unknown-elf-gcc -Werror -g  -I../../sw/include  -I../../sw/sys -L../../libs -DPREALLOCATE=1 -mcmodel=medany -static -std=gnu99 -O3 -g -ffast-math -fno-common -fno-builtin-printf ../../sw/sys/syscalls.c -static -nostdlib ../../sw/sys/crt.S -nostartfiles -lm -lgcc -T ../../sw/sys/linker.ld -march=rv64imafdc_z ifencei -lintegr -Xlinker -Map=main.map -fno-tree-loop-distribute-patterns -fvisibility=hidden -fno-zero-initialized-in-bss -funroll-all-loops -ffunction-sections -fdata-sections -Wl,-gc-sections -falign-jumps=32 -falign-functions=32  -o main.riscv main.c hello_world.c uart.c  -DCOMPILER_FLAGS='"-Werror -g  -I../../sw/include  -I../../sw/sys -L../../libs -DPREALLOCATE=1 -mcmodel=medany -static -std=gnu99 -O3 -g -ffast-math -fno-common -fno-builtin-printf ../../sw/sys/syscalls.c -static -nostdlib ../../sw/sys/crt.S -nostartfiles -lm -lgcc -T ../../sw/sys/linker.ld -march=rv64imafdc_zifencei -lintegr -Xlinker -Map=main.map -fn o-tree-loop-distribute-patterns -fvisibility=hidden -fno-zero-initialized-in-bss -funroll-all-loops -ffunction-sections -fdata-sections -Wl,-gc-sections -falign-jumps=32 -falign-functions=32 "' -DITERATIONS=1 -DPERFORMANCE_RUN -DSKIP_TIME_CHECK -DNOPRINT -DC_MULTICORE=1

# tests/common.mk does not define RV_GXX
RV_GXX := $(RV_TOOL_PREFIX)g++

syscalls.o: $(SYS_DIR)/syscalls.c
	$(RV_GCC) $(CVA6_CFLAGS) -c -o $@ $(SYS_DIR)/syscalls.c

# build objs and libs files for SW
$(LIB_DIR)/libintegr.a:
	@$(MAKE) -C ../.. clean_sw libs/libintegr.a nb_cores

CFLAGS = $(CVA6_CFLAGS) -Idriver -Wall -Wextra -std=gnu11
CXXFLAGS = $(CVA6_CFLAGS) -Idriver -Wall -Wextra -std=c++20
release: CFLAGS += -s -O3
release: CXXFLAGS += -s -O3
debug:   CFLAGS += -g -O1
debug:   CXXFLAGS += -g -O1
LDFLAGS = -lm $(CVA6_LDFLAGS)
RANLIB ?= ranlib

SRCS = $(wildcard driver/*.c)
OBJS = $(filter-out driver/linux.o,$(SRCS:%.c=%.o))
SRCS_CXX = $(wildcard lib/*.cpp)
OBJS_CXX = $(SRCS_CXX:%.cpp=%.o)
LIB = libflexnngine.a
TARGET_SRCS_C = $(wildcard *.c)
TARGET_SRCS_CXX = $(wildcard *.cpp)
TARGETS_C = $(TARGET_SRCS_C:%.c=%)
TARGETS_CXX = $(TARGET_SRCS_CXX:%.cpp=%)
TARGETS = $(TARGETS_C) $(TARGETS_CXX)

.PHONY: all release debug clean
.DEFAULT_GOAL := all

all: release

release: $(TARGETS)
debug: $(TARGETS)

$(TARGETS_C): %: %.c $(LIB) $(CVA6_LIBS)
	$(RV_GCC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGETS_CXX): %: %.cpp $(LIB) $(CVA6_LIBS)
	$(RV_GXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(RV_GCC) $(CFLAGS) -fPIC -c -o $@ $<

%.o: %.cpp
	$(RV_GXX) $(CXXFLAGS) -fPIC -c -o $@ $<

$(LIB): $(OBJS_CXX) $(OBJS)
	-rm -f $@
	$(AR) rc $@ $^
	$(RANLIB) $@

clean:
	rm -f $(OBJS) $(OBJS_CXX) $(LIB) $(TARGETS) $(LIB_DIR)/libintegr.a syscalls.o
