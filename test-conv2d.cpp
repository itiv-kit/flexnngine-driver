#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

#include "lib/conv2dtest.hpp"
#include "lib/utils.hpp"

extern "C" {
    #include <driver.h>
}

#define DEFAULT_DEVICE "/dev/uio4"

using namespace std;

int main(int argc, char** argv) {
    bool dryrun = false;
    unsigned image_size = 32, kernel_size = 3, input_channels = 8, output_channels = 3;
    unsigned throttle = -1;
    enum activation_mode act_mode = act_none;
    bool zero_bias = false;
    bool requantize = false;
    bool padding = false;
    bool debug_mode = false;
    bool interrupts = false;

    #ifdef __linux__
    opterr = 0;
    int c;
    string device_name(DEFAULT_DEVICE);
    string files_path;
    string output_path;

    while ((c = getopt(argc, argv, "hnd:i:o:s:c:k:u:Brpa:DIPt:")) != -1)
        switch (c) {
            case 'h':
                cout << "Usage:" << endl;
                cout << "-h: show this help" << endl;
                cout << "-n: no-op, do not access the accelerator" << endl;
                cout << "-d <device>: use this uio device (default: " << DEFAULT_DEVICE << ")" << endl;
                cout << "-i <path>: load data from path instead of random" << endl;
                cout << "           path must contain _image.txt, _kernel.txt, _convolution.txt" << endl;
                cout << "-o <path>: save output data to path (_output_acc.txt, _output_cpu.txt)" << endl;
                cout << "-s 32: width & height of the input image" << endl;
                cout << "-k 3: width & height of the kernels" << endl;
                cout << "-c 8: number of input channels" << endl;
                cout << "-u 3: number of output channels" << endl;
                cout << "-B: use a zero bias for all channels" << endl;
                cout << "-r: enable requantization" << endl;
                cout << "-p: enable same size padding" << endl;
                cout << "-a relu: enable activation (available: relu)" << endl;
                cout << "-D enable buffer debug mode (fill unused with 0 / 0xaa pattern)" << endl;
                cout << "-I/-P use interrupts or polling (default: polling)" << endl;
                cout << "-t specify psum throttle value (default: guess)" << endl;
                return 0;
                break;
            case 'n':
                dryrun = true;
                break;
            case 'd':
                device_name = string(optarg);
                break;
            case 'i':
                files_path = string(optarg);
                break;
            case 'o':
                output_path = string(optarg);
                break;
            case 's':
                image_size = atoi(optarg);
                break;
            case 'k':
                kernel_size = atoi(optarg);
                break;
            case 'c':
                input_channels = atoi(optarg);
                break;
            case 'u':
                output_channels = atoi(optarg);
                break;
            case 'B':
                zero_bias = true;
                break;
            case 'r':
                requantize = true;
                break;
            case 'p':
                padding = true;
                break;
            case 'I':
                interrupts = true;
                break;
            case 'P':
                interrupts = false;
                break;
            case 't':
                throttle = atoi(optarg);
                break;
            case 'a':
                if (strcmp(optarg, "relu") == 0)
                    act_mode = act_relu;
                else {
                    cerr << "Unknown activation mode " << string(optarg) << endl;
                    return 1;
                }
                break;
            case '?':
                if (optopt == 'd' || optopt == 'p' || optopt == 'o' || optopt == 's' ||
                    optopt == 'c' || optopt == 'k' || optopt == 'u' || optopt == 'a')
                    cerr << "Option -" << char(optopt) << " requires an argument." << endl;
                else if (isprint(optopt))
                    cerr << "Unknown option -" << char(optopt) << endl;
                else
                    cerr << "Unknown option character " << static_cast<int>(optopt) << endl;
                return 1;
            default:
                abort();
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
    c2d.set_verbose(Conv2DTest::Verbosity::Debug);
    c2d.set_image_size(image_size, image_size);
    c2d.set_kernel_size(kernel_size, kernel_size);
    c2d.set_channel_count(input_channels, output_channels);
    c2d.set_activation_mode(act_mode);
    c2d.set_requantize(requantize);
    c2d.set_padding_mode(padding);
    c2d.set_bias(!zero_bias);
    c2d.set_debug_clean_buffers(debug_mode);
    c2d.use_interrupts(interrupts);
    c2d.set_psum_throttle(throttle);

    cout << "preparing parameters and test data" << endl;
    #ifdef __linux__
    c2d.prepare_run(files_path);
    #else
    c2d.prepare_run(string());
    #endif

    cout << "writing accelerator configuration and data" << endl;
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
        recacc_control_stop(&dev);
        return 1;
    }

    if (!dryrun) {
        auto cycles = c2d.get_cycle_count();
        float microseconds = 1.0 * cycles / 100000000 * 100000; // 100 MHz
        cout << "conv2d took " << c2d.get_cycle_count() << " cycles on accelerator (" << microseconds << "us @100MHz)" << endl;
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
