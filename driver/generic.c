#include "generic.h"
#include "defs.h"

#define __USE_MISC

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

bool recacc_verify(recacc_device* dev, bool print_info) {
    uint32_t magic_reg = recacc_reg_read(dev, RECACC_REG_IDX_MAGIC);
    char magic_str[4] = {
        magic_reg >> 24 & 0xFF,
        magic_reg >> 16 & 0xFF,
        magic_reg >>  8 & 0xFF,
        0
    };
    dev->hw_revision = magic_reg & 0xFF;

    bool okay = strcmp(magic_str, RECACC_MAGIC) == 0;
    if (print_info) {
        if (okay)
            printf("Accelerator verified, hw version %u\n", dev->hw_revision);
        else
            printf("Accelerator NOT verified, invalid magic register contents 0x%08x\n", magic_reg);
    }
    return okay;
}

void recacc_reset(recacc_device* dev) {
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, (1 << RECACC_BIT_IDX_CONTROL_RESET));
    #ifdef __linux__
    usleep(10000);
    #else
    for(volatile int i=0; i<10000; i++);
    #endif
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, 0);
}

int recacc_config_read(const recacc_device* dev, recacc_config* cfg) {
    cfg->iact_dimension  = recacc_reg_read(dev, RECACC_REG_IDX_IMAGE_Y); // identical to RECACC_REG_IDX_IMAGE_X
    cfg->wght_dimension  = recacc_reg_read(dev, RECACC_REG_IDX_KERNEL_SIZE);
    cfg->input_channels  = recacc_reg_read(dev, RECACC_REG_IDX_INPUTCHS);
    cfg->output_channels = recacc_reg_read(dev, RECACC_REG_IDX_OUTPUTCHS);
    cfg->c1              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C1);
    cfg->w1              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_W1);
    cfg->h2              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_H2);
    cfg->m0              = recacc_reg_read(dev, RECACC_REG_IDX_CONV_M0);
    cfg->m0_last_m1      = recacc_reg_read(dev, RECACC_REG_IDX_CONV_M0_LAST_M1);
    cfg->rows_last_h2    = recacc_reg_read(dev, RECACC_REG_IDX_CONV_ROWS_LAST_H2);
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
    recacc_reg_write(dev, RECACC_REG_IDX_INPUTCHS, cfg->input_channels);
    recacc_reg_write(dev, RECACC_REG_IDX_OUTPUTCHS, cfg->output_channels);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C1, cfg->c1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_W1, cfg->w1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_H2, cfg->h2);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_M0, cfg->m0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_M0_LAST_M1, cfg->m0_last_m1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_ROWS_LAST_H2, cfg->rows_last_h2);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0, cfg->c0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0_LAST_C1, cfg->c0_last_c1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0W0, cfg->c0w0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0W0_LAST_C1, cfg->c0w0_last_c1);
    // RECACC_REG_IDX_MAGIC can't be written
    return 0;
}

uint32_t recacc_reg_read(const recacc_device* dev, int regidx) {
    uint32_t *ptr = (uint32_t*)(dev->mem + RECACC_REG_ADDR(regidx));
    // printf("recacc_reg_read reg %d off %d ptr %p\n", regidx, RECACC_BYTE_OFFSET(regidx), ptr);
    return *ptr;
}

void recacc_reg_write(const recacc_device* dev, int regidx, uint32_t value) {
    uint32_t *ptr = (uint32_t*)(dev->mem + RECACC_REG_ADDR(regidx));
    // printf("recacc_reg_write reg %d off %d ptr %p val 0x%08x\n", regidx, RECACC_BYTE_OFFSET(regidx), ptr, value);
    *ptr = value;
}

recacc_status recacc_get_status(const recacc_device* dev) {
    union recacc_status_reg status;
    status.raw = recacc_reg_read(dev, RECACC_REG_IDX_STATUS);
    return status.decoded;
}

void recacc_control_start(const recacc_device* dev) {
    uint32_t ctrl = recacc_reg_read(dev, RECACC_REG_IDX_CONTROL);
    ctrl |= (1 << RECACC_BIT_IDX_CONTROL_START);
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, ctrl);
}

void recacc_control_stop(const recacc_device* dev) {
    uint32_t ctrl = recacc_reg_read(dev, RECACC_REG_IDX_CONTROL);
    ctrl &= ~(1 << RECACC_BIT_IDX_CONTROL_START);
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, ctrl);
}

void recacc_get_hwinfo(const recacc_device* dev, recacc_hwinfo* hwinfo) {
    uint32_t tmp;

    tmp = recacc_reg_read(dev, RECACC_REG_IDX_ARRAY_SIZE);
    hwinfo->array_size_x = tmp & 0xffff;
    hwinfo->array_size_y = tmp >> 16 & 0xffff;

    tmp = recacc_reg_read(dev, RECACC_REG_IDX_LINE_LENGTH_1);
    hwinfo->line_length_iact = tmp & 0xffff;
    hwinfo->line_length_wght = tmp >> 16 & 0xffff;

    hwinfo->line_length_psum = recacc_reg_read(dev, RECACC_REG_IDX_LINE_LENGTH_2);

    tmp = recacc_reg_read(dev, RECACC_REG_IDX_DATA_WIDTH);
    hwinfo->data_width_bits_iact = tmp >> 0 & 0xff;
    hwinfo->data_width_bits_wght = tmp >> 8 & 0xff;
    hwinfo->data_width_bits_psum = tmp >> 16 & 0xff;

    tmp = recacc_reg_read(dev, RECACC_REG_IDX_ADDR_WIDTH_1);
    hwinfo->spad_size_iact = powl(2, tmp & 0xffff);
    hwinfo->spad_size_wght = powl(2, tmp >> 16 & 0xffff);

    hwinfo->spad_size_psum = powl(2, recacc_reg_read(dev, RECACC_REG_IDX_ADDR_WIDTH_2));
}

void* recacc_get_buffer(const recacc_device* dev, enum buffer_type type) {
    switch (type) {
        case iact:
            return dev->mem + RECACC_MEM_OFFSET_IACT;
        case wght:
            return dev->mem + RECACC_MEM_OFFSET_WGHT;
        case psum:
            return dev->mem + RECACC_MEM_OFFSET_PSUM;
        default:
            return 0;
    }
}

// returns true if the accelerator is ready or finished
bool recacc_poll(const recacc_device* dev) {
    recacc_status status = recacc_get_status(dev);
    return status.ready || status.done;
}

void recacc_wait(const recacc_device* dev) {
    // TODO: implement with interrupts instead of polling
    while (!recacc_poll(dev)) {
        #ifdef __linux__
        usleep(100000);
        #endif
    }
}
