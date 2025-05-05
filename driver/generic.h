#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "defs.h"
#include "types.h"

// verify the magic register and set the device version
// returns true if verification succeeds, false otherwise
// if print_info is true, print an info string to stdout
bool recacc_verify(recacc_device* dev, bool print_info);

// resets the accelerator through the control register
void recacc_reset(recacc_device* dev);

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
// returns true if wait was successful
// returns false after a 1-second timeout (hardware stuck)
bool recacc_wait(const recacc_device* dev, bool poll);

// read the status register and return its value as a decoded struct
recacc_status recacc_get_status(const recacc_device* dev);

// start the accelerator by writing to the control register
void recacc_control_start(const recacc_device* dev, bool requantize, enum activation_mode mode, bool enable_interrupt);

// clear the start bit by writing to the control register
void recacc_control_stop(const recacc_device* dev);

// clear the interrupt flag in the status register
void recacc_control_clear_irq(const recacc_device* dev);

// retrieve information about the current hardware design (accelerator size etc.)
// TODO: CURRENTLY STATICALLY IMPLEMENTED, returns fixed values only
void recacc_get_hwinfo(const recacc_device* dev, recacc_hwinfo* hwinfo);

// return the virtual buffer address for a specific buffer type
void* recacc_get_buffer(const recacc_device* dev);
