#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "driver/driver.h"

#define DEFAULT_DEVICE "/dev/uio4"
#define FIRST_TEST_REG 2 // exclude control/status regs
#define NUM_TEST_REGS 4
#define TEST_PATTERN(i) (0xDEADBE00U & i)

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

    printf("Status register: 0x%08x\n", recacc_reg_read(&dev, RECACC_REG_IDX_STATUS));

    for (int i=FIRST_TEST_REG; i<FIRST_TEST_REG+NUM_TEST_REGS; i++)
        printf("Register %d: 0x%08x\n", i, recacc_reg_read(&dev, i));

    for (int i=FIRST_TEST_REG; i<FIRST_TEST_REG+NUM_TEST_REGS; i++) {
        uint32_t value = TEST_PATTERN(i);
        printf("Writing 0x%08x to reg %d...\n", value, i);
        recacc_reg_write(&dev, i, value);
    }

    int ok = 0;
    for (int i=FIRST_TEST_REG; i<FIRST_TEST_REG+NUM_TEST_REGS; i++) {
        uint32_t value = recacc_reg_read(&dev, i);
        printf("Register %d: 0x%08x\n", i, value);
        if (value == TEST_PATTERN(i))
            ok++;
    }

    if (ok == NUM_TEST_REGS)
        printf("%d registers tested successfully\n", NUM_TEST_REGS);
    else
        printf("ERROR: Failed to write %d registers\n", NUM_TEST_REGS-ok);

    printf("Resetting registers to 0...\n");
    for (int i=FIRST_TEST_REG; i<FIRST_TEST_REG+NUM_TEST_REGS; i++)
        recacc_reg_write(&dev, i, 0);

end:
    ret = recacc_close(&dev);
    return ret;
}
