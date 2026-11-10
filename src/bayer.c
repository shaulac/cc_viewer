#include "bayer.h"

enum { BLUE, GREEN, RED };

const double RAW_IDENTITY_CCM[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

static int color_at(BayerPattern pattern, int x, int y)
{
    static const unsigned char colors[4][4] = {
        {RED, GREEN, GREEN, BLUE},
        {GREEN, RED, BLUE, GREEN},
        {BLUE, GREEN, GREEN, RED},
        {GREEN, BLUE, RED, GREEN}
    };
    return colors[pattern][(y & 1) * 2 + (x & 1)];
}

static unsigned sample_at(const unsigned char *source, const RawParams *params,
                          int x, int y, unsigned bits)
{
    size_t index = (size_t)y * (size_t)params->width + (size_t)x;
    unsigned value;
    if (bits == 8) return source[index];
    source += index * 2;
    value = params->big_endian ? ((unsigned)source[0] << 8) | source[1] :
                                 ((unsigned)source[1] << 8) | source[0];
    if (params->alignment == RAW_ALIGN_MSB) value >>= 16 - bits;
    return value & ((1u << bits) - 1u);
}

static void apply_pixel(const double rgb[3], unsigned char *out,
                        unsigned maximum, const double ccm[9])
{
    int channel;
    for (channel = 0; channel < 3; ++channel) {
        double value = ccm[channel * 3] * rgb[0] +
                       ccm[channel * 3 + 1] * rgb[1] +
                       ccm[channel * 3 + 2] * rgb[2];
        if (value < 0) value = 0;
        if (value > maximum) value = maximum;
        out[2 - channel] = (unsigned char)(value * 255.0 / maximum + 0.5);
    }
    out[3] = 255;
}

void bayer_apply_ccm(const double *rgb, unsigned char *pixels, size_t count,
                     unsigned maximum, const double ccm[9])
{
    size_t i;
    for (i = 0; i < count; ++i) {
        apply_pixel(rgb + i * 3, pixels + i * 4, maximum, ccm);
    }
}

void bayer_decode(const unsigned char *source, unsigned char *pixels,
                  const RawParams *params, BayerPattern pattern, unsigned bits,
                  const double gain[3], const double ccm[9], double *rgb_cache)
{
    unsigned maximum = (1u << bits) - 1u;
    int y, x;
    for (y = 0; y < params->height; ++y) {
        for (x = 0; x < params->width; ++x) {
            double sum[3] = {0, 0, 0};
            double rgb[3];
            unsigned count[3] = {0, 0, 0};
            int dx, dy, channel;
            unsigned char *out = pixels + ((size_t)y * (size_t)params->width + (size_t)x) * 4;
            for (dy = -1; dy <= 1; ++dy) {
                int yy = y + dy;
                if (yy < 0 || yy >= params->height) continue;
                for (dx = -1; dx <= 1; ++dx) {
                    int xx = x + dx;
                    int color;
                    if (xx < 0 || xx >= params->width) continue;
                    color = color_at(pattern, xx, yy);
                    if ((dx != 0 || dy != 0) && color == color_at(pattern, x, y)) continue;
                    {
                        double value = sample_at(source, params, xx, yy, bits) * gain[2 - color];
                        sum[color] += value > maximum ? maximum : value;
                    }
                    ++count[color];
                }
            }
            for (channel = BLUE; channel <= RED; ++channel) {
                rgb[2 - channel] = sum[channel] / count[channel];
            }
            if (rgb_cache != NULL) {
                size_t index = ((size_t)y * (size_t)params->width + (size_t)x) * 3;
                for (channel = 0; channel < 3; ++channel) rgb_cache[index + channel] = rgb[channel];
            }
            apply_pixel(rgb, out, maximum, ccm);
        }
    }
}
