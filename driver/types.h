#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int fd;
    void* mem;
    uint8_t hw_revision;
} recacc_device;

typedef struct {
    uint16_t iact_dimension;  // width and height of input activations (rectangular shape)
    uint8_t  wght_dimension;  // width and height of kernels
    uint16_t input_channels;  // number of input channels / kernels
    uint16_t output_channels; // number of output channels
    uint16_t c1;
    uint16_t w1;
    uint16_t h2;
    uint16_t m0;
    uint16_t m0_last_m1;
    uint16_t rows_last_h2;
    uint16_t c0;
    uint16_t c0_last_c1;
    uint16_t c0w0;
    uint16_t c0w0_last_c1;
    uint8_t  psum_throttle;
    uint8_t  pad_x;
    uint8_t  pad_y;
    uint32_t base_addr_iact;
    uint32_t base_addr_wght;
    uint32_t base_addr_psum;
    uint32_t base_addr_pad;
    uint32_t stride_iact_w;
    uint32_t stride_iact_hw;
    uint8_t  stride_wght_krnl;
    uint16_t stride_wght_och;
    uint16_t stride_psum_och;
    uint8_t  stride;
} recacc_config;

typedef struct {
    uint32_t array_size_x;
    uint32_t array_size_y;
    uint32_t line_length_iact;
    uint32_t line_length_wght;
    uint32_t line_length_psum;
    uint32_t spad_size;
    uint32_t spad_word_size;
    uint8_t  data_width_bits_iact;
    uint8_t  data_width_bits_wght;
    uint8_t  data_width_bits_psum;
    uint8_t  max_output_channels;
    bool     trs_dataflow;
    bool     bias_requant_available;
} recacc_hwinfo;

typedef struct {
    bool done:1;
    bool ready:1;
    bool ctrl_iact_done:1;
    bool ctrl_wght_done:1;
    bool preload_done:1;
    bool irq:1;
    uint32_t reserved:21;
    bool spad_psum_empty:1;
    bool spad_iact_full:1;
    bool spad_iact_empty:1;
    bool spad_wght_full:1;
    bool spad_wght_empty:1;
} __attribute__((packed)) recacc_status;

union recacc_status_reg {
    uint32_t raw;
    recacc_status decoded;
};

typedef struct {
    bool reset:1;
    bool start:1;
    bool requantize:1;
    uint8_t activation_mode:3;
    bool irq_en:1;
    bool padding:1;
} __attribute__((packed)) recacc_control;

union recacc_control_reg {
    uint32_t raw;
    recacc_control decoded;
};

enum activation_mode {
    act_none, act_relu
};

typedef int8_t input_t;
typedef int32_t psum_t;
