#include <assert.h>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <chrono>
#include <random>

#include "lib/conv2d.hpp"

extern "C" {
    #include "driver/driver.h"
}

#define DEFAULT_DEVICE "/dev/uio4"

using namespace std;

class Conv2DTest {
public:
    Conv2DTest(recacc_device* accelerator)
        : mtrnd(std::chrono::system_clock::now().time_since_epoch().count()) {
        buf_iact = nullptr;
        buf_wght = nullptr;
        buf_result_cpu = nullptr;
        buf_result_acc = nullptr;
        this->dev = accelerator;
    }

    ~Conv2DTest() {
        if (buf_iact) delete[] buf_iact;
        if (buf_wght) delete[] buf_wght;
        if (buf_result_cpu) delete[] buf_result_cpu;
        if (buf_result_acc) delete[] buf_result_acc;
    }

    void generate_random_data_int8(int8_t* ptr, size_t n) {
        for (auto i = 0u; i < n; i++)
            ptr[i] = (int8_t)(mtrnd() % 256);
    }

    void prepare_data() {
        num_iact_elements = iact_w * iact_h * input_channels;
        buf_iact = new int8_t[num_iact_elements];
        generate_random_data_int8(buf_iact, num_iact_elements);

        num_wght_elements = wght_w * wght_h * input_channels;
        buf_wght = new int8_t[num_wght_elements];
        generate_random_data_int8(buf_wght, wght_w * wght_h);

        const int output_size = iact_w * iact_h - wght_w * wght_h + 1; // no padding
        num_result_elements = output_size * output_size;
        buf_result_cpu = new int16_t[num_iact_elements];
        buf_result_acc = new int16_t[num_iact_elements];
    }

    // calculate convolution on cpu as reference
    void run_cpu() {
        conv2d<int8_t, int16_t>(buf_iact, buf_wght, 0, buf_result_cpu,
            input_channels, iact_w, iact_h,
            1, wght_w, wght_h,
            1, 1, 1, 1);
    }

    void prepare_accelerator() {
        // calculate and set accelerator parameters
        recacc_get_hwinfo(dev, &hwinfo);
        assert(iact_w == iact_h);
        int kernel_size = iact_w;
        assert(wght_w == wght_h);
        int image_size = wght_w;

        recacc_config cfg;
        int line_length_wght_usable = hwinfo.line_length_wght - 1;

        cfg.m0 = floor((float)hwinfo.array_size_y / kernel_size);
        int h1 = hwinfo.array_size_x;

        cfg.w1 = image_size - kernel_size + 1;
        cfg.m0_last_m1 = 1; // not used yet

        int size_rows = hwinfo.array_size_x + hwinfo.array_size_y - 1;
        // TODO: check case for M0 = 0. IMHO the else path is incorrect, as H2 is the number of iterations for mapping
        // each set of rows (accelerator.size_x rows) to the pe array.
        if (1) // cfg.m0 == 0:
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

        memset(buf_result_acc, 0, num_result_elements * sizeof(buf_result_acc[0]));
        memset(buf_result_cpu, 0, num_result_elements * sizeof(buf_result_cpu[0]));

        recacc_config_write(dev, &cfg);

        // copy accelerator input data
        void* iact_addr = recacc_get_buffer(dev, buffer_type::iact);
        memcpy(iact_addr, buf_iact, num_iact_elements * sizeof(buf_iact[0]));

        void* wght_addr = recacc_get_buffer(dev, buffer_type::wght);
        memcpy(wght_addr, buf_wght, num_wght_elements * sizeof(buf_wght[0]));

        void* psum_addr = recacc_get_buffer(dev, buffer_type::psum);
        memcpy(psum_addr, buf_result_acc, num_result_elements * sizeof(buf_result_acc[0]));
    }

    // start accelerator
    void run_accelerator() {
        recacc_control_start(dev);
    }

    // wait for accelerator to finish and copy data back
    void get_accelerator_results() {
        // wait for accelerator to finish
        recacc_wait(dev);

        // copy data back
    }

    void verify() {
        cout << "comparing " << num_result_elements << " output values... ";
        int incorrect = 0;
        for (int n = 0; n < num_result_elements; n++)
            if (buf_result_acc[n] != buf_result_cpu[n])
                incorrect++;
        if (incorrect > 0)
            cout << incorrect << " values INCORRECT" << endl;
        else
            cout << "CORRECT" << endl;
    }

private:
    mt19937 mtrnd;
    recacc_device* dev;
    recacc_hwinfo hwinfo;

    int num_iact_elements;
    int num_wght_elements;
    int num_result_elements;

    int8_t* buf_iact;
    int8_t* buf_wght;
    int16_t* buf_result_cpu;
    int16_t* buf_result_acc;

    // static parameters for our test convolution
    const unsigned iact_w = 64;
    const unsigned iact_h = 64;
    const unsigned wght_w = 3;
    const unsigned wght_h = 3;
    const unsigned input_channels = 12;
};

int main(int argc, char** argv) {
    string device_name(DEFAULT_DEVICE);
    if (argc > 1)
        device_name = string(*argv);

    recacc_device dev;
    int ret = recacc_open(&dev, device_name.c_str());
    if (ret)
        return ret;

    if (!recacc_verify(&dev, true)) {
        recacc_close(&dev);
        return ret;
    }

    recacc_status status = recacc_get_status(&dev);
    if (!status.ready) {
        cout << "Accelerator is not ready, status register: "
            << hex << recacc_reg_read(&dev, RECACC_REG_IDX_STATUS)
            << endl;
        return recacc_close(&dev);
    }

    Conv2DTest c2d(&dev);
    c2d.prepare_data();
    c2d.prepare_accelerator();
    c2d.run_accelerator();
    c2d.run_cpu();
    c2d.get_accelerator_results();
    c2d.verify();

    ret = recacc_close(&dev);
    return ret;
}
