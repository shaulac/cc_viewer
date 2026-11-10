#ifndef EAGLE_VIEWER_IMAGE_H
#define EAGLE_VIEWER_IMAGE_H

#include <stddef.h>
#include <wchar.h>

typedef struct Image {
    int width;
    int height;
    size_t stride;
    unsigned char *pixels;
    unsigned char *gray_raw;
} Image;

typedef enum ImageFileType {
    IMAGE_FILE_ERROR = -1,
    IMAGE_FILE_RAW = 0,
    IMAGE_FILE_ENCODED = 1
} ImageFileType;

ImageFileType image_detect_file(const wchar_t *path, wchar_t *error, size_t error_count);
int image_load_file(const wchar_t *path, Image *image, wchar_t *error, size_t error_count);
int image_save_bmp(const wchar_t *path, const Image *image, wchar_t *error, size_t error_count);
int image_save_bmp_opaque(const wchar_t *path, const Image *image, wchar_t *error, size_t error_count);
void image_free(Image *image);

#endif
