#include "conv2d.hpp"

#include <cstdint>
#include <iostream>
#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

using namespace std;

Conv2D::Conv2D() {
    hwinfo.array_size_x = 0;
    dev = nullptr;
};

Conv2D::~Conv2D() {}

Conv2D::Conv2D(unsigned image_size,
        unsigned kernel_size,
        unsigned input_channels,
        unsigned output_channels,
        bool requantize) : Conv2D() {
    set_image_size(image_size, image_size);
    set_kernel_size(kernel_size, kernel_size);
    set_channel_count(input_channels, output_channels);
    set_requantize(requantize);
    set_activation_mode(act_none);
}

void Conv2D::set_image_size(unsigned w, unsigned h) {
    iact_w = w;
    iact_h = h;
}

void Conv2D::set_kernel_size(unsigned w, unsigned h) {
    wght_w = w;
    wght_h = h;
}

void Conv2D::set_channel_count(unsigned input_channels, unsigned output_channels) {
    this->input_channels = input_channels;
    this->output_channels = output_channels;
}

void Conv2D::set_activation_mode(enum activation_mode mode) {
    act_mode = mode;
}

void Conv2D::set_requantize(bool enabled) {
    requantize = enabled;
}

void Conv2D::set_hwinfo(const recacc_hwinfo& hwinfo) {
    this->hwinfo = hwinfo;
}

void Conv2D::set_recacc_device(recacc_device* dev) {
    this->dev = dev;
}

std::tuple<unsigned, unsigned> Conv2D::get_image_size() {
    return {iact_w, iact_h};
}

std::tuple<unsigned, unsigned> Conv2D::get_kernel_size() {
    return {wght_w, wght_h};
}

std::tuple<unsigned, unsigned> Conv2D::get_channel_count() {
    return {input_channels, output_channels};
}

