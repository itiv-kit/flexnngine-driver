#include "generic.h"
#include "defs.h"
#include "types.h"

#define __USE_MISC

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

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

    if (dev->hw_revision < RECACC_MIN_HW_REV) {
        fprintf(stderr, "Hardware revision unsupported anymore, too old: %d\n", dev->hw_revision);
        okay = false;
    }

    if (dev->hw_revision > RECACC_MAX_HW_REV) {
        fprintf(stderr, "Hardware revision unsupported yet, too recent: %d\n", dev->hw_revision);
        okay = false;
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
    cfg->iact_dimension   = recacc_reg_read(dev, RECACC_REG_IDX_IMAGE_Y); // identical to RECACC_REG_IDX_IMAGE_X
    cfg->wght_dimension   = recacc_reg_read(dev, RECACC_REG_IDX_KERNEL_SIZE);
    cfg->input_channels   = recacc_reg_read(dev, RECACC_REG_IDX_INPUTCHS);
    cfg->output_channels  = recacc_reg_read(dev, RECACC_REG_IDX_OUTPUTCHS);
    cfg->c1               = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C1);
    cfg->w1               = recacc_reg_read(dev, RECACC_REG_IDX_CONV_W1);
    cfg->h2               = recacc_reg_read(dev, RECACC_REG_IDX_CONV_H2);
    cfg->m1               = recacc_reg_read(dev, RECACC_REG_IDX_CONV_M1);
    cfg->m0               = recacc_reg_read(dev, RECACC_REG_IDX_CONV_M0);
    cfg->m0_last_m1       = recacc_reg_read(dev, RECACC_REG_IDX_CONV_M0_LAST_M1);
    cfg->rows_last_h2     = recacc_reg_read(dev, RECACC_REG_IDX_CONV_ROWS_LAST_H2);
    cfg->c0               = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0);
    cfg->c0_last_c1       = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0_LAST_C1);
    cfg->c0w0             = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0W0);
    cfg->c0w0_last_c1     = recacc_reg_read(dev, RECACC_REG_IDX_CONV_C0W0_LAST_C1);
    cfg->psum_throttle    = recacc_reg_read(dev, RECACC_REG_IDX_PSUM_THROTTLE);
    uint32_t pad_reg      = recacc_reg_read(dev, RECACC_REG_IDX_CONV_PADDING);
    cfg->pad_x            = pad_reg >> 0 & 0xff;
    cfg->pad_y            = pad_reg >> 8 & 0xff;
    cfg->base_addr_iact   = recacc_reg_read(dev, RECACC_REG_IDX_BASE_ADDR_IACT);
    cfg->base_addr_wght   = recacc_reg_read(dev, RECACC_REG_IDX_BASE_ADDR_WGHT);
    cfg->base_addr_psum   = recacc_reg_read(dev, RECACC_REG_IDX_BASE_ADDR_PSUM);
    cfg->base_addr_pad    = recacc_reg_read(dev, RECACC_REG_IDX_BASE_ADDR_PAD);
    cfg->stride_iact_w    = recacc_reg_read(dev, RECACC_REG_IDX_STRIDE_IACT_W);
    cfg->stride_iact_hw   = recacc_reg_read(dev, RECACC_REG_IDX_STRIDE_IACT_HW);
    cfg->stride_wght_krnl = recacc_reg_read(dev, RECACC_REG_IDX_STRIDE_WGHT_KRNL);
    cfg->stride_wght_och  = recacc_reg_read(dev, RECACC_REG_IDX_STRIDE_WGHT_OCH);
    cfg->stride_psum_och  = recacc_reg_read(dev, RECACC_REG_IDX_STRIDE_PSUM_OCH);

    if (dev->hw_revision >= 100) // stride will probably not be supported in the near future
        cfg->stride      = recacc_reg_read(dev, RECACC_REG_IDX_CONV_STRIDE);

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
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_M1, cfg->m1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_M0, cfg->m0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_M0_LAST_M1, cfg->m0_last_m1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_ROWS_LAST_H2, cfg->rows_last_h2);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0, cfg->c0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0_LAST_C1, cfg->c0_last_c1);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0W0, cfg->c0w0);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_C0W0_LAST_C1, cfg->c0w0_last_c1);
    recacc_reg_write(dev, RECACC_REG_IDX_PSUM_THROTTLE, cfg->psum_throttle);
    recacc_reg_write(dev, RECACC_REG_IDX_CONV_PADDING, cfg->pad_y << 8 | cfg->pad_x);
    recacc_reg_write(dev, RECACC_REG_IDX_BASE_ADDR_IACT, cfg->base_addr_iact);
    recacc_reg_write(dev, RECACC_REG_IDX_BASE_ADDR_WGHT, cfg->base_addr_wght);
    recacc_reg_write(dev, RECACC_REG_IDX_BASE_ADDR_PSUM, cfg->base_addr_psum);
    recacc_reg_write(dev, RECACC_REG_IDX_BASE_ADDR_PAD, cfg->base_addr_pad);
    recacc_reg_write(dev, RECACC_REG_IDX_STRIDE_IACT_W, cfg->stride_iact_w);
    recacc_reg_write(dev, RECACC_REG_IDX_STRIDE_IACT_HW, cfg->stride_iact_hw);
    recacc_reg_write(dev, RECACC_REG_IDX_STRIDE_WGHT_KRNL, cfg->stride_wght_krnl);
    recacc_reg_write(dev, RECACC_REG_IDX_STRIDE_WGHT_OCH, cfg->stride_wght_och);
    recacc_reg_write(dev, RECACC_REG_IDX_STRIDE_PSUM_OCH, cfg->stride_psum_och);

    if (dev->hw_revision >= 100)
        recacc_reg_write(dev, RECACC_REG_IDX_CONV_STRIDE, cfg->stride);

    // RECACC_REG_IDX_MAGIC can't be written
    return 0;
}

