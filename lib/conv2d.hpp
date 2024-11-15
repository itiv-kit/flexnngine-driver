#pragma once

#include "types.h"
#include <cstddef>
#include <string>
#include <tuple>

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

    std::tuple<unsigned, unsigned> get_image_size();
    std::tuple<unsigned, unsigned> get_kernel_size();
    std::tuple<unsigned, unsigned> get_channel_count();
    std::string get_parameter_string();
    unsigned get_cycle_count();

    void compute_accelerator_parameters();
    void print_accelerator_parameters();

    void copy_data_in(void* iact_buf, size_t iact_bytes, void* wght_buf, size_t wght_bytes);
    void set_postproc_data(uint32_t* bias, float* factors, float* zeropoints);
    void configure_accelerator();
    void run_accelerator();
    bool wait_until_accelerator_done();
    void copy_data_out(void* psum_buf, size_t psum_bytes);

protected:
    void ensure_hwinfo();

    unsigned iact_w = 32;
    unsigned iact_h = 32;
    unsigned wght_w = 3;
    unsigned wght_h = 3;
    unsigned input_channels = 4;
    unsigned output_channels = 3;
    unsigned cycles = 0;
    bool requantize = false;
    enum activation_mode act_mode = act_none;

    recacc_device* dev;
    recacc_hwinfo hwinfo;
    recacc_config cfg;
};
