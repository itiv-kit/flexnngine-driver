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

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <offset> <nr_bytes> [device] [output_file]\n", argv[0]);
        return 1;
    }

    int offset = atoi(argv[1]);
    int bytes = atoi(argv[2]);

    string device_name(DEFAULT_DEVICE);
    if (argc > 3)
        device_name = string(argv[3]);

    string file_name;
    if (argc > 4)
        file_name = string(argv[4]);

    recacc_device dev;
    int ret = recacc_open(&dev, device_name.c_str());
    if (ret)
        return ret;

    if (!recacc_verify(&dev, true)) {
        recacc_close(&dev);
        return ret;
    }

    recacc_hwinfo hwinfo;
    recacc_get_hwinfo(&dev, &hwinfo);

    uint8_t* buf_psum = new uint8_t[bytes];
    uint8_t* psum_addr = static_cast<uint8_t*>(recacc_get_buffer(&dev)) + offset;
    cerr << "reading " << bytes << " bytes from psum buffer at " << RECACC_MEM_OFFSET_SPAD + offset << " (mapped at " << psum_addr << ")" << endl;
    memcpy(buf_psum, psum_addr, bytes);

    if (file_name.length()) {
        ofstream myfile;
        myfile.open (file_name, ios::out | ios::binary);
        myfile.write(reinterpret_cast<char*>(buf_psum), bytes);
        myfile.close();
    } else {
        print_buffer(buf_psum, bytes);
    }

    ret = recacc_close(&dev);
    return ret;
}
