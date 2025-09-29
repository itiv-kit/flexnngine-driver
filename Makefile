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

# $(SYS_DIR)/syscalls.c
# $(SYS_DIR)/crt.S
# -lintegr
# -nostdlib
# CFLAGS_CVA6 := \
# 	$(INCLUDES) \
# 	-mcmodel=medany \
# 	-DPREALLOCATE=1 \
# 	-ffast-math \
# 	-fno-common \
# 	-fno-builtin-printf \
# 	-static -nostdlib \
# 	-nostartfiles \
# 	-lgcc \
# 	-T $(SYS_DIR)/linker.ld \
# 	-march=rv64imafdc_zifencei -mabi=lp64d \
# 	-Xlinker \
# 	-Map=main.map \
# 	-fno-tree-loop-distribute-patterns \
# 	-fvisibility=hidden \
# 	-funroll-all-loops \
# 	-ffunction-sections -fdata-sections \
# 	-Wl,-gc-sections \
# 	-falign-jumps=32 -falign-functions=32
CFLAGS_CVA6 := \
	$(INCLUDES) \
	-ffast-math -static \
	-T $(SYS_DIR)/linker.ld \
	-march=rv64imafdc_zifencei -mabi=lp64d \
	-falign-jumps=32 -falign-functions=32
CVA6_LIBS := $(SYS_DIR)/crt.S $(LIB_DIR)/libintegr.a syscalls.o

# tests/common.mk does not define RV_GXX
RV_GXX := $(RV_TOOL_PREFIX)g++

syscalls.o: $(SYS_DIR)/syscalls.c
	$(RV_GCC) $(CFLAGS_CVA6) -c -o $@ $(SYS_DIR)/syscalls.c

#b uild objs and libs files for SW
$(LIB_DIR)/libintegr.a:
	@$(MAKE) -C ../../ clean_sw libs/libintegr.a nb_cores

CFLAGS = $(CFLAGS_CVA6) -Idriver -Wall -Wextra -std=gnu11
CXXFLAGS = $(CFLAGS_CVA6) -Idriver -Wall -Wextra -std=c++20
release: CFLAGS += -s -O3
release: CXXFLAGS += -s -O3
debug:   CFLAGS += -g -O1
debug:   CXXFLAGS += -g -O1
LDFLAGS = -lm
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
