#include "pixel_render.h"

void render_nearest_pixels(unsigned char *destination, size_t stride, int width, int height,
                           int left, int top, double image_left, double image_top,
                           double scale, const Image *image)
{
    int x, y;
    for (y = 0; y < height; ++y) {
        int source_y = (int)(((double)top + y - image_top) / scale);
        const unsigned char *row;
        if (source_y >= image->height) source_y = image->height - 1;
        row = image->pixels + (size_t)source_y * image->stride;
        for (x = 0; x < width; ++x) {
            int source_x = (int)(((double)left + x - image_left) / scale);
            unsigned char *dest = destination + (size_t)y * stride + (size_t)x * 4;
            const unsigned char *source;
            if (source_x >= image->width) source_x = image->width - 1;
            source = row + (size_t)source_x * 4;
            dest[0] = source[0];
            dest[1] = source[1];
            dest[2] = source[2];
            dest[3] = source[3];
        }
    }
}
