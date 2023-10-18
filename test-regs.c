#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "driver/driver.h"

#define DEFAULT_DEVICE "/dev/uio4"
#define FIRST_TEST_REG 2 // exlude control/status regs
#define NUM_TEST_REGS 4
#define TEST_PATTERN(i) (0xDEAD000 & i)

int main(int argc, char** argv) {
    char device_name[256] = DEFAULT_DEVICE;
    if (argc > 1) {
        strncpy(device_name, *++argv, 255);
        device_name[255] = 0;
    }

    recacc_device dev = {0};
    int ret = recacc_open(&dev, device_name);
    if (ret)
        return ret;

    ret = recacc_verify(&dev, true);
    if (ret)
        goto end;

    for (int i=0; i<NUM_TEST_REGS; i++)
        printf("Register %d: 0x%08x\n", i, recacc_reg_read(&dev, FIRST_TEST_REG+i));

    for (int i=0; i<NUM_TEST_REGS; i++) {
        uint32_t value = TEST_PATTERN(i);
        printf("Writing 0x%08x to reg %d...\n", value, i);
        recacc_reg_write(&dev, FIRST_TEST_REG+i, value);
    }

    int ok = 0;
    for (int i=0; i<NUM_TEST_REGS; i++) {
        uint32_t value = recacc_reg_read(&dev, FIRST_TEST_REG+i);
        printf("Register %d: 0x%08x\n", i, value);
        if (value == TEST_PATTERN(i))
            ok++;
    }

    if (ok == NUM_TEST_REGS)
        printf("%d registers tested successfully\n", NUM_TEST_REGS);
    else
        printf("ERROR: Failed to write %d registers\n", NUM_TEST_REGS-ok);

    printf("Resetting registers to 0...\n");
    for (int i=0; i<NUM_TEST_REGS; i++)
        recacc_reg_write(&dev, FIRST_TEST_REG+i, 0);

end:
    ret = recacc_close(&dev);
    return ret;
}
