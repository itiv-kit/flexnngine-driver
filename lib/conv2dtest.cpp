#include "conv2dtest.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <cstring>
#include <cassert>

#include "conv2d_cpu.hpp"
#include "types.h"
#include "utils.hpp"

using namespace std;

// using timer = std::chrono::high_resolution_clock;
using timer = std::chrono::steady_clock;
using std::chrono::microseconds;

const unsigned hw_freq_mhz = 100;

// Create a test instance with default parameters for demonstration
Conv2DTest::Conv2DTest(recacc_device* accelerator) : Conv2DTest(accelerator, Conv2D(32, 3, 4, 3)) {}

Conv2DTest::Conv2DTest(recacc_device* accelerator, Conv2D operation)
    : Conv2D(operation) {
    this->dev = accelerator;

    buf_iact = nullptr;
    buf_wght = nullptr;
    buf_result_cpu = nullptr;
    buf_result_acc = nullptr;
    buf_result_files = nullptr;
    num_iact_elements = 0;
    num_wght_elements = 0;
    num_result_elements = 0;
    num_iact_elements_aligned = 0;
    num_wght_elements_aligned = 0;
    num_result_elements_aligned = 0;
    dryrun = false;
    verbose = Verbosity::Info;
    hwinfo.array_size_x = 0;
}

Conv2DTest::~Conv2DTest() {
    if (buf_iact) delete[] buf_iact;
    if (buf_wght) delete[] buf_wght;
    if (buf_result_cpu) delete[] buf_result_cpu;
    if (buf_result_acc) delete[] buf_result_acc;
    if (buf_result_files) delete[] buf_result_files;
}

void Conv2DTest::set_verbose(Verbosity level) {
    verbose = level;
}

void Conv2DTest::ensure_hwinfo() {
    if (hwinfo.array_size_x != 0)
        return;

    if (dryrun) {
        // dummy configuration for dry run
        hwinfo.array_size_x = 7;
        hwinfo.array_size_y = 10;
        hwinfo.line_length_iact = 64;
        hwinfo.line_length_wght = 64;
        hwinfo.line_length_psum = 128;
        hwinfo.spad_size_iact = 0x10000;
        hwinfo.spad_size_wght = 0x10000;
        hwinfo.spad_size_psum = 0x20000;
        hwinfo.data_width_bits_iact = 8;
        hwinfo.data_width_bits_wght = 8;
        hwinfo.data_width_bits_psum = 16;
    }

    Conv2D::ensure_hwinfo();
}

