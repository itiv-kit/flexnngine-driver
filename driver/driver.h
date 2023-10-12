#pragma once

#include <stdint.h>

#include "defs.h"

typedef struct {
    int fd;
    void* mem;
} recacc_device;

typedef struct {
     int iact_dimension; // width and height of input activations (rectangular shape)
     int wght_dimension; // width and height of kernels
     int input_channels; // number of input channels
} recacc_config;

// open the accelerator device at uio_name (e.g. /dev/uio4)
// dev needs to be allocated by the caller
int recacc_open(recacc_device* dev, const char* uio_name);

// close the accelerator device
// the caller keeps ownership of dev and needs to release it
int recacc_close(recacc_device* dev);

// read the current set of configuration registers from the accelerator
int recacc_config_read(const recacc_device* dev, recacc_config* cfg);

// write a full set of configuration data to the accelerator
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
int recacc_poll(const recacc_device* dev);

// wait until the accelerator is ready
// this call blocks until an interrupt is received
int recacc_wait(const recacc_device* dev);
