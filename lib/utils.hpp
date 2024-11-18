#include "types.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <random>
#include <chrono>

template<typename T> void print_buffer(void* data, size_t words, size_t offset = 0, size_t words_per_line = 8, int32_t highlight = -1) {
    T* ints = static_cast<T*>(data) + offset;
    if (highlight != -1)
        highlight = (highlight - offset) / words_per_line;
    for (size_t i=0; i<words; i+=1) {
        if (i % words_per_line == 0)
            std::cout << "0x" << std::setfill('0') << std::hex << std::setw(4) << i + offset << ": ";
        using unsigned_type = std::make_unsigned_t<T>;
        std::cout << std::setfill('0') << std::hex << std::setw(sizeof(T)*2)
             << (static_cast<int32_t>(ints[i]) & std::numeric_limits<unsigned_type>::max());
        if (i % words_per_line != 7)
            std::cout << ' ';
        else {
            if (highlight != -1 && static_cast<size_t>(highlight) == (i / words_per_line))
                std::cout << " <--";
            std::cout << std::endl;
        }
    }
    std::cout << std::dec;
    if (words % words_per_line)
        putchar('\n');
}

static std::mt19937 mtrnd(std::chrono::system_clock::now().time_since_epoch().count());

template <typename T> void generate_random_data(T* ptr, size_t n) {
    for (auto i = 0u; i < n; i++)
        ptr[i] = static_cast<T>(mtrnd() % 256);
}

// read integers from simulation files
template<typename T> size_t read_text_data(T* buffer, size_t size, std::string path) {
    size_t i = 0;
    long int tmp;
    std::ifstream infile(path);
    while (i < size && infile >> tmp)
        buffer[i++] = tmp;
    return i;
}

template<typename T> size_t write_text_data(T* buffer, size_t size, size_t stride, std::string path) {
    size_t i = 0;
    std::ofstream outfile(path);
    while (i < size && outfile.good()) {
        outfile << buffer[i++] << ' ';
        if (i % stride == 0)
            outfile << '\n';
    }
    return i;
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

static auto make_multiple_of(auto div, auto value) {
    auto remainder = value % div;
    if (remainder > 0)
        value += div - remainder;
    return value;
}

void print_hwinfo(const recacc_hwinfo& hwinfo) {
    std::cout << "Accelerator configuration:" << std::endl;
    std::cout << " array size: " << hwinfo.array_size_y
              << "x" << hwinfo.array_size_x
              << ", max och " << static_cast<int>(hwinfo.max_output_channels) << std::endl;
    std::cout << " data width:"
              << " iact " << static_cast<int>(hwinfo.data_width_bits_iact)
              << " wght " << static_cast<int>(hwinfo.data_width_bits_wght)
              << " psum " << static_cast<int>(hwinfo.data_width_bits_psum) << std::endl;
    std::cout << " pe buffers:"
              << " iact " << hwinfo.line_length_iact
              << " wght " << hwinfo.line_length_wght
              << " psum " << hwinfo.line_length_psum << std::endl;
    std::cout << " scratchpad sizes:";
    std::cout << " iact " << hwinfo.spad_size_iact
              << " wght " << hwinfo.spad_size_wght
              << " psum " << hwinfo.spad_size_psum << std::endl;
}
