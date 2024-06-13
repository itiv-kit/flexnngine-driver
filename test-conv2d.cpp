#include <assert.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <string>
#include <fstream>
#include <chrono>
#include <random>
#include <unistd.h>

#include "lib/conv2d.hpp"

extern "C" {
    #include "driver/driver.h"
}

#define DEFAULT_DEVICE "/dev/uio4"

using namespace std;

// read integers from simulation files
template<typename T> size_t read_text_data(T* buffer, size_t size, string path) {
    size_t i = 0;
    long int tmp;
    ifstream infile(path);
    while (i < size && infile >> tmp)
        buffer[i++] = tmp;
    return i;
}

template<typename T> size_t write_text_data(T* buffer, size_t size, size_t stride, string path) {
    size_t i = 0;
    ofstream outfile(path);
    while (i < size && outfile.good()) {
        outfile << buffer[i++] << ' ';
        if (i % stride == 0)
            outfile << '\n';
    }
    return i;
}

template<typename T> void print_buffer(void* data, size_t words, size_t offset = 0, size_t words_per_line = 8) {
    T* ints = static_cast<T*>(data) + offset;
    for (size_t i=0; i<words; i+=1) {
        if (i % words_per_line == 0)
            printf("0x%04lx: ", i + offset);
        using unsigned_type = std::make_unsigned_t<T>;
        cout << setfill('0') << hex << setw(sizeof(T)*2)
             << (static_cast<int32_t>(ints[i]) & numeric_limits<unsigned_type>::max());
        if (i % words_per_line != 7)
            putchar(' ');
        else
            putchar('\n');
    }
    if (words % words_per_line)
        putchar('\n');
}

void test_print_buffer() {
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
    print_buffer<int8_t>(buf, 16, 16);
    print_buffer<int16_t>(buf, 8,  8);
    print_buffer<int32_t>(buf, 4,  4);
}

auto make_multiple_of(auto div, auto value) {
    auto remainder = value % div;
    if (remainder > 0)
        value += div - remainder;
    return value;
}

class Conv2DTest {
public:
    Conv2DTest(recacc_device* accelerator)
        : mtrnd(std::chrono::system_clock::now().time_since_epoch().count()) {
        buf_iact = nullptr;
        buf_wght = nullptr;
        buf_result_cpu = nullptr;
        buf_result_acc = nullptr;
        buf_result_files = nullptr;
        num_iact_elements = 0;
        num_wght_elements = 0;
        num_result_elements = 0;
        this->dev = accelerator;
        dryrun = false;
        hwinfo.array_size_x = 0;
    }

    ~Conv2DTest() {
        if (buf_iact) delete[] buf_iact;
        if (buf_wght) delete[] buf_wght;
        if (buf_result_cpu) delete[] buf_result_cpu;
        if (buf_result_acc) delete[] buf_result_acc;
        if (buf_result_files) delete[] buf_result_files;
    }

