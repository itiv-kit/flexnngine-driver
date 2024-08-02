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
        unsigned output_channels);
    ~Conv2D();

    void set_image_size(unsigned w, unsigned h);
    void set_kernel_size(unsigned w, unsigned h);
    void set_channel_count(unsigned input_channels, unsigned output_channels);
    void set_hwinfo(const recacc_hwinfo& hwinfo);
    void set_recacc_device(recacc_device* dev);

    std::tuple<unsigned, unsigned> get_image_size();
    std::tuple<unsigned, unsigned> get_kernel_size();
    std::tuple<unsigned, unsigned> get_channel_count();
    std::string get_parameter_string();

    void compute_accelerator_parameters();
    void print_accelerator_parameters();

    // void set_dryrun(bool enabled);
    // void prepare_data(bool data_from_files, const std::string& files_path);
    // void prepare_accelerator();
    void copy_data_in(void* iact_buf, size_t iact_size, void* wght_buf, size_t wght_size);
    void configure_accelerator();
    void run_accelerator();
    // void run_cpu();
    bool wait_until_accelerator_done();
    void copy_data_out(void* psum_buf, size_t length);
    // void verify();
    // void write_data(const std::string& output_path);

    // void test_print_buffer();

protected:
    void ensure_hwinfo();

    unsigned iact_w = 32;
    unsigned iact_h = 32;
    unsigned wght_w = 3;
    unsigned wght_h = 3;
    unsigned input_channels = 4;
    unsigned output_channels = 3;

    recacc_device* dev;
    recacc_hwinfo hwinfo;
    recacc_config cfg;
};
