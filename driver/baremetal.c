#include "baremetal.h"

int recacc_open(recacc_device* dev, void* addr) {
    dev->fd = 0;
    dev->mem = addr;
    dev->hw_revision = 0;

    return 0;
}

int recacc_close(recacc_device* dev) {
    dev->mem = 0;
    dev->fd = 0;

    return 0;
}
