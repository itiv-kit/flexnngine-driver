#include <stdio.h>
#include <stdint.h>

#include "driver/driver.h"

int main() {
    recacc_device dev = {0};
    int ret = recacc_open(&dev, "/dev/uio4");
    if (ret)
        return ret;

    for (int i=0; i<4; i++)
        printf("Control reg %d: 0x%08x\n", i, recacc_reg_read(&dev, i));

    printf("Writing to reg 0...\n");
    recacc_reg_write(&dev, 0, 0xDEADBEEF);

    for (int i=0; i<4; i++)
        printf("Control reg %d: 0x%08x\n", i, recacc_reg_read(&dev, i));

    ret = recacc_close(&dev);
    return ret;
}
