#pragma once

#include "types.h"

// open the accelerator device at uio_name (e.g. /dev/uio4)
// dev needs to be allocated by the caller
int recacc_open(recacc_device* dev, const char* uio_name);

// close the accelerator device
// the caller keeps ownership of dev and needs to release it
int recacc_close(recacc_device* dev);
