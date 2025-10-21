#include "utils.hpp"

using namespace std;

void print_hwinfo(const recacc_hwinfo& hwinfo) {
    cout << "Accelerator configuration:" << endl;
    cout << " array size: " << hwinfo.array_size_y
         << "x" << hwinfo.array_size_x << endl;
    cout << " data width:"
         << " iact " << static_cast<int>(hwinfo.data_width_bits_iact)
         << " wght " << static_cast<int>(hwinfo.data_width_bits_wght)
         << " psum " << static_cast<int>(hwinfo.data_width_bits_psum) << endl;
    cout << " pe buffers:"
         << " iact " << hwinfo.line_length_iact
         << " wght " << hwinfo.line_length_wght
         << " psum " << hwinfo.line_length_psum << endl;
    cout << " fifos:"
         << " psum " << hwinfo.fifo_size_psum << endl;
    cout << " scratchpad: " << hwinfo.spad_size
         << " bytes (word size " << hwinfo.spad_word_size << ")" << endl;
    cout << " trs " << hwinfo.trs_dataflow
         << " postproc " << hwinfo.bias_requant_available
         << " max och " << static_cast<int>(hwinfo.max_output_channels) << endl;
}

void memcpy_align_src(void* dst, void* src, size_t size) {
    const size_t align_to = 16;
    const size_t src_addr = reinterpret_cast<size_t>(src);

    size_t unaligned_bytes_start = (align_to - src_addr % align_to) % align_to;
    size_t unaligned_bytes_end = (align_to - (size - unaligned_bytes_start) % align_to) % align_to;
    size_t aligned_bytes = size - unaligned_bytes_end - unaligned_bytes_start;

    // making these pointers volatile stops the compiler from fusing the byte-wise copy with the large copy
    volatile uint8_t* src_u8 = reinterpret_cast<volatile uint8_t*>(src);
    uint8_t* dst_u8 = reinterpret_cast<uint8_t*>(dst);

    // cout << "memcpy_align_src " << unaligned_bytes_start << "+" << aligned_bytes << "+" << unaligned_bytes_end
    //      << "=" << size << " bytes from " << src << " to " << dst << endl;

    for (size_t n = 0; n < unaligned_bytes_start; n++)
        *dst_u8++ = *src_u8++;

    // memcpy(dst_u8, src_u8, aligned_bytes);
    copy(src_u8, src_u8 + aligned_bytes, dst_u8);
    dst_u8 += aligned_bytes;
    src_u8 += aligned_bytes;

    for (size_t n = 0; n < unaligned_bytes_end; n++)
        *dst_u8++ = *src_u8++;
}

void dump_status_register(recacc_device* dev) {
    union recacc_status_reg status;
    status = recacc_get_status(dev);
    cout << "Status register 0x" << hex << setfill('0') << setw(8) << status.raw << dec << ":" << endl;
    cout << "  ready     " << status.decoded.ready << endl;
    cout << "  done      " << status.decoded.done << endl;
    cout << "  irq       " << status.decoded.irq << endl;
    cout << "  iact_done " << status.decoded.ctrl_iact_done << endl;
    cout << "  wght_done " << status.decoded.ctrl_wght_done << endl;
    cout << "  preload   " << status.decoded.preload_done << endl;
}