void Conv2DTest::prepare_data(bool data_from_files, const string& files_path) {
    if (verbose > Verbosity::Errors) {
        cout << "preparing conv2d data with:" << endl;
        cout << "  " << iact_w << "x" << iact_h << ", " << input_channels << " ch input activations" << endl;
        cout << "  " << wght_w << "x" << wght_h << " kernels and " << output_channels << " output channels" << endl;
    }

    num_iact_elements = iact_w * iact_h * input_channels;
    num_iact_elements_aligned = make_multiple_of(8, num_iact_elements);
    buf_iact = new int8_t[num_iact_elements_aligned];
    if (data_from_files) {
        size_t n = read_text_data<int8_t>(buf_iact, num_iact_elements, files_path + "/_image.txt");
        if (num_iact_elements != n) {
            cout << "warning: only " << n << " iact words read, " << num_iact_elements << " expected." << endl;
            memset(buf_iact + n, 0, num_iact_elements - n);
        }
    } else
        generate_random_data<int8_t>(buf_iact, num_iact_elements);
    memset(buf_iact + num_iact_elements, 0, num_iact_elements_aligned - num_iact_elements);

    num_wght_elements = wght_w * wght_h * input_channels * output_channels;
    num_wght_elements_aligned = make_multiple_of(8, num_wght_elements);
    buf_wght = new int8_t[num_wght_elements_aligned];
    if (data_from_files) {
        size_t n = read_text_data<int8_t>(buf_wght, num_wght_elements, files_path + "/_kernel_stack.txt");
        if (num_wght_elements != n) {
            cout << "warning: only " << n << " wght words read, " << num_wght_elements << " expected." << endl;
            memset(buf_wght + n, 0, num_wght_elements - n);
        }
    } else
        generate_random_data<int8_t>(buf_wght, num_wght_elements);
    memset(buf_wght + num_wght_elements, 0, num_wght_elements_aligned - num_wght_elements);

    ensure_hwinfo();

    buf_bias.resize(output_channels);
    if (bias && hwinfo.bias_requant_available)
        generate_random_data<int16_t>(buf_bias.data(), output_channels); // bias is still % 256 even though its type is larger
    else {
        if (bias)
            std::cout << "warning: non-zero bias requested but no postproc support in hardware, using zero bias" << endl;
        fill(buf_bias.begin(), buf_bias.end(), 0);
    }

    buf_scale.resize(output_channels);
    buf_zeropoint.resize(output_channels);
    if (requantize) {
        // TODO: provide dynamic scale/zeropoint values instead of fixed ones
        fill(buf_scale.begin(), buf_scale.end(), 0.0025);
        fill(buf_zeropoint.begin(), buf_zeropoint.end(), -5);

        if (verbose > Verbosity::Errors)
            cout << "  requantizing output data of ch0 with scale " << buf_scale[0] << ", zeropoint " << buf_zeropoint[0] << endl;
    } else {
        fill(buf_scale.begin(), buf_scale.end(), 1.0);
        fill(buf_zeropoint.begin(), buf_zeropoint.end(), 0.0);
    }

    output_size = (iact_w - wght_w + 1) * (iact_h - wght_h + 1); // no padding
    num_result_elements = output_size * output_channels;
    num_result_elements_aligned = make_multiple_of(8, num_result_elements);
    alloc_bytes_acc = requantize ? num_result_elements_aligned : num_result_elements_aligned * 2;
    buf_result_cpu = new int8_t[num_result_elements * 2]; // cpu always needs a full-psum buffer (first conv2d, then requantize if enabled)
    buf_result_acc = new int8_t[alloc_bytes_acc];
    // setup helper pointer to access psums when requantize is disabled
    buf_result_acc_psums = reinterpret_cast<int16_t*>(buf_result_acc);
    if (data_from_files) {
        buf_result_files = new int16_t[num_result_elements];
        size_t n = read_text_data<int16_t>(buf_result_files, num_result_elements, files_path + "/_convolution_stack.txt");
        if (num_result_elements != n)
            cout << "warning: only " << n << " result words read, " << num_result_elements << " expected." << endl;
    }

    if (verbose > Verbosity::Errors)
        cout << "using "
            << num_iact_elements * sizeof(buf_iact[0]) << " bytes iact, "
            << num_wght_elements * sizeof(buf_wght[0]) << " bytes wght, "
            << num_result_elements * sizeof(buf_result_acc[0]) << " bytes psum" << endl;

    if (num_iact_elements * sizeof(buf_iact[0]) > hwinfo.spad_size_iact)
        throw runtime_error("iact spad memory too small!");

    if (num_wght_elements * sizeof(buf_wght[0]) > hwinfo.spad_size_wght)
        throw runtime_error("wght spad memory too small!");

    if (alloc_bytes_acc > hwinfo.spad_size_psum)
        throw runtime_error("psum spad memory too small!");
}

// calculate convolution on cpu as reference
void Conv2DTest::run_cpu() {
    #ifdef __linux__
    auto t1 = timer::now();
    #endif

    conv2d_cpu<int8_t, int16_t>(buf_iact, buf_wght, buf_bias.data(), reinterpret_cast<int16_t*>(buf_result_cpu),
        input_channels, iact_w, iact_h,
        output_channels, wght_w, wght_h,
        1, 1, 0, 0);

    if (requantize)
        requantize_cpu<int16_t, int8_t>(reinterpret_cast<int16_t*>(buf_result_cpu), buf_result_cpu,
            buf_scale.data(), buf_zeropoint.data(),
            output_channels, output_size);

    #ifdef __linux__
    auto t2 = timer::now();
    duration_cpu = t2 - t1;
    #endif
}

void Conv2DTest::prepare_accelerator() {
    ensure_hwinfo();

    if (verbose > Verbosity::Errors)
        print_hwinfo(hwinfo);

    compute_accelerator_parameters();

    if (verbose > Verbosity::Errors)
        print_accelerator_parameters();

    // clear software result buffers
    memset(buf_result_acc, 0, num_result_elements_aligned * sizeof(buf_result_acc[0]));
    memset(buf_result_cpu, 0, num_result_elements * sizeof(buf_result_cpu[0]));

    if (dryrun)
        return;

    configure_accelerator();

    set_postproc_data(buf_bias, buf_scale, buf_zeropoint);

    if (verbose > Verbosity::Errors)
        cout << "copying input data to accelerator" << endl;

    #ifdef __linux__
    auto t1 = timer::now();
    #endif

    copy_data_in(buf_iact, num_iact_elements_aligned * sizeof(buf_iact[0]),
        buf_wght, num_wght_elements_aligned * sizeof(buf_wght[0]));

    #ifdef __linux__
    auto t2 = timer::now();
    duration_copy_in = t2 - t1;
    #endif

    // also clear the hardware result buffer to ease debugging
    void* psum_addr = recacc_get_buffer(dev, buffer_type::psum);
    memcpy(psum_addr, buf_result_acc, num_result_elements_aligned * sizeof(buf_result_acc[0]));
}

// start accelerator
void Conv2DTest::run_accelerator() {
    if (dryrun)
        return;

    Conv2D::run_accelerator();
}

