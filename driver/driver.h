#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "defs.h"

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
} recacc_hwinfo;

typedef struct {
    bool done:1;
    bool ready:1;
    bool status_scratchpad:1;
    bool status_addressgen:1;
} __attribute__((packed)) recacc_status;

union recacc_status_reg {
    uint32_t raw;
    recacc_status decoded;
};

enum buffer_type {
    iact, wght, psum
};

// open the accelerator device at uio_name (e.g. /dev/uio4)
// dev needs to be allocated by the caller
int recacc_open(recacc_device* dev, const char* uio_name);

// close the accelerator device
// the caller keeps ownership of dev and needs to release it
int recacc_close(recacc_device* dev);

// verify the magic register and set the device version
// if print_info is true, print an info string to stdout
int recacc_verify(recacc_device* dev, bool print_info);

// read the current set of configuration registers from the accelerator
int recacc_config_read(const recacc_device* dev, recacc_config* cfg);

// write a full set of configuration data to the accelerator
// does not write the control register (RECACC_REG_IDX_CONTROL)
int recacc_config_write(const recacc_device* dev, const recacc_config* cfg);

// read a single register from the accelerator
// see defs.h for index definitions
uint32_t recacc_reg_read(const recacc_device* dev, int regidx);

// write a single register within the accelerator
// see defs.h for index definitions
void recacc_reg_write(const recacc_device* dev, int regidx, uint32_t value);

// poll the accelerator status register once
// returns true if the accelerator is busy
// returns false if the operation is done / not started yet
bool recacc_poll(const recacc_device* dev);

// wait until the accelerator is ready
// this call blocks until an interrupt is received
void recacc_wait(const recacc_device* dev);

// read the status register and return its value as a decoded struct
recacc_status recacc_get_status(const recacc_device* dev);

// start the accelerator by writing to the control register
void recacc_control_start(const recacc_device* dev);

// reset the accelerator by writing to the control register
void recacc_control_reset(const recacc_device* dev);

// retrieve information about the current hardware design (accelerator size etc.)
// TODO: CURRENTLY STATICALLY IMPLEMENTED, returns fixed values only
void recacc_get_hwinfo(const recacc_device* dev, recacc_hwinfo* hwinfo);

// return the virtual buffer address for a specific buffer type
void* recacc_get_buffer(const recacc_device* dev, enum buffer_type type);
