# FleXNNgine Test Software

This repo contains test and benchmarking software for the single-tile prototype on a ZCU104 board.

## Bitstream & Device Tree preparation

Recent Petalinux and Ubuntu builds for AMD/Xilinx boards do not configure any PL devices nor enable any PL clocks.
Thus, we need to load a device tree overlay at runtime alongside the PL bitstream.

1. Build the device tree overlays in the `devicetree` subdirectory:
```bash
$ make -C devicetree
make: Verzeichnis „/home/ad9150/git/cecas/flexnngine-driver/devicetree“ wird betreten
dtc -@ -O dtb -o zcu104-uio-dma.dtbo zcu104-uio-dma.dtsi
zcu104-uio-dma.dtsi:74.24-81.5: Warning (unit_address_vs_reg): /fragment@1/__overlay__/dma@a1000000/dma-channel@a1000000: node has a unit name, but no reg or ranges property
dtc -@ -O dtb -o zcu104-uio-only.dtbo zcu104-uio-only.dtsi
make: Verzeichnis „/home/ad9150/git/cecas/flexnngine-driver/devicetree“ wird verlassen
```

2. Copy bitstream (`*.bit` or `*.bit.bin`) & device tree overlays (`*.dtbo`) to the target board

3. Use `load-bistream-and-overlay.sh` to load both:

```bash
$ ./load-bistream-and-overlay.sh flexnngine-rev6-srs.bit zcu104-uio-only.dtbo
```

## Building

Make sure to check out the proper driver version for the hardware revision you are using.
For example, hardware revision 3 requires the driver tagged with `rev3`:
```bash
$ git checkout rev3
```
The driver will complain if the revision does not match,.

Running `make` compiles the driver and all test programs.
Use `make -j4` to build faster and `make debug -j4` to build with debug information.

*Note: sometimes non-debug builds crash, probably due to -O3 and the hacky result memcpy implementation without alignment. Use debug builds if you experience spurious segfaults.*

## Test a single convolution operation

`./test-conv2d` runs a simple standard configuration of a 2D convolution with 32x32 input images, 3x3 kernels, 8 input channels and 3 output channels.
Random input data is used and the result is calculated both on FleXNNgine and on CPU.
If the result shows errors to the CPU reference, an error is shown.
Use `./test-conv2d -h` to list available options.
For example, another convolution can be run as follows:
```bash
$ ./test-conv2d -p -r -a relu -s 128 -c 24 -k 7 -u 1
```

## The Conv2D Testsuite

`./conv2d-testsuite` runs lots of convolutions with different parameter permutations.
The performance is evaluated between pure CPU execution and copy-in+FleXNNgine+copy-out, which is printed as the "speed-up" factor alongside the timing measurements.
