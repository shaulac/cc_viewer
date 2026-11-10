#include "image.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void set_error(wchar_t *error, size_t error_count, const wchar_t *message)
{
    if (error == NULL || error_count == 0) {
        return;
    }

    wcsncpy(error, message, error_count - 1);
    error[error_count - 1] = L'\0';
}

static void set_decoder_error(wchar_t *error, size_t error_count)
{
    const char *reason = stbi_failure_reason();

    if (error == NULL || error_count == 0) {
        return;
    }

    if (reason == NULL ||
        MultiByteToWideChar(CP_UTF8, 0, reason, -1, error, (int)error_count) == 0) {
        set_error(error, error_count, L"Unsupported or invalid image file.");
    }
}

ImageFileType image_detect_file(const wchar_t *path, wchar_t *error, size_t error_count)
{
    static const wchar_t *extensions[] = {
        L".bmp", L".gif", L".jpg", L".jpeg", L".png", L".tga",
        L".psd", L".pic", L".pnm", L".pgm", L".ppm"
    };
    unsigned char header[8];
    DWORD count = 0;
    HANDLE file;
    const wchar_t *dot, *slash, *other_slash;
    size_t i;
    if (path == NULL) {
        set_error(error, error_count, L"Invalid image path.");
        return IMAGE_FILE_ERROR;
    }
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        set_error(error, error_count, L"Could not open the image file.");
        return IMAGE_FILE_ERROR;
    }
    if (!ReadFile(file, header, sizeof(header), &count, NULL)) {
        CloseHandle(file);
        set_error(error, error_count, L"Could not read the image file.");
        return IMAGE_FILE_ERROR;
    }
    if (!CloseHandle(file)) {
        set_error(error, error_count, L"Could not close the image file after detection.");
        return IMAGE_FILE_ERROR;
    }
    dot = wcsrchr(path, L'.');
    slash = wcsrchr(path, L'\\');
    other_slash = wcsrchr(path, L'/');
    if (dot != NULL && (slash == NULL || dot > slash) &&
        (other_slash == NULL || dot > other_slash) && _wcsicmp(dot, L".raw") == 0) {
        return IMAGE_FILE_RAW;
    }
    if ((count >= 8 && memcmp(header, "\x89PNG\r\n\x1a\n", 8) == 0) ||
        (count >= 3 && memcmp(header, "\xff\xd8\xff", 3) == 0) ||
        (count >= 6 && (memcmp(header, "GIF87a", 6) == 0 || memcmp(header, "GIF89a", 6) == 0)) ||
        (count >= 2 && memcmp(header, "BM", 2) == 0) ||
        (count >= 4 && (memcmp(header, "8BPS", 4) == 0 ||
                        memcmp(header, "\x53\x80\xf6\x34", 4) == 0)) ||
        (count >= 3 && header[0] == 'P' && (header[1] == '5' || header[1] == '6') &&
         (header[2] == ' ' || header[2] == '\t' || header[2] == '\r' || header[2] == '\n'))) {
        return IMAGE_FILE_ENCODED;
    }
    if (dot != NULL && (slash == NULL || dot > slash) &&
        (other_slash == NULL || dot > other_slash)) {
        for (i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
            if (_wcsicmp(dot, extensions[i]) == 0) return IMAGE_FILE_ENCODED;
        }
    }
    return IMAGE_FILE_RAW;
}

int image_load_file(const wchar_t *path, Image *image, wchar_t *error, size_t error_count)
{
    HANDLE file;
    LARGE_INTEGER file_size;
    unsigned char *file_data;
    DWORD bytes_read;
    int width;
    int height;
    size_t stride;
    size_t pixel_bytes;
    unsigned char *pixels;
    size_t offset;

    if (image == NULL || path == NULL) {
        set_error(error, error_count, L"Invalid image path or output image.");
        return 0;
    }

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        set_error(error, error_count, L"Could not open the image file.");
        return 0;
    }

    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0 ||
        file_size.QuadPart > INT_MAX) {
        CloseHandle(file);
        set_error(error, error_count, L"Image file is empty or too large to decode.");
        return 0;
    }

    file_data = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)file_size.QuadPart);
    if (file_data == NULL) {
        CloseHandle(file);
        set_error(error, error_count, L"Not enough memory to read the image file.");
        return 0;
    }

    if (!ReadFile(file, file_data, (DWORD)file_size.QuadPart, &bytes_read, NULL) ||
        bytes_read != (DWORD)file_size.QuadPart) {
        HeapFree(GetProcessHeap(), 0, file_data);
        CloseHandle(file);
        set_error(error, error_count, L"Could not read the complete image file.");
        return 0;
    }
    CloseHandle(file);

    pixels = stbi_load_from_memory(file_data, (int)file_size.QuadPart,
                                   &width, &height, NULL, 4);
    HeapFree(GetProcessHeap(), 0, file_data);

    if (pixels == NULL) {
        set_decoder_error(error, error_count);
        return 0;
    }

    if (width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / 4) {
        stbi_image_free(pixels);
        set_error(error, error_count, L"Image dimensions are invalid or too large.");
        return 0;
    }

    stride = (size_t)width * 4;
    if ((size_t)height > SIZE_MAX / stride) {
        stbi_image_free(pixels);
        set_error(error, error_count, L"Image dimensions are too large.");
        return 0;
    }
    pixel_bytes = stride * (size_t)height;

    for (offset = 0; offset < pixel_bytes; offset += 4) {
        unsigned char red = pixels[offset];
        pixels[offset] = pixels[offset + 2];
        pixels[offset + 2] = red;
    }

    image->width = width;
    image->height = height;
    image->stride = stride;
    image->pixels = pixels;
    image->gray_raw = NULL;
    if (error != NULL && error_count > 0) {
        error[0] = L'\0';
    }
    return 1;
}