    void ensure_hwinfo() {
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
        } else {
            recacc_get_hwinfo(dev, &hwinfo);
        }
    }

    template <typename T> void generate_random_data(T* ptr, size_t n) {
        for (auto i = 0u; i < n; i++)
            ptr[i] = static_cast<T>(mtrnd() % 256);
    }

    void prepare_data(bool data_from_files, string files_path) {
        cout << "preparing conv2d data with:" << endl;
        cout << "  " << iact_w << "x" << iact_h << ", " << input_channels << " ch input activations" << endl;
        cout << "  " << wght_w << "x" << wght_h << " kernels and " << output_channels << " output channels" << endl;

        num_iact_elements = make_multiple_of(8, iact_w * iact_h * input_channels);
        buf_iact = new int8_t[num_iact_elements];
        if (data_from_files) {
            size_t n = read_text_data<int8_t>(buf_iact, num_iact_elements, files_path + "/_image.txt");
            if (num_iact_elements != n)
                cout << "warning: only " << n << " iact words read, " << num_iact_elements << " expected." << endl;
        } else
            generate_random_data<int8_t>(buf_iact, num_iact_elements);

        num_wght_elements = make_multiple_of(8, wght_w * wght_h * input_channels * output_channels);
        buf_wght = new int8_t[num_wght_elements];
        if (data_from_files) {
            size_t n = read_text_data<int8_t>(buf_wght, num_wght_elements, files_path + "/_kernel_stack.txt");
            if (num_wght_elements != n)
                cout << "warning: only " << n << " wght words read, " << num_wght_elements << " expected." << endl;
        } else
            generate_random_data<int8_t>(buf_wght, wght_w * wght_h);

        const int output_size = (iact_w - wght_w + 1) * (iact_h - wght_h + 1); // no padding
        num_result_elements = make_multiple_of(8, output_size * output_channels);
        buf_result_cpu = new int16_t[num_result_elements];
        buf_result_acc = new int16_t[num_result_elements];
        if (data_from_files) {
            buf_result_files = new int16_t[num_result_elements];
            size_t n = read_text_data<int16_t>(buf_result_files, num_result_elements, files_path + "/_convolution_stack.txt");
            if (num_result_elements != n)
                cout << "warning: only " << n << " result words read, " << num_result_elements << " expected." << endl;
        }

        cout << "using "
            << num_iact_elements * sizeof(buf_iact[0]) << " bytes iact, "
            << num_wght_elements * sizeof(buf_wght[0]) << " bytes wght, "
            << num_result_elements * sizeof(buf_result_acc[0]) << " bytes psum" << endl;

        ensure_hwinfo();

        if (num_iact_elements * sizeof(buf_iact[0]) > hwinfo.spad_size_iact)
            throw runtime_error("iact spad memory too small!");

        if (num_wght_elements * sizeof(buf_wght[0]) > hwinfo.spad_size_wght)
            throw runtime_error("wght spad memory too small!");

        if (num_result_elements * sizeof(buf_result_acc[0]) > hwinfo.spad_size_psum)
            throw runtime_error("psum spad memory too small!");
    }

    // calculate convolution on cpu as reference
    void run_cpu() {
        conv2d<int8_t, int16_t>(buf_iact, buf_wght, nullptr, buf_result_cpu,
            input_channels, iact_w, iact_h,
            output_channels, wght_w, wght_h,
            1, 1, 0, 0);
    }

    void prepare_accelerator() {
        // calculate and set accelerator parameters
        assert(iact_w == iact_h);
        int image_size = iact_w;
        assert(wght_w == wght_h);
        int kernel_size = wght_w;

        ensure_hwinfo();

        cout << "Accelerator configuration:" << endl;
        cout << " array size: " << hwinfo.array_size_y
             << "x" << hwinfo.array_size_x << endl;
        cout << " data width:"
             << " iact " << static_cast<int>(hwinfo.data_width_bits_iact)
             << " wght " << static_cast<int>(hwinfo.data_width_bits_wght)
             << " psum " << static_cast<int>(hwinfo.data_width_bits_psum) << endl;
        cout << " pe buffers:"
             << " iact " << hwinfo.line_length_iact
             << " wght " << hwinfo.line_length_wght
             << " psum " << hwinfo.line_length_psum << endl;
        cout << " scratchpad sizes:";
        cout << " iact " << hwinfo.spad_size_iact
             << " wght " << hwinfo.spad_size_wght
             << " psum " << hwinfo.spad_size_psum << endl;

        recacc_config cfg;
        cfg.iact_dimension = image_size;
        cfg.wght_dimension = kernel_size;
        cfg.input_channels = input_channels;
        cfg.output_channels = output_channels;
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

        // clear software result buffers
        memset(buf_result_acc, 0, num_result_elements * sizeof(buf_result_acc[0]));
        memset(buf_result_cpu, 0, num_result_elements * sizeof(buf_result_cpu[0]));

        if (dryrun)
            return;

        recacc_config_write(dev, &cfg);

        cout << "copying input data to accelerator" << endl;

        // copy accelerator input data
        void* iact_addr = recacc_get_buffer(dev, buffer_type::iact);
        memcpy(iact_addr, buf_iact, num_iact_elements * sizeof(buf_iact[0]));

        void* wght_addr = recacc_get_buffer(dev, buffer_type::wght);
        memcpy(wght_addr, buf_wght, num_wght_elements * sizeof(buf_wght[0]));

        // also clear the hardware result buffer to ease debugging
        void* psum_addr = recacc_get_buffer(dev, buffer_type::psum);
        memcpy(psum_addr, buf_result_acc, num_result_elements * sizeof(buf_result_acc[0]));
    }

    // start accelerator
    void run_accelerator() {
        if (dryrun)
            return;

        recacc_control_start(dev);
    }

    // wait for accelerator to finish and copy data back
    void get_accelerator_results() {
        if (dryrun)
            return;

        // wait for accelerator to finish
        recacc_wait(dev);

        // read diagnostic registers and validate hardware status after processing
        uint32_t psum_overflows = recacc_reg_read(dev, RECACC_REG_IDX_PSUM_OVERFLOWS);
        if (psum_overflows)
            cout << "WARNING: psum overflows in hardware (" << psum_overflows << "), results may be invalid." << endl;

        // copy data back
        void* psum_addr = recacc_get_buffer(dev, buffer_type::psum);
        memcpy(buf_result_acc, psum_addr, num_result_elements * sizeof(buf_result_acc[0]));

        // deassert start bit, this resets the control logic and allows for starting the next iteration
        recacc_control_stop(dev);
    }

    size_t compare_buffers(int16_t* buf_a, int16_t* buf_b, size_t buf_size, size_t& incorrect_cnt) {
        size_t incorrect_offset = ~0LU;
        incorrect_cnt = 0;
        for (size_t n = 0; n < buf_size; n++)
            if (buf_a[n] != buf_b[n]) {
                if (incorrect_offset == ~0LU)
                    incorrect_offset = n;
                incorrect_cnt++;
            }
        return incorrect_offset;
    }

    void verify() {
        if (buf_result_files) {
            cout << "comparing " << num_result_elements << " cpu values to the file reference... ";
            size_t incorrect;
            size_t incorrect_offset = compare_buffers(buf_result_cpu, buf_result_files, num_result_elements, incorrect);
            if (incorrect > 0)
                cout << incorrect << " values INCORRECT, first at " << incorrect_offset << endl;
            else
                cout << "CORRECT" << endl;

            if (incorrect > 0) {
                cout << "CPU result (128 words at " << incorrect_offset << "):" << endl;
                if (incorrect_offset > 16)
                    incorrect_offset -= incorrect_offset % 16;
                if (incorrect_offset > 32)
                    incorrect_offset -= 32;
                print_buffer<int16_t>(buf_result_cpu, 128, incorrect_offset);
                cout << "Reference result from file:" << endl;
                print_buffer<int16_t>(buf_result_files, 128, incorrect_offset);
            }
        }

        cout << "comparing " << num_result_elements << " output values... ";
        size_t incorrect;
        size_t incorrect_offset = compare_buffers(buf_result_acc, buf_result_cpu, num_result_elements, incorrect);
        if (incorrect > 0)
            cout << incorrect << " values INCORRECT, first at " << incorrect_offset << endl;
        else
            cout << "CORRECT" << endl;

        if (incorrect > 0) {
            if (incorrect_offset > 16)
                incorrect_offset -= incorrect_offset % 16;
            if (incorrect_offset > 32)
                incorrect_offset -= 32;
            cout << "CPU result (128 words at " << incorrect_offset << "):" << endl;
            print_buffer<int16_t>(buf_result_cpu, 128, incorrect_offset);
            cout << "ACC result:" << endl;
            print_buffer<int16_t>(buf_result_acc, 128, incorrect_offset);
        }
    }

    void write_data(string output_path) {
        size_t written = write_text_data<int16_t>(
            buf_result_cpu,
            num_result_elements,
            iact_w - wght_w + 1,
            output_path + "/_output_cpu.txt");
        if (written != num_result_elements)
            cerr << "wrote only " << written << " elements to _output_cpu.txt" << endl;

        written = write_text_data<int16_t>(
            buf_result_acc,
            num_result_elements,
            iact_w - wght_w + 1,
            output_path + "/_output_acc.txt");
        if (written != num_result_elements)
            cerr << "wrote only " << written << " elements to _output_acc.txt" << endl;
    }

    void set_dryrun(bool enabled) {
        dryrun = enabled;
    }