uint32_t recacc_reg_read(const recacc_device* dev, int regidx) {
    volatile uint32_t *ptr = (volatile uint32_t*)(dev->mem + RECACC_REG_ADDR(regidx));
    // printf("recacc_reg_read reg %d off %d ptr %p\n", regidx, RECACC_BYTE_OFFSET(regidx), ptr);
    return *ptr;
}

void recacc_reg_write(const recacc_device* dev, int regidx, uint32_t value) {
    volatile uint32_t *ptr = (volatile uint32_t*)(dev->mem + RECACC_REG_ADDR(regidx));
    // printf("recacc_reg_write reg %d off %d ptr %p val 0x%08x\n", regidx, RECACC_BYTE_OFFSET(regidx), ptr, value);
    *ptr = value;
}

recacc_status recacc_get_status(const recacc_device* dev) {
    union recacc_status_reg status;
    status.raw = recacc_reg_read(dev, RECACC_REG_IDX_STATUS);
    return status.decoded;
}

void recacc_control_start(const recacc_device* dev, bool requantize, enum activation_mode mode, bool enable_interrupt, bool enable_padding) {
    union recacc_control_reg control;
    control.raw = recacc_reg_read(dev, RECACC_REG_IDX_CONTROL);
    control.decoded.reset = 0;
    control.decoded.start = 1;
    control.decoded.requantize = requantize ? 1 : 0;
    control.decoded.activation_mode = (uint8_t) mode;
    control.decoded.irq_en = enable_interrupt ? 1 : 0;
    control.decoded.padding = enable_padding ? 1 : 0;
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, control.raw);
}

void recacc_control_stop(const recacc_device* dev) {
    uint32_t ctrl = recacc_reg_read(dev, RECACC_REG_IDX_CONTROL);
    ctrl &= ~(1 << RECACC_BIT_IDX_CONTROL_START | 1 << RECACC_BIT_IDX_CONTROL_IRQ_EN);
    recacc_reg_write(dev, RECACC_REG_IDX_CONTROL, ctrl);
}

void recacc_control_clear_irq(const recacc_device* dev) {
    recacc_reg_write(dev, RECACC_REG_IDX_STATUS, 0);
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

    tmp = recacc_reg_read(dev, RECACC_REG_IDX_ADDR_WIDTH);
    hwinfo->spad_size      = powl(2, tmp >> 0 & 0xff);
    hwinfo->spad_word_size = tmp >> 8 & 0xff;

    tmp = recacc_reg_read(dev, RECACC_REG_IDX_CAPABILITIES);
    hwinfo->max_output_channels = tmp & 0xff;
    hwinfo->trs_dataflow = tmp & (1 << RECACC_BIT_IDX_CAP_DATAFLOW);
    hwinfo->bias_requant_available = tmp & (1 << RECACC_BIT_IDX_CAP_BIAS_REQUANT);

    assert(hwinfo->max_output_channels > 0);
}

void* recacc_get_buffer(const recacc_device* dev) {
    return dev->mem + RECACC_MEM_OFFSET_SPAD;
}

// returns true if the accelerator is ready or finished
bool recacc_poll(const recacc_device* dev) {
    recacc_status status = recacc_get_status(dev);
    return status.ready || status.done;
}

#ifdef __linux__
#include <sys/select.h>
static inline bool _recacc_wait_linux(const recacc_device* dev, bool poll) {
    if (poll) {
        time_t start = time(NULL);
        while (!recacc_poll(dev)) {
            usleep(100000);
            if (time(NULL) > start)
                return false;
        }
        return true;
    } else {
        struct timeval timeout;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(dev->fd, &readfds);
        int nfds = dev->fd;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        int ret = select(nfds+1, &readfds, 0, 0, &timeout);

        if (ret < 0) {
            perror("Error while waiting for data");
            return false;
        }

        if (FD_ISSET(dev->fd, &readfds)) {
            uint32_t irq_count = 0;
            size_t cnt = read(dev->fd, &irq_count, sizeof(irq_count));
            return cnt == 4 && irq_count > 0;
        } else
            return false;
    }
}
#else
static inline bool _recacc_wait_baremetal(const recacc_device* dev, bool poll) {
    // TODO: implement with interrupts instead of polling
    int timeout = 1000;
    while (timeout && !recacc_poll(dev)) {
        for(volatile int i=0; i<10000; i++);
        timeout--;
    }
    if (timeout == 0)
        return false;
    return true;
}
#endif

bool recacc_wait(const recacc_device* dev, bool poll) {
    #ifdef __linux__
    return _recacc_wait_linux(dev, poll);
    #else
    return _recacc_wait_baremetal(dev, poll);
    #endif
}
