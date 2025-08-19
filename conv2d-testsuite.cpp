#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <array>

#include "lib/conv2d.hpp"
#include "lib/conv2dtest.hpp"
#include "lib/VariadicTable.h"
#include "types.h"

extern "C" {
    #include "driver/driver.h"
}

#define DEFAULT_DEVICE "/dev/uio4"

using namespace std;

array<tuple<bool, bool, bool, bool>, 8> variants = {
    // enable requantize, padding, relu activation, use alternative dataflow
    make_tuple(false, false, false, false),
    make_tuple(true,  false, false, false),
    make_tuple(false, true,  false, false),
    make_tuple(true,  true,  false, false),
    make_tuple(false, false, true,  false),
    make_tuple(true,  false, true,  false),
    make_tuple(false, true,  true,  false),
    make_tuple(true,  true,  true,  false),
};

// number of output channels currently depends on m0, number of spatially mapped kernels
// on a 10x7 accelerator, three 3x3 kernels and two 5x5 kernels can be mapped on the Y axis, thus 3 / 2 channels
array<Conv2D, 33> tests = {
    //Conv2D(8, 3, 8, 3, true),
    //Conv2D(8, 5, 8, 2, true),
    Conv2D(16, 3, 8, 3), // small image + few channels
    Conv2D(16, 5, 8, 2),
    Conv2D(16, 7, 8, 1),
    Conv2D(16, 3, 32, 3), // small image + more channels
    Conv2D(16, 5, 32, 2),
    Conv2D(16, 7, 32, 1),
    Conv2D(16, 3, 512, 3), // small image + many channels
    Conv2D(16, 5, 512, 2),
    Conv2D(16, 7, 512, 1),
    Conv2D(32, 3, 8, 3), // medium image + few channels
    Conv2D(32, 5, 8, 2),
    Conv2D(32, 7, 8, 1),
    Conv2D(32, 3, 64, 3), // medium image + more channels
    Conv2D(32, 5, 64, 2),
    Conv2D(32, 7, 64, 1),
    Conv2D(32, 3, 256, 3), // medium image + many channels
    Conv2D(32, 5, 256, 2),
    Conv2D(32, 7, 256, 1),
    Conv2D(64, 3, 8, 3), // large image + few channels
    Conv2D(64, 5, 8, 2),
    Conv2D(64, 7, 8, 1),
    Conv2D(64, 3, 32, 3),
    Conv2D(64, 5, 32, 2),
    Conv2D(64, 7, 32, 1),
    Conv2D(64, 3, 96, 3), // large image + many channels
    Conv2D(64, 5, 96, 2),
    Conv2D(64, 7, 96, 1),
    Conv2D(128, 3, 8, 3), // huge image + few channels
    Conv2D(128, 5, 8, 2),
    Conv2D(128, 7, 8, 1),
    Conv2D(128, 3, 16, 3), // the largest configuration for 512KiB scratchpad
    Conv2D(128, 5, 16, 2),
    Conv2D(128, 7, 16, 1),
};

VariadicTable<int, int, int, int, int, string, string, string, string, float, float, float, float, float> vt({
    "#", "HxW", "RxS", "i-ch", "o-ch", "pad", "act", "requant", "status",
    "cpu us", "copy-in us", "acc us", "copy-out us", "speedup"}, 10);

void list_tests() {
    int test_number = 0;
    for (auto t : tests)
        cout << "Test " << test_number++ << ": " << t.get_parameter_string() << endl;
}

string format_unit(float value, const string& unit) {
    ostringstream oss;
    oss << value << unit;
    return oss.str();
}

// run one of the tests, return true on success
bool run_test(recacc_device* dev, Conv2D& test) {
    static int test_number = 1;
    bool do_run = true;
    bool success = false;
    float speedup = 0.0;

    Conv2DTest testrun(dev, test);
    cout << "Running test " << test.get_parameter_string() << endl;

    testrun.set_verbose(Conv2DTest::Verbosity::Errors);
    try {
        testrun.prepare_run(string());
        testrun.prepare_accelerator();
    } catch (const exception& e) {
        cout << "SKIPPED due to exception: " << e.what() << endl;
        do_run = false;
    }

    if (do_run) {
        testrun.run_accelerator();
        testrun.run_cpu();

        success = testrun.get_accelerator_results();
        if (success) {
            success = testrun.verify();
            speedup = testrun.duration_cpu / (testrun.duration_copy_in + testrun.duration_acc + testrun.duration_copy_out);
        }
    }

    string activation_str{"none"};
    switch (testrun.get_activation_mode()) {
        case act_none:
            break;
        case act_relu:
            activation_str = "relu";
            break;
        default:
            activation_str = "unknown";
    }

    string success_str{"FAILED"};
    if (!do_run)
        success_str = "SKIPPED";
    else if (success)
        success_str = "SUCCESS";

    vt.addRow(
        test_number,
        get<0>(testrun.get_image_size()),
        get<0>(testrun.get_kernel_size()),
        get<0>(testrun.get_channel_count()),
        get<1>(testrun.get_channel_count()),
        testrun.get_padding_mode() ? "yes" : "no",
        activation_str,
        testrun.get_requantize() ? "yes" : "no",
        success_str,
        testrun.duration_cpu.count(),
        testrun.duration_copy_in.count(),
        testrun.duration_acc.count(),
        testrun.duration_copy_out.count(),
        speedup
    );
    test_number++;

    return success;
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

    vt.setColumnFormat({VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::AUTO,
                        VariadicTableColumnFormat::AUTO,
                        VariadicTableColumnFormat::AUTO,
                        VariadicTableColumnFormat::AUTO,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED,
                        VariadicTableColumnFormat::FIXED});
    vt.setColumnPrecision({0,0,0,0,0,0,0,0,0,3,3,3,3,2});

    cout << "Running tests..." << endl;
    for (auto [requantize, padding, relu, dataflow] : variants) {
        for (auto t : tests) {
            t.set_requantize(requantize);
            t.set_padding_mode(padding);
            t.set_activation_mode(relu ? act_relu : act_none);
            // TODO: implement switching to alternative dataflow
            run_test(&dev, t);
        }
    }

    // print a nice result table
    vt.print(cout);

    if (!dryrun)
        ret = recacc_close(&dev);

    return ret;
}
