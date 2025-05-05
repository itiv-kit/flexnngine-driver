#pragma once

#include "types.h"
#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

extern "C" {
    #include <driver.h>
}

// represents a 2D convolution operation with a set of parameters

class Conv2D {
public:
    Conv2D();
    Conv2D(unsigned image_size,
        unsigned kernel_size,
        unsigned input_channels,
        unsigned output_channels,
        bool requantize = false);
    ~Conv2D();

    void set_image_size(unsigned w, unsigned h);
    void set_kernel_size(unsigned w, unsigned h);
    void set_channel_count(unsigned input_channels, unsigned output_channels);
    void set_activation_mode(enum activation_mode mode);
    virtual void set_requantize(bool enabled);
    void set_hwinfo(const recacc_hwinfo& hwinfo);
    void set_recacc_device(recacc_device* dev);
    void use_interrupts(bool enabled);

    std::tuple<unsigned, unsigned> get_image_size() const;
    std::tuple<unsigned, unsigned> get_kernel_size() const;
    std::tuple<unsigned, unsigned> get_channel_count() const;
    std::string get_parameter_string() const;
    unsigned get_cycle_count() const;
    bool get_requantize() const;
    enum activation_mode get_activation_mode() const;

    void allocate_spad_auto();
    std::tuple<unsigned, unsigned, unsigned> get_buffer_offsets() const;
    void set_buffer_offsets(unsigned offset_iact, unsigned offset_wght, unsigned offset_psum);

    void compute_accelerator_parameters(bool fixup_channel_alignment = true);
    void print_accelerator_parameters();

    void copy_data_in(void* iact_buf, size_t iact_bytes, void* wght_buf, size_t wght_bytes);
    void set_postproc_data(const std::vector<int16_t>& bias, const std::vector<float>& factors, const std::vector<float>& zeropoints);
    void configure_accelerator();
    void run_accelerator();
    bool wait_until_accelerator_done();
    void copy_data_out(void* psum_buf, size_t psum_bytes);
    bool validate_hw_state();

protected:
    void ensure_hwinfo();
    size_t _copy_in_columnwise(int8_t* dst, size_t stride_size, int8_t* buf, size_t bytes_avail, bool zeropad = true);

    unsigned iact_w = 32;
    unsigned iact_h = 32;
    unsigned wght_w = 3;
    unsigned wght_h = 3;
    unsigned input_channels = 4;
    unsigned output_channels = 3;
    unsigned dummy_channels = 3;
    unsigned cycles = 0;
    bool requantize = false;
    bool use_irq = false;
    enum activation_mode act_mode = act_none;

    unsigned base_iact = 0;
    unsigned base_wght = 0;
    unsigned base_psum = 0;
    unsigned alloc_size_iact = 0;
    unsigned alloc_size_wght = 0;
    unsigned alloc_size_psum = 0;
    unsigned spad_column_stride = 0;
    unsigned channels_per_column = 0;
    unsigned bytes_per_channel = 0;
    unsigned bytes_per_kernel = 0;
    unsigned bytes_per_output_channel = 0;

    recacc_device* dev;
    recacc_hwinfo hwinfo;
    recacc_config cfg;
};
