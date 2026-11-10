#ifndef EAGLE_VIEWER_PIXEL_RENDER_H
#define EAGLE_VIEWER_PIXEL_RENDER_H

#include "image.h"

void render_nearest_pixels(unsigned char *destination, size_t stride, int width, int height,
                           int left, int top, double image_left, double image_top,
                           double scale, const Image *image);

#endif
