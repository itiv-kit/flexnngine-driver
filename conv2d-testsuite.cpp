#include <iostream>
#include <sstream>
// #include <stdexcept>
#include <string>
#include <unistd.h>
// #include <cstdint>
#include <array>

#include "lib/conv2d.hpp"
#include "lib/conv2dtest.hpp"
#include "types.h"

extern "C" {
    #include "driver/driver.h"
}

#define DEFAULT_DEVICE "/dev/uio4"

using namespace std;

array<Conv2D, 1> tests = {
    Conv2D(32, 3, 4, 3)
};

void list_tests() {
    int test_number = 0;
    for (auto t : tests) {
        cout << "Test " << test_number++ << ": " << t.get_parameter_string() << endl;
    }
}

// run one of the tests, return true on success
bool run_test(recacc_device* dev, Conv2D& test) {
    Conv2DTest testrun(dev, test);
    cout << "Running test " << test.get_parameter_string() << endl;

    testrun.prepare_data(false, string());
    testrun.prepare_accelerator();
    testrun.run_accelerator();
    testrun.run_cpu();

    cout << "conv2d done on cpu, waiting for accelerator" << endl;
    bool success = testrun.get_accelerator_results();
    if (!success)
        return false;

    return testrun.verify(false);
}

int main(int argc, char** argv) {
    bool dryrun = false;

    #ifdef __linux__
    opterr = 0;
    int c;
    string device_name(DEFAULT_DEVICE);
    string files_path;
    string output_path;

    while ((c = getopt (argc, argv, "hlt:")) != -1)
        switch (c) {
            case 'h':
                cout << "Usage:" << endl;
                cout << "-h: show this help" << endl;
                cout << "-l: list all built-in tests" << endl;
                cout << "-t <test1,test2>: only run specific tests" << endl;
                return 0;
                break;
            case 'l':
                list_tests();
                return 0;
                break;
            case 't': {
                device_name = string(optarg);
                std::istringstream iss(string{optarg});
                std::string s;
                while (std::getline(iss, s, ',')) {
                    std::cout << "got " << s << std::endl;
                }
                break;
            }
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
        ret = recacc_open(&dev, (void*) 0x50000000);
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

    cout << "Running tests..." << endl;
    for (auto t : tests)
        run_test(&dev, t);

    if (!dryrun)
        ret = recacc_close(&dev);

    return ret;
}