private:
    mt19937 mtrnd;
    recacc_device* dev;
    recacc_hwinfo hwinfo;

    size_t num_iact_elements;
    size_t num_wght_elements;
    size_t num_result_elements;

    int8_t* buf_iact;
    int8_t* buf_wght;
    int16_t* buf_result_cpu;
    int16_t* buf_result_acc;
    int16_t* buf_result_files;
    bool dryrun;

    // static parameters for our test convolution
    const unsigned iact_w = 32;
    const unsigned iact_h = 32;
    const unsigned wght_w = 3;
    const unsigned wght_h = 3;
    const unsigned input_channels = 4;
    const unsigned output_channels = 3;
};

int main(int argc, char** argv) {
    bool dryrun = false;

    #ifdef __linux__
    opterr = 0;
    int c;
    string device_name(DEFAULT_DEVICE);
    string files_path;
    string output_path;

    while ((c = getopt (argc, argv, "hnd:p:o:")) != -1)
        switch (c) {
            case 'h':
                cout << "Usage:" << endl;
                cout << "-h: show this help" << endl;
                cout << "-n: no-op, do not access the accelerator" << endl;
                cout << "-d <device>: use this uio device (default: " << DEFAULT_DEVICE << ")" << endl;
                cout << "-p <path>: load data from path instead of random" << endl;
                cout << "           path must contain _image.txt, _kernel.txt, _convolution.txt" << endl;
                cout << "-o <path>: save output data to path (_output_acc.txt, _output_cpu.txt)" << endl;
                return 0;
                break;
            case 'n':
                dryrun = true;
                break;
            case 'd':
                device_name = string(optarg);
                break;
            case 'p':
                files_path = string(optarg);
                break;
            case 'o':
                output_path = string(optarg);
                break;
            case '?':
                if (optopt == 'c')
                    cerr << "Option -" << optopt << " requires an argument." << endl;
                else if (isprint (optopt))
                    cerr << "Unknown option -" << optopt << endl;
                else
                    cerr << "Unknown option character " << static_cast<int>(optopt) << endl;
                return 1;
            default:
                abort ();
        }
    #endif

    recacc_device dev;
    int ret = 0;
    if (!dryrun) {
        #ifdef __linux__
        ret = recacc_open(&dev, device_name.c_str());
        if (ret)
            return ret;
        #else
        ret = recacc_open(&dev, 0x50000000);
        #endif

        if (!recacc_verify(&dev, true)) {
            recacc_close(&dev);
            return ret;
        }

        recacc_reset(&dev);

        recacc_status status = recacc_get_status(&dev);
        if (!status.ready) {
            cout << "Accelerator is not ready, status register: "
                << hex << recacc_reg_read(&dev, RECACC_REG_IDX_STATUS)
                << endl;
            return recacc_close(&dev);
        }
    }

    Conv2DTest c2d(&dev);
    c2d.set_dryrun(dryrun);

    cout << "preparing random test data" << endl;
    c2d.prepare_data(files_path.length() > 0, files_path);

    cout << "calculating accelerator parameters" << endl;
    c2d.prepare_accelerator();

    cout << "launching conv2d on accelerator" << endl;
    c2d.run_accelerator();

    cout << "launching conv2d on cpu" << endl;
    c2d.run_cpu();

    cout << "conv2d done on cpu, waiting for accelerator" << endl;
    c2d.get_accelerator_results();

    cout << "comparing cpu and accelerator results" << endl;
    c2d.verify();

    if (output_path.length())
        c2d.write_data(output_path);

    if (!dryrun)
        ret = recacc_close(&dev);

    return ret;
}
