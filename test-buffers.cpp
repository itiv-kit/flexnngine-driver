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

static recacc_hwinfo hwinfo;

using namespace std;

void print_buffer(void* data, size_t len) {
    uint8_t* ints = (uint8_t*)data;
    for (size_t i=0; i<len; i+=1) {
        if (i%8 == 0)
            printf("0x%04lx: ", i);
        printf("%02x", ints[i]);
        if (i%8 != 7)
            putchar(' ');
        else
            putchar('\n');
    }
    if (len%8)
        putchar('\n');
}

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
        cout << "generating random test data for each buffer" << endl;

        buf_iact = new int8_t[hwinfo.spad_size_iact];
        generate_random_data_int8(buf_iact, hwinfo.spad_size_iact);

        buf_wght = new int8_t[hwinfo.spad_size_wght];
        generate_random_data_int8(buf_wght, hwinfo.spad_size_wght);

        buf_psum = new int8_t[hwinfo.spad_size_psum];
        generate_random_data_int8(buf_psum, hwinfo.spad_size_psum);
    }

    void test_buffers() {
        // clear result buffers
        // memset(buf_result_acc, 0, num_result_elements * sizeof(buf_result_acc[0]));
        // memset(buf_psum, 0, num_result_elements * sizeof(buf_psum[0]));

        // copy random data to buffers
        void* iact_addr = recacc_get_buffer(dev, buffer_type::iact);
        cout << "copy " << hwinfo.spad_size_iact << " bytes to iact buffer at " << RECACC_MEM_OFFSET_IACT << " (mapped at " << iact_addr << ")" << endl;
        memcpy(iact_addr, buf_iact, hwinfo.spad_size_iact);

        void* wght_addr = recacc_get_buffer(dev, buffer_type::wght);
        cout << "copy " << hwinfo.spad_size_wght << " bytes to wght buffer at " << RECACC_MEM_OFFSET_WGHT << " (mapped at " << wght_addr << ")" << endl;
        memcpy(wght_addr, buf_wght, hwinfo.spad_size_wght);

        void* psum_addr = recacc_get_buffer(dev, buffer_type::psum);
        cout << "copy " << hwinfo.spad_size_psum << " bytes to psum buffer at " << RECACC_MEM_OFFSET_PSUM << " (mapped at " << psum_addr << ")" << endl;
        memcpy(psum_addr, buf_psum, hwinfo.spad_size_psum);

        sleep(1);

        // now read data back and compare
        verify("iact", iact_addr, buf_iact, hwinfo.spad_size_iact);
        verify("wght", wght_addr, buf_wght, hwinfo.spad_size_wght);
        verify("psum", psum_addr, buf_psum, hwinfo.spad_size_psum);
    }

    void verify(const string& name, void* src, int8_t* reference, size_t size) {
        int8_t* buf_test = new int8_t[size];

        cout << "reading back " << name << " buffer" << endl;
        memcpy(buf_test, src, size);

        int ret = memcmp(buf_test, reference, size);
        if (ret != 0) {
            cout << "ERROR: " << name << " buffer differs at " << ret
                << " (got " << static_cast<int>(buf_test[abs(ret)])
                << " expected " << static_cast<int>(reference[abs(ret)]) << ")" << endl;

            cout << "Wrote:" << endl;
            print_buffer(reference, size);
            cout << "Got:" << endl;
            print_buffer(buf_test, size);
        } else {
            cout << "SUCCESS: " << name << " buffer test ok" << endl;
        }

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

    recacc_get_hwinfo(&dev, &hwinfo);

    Conv2DTest c2d(&dev);
    c2d.prepare_data();
    c2d.test_buffers();

    ret = recacc_close(&dev);
    return ret;
}
