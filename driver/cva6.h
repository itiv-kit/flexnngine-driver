#pragma once

// hacks for the Ariane / CVA6 port

// control regs base address, spad is mapped at +RECACC_MEM_OFFSET_SPAD 
#define FLEXNNGINE_BASE_ADDR 0x40700000

// define fprintf to just do normal printf
#define fprintf(_, ...) do { \
    printf(__VA_ARGS__); \
  } while (0)