static int write_all(HANDLE file, const void *data, DWORD size)
{
    DWORD written = 0;
    return WriteFile(file, data, size, &written, NULL) && written == size;
}

static int save_bmp(const wchar_t *path, const Image *image, int opaque,
                    wchar_t *error, size_t error_count)
{
    BITMAPFILEHEADER file_header = {0};
    BITMAPINFOHEADER info_header = {0};
    size_t pixel_bytes;
    size_t row_size;
    size_t input_row_size;
    unsigned char *converted = NULL;
    int row;
    int ok = 1;
    HANDLE file;

    if (path == NULL || image == NULL || image->pixels == NULL ||
        image->width <= 0 || image->height <= 0 ||
        (size_t)image->width > SIZE_MAX / 4) {
        set_error(error, error_count, L"Invalid image or output path.");
        return 0;
    }

    input_row_size = (size_t)image->width * 4;
    row_size = opaque ? ((size_t)image->width * 3 + 3) & ~(size_t)3 : input_row_size;
    if (image->stride < input_row_size || (size_t)image->height > SIZE_MAX / image->stride ||
        (size_t)image->height > (UINT32_MAX - sizeof(file_header) - sizeof(info_header)) / row_size) {
        set_error(error, error_count, L"Image is too large to save as BMP.");
        return 0;
    }
    pixel_bytes = row_size * (size_t)image->height;
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = (DWORD)(file_header.bfOffBits + pixel_bytes);
    info_header.biSize = sizeof(info_header);
    info_header.biWidth = image->width;
    info_header.biHeight = -image->height;
    info_header.biPlanes = 1;
    info_header.biBitCount = opaque ? 24 : 32;
    info_header.biCompression = BI_RGB;
    info_header.biSizeImage = (DWORD)pixel_bytes;

    if (opaque) {
        converted = (unsigned char *)calloc(row_size, 1);
        if (converted == NULL) {
            set_error(error, error_count, L"Not enough memory to save the BMP.");
            return 0;
        }
    }
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        free(converted);
        set_error(error, error_count, L"Could not create the BMP file.");
        return 0;
    }
    ok = write_all(file, &file_header, sizeof(file_header)) &&
         write_all(file, &info_header, sizeof(info_header));
    for (row = 0; ok && row < image->height; ++row) {
        const unsigned char *source = image->pixels + (size_t)row * image->stride;
        if (opaque) {
            int x;
            for (x = 0; x < image->width; ++x) {
                memcpy(converted + (size_t)x * 3, source + (size_t)x * 4, 3);
            }
            source = converted;
        }
        ok = write_all(file, source, (DWORD)row_size);
    }
    free(converted);
    if (!CloseHandle(file)) {
        ok = 0;
    }
    if (!ok) {
        set_error(error, error_count, L"Could not write the complete BMP file.");
        return 0;
    }
    if (error != NULL && error_count > 0) {
        error[0] = L'\0';
    }
    return 1;
}

int image_save_bmp(const wchar_t *path, const Image *image, wchar_t *error, size_t error_count)
{
    return save_bmp(path, image, 0, error, error_count);
}

int image_save_bmp_opaque(const wchar_t *path, const Image *image, wchar_t *error, size_t error_count)
{
    return save_bmp(path, image, 1, error, error_count);
}

void image_free(Image *image)
{
    if (image == NULL) {
        return;
    }

    stbi_image_free(image->pixels);
    free(image->gray_raw);
    image->pixels = NULL;
    image->gray_raw = NULL;
    image->width = 0;
    image->height = 0;
    image->stride = 0;
}
