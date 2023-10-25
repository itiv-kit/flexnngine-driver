#include "driver.h"
#include "defs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int recacc_open(recacc_device* dev, const char* uio_name) {
    dev->fd = open(uio_name, O_RDWR);
    if (dev->fd == -1) {
        printf("Failed to open /dev/uio4: %s\n", strerror(errno));
        return errno;
    }

    dev->mem = mmap(NULL, RECACC_MEM_MAP_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, dev->fd, 0);
    if (dev->mem == MAP_FAILED) {
        int err = errno;
        printf("Failed to map memory: %s\n", strerror(errno));
        recacc_close(dev);
        return err;
    }
    printf("Mapped region at %p\n", dev->mem);

    return 0;
}

int recacc_close(recacc_device* dev) {
    int ret = 0;

    if (dev->mem) {
        ret = munmap(dev->mem, RECACC_MEM_MAP_SIZE);
        if (ret)
            return ret;
        dev->mem = 0;
    }

    if (dev->fd) {
        ret = close(dev->fd);
        if (ret)
            return ret;
        dev->fd = 0;
    }

    return ret;
}

int recacc_verify(recacc_device* dev, bool print_info) {
    uint32_t magic_reg = recacc_reg_read(dev, RECACC_REG_IDX_MAGIC);
    char magic_str[4] = {0};
    strncpy(magic_str, (char*)&magic_reg, 3);
    dev->hw_revision = ((uint8_t*)&magic_reg)[3];

    bool okay = strcmp(magic_str, RECACC_MAGIC) == 0;
    if (print_info) {
        if (okay)
            printf("Accelerator verified, hw version %u\n", dev->hw_revision);
        else
            printf("Accelerator NOT verified, invalid magic register contents 0x%08x\n", magic_reg);
    }
    return okay;
}

int recacc_config_read(const recacc_device* dev, recacc_config* cfg) {
    cfg->iact_dimension  = recacc_reg_read(dev, RECACC_REG_IDX_IMAGE_Y); // identical to RECACC_REG_IDX_IMAGE_X
    cfg->wght_dimension  = recacc_reg_read(dev, RECACC_REG_IDX_KERNEL_SIZE);
    cfg->input_channels  = recacc_reg_read(dev, RECACC_REG_IDX_KERNELS);
    cfg->output_channels = recacc_reg_read(dev, RECACC_REG_IDX_CHANNELS);
    cfg->c1              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C1);
    cfg->w1              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_W1);
    cfg->h2              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_H2);
    cfg->m0              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_M0);
    cfg->m0_last_m1      = recacc_reg_read(dev, RECACC_REG_IDX_CONV_M0_LAST_M1);
    cfg->row_last_h2     = recacc_reg_read(dev, RECACC_REG_IDX_CONV_ROW_LAST_H2);
    cfg->c0              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0);
    cfg->c0_last_c1      = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0_LAST_C1);
    cfg->c0w0            = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0W0);
    cfg->c0w0_last_c1    = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0W0_LAST_C1);
    cfg->magic           = recacc_reg_read(dev, RECACC_REG_IDX_MAGIC);
    return 0;
}

int recacc_config_write(const recacc_device* dev, const recacc_config* cfg) {
    // RECACC_REG_IDX_CONTROL is excluded
    // RECACC_REG_IDX_STATUS can't be written
    recacc_reg_write(dev, RECACC_REG_IDX_IMAGE_X, cfg->iact_dimension); // rectangular shapes only (for now)
    recacc_reg_write(dev, RECACC_REG_IDX_IMAGE_Y, cfg->iact_dimension);
    recacc_reg_write(dev, RECACC_REG_IDX_KERNEL_SIZE, cfg->wght_dimension);
    recacc_reg_write(dev, RECACC_REG_IDX_KERNELS, cfg->input_channels);
    recacc_reg_write(dev, RECACC_REG_IDX_CHANNELS, cfg->output_channels);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C1, cfg->c1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_W1, cfg->w1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_H2, cfg->h2);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_M0, cfg->m0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_M0_LAST_M1, cfg->m0_last_m1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_ROW_LAST_H2, cfg->row_last_h2);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0, cfg->c0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0_LAST_C1, cfg->c0_last_c1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0W0, cfg->c0w0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0W0_LAST_C1, cfg->c0w0_last_c1);
    // RECACC_REG_IDX_MAGIC can't be written
    return 0;
}

uint32_t recacc_reg_read(const recacc_device* dev, int regidx) {
    uint32_t *ptr = (uint32_t*)(dev->mem + RECACC_BYTE_OFFSET(regidx));
    return *ptr;
}

void recacc_reg_write(const recacc_device* dev, int regidx, uint32_t value) {
    uint32_t *ptr = (uint32_t*)(dev->mem + RECACC_BYTE_OFFSET(regidx));
    *ptr = value;
}

recacc_status recacc_get_status(const recacc_device* dev) {
    union recacc_status_reg status;
    status.raw = recacc_reg_read(dev, RECACC_REG_IDX_STATUS);
    return status.decoded;
}

void recacc_control_start(const recacc_device* dev) {
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, (1 << RECACC_BIT_IDX_CONTROL_START));
}

void recacc_control_reset(const recacc_device* dev) {
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, (1 << RECACC_BIT_IDX_CONTROL_RESET));
}