// wait for accelerator to finish and copy data back, returns true on success
bool Conv2DTest::get_accelerator_results() {
    if (dryrun)
        return false;

    bool success = wait_until_accelerator_done();
    if (!success)
        return false;

    #ifdef __linux__
    auto t1 = timer::now();
    #endif

    copy_data_out(buf_result_acc, alloc_bytes_acc);

    #ifdef __linux__
    auto t2 = timer::now();
    duration_copy_out = t2 - t1;
    #endif

    auto cycles = get_cycle_count();
    duration_acc = 1.0us * cycles / hw_freq_mhz;

    return true;
}

// compares size words of input to reference, returns number of incorrect words
// prints verbose output if verbose is true and uses the given buffer names on output
size_t Conv2DTest::_verify_buffers(int8_t* input, int8_t* reference, const string& name_input, const string& name_reference) {
    const size_t size = num_result_elements;

    size_t incorrect, deviations, incorrect_offset;
    size_t deviations_count[3] = { 0 };
    int acceptable_delta = 0;
    if (requantize) {
        acceptable_delta = 3;
        incorrect_offset = compare_buffers<int8_t>(input, reference, size, acceptable_delta, incorrect, deviations, deviations_count);
    } else
        incorrect_offset = compare_buffers<int16_t>(reinterpret_cast<int16_t*>(input), reinterpret_cast<int16_t*>(reference), size, acceptable_delta, incorrect, deviations, nullptr);

    cout << "comparing " << size << " " << name_input << " values to " << name_reference << " reference... ";
    if (incorrect > 0)
        cout << incorrect << " values INCORRECT, first at " << incorrect_offset;
    else
        cout << "CORRECT";

    if (verbose != Verbosity::Errors && acceptable_delta > 0) {
        cout << ", " << deviations << " deviations of <= " << acceptable_delta << endl;
        for (int i = 0; i < 3; i++)
            cout << "  - " << deviations_count[i] << " deviations of " << i + 1 << endl;
    } else
        cout << endl;

    if (verbose != Verbosity::Errors && incorrect > 0) {
        size_t display_offset = incorrect_offset;
        display_offset -= display_offset % 16;
        if (display_offset > 32)
            display_offset -= 32;

        cout << name_input << " result (128 words at " << display_offset << "):" << endl;
        print_buffer<int16_t>(input, 128, display_offset, 8, incorrect_offset);
        cout << name_reference << " result from file:" << endl;
        print_buffer<int16_t>(reference, 128, display_offset, 8, incorrect_offset);
    }

    return incorrect;
}

bool Conv2DTest::verify() {
    bool success = true;

    if (buf_result_files)
        success = _verify_buffers(buf_result_cpu, reinterpret_cast<int8_t*>(buf_result_files), "CPU", "file") == 0;

    if (!dryrun)
        success = _verify_buffers(buf_result_acc, buf_result_cpu, "ACC", "CPU") == 0;
    else
        cout << "Skipping CPU/ACC comparison in dryrun" << endl;

    return success;
}

void Conv2DTest::write_data(const string& output_path) {
    if (requantize) {
        size_t written = write_text_data<int8_t>(
            buf_result_cpu,
            num_result_elements,
            iact_w - wght_w + 1,
            output_path + "/_output_cpu.txt");
        if (written != num_result_elements)
            cerr << "wrote only " << written << " elements to _output_cpu.txt" << endl;

        written = write_text_data<int8_t>(
            buf_result_acc,
            num_result_elements,
            iact_w - wght_w + 1,
            output_path + "/_output_acc.txt");
        if (written != num_result_elements)
            cerr << "wrote only " << written << " elements to _output_acc.txt" << endl;
    } else {
        size_t written = write_text_data<int16_t>(
            reinterpret_cast<int16_t*>(buf_result_cpu),
            num_result_elements,
            iact_w - wght_w + 1,
            output_path + "/_output_cpu.txt");
        if (written != num_result_elements)
            cerr << "wrote only " << written << " elements to _output_cpu.txt" << endl;

        written = write_text_data<int16_t>(
            buf_result_acc_psums,
            num_result_elements,
            iact_w - wght_w + 1,
            output_path + "/_output_acc.txt");
        if (written != num_result_elements)
            cerr << "wrote only " << written << " elements to _output_acc.txt" << endl;
    }
}

void Conv2DTest::set_dryrun(bool enabled) {
    dryrun = enabled;
}

void Conv2DTest::test_print_buffer() {
    uint32_t buf[] = {
        0xdeadbeef,
        0xdeadbeef,
        0xdeadbeef,
        0xdeadbeef,
        0x11223344,
        0x55667788,
        0xaabbccdd,
        0x00000011
    };
    print_buffer<int8_t>(buf, 16, 16, 8, 8);
    print_buffer<int16_t>(buf, 8,  8, 8, 8);
    print_buffer<int32_t>(buf, 4,  4, 8, 8);
}

void Conv2DTest::set_bias(bool enabled) {
    bias = enabled;
}