void Conv2D::compute_accelerator_parameters() {
    assert(iact_w > 0);
    assert(wght_w > 0);
    assert(input_channels > 0);
    assert(output_channels > 0);
    assert(hwinfo.array_size_x > 0);

    assert(iact_w == iact_h);
    const int image_size = iact_w;
    assert(wght_w == wght_h);
    const int kernel_size = wght_w;

    cfg.iact_dimension = image_size;
    cfg.wght_dimension = kernel_size;
    cfg.input_channels = input_channels;
    cfg.output_channels = output_channels;

    // currently only stride = 1 is implemented in software
    cfg.stride = 1;

    int line_length_wght_usable = hwinfo.line_length_wght - 1;

    // m0 is how many kernels are mapped at once (vertically)
    cfg.m0 = floor((float)hwinfo.array_size_y / kernel_size);
    // h1 is how many image rows are processed at once
    // for RS dataflow, each accelerator column processes one input image row
    // value is not needed, so maybe remove it some day
    // int h1 = hwinfo.array_size_x;

    cfg.w1 = image_size - kernel_size + 1;
    cfg.m0_last_m1 = 1; // not used yet

    int size_rows = hwinfo.array_size_x + hwinfo.array_size_y - 1;
    // TODO: check case for M0 = 0. IMHO the else path is incorrect, as H2 is the number of iterations for mapping
    // each set of rows (accelerator.size_x rows) to the pe array.
    // h2 is how many iterations with one set of m0 kernels are required to process all image rows
    if (cfg.m0 == 0)
        cfg.h2 = ceil((float)(image_size - kernel_size + 1) / hwinfo.array_size_x);
    else
        cfg.h2 = ceil((float)image_size / hwinfo.array_size_x);

    cfg.c1 = ceil((float)input_channels * kernel_size / line_length_wght_usable);
    cfg.c0 = floor((float)input_channels / cfg.c1);

    cfg.c0_last_c1 = input_channels - (cfg.c1 - 1) * cfg.c0;
    cfg.rows_last_h2 = 1; // not required for dataflow 0;
    cfg.c0w0 = cfg.c0 * kernel_size;
    cfg.c0w0_last_c1 = cfg.c0_last_c1 * kernel_size;

    cfg.c1 = ceil((float)input_channels / cfg.c0);
    cfg.c0_last_c1 = input_channels - (cfg.c1 - 1) * cfg.c0;
    cfg.c0w0 = cfg.c0 * kernel_size;
    cfg.c0w0_last_c1 = cfg.c0_last_c1 * kernel_size;

    // C0W0 must not be too short to allow for disabling of PE array while reading data
    if (cfg.c0w0_last_c1 < 6) {
        cfg.c1 = cfg.c1 - 1;
        cfg.c0_last_c1 = input_channels - (cfg.c1 - 1) * cfg.c0;
        cfg.c0w0 = cfg.c0 * kernel_size;
        cfg.c0w0_last_c1 = cfg.c0_last_c1 * kernel_size;
    }

    if (cfg.c0w0 * (cfg.c1 - 1) + cfg.c0w0_last_c1 != input_channels * kernel_size) {
        cout << "h2 = " << cfg.h2 << endl;
        cout << "rows_last_h2 = " << cfg.rows_last_h2 << endl;
        cout << "c1 = " << cfg.c1 << endl;
        cout << "c0 = " << cfg.c0 << endl;
        cout << "c0_last_c1 = " << cfg.c0_last_c1 << endl;
        cout << "c0w0 = " << cfg.c0w0 << endl;
        cout << "c0w0_last_c1 = " << cfg.c0w0_last_c1 << endl;
        throw runtime_error("BUG: mismatch of calculated accelerator parameters");
    }

    if (cfg.c0w0_last_c1 < 6) {
        cout << "h2 = " << cfg.h2 << endl;
        cout << "rows_last_h2 = " << cfg.rows_last_h2 << endl;
        cout << "c1 = " << cfg.c1 << endl;
        cout << "c0 = " << cfg.c0 << endl;
        cout << "c0_last_c1 = " << cfg.c0_last_c1 << endl;
        cout << "c0w0 = " << cfg.c0w0 << endl;
        cout << "c0w0_last_c1 = " << cfg.c0w0_last_c1 << endl;
        throw runtime_error("BUG: mismatch of calculated accelerator parameters");
    }

    if (cfg.c0w0_last_c1 >= hwinfo.line_length_wght) {
        cout << "h2 = " << cfg.h2 << endl;
        cout << "rows_last_h2 = " << cfg.rows_last_h2 << endl;
        cout << "c1 = " << cfg.c1 << endl;
        cout << "c0 = " << cfg.c0 << endl;
        cout << "c0_last_c1 = " << cfg.c0_last_c1 << endl;
        cout << "c0w0 = " << cfg.c0w0 << endl;
        cout << "c0w0_last_c1 = " << cfg.c0w0_last_c1 << endl;
        throw runtime_error("BUG: mismatch of calculated accelerator parameters");
    }

    if (cfg.m0 != output_channels)
        cout << "WARNING: " << output_channels << " output channels requested, but have to map " << cfg.m0 << " output channels." << endl;
}

void Conv2D::print_accelerator_parameters() {
    cout << "Accelerator run parameters:" << endl;
    cout << "  iact_dimension  " << cfg.iact_dimension << endl;
    cout << "  wght_dimension  " << cfg.wght_dimension << endl;
    cout << "  input_channels  " << cfg.input_channels << endl;
    cout << "  output_channels " << cfg.output_channels << endl;
    cout << "  c1              " << cfg.c1 << endl;
    cout << "  w1              " << cfg.w1 << endl;
    cout << "  h2              " << cfg.h2 << endl;
    cout << "  m0              " << cfg.m0 << endl;
    cout << "  m0_last_m1      " << cfg.m0_last_m1 << endl;
    cout << "  rows_last_h2    " << cfg.rows_last_h2 << endl;
    cout << "  c0              " << cfg.c0 << endl;
    cout << "  c0_last_c1      " << cfg.c0_last_c1 << endl;
    cout << "  c0w0            " << cfg.c0w0 << endl;
    cout << "  c0w0_last_c1    " << cfg.c0w0_last_c1 << endl;
}

