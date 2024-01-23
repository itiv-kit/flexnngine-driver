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
#include <unistd.h>

#include "driver/defs.h"
#include "lib/conv2d.hpp"

extern "C" {
    #include "driver/driver.h"
}

#define DEFAULT_DEVICE "/dev/uio4"
const uint16_t bytes_to_test = 0x2000; // per buffer

using namespace std;

class Conv2DTest {
public:
    Conv2DTest(recacc_device* accelerator)
        : mtrnd(std::chrono::system_clock::now().time_since_epoch().count()) {
        buf_iact = nullptr;
        buf_wght = nullptr;
        buf_psum = nullptr;
        this->dev = accelerator;
    }

    ~Conv2DTest() {
        if (buf_iact) delete[] buf_iact;
        if (buf_wght) delete[] buf_wght;
        if (buf_psum) delete[] buf_psum;
    }

    void generate_random_data_int8(int8_t* ptr, size_t n) {
        for (auto i = 0u; i < n; i++)
            ptr[i] = (int8_t)(mtrnd() % 256);
    }

    void prepare_data() {
        cout << "generating " << bytes_to_test << " bytes of random test data for each buffer" << endl;

        buf_iact = new int8_t[bytes_to_test];
        generate_random_data_int8(buf_iact, bytes_to_test);

        buf_wght = new int8_t[bytes_to_test];
        generate_random_data_int8(buf_wght, bytes_to_test);

        buf_psum = new int8_t[bytes_to_test];
        generate_random_data_int8(buf_psum, bytes_to_test);
    }

    void test_buffers() {
        // clear result buffers
        // memset(buf_result_acc, 0, num_result_elements * sizeof(buf_result_acc[0]));
        // memset(buf_psum, 0, num_result_elements * sizeof(buf_psum[0]));

        // copy random data to buffers
        void* iact_addr = recacc_get_buffer(dev, buffer_type::iact);
        cout << "copy data to iact buffer at " << RECACC_MEM_OFFSET_IACT << "(mapped at " << iact_addr << ")" << endl;
        memcpy(iact_addr, buf_iact, bytes_to_test);

        void* wght_addr = recacc_get_buffer(dev, buffer_type::wght);
        cout << "copy data to wght buffer at " << RECACC_MEM_OFFSET_WGHT << "(mapped at " << wght_addr << ")" << endl;
        memcpy(wght_addr, buf_wght, bytes_to_test);

        // also clear the result buffer to ease debugging
        void* psum_addr = recacc_get_buffer(dev, buffer_type::psum);
        cout << "copy data to psum buffer at " << RECACC_MEM_OFFSET_PSUM << "(mapped at " << psum_addr << ")" << endl;
        memcpy(psum_addr, buf_psum, bytes_to_test);

        sleep(1);

        // now read data back and compare
        verify("iact", iact_addr, buf_iact);
        verify("wght", wght_addr, buf_wght);
        verify("psum", psum_addr, buf_psum);
    }

    void verify(const string& name, void* src, int8_t* reference) {
        int8_t* buf_test = new int8_t[bytes_to_test];

        cout << "reading back " << name << " buffer" << endl;
        memcpy(buf_test, src, bytes_to_test);

        int ret = memcmp(buf_test, reference, bytes_to_test);
        if (ret != 0)
            cout << "ERROR: " << name << " buffer differs at " << ret << "(got " << buf_test[abs(ret)] << " expected " << reference[abs(ret)] << ")" << endl;
        else
            cout << "SUCCESS: " << name << " buffer test ok" << endl;

        delete[] buf_test;
    }

private:
    mt19937 mtrnd;
    recacc_device* dev;

    int8_t* buf_iact;
    int8_t* buf_wght;
    int8_t* buf_psum;
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

    recacc_reset(&dev);

    recacc_status status = recacc_get_status(&dev);
    if (!status.ready) {
        cout << "Accelerator is not ready, status register: "
            << hex << recacc_reg_read(&dev, RECACC_REG_IDX_STATUS)
            << endl;
        return recacc_close(&dev);
    }

    Conv2DTest c2d(&dev);
    c2d.prepare_data();
    c2d.test_buffers();

    ret = recacc_close(&dev);
    return ret;
}
