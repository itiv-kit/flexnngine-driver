#include "conv2d.hpp"

#include <cstdint>
#include <iostream>
#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "utils.hpp"

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
    dummy_channels = 0;
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

void Conv2D::set_recacc_device(const recacc_device* dev) {
    this->dev = dev;
}

void Conv2D::use_interrupts(bool enabled) {
    use_irq = enabled;
}

void Conv2D::set_padding_mode(bool enable_same_size_padding) {
    padding = enable_same_size_padding;
}

std::tuple<unsigned, unsigned> Conv2D::get_image_size() const {
    return {iact_w, iact_h};
}

std::tuple<unsigned, unsigned> Conv2D::get_kernel_size() const {
    return {wght_w, wght_h};
}

std::tuple<unsigned, unsigned> Conv2D::get_channel_count() const {
    return {input_channels, output_channels};
}

bool Conv2D::get_padding_mode() const {
    return padding;
}
bool Conv2D::get_requantize() const {
    return requantize;
}

enum activation_mode Conv2D::get_activation_mode() const {
    return act_mode;
}

void Conv2D::compute_accelerator_parameters(bool fixup_channel_alignment) {
    ensure_hwinfo();

    assert(iact_w > 0);
    assert(wght_w > 0);
    assert(iact_w == iact_h);
    assert(wght_w == wght_h);
    assert(input_channels > 0);
    assert(output_channels > 0);
    assert(hwinfo.array_size_x > 0);
    assert(base_iact != base_wght);
    assert(base_iact != base_psum);

    // fixup by default. this could change in the future.
    if (fixup_channel_alignment) {
        uint32_t align_to = hwinfo.spad_word_size;
        dummy_channels = make_multiple_of(align_to, input_channels) - input_channels;
        if (dummy_channels)
            cerr << "WARNING: adding " << dummy_channels << " dummy channels to align to scratchpad layout" << endl;
    } else {
        dummy_channels = 0;
    }

    cfg.iact_dimension = iact_w; // equals iact_h, for now only square images are supported
    cfg.wght_dimension = wght_w; // equals wght_h, for now only square kernels are supported
    cfg.input_channels = input_channels + dummy_channels;
    cfg.output_channels = output_channels;
    cfg.base_addr_iact = base_iact;
    cfg.base_addr_wght = base_wght;
    cfg.base_addr_psum = base_psum;
    cfg.base_addr_pad = base_padding;
    cfg.stride_iact_w = ceil(1.0 * iact_w / hwinfo.spad_word_size);
    cfg.stride_iact_hw = ceil(1.0 * iact_w * iact_h / hwinfo.spad_word_size);;
    cfg.stride_wght_krnl = bytes_per_kernel;
    // align offset between output channel kernel sets for easy copy
    cfg.stride_wght_och = make_multiple_of(hwinfo.spad_word_size,
        lround(ceil(1.0 * bytes_per_kernel * cfg.input_channels / hwinfo.spad_word_size)));
    cfg.stride_psum_och = ceil(1.0 * bytes_per_output_channel / hwinfo.spad_word_size);

    // check alignment to number of scratchpad columns
    assert(cfg.input_channels % hwinfo.spad_word_size == 0);

    // currently only stride = 1 is implemented in software
    cfg.stride = 1;

    int line_length_wght_usable = hwinfo.line_length_wght - 1;

    // m0 is how many kernels are mapped at once (vertically)
    cfg.m0 = floor(1.0 * hwinfo.array_size_y / wght_h);
    cfg.m1 = ceil(1.0 * output_channels / cfg.m0);
    cfg.m0_last_m1 = output_channels - (cfg.m1 - 1) * cfg.m0;
    // h1 is how many image rows are processed at once
    // for RS dataflow, each accelerator column processes one input image row
    // int h1 = hwinfo.array_size_x;

    // only symmetric same-size padding for now. hardware can do anything from 0..kernel_size-1 on each edge
    if (padding) {
        cfg.w1 = iact_w;
        cfg.pad_x = cfg.pad_y = (wght_w - 1) / 2;
    } else {
        cfg.w1 = iact_w - wght_w + 1;
        cfg.pad_x = cfg.pad_y = 0;
    }
    cfg.rows_last_h2 = 1; // not required for dataflow 0;

    int size_rows = hwinfo.array_size_x + hwinfo.array_size_y - 1;
    // TODO: check case for M0 = 0. IMHO the else path is incorrect, as H2 is the number of iterations for mapping
    // each set of rows (accelerator.size_x rows) to the pe array.
    // h2 is how many iterations with one set of m0 kernels are required to process all image rows
    if (cfg.m0 == 0)
        cfg.h2 = ceil(1.0 * (iact_h - wght_h + 1) / hwinfo.array_size_x);
    else
        cfg.h2 = ceil(1.0 * iact_h / hwinfo.array_size_x);

    cfg.c0 = min(cfg.input_channels, static_cast<uint16_t>(floor(1.0 * line_length_wght_usable / wght_w / hwinfo.spad_word_size) * hwinfo.spad_word_size));
    cfg.c1 = ceil(1.0 * cfg.input_channels / cfg.c0);

    cfg.c0_last_c1 = cfg.input_channels - (cfg.c1 - 1) * cfg.c0;
    cfg.c0w0 = cfg.c0 * wght_w;
    cfg.c0w0_last_c1 = cfg.c0_last_c1 * wght_w;

    // TODO: calculate psum throttle when implementing TRS dataflow
    cfg.psum_throttle = 0;

    // C0W0 must not be too short to allow for disabling of PE array while reading data
    if (cfg.c0w0_last_c1 < 6) {
        cfg.c1 = cfg.c1 - 1;
        cfg.c0_last_c1 = cfg.input_channels - (cfg.c1 - 1) * cfg.c0;
        cfg.c0w0 = cfg.c0 * wght_w;
        cfg.c0w0_last_c1 = cfg.c0_last_c1 * wght_w;
    }

    if (cfg.c0w0 * (cfg.c1 - 1) + cfg.c0w0_last_c1 != cfg.input_channels * wght_w) {
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

    if (cfg.m0 != output_channels) {
        cout << "WARNING: " << output_channels << " output channels requested, but have to map " << cfg.m0 << " output channels." << endl;
        // TODO: allow more than m0 output channels -> implement m1 tiling, maybe in software?
        output_channels = cfg.output_channels = cfg.m0;
    }
}

void Conv2D::print_accelerator_parameters() {
    cout << "Accelerator run parameters:" << endl;
    cout << "  iact_dimension   " << cfg.iact_dimension << endl;
    cout << "  wght_dimension   " << (int)cfg.wght_dimension << endl;
    cout << "  input_channels   " << cfg.input_channels << endl;
    cout << "  output_channels  " << cfg.output_channels << endl;
    cout << "  c1               " << cfg.c1 << endl;
    cout << "  w1               " << cfg.w1 << endl;
    cout << "  h2               " << cfg.h2 << endl;
    cout << "  m0               " << cfg.m0 << endl;
    cout << "  m0_last_m1       " << cfg.m0_last_m1 << endl;
    cout << "  rows_last_h2     " << cfg.rows_last_h2 << endl;
    cout << "  c0               " << cfg.c0 << endl;
    cout << "  c0_last_c1       " << cfg.c0_last_c1 << endl;
    cout << "  c0w0             " << cfg.c0w0 << endl;
    cout << "  c0w0_last_c1     " << cfg.c0w0_last_c1 << endl;
    cout << "  psum_throttle    " << (int)cfg.psum_throttle << endl;
    cout << "  pad_x/pad_y      " << (int)cfg.pad_x << "/" << (int)cfg.pad_y << endl;
    cout << "  strd iact_w      " << cfg.stride_iact_w << endl;
    cout << "  strd iact_hw     " << cfg.stride_iact_hw << endl;
    cout << "  strd wght_krnl   " << (int)cfg.stride_wght_krnl << endl;
    cout << "  strd wght_och    " << cfg.stride_wght_och << endl;
    cout << "  strd psum_och    " << cfg.stride_psum_och << endl;

    if (dummy_channels)
        cout << "  dummy_channels   " << dummy_channels << " (align to scratchpad layout)" << endl;
}

// this function is a simple greedy memory allocator and just places iact, wght, psum after each other
void Conv2D::allocate_spad_auto() {
    ensure_hwinfo();

    bytes_per_channel = iact_h * iact_w;
    bytes_per_kernel = wght_h * wght_w;

    if (padding)
        bytes_per_output_channel = iact_w * iact_h;
    else
        bytes_per_output_channel = (iact_w - wght_w + 1) * (iact_h - wght_h + 1);

    if (requantize)
        bytes_per_output_channel *= pow(2, ceil(log2(hwinfo.data_width_bits_iact)) - 3);
    else
        bytes_per_output_channel *= pow(2, ceil(log2(hwinfo.data_width_bits_psum)) - 3);

    spad_column_stride = hwinfo.spad_size / hwinfo.spad_word_size;
    channels_per_column = ceil(1.0 * input_channels / hwinfo.spad_word_size);

    unsigned size_iact = channels_per_column * bytes_per_channel;
    unsigned size_kernel_set = channels_per_column * bytes_per_kernel;
    unsigned alloc_size_kernel_set = make_multiple_of(8, size_kernel_set);
    unsigned size_wght = output_channels * alloc_size_kernel_set;

    // place iact at scratchpad start
    base_iact = 0;

    // place wght directly after iact, aligned to 32 bytes
    // should actually use cfg.m0 here, but allocate_spad_auto is usually called before compute_accelerator_parameters. change when supporting more than m0 ochs.
    base_wght = make_multiple_of(32, size_iact);

    base_padding = 0;
    // if padding is enabled, a row of zeros if required:
    // 1) try to fit padding bytes between iact and wght
    // 2) try to fit it between kernel sets for different output channels (succeeds in most cases)
    // 3) try to fit after kernels and before psum
    // 4) move psum start to make space for padding bytes
    if (padding) {
        if (size_iact < base_wght)
            base_padding = size_iact;
        else if (size_kernel_set < alloc_size_kernel_set)
            base_padding = base_wght + size_kernel_set;
    }

    // place psum directly after wght, aligned to 32 bytes
    base_psum = make_multiple_of(32, base_wght + size_wght);

    if (padding) {
        if (base_padding == 0 && base_wght + size_wght < base_psum)
            base_padding = base_wght + size_wght;
        else {
            base_padding = base_psum;
            base_psum += 32;
        }
    }

    // calculate total allocated memory size per data type in bytes
    alloc_size_iact = base_wght * hwinfo.spad_word_size;
    alloc_size_wght = (base_psum - base_wght) * hwinfo.spad_word_size;
    alloc_size_psum = hwinfo.spad_size - alloc_size_iact - alloc_size_wght;

    // preliminary sanity checks, iact and wght should be fine
    if (base_iact >= spad_column_stride || alloc_size_iact < bytes_per_channel * input_channels)
        throw runtime_error("spad allocation size too small for iact data!");

    if (base_wght >= spad_column_stride || alloc_size_wght < bytes_per_kernel * input_channels * output_channels)
        throw runtime_error("spad allocation size too small for wght data!");

    if (base_psum >= spad_column_stride || bytes_per_output_channel >= spad_column_stride - base_psum)
        throw runtime_error("spad allocation size too small for psum data!");
}

std::tuple<unsigned, unsigned, unsigned, unsigned> Conv2D::get_buffer_offsets() const {
    return {base_iact, base_wght, base_psum, base_padding};
}

void Conv2D::set_buffer_offsets(unsigned offset_iact, unsigned offset_wght, unsigned offset_psum, unsigned offset_padding) {
    base_iact = offset_iact;
    base_wght = offset_wght;
    base_psum = offset_psum;
    base_padding = offset_padding;
}

// consumes at most bytes_avail bytes from buf, returns number of remaining bytes in buf
size_t Conv2D::_copy_in_columnwise(input_t* dst, size_t stride_size, const input_t* buf, size_t bytes_avail, bool zeropad) {
    for (unsigned col = 0; col < hwinfo.spad_word_size; col++) {
        // global channels_per_column can be used for both iact and wght channel count per column
        size_t col_bytes = channels_per_column * stride_size;

        // try to copy all data for this column from the input buffer
        size_t col_bytes_buf = col_bytes;

        // don't copy all channels if dummies are used, if so reduce by one channel
        if (col >= hwinfo.spad_word_size - dummy_channels)
            col_bytes_buf -= stride_size;

        if (col_bytes_buf > bytes_avail)
            col_bytes_buf = bytes_avail;

        if (col_bytes_buf) {
            // cout << "col " << col << " copy " << col_bytes_buf << " bytes to " << (void*)dst << endl;
            // align copy to multiples of spad_word_size, byte-wise access may be illegal
            size_t col_bytes_buf_aligned = make_multiple_of(hwinfo.spad_word_size, col_bytes_buf);
            if (bytes_avail >= col_bytes_buf_aligned)
                copy(buf, buf + col_bytes_buf_aligned, dst);
            else {
                // make a temporary copy if the buffer is too small
                input_t tmp[col_bytes_buf_aligned];
                copy(buf, buf + col_bytes_buf, tmp);
                fill(tmp + col_bytes_buf, tmp + col_bytes_buf_aligned, 0);
                copy(tmp, tmp + col_bytes_buf_aligned, dst);
            }
            buf += col_bytes_buf;
            bytes_avail -= col_bytes_buf;
            col_bytes -= col_bytes_buf;
        }

        // if input buffer is insufficient, pad with zeros (happens when dummy_channels > 0 or insufficient data provided by caller)
        if (zeropad && col_bytes) {
            // cout << "col " << col << " zero " << col_bytes << " bytes at " << (void*)(dst + col_bytes_buf) << endl;
            fill(dst + col_bytes_buf, dst + col_bytes_buf + col_bytes, 0);
        }

        // advance to next column
        dst += spad_column_stride;
    }

    return bytes_avail;
}

void Conv2D::copy_data_in(const void* iact_buf, size_t iact_bytes, const void* wght_buf, size_t wght_bytes) {
    ensure_hwinfo();

    // these checks are purely based on the input buffer size, not on the conv2d parameters
    if (iact_bytes > alloc_size_iact)
        throw runtime_error("spad memory too small for iact data!");

    if (wght_bytes > alloc_size_wght)
        throw runtime_error("spad memory too small for wght data!");

    input_t* spad = static_cast<input_t*>(recacc_get_buffer(dev));

    // copy iact data column-wise
    // to speed up copying, channels are not mapped ch0 -> col0, ch1 -> col1,
    // but vertically first (ch0+ch1 -> col1, ch2+ch3 -> col2) (for 16 channels)
    input_t* iact_addr = spad + base_iact;
    // cout << "copy iact from " << (void*)iact_buf << " to " << (void*)iact_addr << " " << iact_bytes << " bytes" << endl;
    _copy_in_columnwise(iact_addr, bytes_per_channel, static_cast<const input_t*>(iact_buf), iact_bytes, false);

    input_t* wght_addr = spad + base_wght;
    const input_t* wght_buf_i8 = static_cast<const input_t*>(wght_buf);
    for (unsigned och = 0; och < cfg.m0; och++) {
        // cout << "copy wght for och " << och << " from " << (void*)wght_buf << " to " << (void*)wght_addr << " " << wght_bytes << " bytes" << endl;
        wght_bytes = _copy_in_columnwise(wght_addr, bytes_per_kernel, wght_buf_i8, wght_bytes, true);
        wght_buf_i8 += input_channels * bytes_per_kernel;
        wght_addr += cfg.stride_wght_och;
    }

    if (padding) {
        // make sure there is one zero bytes per column for zero padding
        input_t* pad_addr = spad + base_padding;
        for (unsigned col = 0; col < hwinfo.spad_word_size; col++) {
            *pad_addr = 0;
            pad_addr += spad_column_stride;
        }
    }
}

void Conv2D::set_postproc_data(const vector<psum_t>& bias, const vector<float>& factors, const vector<float>& zeropoints) {
    ensure_hwinfo();

    // cout << "writing " << static_cast<unsigned>(hwinfo.max_output_channels) << " bias/scale regs" << endl;

    // write to bias birst, then factors, then zeropoints continuously to potentially merge writes on AXI
    for (size_t n = 0; n < hwinfo.max_output_channels; n++) {
        const unsigned idx = RECACC_REG_IDX_BIAS_REQUANT_BASE + n;
        uint32_t value = 0;
        if (n < bias.size())
            value = bias[n];
        recacc_reg_write(dev, idx, value);
    }

    for (size_t n = 0; n < hwinfo.max_output_channels; n++) {
        const unsigned idx = RECACC_REG_IDX_BIAS_REQUANT_BASE + hwinfo.max_output_channels + n;
        float value = 0.0;
        if (n < factors.size())
            value = factors[n];
        uint32_t buf;
        memcpy(&buf, &value, sizeof(buf));
        recacc_reg_write(dev, idx, buf);
    }

    for (size_t n = 0; n < hwinfo.max_output_channels; n++) {
        const unsigned idx = RECACC_REG_IDX_BIAS_REQUANT_BASE + 2 * hwinfo.max_output_channels + n;
        float value = 0.0;
        if (n < zeropoints.size())
            value = zeropoints[n];
        uint32_t buf;
        memcpy(&buf, &value, sizeof(buf));
        recacc_reg_write(dev, idx, buf);
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

string Conv2D::get_parameter_string() const {
    ostringstream oss;
    oss << iact_w << "x" << iact_h << ", ";
    oss << wght_w << "x" << wght_h << ", ";
    oss << input_channels << " input channels, ";
    oss << output_channels << " output channels, ";
    if (padding)
        oss << "padding on, ";
    else
        oss << "padding off, ";
    switch (act_mode) {
        case act_none:
            oss << "activation none, ";
            break;
        case act_relu:
            oss << "activation relu, ";
            break;
        default:
            oss << "activation UNKNOWN, ";
    }
    if (requantize)
        oss << "requantize on";
    else
        oss << "requantize off";
    return oss.str();
}

unsigned Conv2D::get_cycle_count() const {
    return cycles;
}

void Conv2D::run_accelerator() {
    ensure_hwinfo();

    if (!hwinfo.bias_requant_available) {
        if (requantize)
            throw runtime_error("requantize requested but no postproc support in hardware");

        if (act_mode != act_none)
            throw runtime_error("activation requested but no postproc support in hardware");
    }

    recacc_control_start(dev, requantize, act_mode, use_irq, padding);
}

// wait for accelerator to finish and copy data back, returns true on success
bool Conv2D::wait_until_accelerator_done() {
    // wait for accelerator to finish
    bool success = recacc_wait(dev, !use_irq);
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
    // TODO: support raw mode without postprocessing -> 16bit psums

    // we actually don't care if the buffer is too small, the user does not get all results
    if (psum_bytes > alloc_size_psum)
        throw runtime_error("copy_data_out requesting more data than allocated");

    // limit number of channels to available buffer size
    size_t copy_och_count = psum_bytes / bytes_per_output_channel;
    if (copy_och_count > output_channels)
        copy_och_count = output_channels;

    int8_t* dst = static_cast<int8_t*>(psum_buf);
    int8_t* psum_addr = nullptr;
    for (unsigned och = 0; och < copy_och_count; och++) {
        psum_addr = static_cast<int8_t*>(recacc_get_buffer(dev)) + base_psum
                    + bytes_per_output_channel * (och / hwinfo.spad_word_size)
                    + spad_column_stride * (och % hwinfo.spad_word_size);

        // TODO: improve copy-out. we do manual 32bit reads here to allow 4-byte aligned output channel sizes
        // i.e. reading 900 bytes for 30x30 output image would fail with memcpy for 2nd channel,
        // cause the target address + 900 is not aligned to 8-byte anymore (and on aarch64 64bit copy is default)
        uint32_t* psum_addr32 = reinterpret_cast<uint32_t*>(psum_addr);
        uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst);
        for (size_t n = 0; n < bytes_per_output_channel / 4; n++)
            dst32[n] = psum_addr32[n];

        // this special memcpy makes sure to align the call to actual memcpy to work around device memory alignment issues
        // however, it turns out to be even slower than the 32bit for-loop copy
        // memcpy_align_src(dst, psum_addr, bytes_per_output_channel);

        dst += bytes_per_output_channel;
    }

    // deassert start bit, this resets the control logic and allows for starting the next iteration
    recacc_control_stop(dev);
}

bool Conv2D::validate_hw_state() {
    recacc_status status = recacc_get_status(dev);
    bool ok = true;

    if (status.spad_iact_full) {
        ok = false;
        cerr << "WARNING: hw idle but some spad iact fifos are full" << endl;
    } else if (!status.spad_iact_empty) {
        ok = false;
        cerr << "WARNING: hw idle but not all spad iact fifos empty" << endl;
    }

    if (status.spad_wght_full) {
        ok = false;
        cerr << "WARNING: hw idle but some spad wght fifos are full" << endl;
    } else if (!status.spad_wght_empty) {
        ok = false;
        cerr << "WARNING: hw idle but not all spad wght fifos empty" << endl;
    }

    if (!status.spad_psum_empty) {
        ok = false;
        cerr << "WARNING: hw idle but not all psum fifos empty" << endl;
    }

    return ok;
}
