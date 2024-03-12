#pragma once

#include "types.h"

// open the accelerator device at a fixed memory base address
// dev needs to be allocated by the caller
int recacc_open(recacc_device* dev, void* addr);

// close the accelerator device (no-op)
int recacc_close(recacc_device* dev);
