#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

#include "conv2d.hpp"

extern "C" {
    #include <driver.h>
}

class Conv2DTest : public Conv2D {
public:
    Conv2DTest(recacc_device* accelerator);
    Conv2DTest(recacc_device* accelerator, Conv2D operation);
    ~Conv2DTest();

    enum class Verbosity {Errors, Info, Debug};

    void set_dryrun(bool enabled);
    void set_verbose(Verbosity level);
    void set_bias(bool enabled);
    void prepare_data(bool data_from_files, const std::string& files_path);
    void prepare_accelerator();
    void run_accelerator();
    void run_cpu();
    bool get_accelerator_results();
    bool verify();
    void write_data(const std::string& output_path);

    void test_print_buffer();

    std::chrono::duration<float, std::micro> duration_cpu;
    std::chrono::duration<float, std::micro> duration_acc;
    std::chrono::duration<float, std::micro> duration_copy_in;
    std::chrono::duration<float, std::micro> duration_copy_out;

private:
    void ensure_hwinfo();
    size_t _verify_buffers(int8_t* input, int8_t* reference, const std::string& name_input, const std::string& name_reference);

    size_t num_iact_elements;
    size_t num_wght_elements;
    size_t num_result_elements;
    size_t num_iact_elements_aligned;
    size_t num_wght_elements_aligned;
    size_t num_result_elements_aligned;
    size_t alloc_bytes_acc;
    int    output_size;

    int8_t*  buf_iact;
    int8_t*  buf_wght;
    int8_t* buf_result_cpu;
    int8_t* buf_result_acc;
    int16_t* buf_result_acc_psums;
    int16_t* buf_result_files;

    std::vector<int16_t> buf_bias;
    std::vector<float>   buf_scale;
    std::vector<float>   buf_zeropoint;

    bool bias;
    bool dryrun;
    Verbosity verbose;
};
