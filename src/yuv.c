#include "yuv.h"

#include <stdint.h>

YuvFormat yuv_format(const wchar_t *name)
{
    static const wchar_t *names[] = {L"", L"uyvy", L"vyuy", L"yvyu", L"yuyv",
                                     L"yv12", L"i420", L"nv12", L"nv21"};
    int i;
    if (name == NULL) return YUV_NONE;
    for (i = YUV_UYVY; i <= YUV_NV21; ++i) {
        if (_wcsicmp(name, names[i]) == 0) return (YuvFormat)i;
    }
    return YUV_NONE;
}

int yuv_frame_size(YuvFormat format, int width, int height, size_t *size)
{
    size_t pixels;
    if (format <= YUV_NONE || format > YUV_NV21 || width <= 0 || height <= 0 ||
        (width & 1) != 0 || (format >= YUV_YV12 && (height & 1) != 0) ||
        (size_t)width > SIZE_MAX / (size_t)height) return 0;
    pixels = (size_t)width * (size_t)height;
    if (format <= YUV_YUYV) {
        if (pixels > SIZE_MAX / 2) return 0;
        *size = pixels * 2;
    } else {
        if (pixels > SIZE_MAX - pixels / 2) return 0;
        *size = pixels + pixels / 2;
    }
    return 1;
}

static double clamp(double value)
{
    if (value < 0.0) return 0.0;
    if (value > 255.0) return 255.0;
    return value;
}

void yuv_decode(YuvFormat format, int width, int height, const unsigned char *source,
                unsigned char *pixels, const double gain[3])
{
    size_t plane = (size_t)width * (size_t)height;
    int x, y;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            unsigned yy, u, v;
            size_t index = (size_t)y * (size_t)width + (size_t)x;
            unsigned char *out = pixels + index * 4;
            if (format <= YUV_YUYV) {
                const unsigned char *p = source + ((size_t)y * (size_t)width + (size_t)(x & ~1)) * 2;
                switch (format) {
                case YUV_UYVY: u = p[0]; yy = p[(x & 1) ? 3 : 1]; v = p[2]; break;
                case YUV_VYUY: v = p[0]; yy = p[(x & 1) ? 3 : 1]; u = p[2]; break;
                case YUV_YVYU: yy = p[(x & 1) ? 2 : 0]; v = p[1]; u = p[3]; break;
                default: yy = p[(x & 1) ? 2 : 0]; u = p[1]; v = p[3]; break;
                }
            } else {
                size_t chroma = (size_t)(y / 2) * (size_t)(width / 2) + (size_t)(x / 2);
                yy = source[index];
                if (format == YUV_I420 || format == YUV_YV12) {
                    unsigned first = source[plane + chroma];
                    unsigned second = source[plane + plane / 4 + chroma];
                    u = format == YUV_I420 ? first : second;
                    v = format == YUV_I420 ? second : first;
                } else {
                    unsigned first = source[plane + chroma * 2];
                    unsigned second = source[plane + chroma * 2 + 1];
                    u = format == YUV_NV12 ? first : second;
                    v = format == YUV_NV12 ? second : first;
                }
            }
            {
                double luminance = clamp(yy * gain[0]);
                double cb = clamp(128.0 + ((double)u - 128.0) * gain[1]) - 128.0;
                double cr = clamp(128.0 + ((double)v - 128.0) * gain[2]) - 128.0;
                out[2] = (unsigned char)(clamp(luminance + 1.402 * cr) + 0.5);
                out[1] = (unsigned char)(clamp(luminance - 0.344136 * cb - 0.714136 * cr) + 0.5);
                out[0] = (unsigned char)(clamp(luminance + 1.772 * cb) + 0.5);
                out[3] = 255;
            }
        }
    }
}
