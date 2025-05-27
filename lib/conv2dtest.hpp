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
    void set_debug_clean_buffers(bool enabled);
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
    size_t _verify_buffers(input_t* input, input_t* reference, const std::string& name_input, const std::string& name_reference);

    size_t num_iact_elements;
    size_t num_wght_elements;
    size_t num_result_elements;
    size_t num_iact_elements_aligned;
    size_t num_wght_elements_aligned;
    size_t num_result_elements_aligned;
    size_t alloc_bytes_acc;
    int    output_size;

    input_t* buf_iact;
    input_t* buf_wght;
    input_t* buf_result_cpu;
    input_t* buf_result_acc;
    psum_t*  buf_result_cpu_psums;
    psum_t*  buf_result_acc_psums;
    input_t* buf_result_files;
    psum_t*  buf_result_files_psums;

    std::vector<psum_t> buf_bias;
    std::vector<float>  buf_scale;
    std::vector<float>  buf_zeropoint;

    bool bias;
    bool dryrun;
    bool debug_clean_buffers;
    Verbosity verbose;
};
