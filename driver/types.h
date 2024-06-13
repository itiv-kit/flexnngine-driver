#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int fd;
    void* mem;
    uint8_t hw_revision;
} recacc_device;

typedef struct {
    uint32_t iact_dimension;  // width and height of input activations (rectangular shape)
    uint32_t wght_dimension;  // width and height of kernels
    uint32_t input_channels;  // number of input channels / kernels
    uint32_t output_channels; // number of output channels
    uint32_t c1;
    uint32_t w1;
    uint32_t h2;
    uint32_t m0;
    uint32_t m0_last_m1;
    uint32_t rows_last_h2;
    uint32_t c0;
    uint32_t c0_last_c1;
    uint32_t c0w0;
    uint32_t c0w0_last_c1;
    uint32_t magic;
} recacc_config;

typedef struct {
    uint32_t array_size_x;
    uint32_t array_size_y;
    uint32_t line_length_iact;
    uint32_t line_length_wght;
    uint32_t line_length_psum;
    uint32_t spad_size_iact;
    uint32_t spad_size_wght;
    uint32_t spad_size_psum;
    uint8_t  data_width_bits_iact;
    uint8_t  data_width_bits_wght;
    uint8_t  data_width_bits_psum;
} recacc_hwinfo;

typedef struct {
    bool done:1;
    bool ready:1;
    bool ctrl_iact_done:1;
    bool ctrl_wght_done:1;
} __attribute__((packed)) recacc_status;

union recacc_status_reg {
    uint32_t raw;
    recacc_status decoded;
};

enum buffer_type {
    iact, wght, psum
};
