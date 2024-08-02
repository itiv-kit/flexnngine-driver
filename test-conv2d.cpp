#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <cstdint>

#include "lib/conv2dtest.hpp"

extern "C" {
    #include "driver/driver.h"
}

#define DEFAULT_DEVICE "/dev/uio4"

using namespace std;

void dump_status_register(recacc_device* dev) {
    union recacc_status_reg status;
    status.raw = recacc_reg_read(dev, RECACC_REG_IDX_STATUS);
    cout << "Status register 0x" << hex << setfill('0') << setw(8) << status.raw << dec << ":" << endl;
    cout << "  ready     " << status.decoded.ready << endl;
    cout << "  done      " << status.decoded.done << endl;
    cout << "  iact_done " << status.decoded.ctrl_iact_done << endl;
    cout << "  wght_done " << status.decoded.ctrl_wght_done << endl;
    cout << "  preload   " << status.decoded.preload_done << endl;
}

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

    Conv2DTest c2d(&dev);
    c2d.set_dryrun(dryrun);

    cout << "preparing random test data" << endl;
    #ifdef __linux__
    c2d.prepare_data(files_path.length() > 0, files_path);
    #else
    c2d.prepare_data(false, string());
    #endif

    cout << "calculating accelerator parameters" << endl;
    c2d.prepare_accelerator();
    if (!dryrun)
        dump_status_register(&dev);

    cout << "launching conv2d on accelerator" << endl;
    c2d.run_accelerator();

    cout << "launching conv2d on cpu" << endl;
    c2d.run_cpu();

    cout << "conv2d done on cpu, waiting for accelerator" << endl;
    bool success = c2d.get_accelerator_results();
    if (!success && !dryrun) {
        dump_status_register(&dev);
        return 1;
    }

    cout << "comparing cpu and accelerator results" << endl;
    c2d.verify();

    #ifdef __linux__
    if (output_path.length())
        c2d.write_data(output_path);
    #endif

    if (!dryrun)
        ret = recacc_close(&dev);

    return ret;
}
