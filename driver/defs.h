#pragma once

#define RECACC_MEM_OFFSET_REGS 0x00000
#define RECACC_MEM_OFFSET_SPAD 0x80000
#define RECACC_MEM_MAP_SIZE    0x100000 // covers all mapped areas

#define RECACC_MAGIC "ACC"
#define RECACC_MIN_HW_REV 5
#define RECACC_MAX_HW_REV 5

#define RECACC_BYTE_OFFSET(idx) (idx*4)
#define RECACC_REG_ADDR(idx)    (RECACC_MEM_OFFSET_REGS+RECACC_BYTE_OFFSET(idx))

// main control & status registers
#define RECACC_REG_IDX_CONTROL           0
#define RECACC_REG_IDX_STATUS            1

// control unit loop parameters
#define RECACC_REG_IDX_INPUTCHS          2
#define RECACC_REG_IDX_OUTPUTCHS         3
#define RECACC_REG_IDX_IMAGE_Y           4
#define RECACC_REG_IDX_IMAGE_X           5
#define RECACC_REG_IDX_KERNEL_SIZE       6
#define RECACC_REG_IDX_CONV_C1           7
#define RECACC_REG_IDX_CONV_W1           8
#define RECACC_REG_IDX_CONV_H2           9
#define RECACC_REG_IDX_CONV_M0           10
#define RECACC_REG_IDX_CONV_M0_LAST_M1   11
#define RECACC_REG_IDX_CONV_ROWS_LAST_H2 12
#define RECACC_REG_IDX_CONV_C0           13
#define RECACC_REG_IDX_CONV_C0_LAST_C1   14
#define RECACC_REG_IDX_CONV_C0W0         15
#define RECACC_REG_IDX_CONV_C0W0_LAST_C1 16
#define RECACC_REG_IDX_PSUM_THROTTLE     17
#define RECACC_REG_IDX_CONV_PADDING      18
#define RECACC_REG_IDX_CONV_STRIDE       19 // not implemented yet

// memory address & layout registers
#define RECACC_REG_IDX_BASE_ADDR_IACT    19
#define RECACC_REG_IDX_BASE_ADDR_WGHT    20
#define RECACC_REG_IDX_BASE_ADDR_PSUM    21
#define RECACC_REG_IDX_BASE_ADDR_PAD     22
#define RECACC_REG_IDX_STRIDE_IACT_W     23
#define RECACC_REG_IDX_STRIDE_IACT_HW    24
#define RECACC_REG_IDX_STRIDE_WGHT_KRNL  25
#define RECACC_REG_IDX_STRIDE_WGHT_OCH   26
#define RECACC_REG_IDX_STRIDE_PSUM_OCH   27

// hardware information registers
#define RECACC_REG_IDX_MAGIC             31
#define RECACC_REG_IDX_ARRAY_SIZE        32
#define RECACC_REG_IDX_LINE_LENGTH_1     33
#define RECACC_REG_IDX_LINE_LENGTH_2     34
#define RECACC_REG_IDX_DATA_WIDTH        35
#define RECACC_REG_IDX_ADDR_WIDTH        36
#define RECACC_REG_IDX_CAPABILITIES      37

// extended status & debug registers
#define RECACC_REG_IDX_CYCLE_COUNTER     38
#define RECACC_REG_IDX_PSUM_OVERFLOWS    39

// start of the postprocessing parameter register block
#define RECACC_REG_IDX_BIAS_REQUANT_BASE 40

#define RECACC_BIT_IDX_CONTROL_RESET   0
#define RECACC_BIT_IDX_CONTROL_START   1
#define RECACC_BIT_IDX_CONTROL_REQUANT 2
#define RECACC_BIT_IDX_CONTROL_ACTMODE 3
#define RECACC_BIT_IDX_CONTROL_IRQ_EN  6
#define RECACC_BIT_IDX_CONTROL_PADDING 7

#define RECACC_BIT_IDX_STATUS_IRQ 5

#define RECACC_BIT_IDX_CAP_DATAFLOW     8
#define RECACC_BIT_IDX_CAP_BIAS_REQUANT 9
