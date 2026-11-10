#ifndef EAGLE_VIEWER_YUV_H
#define EAGLE_VIEWER_YUV_H

#include <stddef.h>
#include <wchar.h>

typedef enum YuvFormat {
    YUV_NONE, YUV_UYVY, YUV_VYUY, YUV_YVYU, YUV_YUYV,
    YUV_YV12, YUV_I420, YUV_NV12, YUV_NV21
} YuvFormat;

YuvFormat yuv_format(const wchar_t *name);
int yuv_frame_size(YuvFormat format, int width, int height, size_t *size);
void yuv_decode(YuvFormat format, int width, int height, const unsigned char *source,
                unsigned char *pixels, const double gain[3]);

#endif
