#pragma once

#include <cstdint>
#include <string>

#include "conv2d.hpp"

extern "C" {
    #include <driver.h>
}

class Conv2DTest : public Conv2D {
public:
    Conv2DTest(recacc_device* accelerator);
    Conv2DTest(recacc_device* accelerator, Conv2D operation);
    ~Conv2DTest();

    void set_dryrun(bool enabled);
    void prepare_data(bool data_from_files, const std::string& files_path);
    void prepare_accelerator();
    void run_accelerator();
    void run_cpu();
    bool get_accelerator_results();
    bool verify(bool verbose = true);
    void write_data(const std::string& output_path);

    void test_print_buffer();

private:
    void ensure_hwinfo();
    static bool _verify_buffers(int16_t* input, int16_t* reference, size_t size, bool verbose, const std::string& name_input, const std::string& name_reference);

    size_t num_iact_elements;
    size_t num_wght_elements;
    size_t num_result_elements;
    size_t num_iact_elements_aligned;
    size_t num_wght_elements_aligned;
    size_t num_result_elements_aligned;

    int8_t* buf_iact;
    int8_t* buf_wght;
    int16_t* buf_result_cpu;
    int16_t* buf_result_acc;
    int16_t* buf_result_files;

    bool dryrun;
};