void Conv2D::copy_data_in(void* iact_buf, size_t iact_bytes, void* wght_buf, size_t wght_bytes) {
    void* iact_addr = recacc_get_buffer(dev, buffer_type::iact);
    memcpy(iact_addr, iact_buf, iact_bytes);

    void* wght_addr = recacc_get_buffer(dev, buffer_type::wght);
    memcpy(wght_addr, wght_buf, wght_bytes);

    ensure_hwinfo();

    if (iact_bytes > hwinfo.spad_size_iact)
        throw runtime_error("iact spad memory too small!");

    if (wght_bytes > hwinfo.spad_size_wght)
        throw runtime_error("wght spad memory too small!");
}

void Conv2D::set_postproc_data(uint32_t* bias, float* factors, float* zeropoints) {
    if (!requantize && (factors != nullptr || zeropoints != nullptr))
        throw runtime_error("setting postproc data for factors/zeropoints but requantization is disabled");

    ensure_hwinfo();

    cout << "writing " << hwinfo.max_output_channels << " bias/scale regs" << endl;

    for (size_t n = 0; n < hwinfo.max_output_channels; n++) {
        unsigned idx = RECACC_REG_IDX_BIAS_REQUANT_BASE + n;

        uint32_t value = 0;
        if (bias != nullptr && n < output_channels)
            value = bias[n];
        recacc_reg_write(dev, idx, value);

        idx += hwinfo.max_output_channels;
        value = 0;
        if (factors != nullptr && n < output_channels)
            value = reinterpret_cast<uint32_t*>(factors)[n];
        recacc_reg_write(dev, idx, value);

        idx += hwinfo.max_output_channels;
        value = 0;
        if (zeropoints != nullptr && n < output_channels)
            value = reinterpret_cast<uint32_t*>(zeropoints)[n];
        recacc_reg_write(dev, idx, value);
    }
}

void Conv2D::configure_accelerator() {
    recacc_config_write(dev, &cfg);
}

void Conv2D::ensure_hwinfo() {
    if (hwinfo.array_size_x != 0)
        return;

    recacc_get_hwinfo(dev, &hwinfo);
    assert(hwinfo.array_size_x != 0);
}

string Conv2D::get_parameter_string() {
    ostringstream oss;
    oss << iact_w << "x" << iact_h << ", ";
    oss << wght_w << "x" << wght_h << ", ";
    oss << input_channels << " input channels, ";
    oss << output_channels << " output channels";
    return oss.str();
}

unsigned Conv2D::get_cycle_count() {
    return cycles;
}

void Conv2D::run_accelerator() {
    recacc_control_start(dev, requantize, act_mode);
}

// wait for accelerator to finish and copy data back, returns true on success
bool Conv2D::wait_until_accelerator_done() {
    // wait for accelerator to finish
    bool success = recacc_wait(dev);
    if (!success) {
        cerr << "ERROR: timeout waiting for hardware, probably stuck!" << endl;
        return false;
    }

    // read cycle count and store for later use before it gets invalidated
    cycles = recacc_reg_read(dev, RECACC_REG_IDX_CYCLE_COUNTER);

    // read diagnostic registers and validate hardware status after processing
    uint32_t psum_overflows = recacc_reg_read(dev, RECACC_REG_IDX_PSUM_OVERFLOWS);
    if (psum_overflows)
        cerr << "WARNING: psum overflows in hardware (" << psum_overflows << "), results may be invalid." << endl;

    return true;
}

void Conv2D::copy_data_out(void* psum_buf, size_t psum_bytes) {
    // const size_t output_size = (iact_w - wght_w + 1) * (iact_h - wght_h + 1);
    // const size_t expected_bytes = output_size * output_channels * 2;

    // size_t length = expected_bytes;
    // if (max_size < length)
    //     length = max_size;

    if (psum_bytes > hwinfo.spad_size_psum)
        throw runtime_error("copy data out larger than scratchpad size");

    void* psum_addr = recacc_get_buffer(dev, buffer_type::psum);
    memcpy(psum_buf, psum_addr, psum_bytes);

    // deassert start bit, this resets the control logic and allows for starting the next iteration
    recacc_control_stop(dev);
}
