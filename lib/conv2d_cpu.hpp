// act = 3d tensor with batch size always 1
#include <algorithm>
#include <cmath>
#include <limits>

template <typename Tin, typename Tout> void conv2d_cpu(
    Tin* act, Tin* wght, Tout* bias, Tout* result,
    int in_channels, int in_width, int in_height,
    int k_count, int k_width, int k_height,
    int stride_x, int stride_y, int padding_x, int padding_y)
{
    // convert values to output type to ensure calculations use output type width
    Tout accumulator, act_value, wght_value;
    int x, y;
    int out_height, out_width;

    out_width = (in_width + 2 * padding_x - k_width) / stride_x + 1;
    out_height = (in_height + 2 * padding_y - k_height) / stride_y + 1;

    for (int k_id = 0; k_id < k_count; k_id++) {
    for (int oy = 0; oy < out_height; oy++) {
    for (int ox = 0; ox < out_width; ox++) {
        accumulator = 0.0f;
        for (int ch = 0; ch < in_channels; ch++) {
        for (int offset_y = 0; offset_y < k_height; offset_y++) {
        for (int offset_x = 0; offset_x < k_width; offset_x++) {
            x = (ox * stride_x) + offset_x - padding_x;
            y = (oy * stride_y) + offset_y - padding_y;

            if ((x < 0 || y < 0) || (x >= in_width || y >= in_height)) // padding case: here only zero padding
                act_value = 0.0f;
            else
                act_value = act[(ch * in_height * in_width) + (y * in_width) + x];

            wght_value = wght[(k_id * in_channels * k_height * k_width)
                + (ch * k_height * k_width)
                + (offset_y * k_width) + offset_x];

            accumulator += act_value * wght_value;
        }
        }
        }

        if (bias != nullptr)
            accumulator += bias[k_id];
        result[(k_id * out_height * out_width) + (oy * out_width) + ox] = accumulator;
    }
    }
    }
}

template <typename Tin, typename Tout> void requantize_cpu(
    Tin* psum, Tout* result, float* factors, float* zeropts,
    int channels, int image_size)
{
    for (int channel = 0; channel < channels; channel++) {
        const float scale = factors[channel];
        const float zeropt = zeropts[channel];
        const int chan_offset = image_size * channel;
        for (int i = 0; i < image_size; i++) {
            float tmp = psum[chan_offset + i];
            tmp = tmp * scale + zeropt;
            std::numeric_limits<Tout> limits;
            result[chan_offset + i] = std::clamp(
                static_cast<Tin>(round(tmp)),
                static_cast<Tin>(limits.min()),
                static_cast<Tin>(limits.max())
            );
        }
    }
}
