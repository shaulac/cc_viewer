#include "raw.h"
#include "bayer.h"
#include "yuv.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

typedef void (*DecodePixel)(const unsigned char *source, unsigned char *target,
                            const RawParams *params);

typedef struct RawFormat {
    const wchar_t *name;
    unsigned bytes_per_pixel;
    DecodePixel decode;
    unsigned bayer_bits;
    BayerPattern bayer_pattern;
} RawFormat;

static void decode_gray8(const unsigned char *source, unsigned char *target,
                         const RawParams *params)
{
    (void)params;
    target[0] = target[1] = target[2] = source[0];
    target[3] = 255;
}

static unsigned read_word(const unsigned char *source, const RawParams *params)
{
    return params->big_endian ?
        ((unsigned)source[0] << 8) | source[1] :
        ((unsigned)source[1] << 8) | source[0];
}

static void decode_gray16(const unsigned char *source, unsigned char *target,
                          const RawParams *params)
{
    unsigned bits = _wcsicmp(params->format, L"gray10") == 0 ? 10 :
                    _wcsicmp(params->format, L"gray12") == 0 ? 12 : 16;
    unsigned maximum = (1u << bits) - 1u;
    unsigned value = read_word(source, params);
    if (params->alignment == RAW_ALIGN_MSB) value >>= 16 - bits;
    value &= maximum;
    value = (value * 255u + maximum / 2u) / maximum;
    target[0] = target[1] = target[2] = (unsigned char)value;
    target[3] = 255;
}

static void decode_rgb565(const unsigned char *source, unsigned char *target,
                          const RawParams *params)
{
    unsigned value = read_word(source, params);
    unsigned red = (value >> 11) & 31;
    unsigned green = (value >> 5) & 63;
    unsigned blue = value & 31;
    target[0] = (unsigned char)((blue << 3) | (blue >> 2));
    target[1] = (unsigned char)((green << 2) | (green >> 4));
    target[2] = (unsigned char)((red << 3) | (red >> 2));
    target[3] = 255;
}

static void decode_rgb555(const unsigned char *source, unsigned char *target,
                          const RawParams *params)
{
    unsigned value = read_word(source, params);
    unsigned red = (value >> 10) & 31;
    unsigned green = (value >> 5) & 31;
    unsigned blue = value & 31;
    target[0] = (unsigned char)((blue << 3) | (blue >> 2));
    target[1] = (unsigned char)((green << 3) | (green >> 2));
    target[2] = (unsigned char)((red << 3) | (red >> 2));
    target[3] = 255;
}

static void decode_rgb888(const unsigned char *source, unsigned char *target,
                          const RawParams *params)
{
    (void)params;
    target[0] = source[2];
    target[1] = source[1];
    target[2] = source[0];
    target[3] = 255;
}

static const RawFormat formats[] = {
    {L"gray8", 1, decode_gray8, 0, BAYER_RGGB},
    {L"gray10", 2, decode_gray16, 0, BAYER_RGGB},
    {L"gray12", 2, decode_gray16, 0, BAYER_RGGB},
    {L"gray16", 2, decode_gray16, 0, BAYER_RGGB},
    {L"rgb555", 2, decode_rgb555, 0, BAYER_RGGB},
    {L"rgb565", 2, decode_rgb565, 0, BAYER_RGGB},
    {L"rgb888", 3, decode_rgb888, 0, BAYER_RGGB},
#define BAYER_FORMATS(name, pattern) \
    {L##name L"8", 1, NULL, 8, pattern}, \
    {L##name L"10", 2, NULL, 10, pattern}, \
    {L##name L"12", 2, NULL, 12, pattern}, \
    {L##name L"16", 2, NULL, 16, pattern}
    BAYER_FORMATS("rggb", BAYER_RGGB),
    BAYER_FORMATS("grbg", BAYER_GRBG),
    BAYER_FORMATS("bggr", BAYER_BGGR),
    BAYER_FORMATS("gbrg", BAYER_GBRG),
#undef BAYER_FORMATS
    {L"uyvy", 0, NULL, 0, BAYER_RGGB}, {L"vyuy", 0, NULL, 0, BAYER_RGGB},
    {L"yvyu", 0, NULL, 0, BAYER_RGGB}, {L"yuyv", 0, NULL, 0, BAYER_RGGB},
    {L"yv12", 0, NULL, 0, BAYER_RGGB}, {L"i420", 0, NULL, 0, BAYER_RGGB},
    {L"nv12", 0, NULL, 0, BAYER_RGGB}, {L"nv21", 0, NULL, 0, BAYER_RGGB}
};

size_t raw_format_count(void)
{
    return sizeof(formats) / sizeof(formats[0]);
}

int raw_is_yuv(const wchar_t *format)
{
    return yuv_format(format) != YUV_NONE;
}

const wchar_t *raw_format_name(size_t index)
{
    return index < raw_format_count() ? formats[index].name : NULL;
}

int raw_is_bayer(const wchar_t *format)
{
    size_t i;
    if (format == NULL) return 0;
    for (i = 0; i < raw_format_count(); ++i) {
        if (_wcsicmp(format, formats[i].name) == 0) return formats[i].bayer_bits != 0;
    }
    return 0;
}

int raw_gray_value(const Image *image, const RawParams *params, int x, int y, unsigned *value)
{
    unsigned bits;
    size_t index;
    if (image == NULL || params == NULL || params->format == NULL || value == NULL ||
        image->gray_raw == NULL || x < 0 || y < 0 || x >= image->width || y >= image->height) return 0;
    if (_wcsicmp(params->format, L"gray8") == 0) bits = 8;
    else if (_wcsicmp(params->format, L"gray10") == 0) bits = 10;
    else if (_wcsicmp(params->format, L"gray12") == 0) bits = 12;
    else if (_wcsicmp(params->format, L"gray16") == 0) bits = 16;
    else return 0;
    index = (size_t)y * (size_t)image->width + (size_t)x;
    *value = bits == 8 ? image->gray_raw[index] : read_word(image->gray_raw + index * 2, params);
    if (bits != 8 && params->alignment == RAW_ALIGN_MSB) *value >>= 16 - bits;
    *value &= (1u << bits) - 1u;
    return 1;
}

static void set_error(wchar_t *error, size_t error_count, const wchar_t *message)
{
    if (error != NULL && error_count > 0) {
        wcsncpy(error, message, error_count - 1);
        error[error_count - 1] = L'\0';
    }
}

static int valid_ccm(const double ccm[9])
{
    size_t i;
    if (ccm == NULL) return 0;
    for (i = 0; i < 9; ++i) {
        if (!isfinite(ccm[i]) || fabs(ccm[i]) > DBL_MAX / (3.0 * 65535.0)) return 0;
    }
    return 1;
}

void raw_bayer_cache_free(RawBayerCache *cache)
{
    if (cache == NULL) return;
    free(cache->rgb);
    cache->rgb = NULL;
    cache->pixel_count = 0;
    cache->maximum = 0;
}

int raw_bayer_apply_ccm(const RawBayerCache *cache, Image *image, const double ccm[9],
                        wchar_t *error, size_t error_count)
{
    if (cache == NULL || cache->rgb == NULL || cache->maximum == 0 ||
        image == NULL || image->pixels == NULL || image->width <= 0 || image->height <= 0 ||
        (size_t)image->width > SIZE_MAX / (size_t)image->height ||
        (size_t)image->width > SIZE_MAX / 4 ||
        (size_t)image->width * (size_t)image->height != cache->pixel_count ||
        image->stride != (size_t)image->width * 4) {
        set_error(error, error_count, L"Invalid Bayer image cache.");
        return 0;
    }
    if (!valid_ccm(ccm)) {
        set_error(error, error_count, L"CCM coefficients must be finite and within the supported range.");
        return 0;
    }
    bayer_apply_ccm(cache->rgb, image->pixels, cache->pixel_count, cache->maximum, ccm);
    if (error != NULL && error_count > 0) error[0] = L'\0';
    return 1;
}

RawViewAction raw_view_action(const wchar_t *current_path, const RawParams *current,
                              const double current_gain[3], const double current_ccm[9],
                              const RawBayerCache *cache, const wchar_t *path,
                              const RawParams *params, const double gain[3], const double ccm[9])
{
    size_t i;
    int bayer;
    if (current_path == NULL || current == NULL || current->format == NULL ||
        path == NULL || params == NULL || params->format == NULL ||
        _wcsicmp(current_path, path) != 0 || current->width != params->width ||
        current->height != params->height || _wcsicmp(current->format, params->format) != 0 ||
        current->big_endian != params->big_endian || current->alignment != params->alignment) {
        return RAW_VIEW_RELOAD;
    }
    bayer = raw_is_bayer(params->format);
    if (bayer || raw_is_yuv(params->format)) {
        if (gain == NULL || current_gain == NULL) return RAW_VIEW_RELOAD;
        for (i = 0; i < 3; ++i) {
            if (gain[i] != current_gain[i]) return RAW_VIEW_RELOAD;
        }
    }
    if (bayer) {
        if (ccm == NULL || current_ccm == NULL) return RAW_VIEW_RELOAD;
        for (i = 0; i < 9; ++i) {
            if (ccm[i] != current_ccm[i]) {
                return cache != NULL && cache->rgb != NULL ? RAW_VIEW_APPLY_CCM : RAW_VIEW_RELOAD;
            }
        }
    }
    return RAW_VIEW_REUSE;
}

int raw_load_file_cached(const wchar_t *path, const RawParams *params, Image *image,
                         const double gain[3], const double ccm[9], RawBayerCache *cache,
                         wchar_t *error, size_t error_count)
{
    const RawFormat *format = NULL;
    size_t i;
    size_t pixel_count;
    size_t source_size;
    unsigned char *source;
    unsigned char *pixels;
    double *rgb_cache = NULL;
    HANDLE file;
    LARGE_INTEGER file_size;
    DWORD read_size;
    YuvFormat yuv;

    if (path == NULL || params == NULL || image == NULL || params->format == NULL ||
        params->width <= 0 || params->height <= 0) {
        set_error(error, error_count, L"RAW requires a format and positive width and height.");
        return 0;
    }
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        if (_wcsicmp(params->format, formats[i].name) == 0) {
            format = &formats[i];
            break;
        }
    }
    if (format == NULL) {
        set_error(error, error_count,
                  L"Unsupported RAW format. Select a supported format from the toolbar.");
        return 0;
    }
    yuv = yuv_format(format->name);
    if ((yuv != YUV_NONE || format->bayer_bits != 0) &&
        (gain == NULL || !isfinite(gain[0]) || !isfinite(gain[1]) ||
        !isfinite(gain[2]) || gain[0] < 0 || gain[1] < 0 || gain[2] < 0)) {
        set_error(error, error_count, L"Channel gains must be non-negative finite numbers.");
        return 0;
    }
    if (format->bayer_bits != 0) {
        if (!valid_ccm(ccm)) {
            set_error(error, error_count, L"CCM coefficients must be finite and within the supported range.");
            return 0;
        }
    }
    if (format->bayer_bits != 0 && (params->width < 2 || params->height < 2)) {
        set_error(error, error_count, L"Bayer images must be at least 2 by 2 pixels.");
        return 0;
    }
    if ((size_t)params->width > SIZE_MAX / (size_t)params->height) {
        set_error(error, error_count, L"RAW dimensions are too large.");
        return 0;
    }
    pixel_count = (size_t)params->width * (size_t)params->height;
    if (pixel_count > SIZE_MAX / 4) {
        set_error(error, error_count, L"RAW image is too large.");
        return 0;
    }
    if (format->bayer_bits != 0 && cache != NULL &&
        pixel_count > SIZE_MAX / (3 * sizeof(*rgb_cache))) {
        set_error(error, error_count, L"Bayer image is too large to cache.");
        return 0;
    }
    if (yuv != YUV_NONE) {
        if (!yuv_frame_size(yuv, params->width, params->height, &source_size)) {
            set_error(error, error_count, L"YUV 4:2:2 requires even width; YUV 4:2:0 requires even width and height.");
            return 0;
        }
    } else {
        if (pixel_count > SIZE_MAX / format->bytes_per_pixel) {
            set_error(error, error_count, L"RAW image is too large.");
            return 0;
        }
        source_size = pixel_count * format->bytes_per_pixel;
    }
    if (source_size > UINT32_MAX) {
        set_error(error, error_count, L"RAW frame is too large.");
        return 0;
    }
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        set_error(error, error_count, L"Could not open the RAW file.");
        return 0;
    }
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < (LONGLONG)source_size) {
        CloseHandle(file);
        set_error(error, error_count, L"RAW file is shorter than one image frame.");
        return 0;
    }
    source = (unsigned char *)malloc(source_size);
    pixels = (unsigned char *)malloc(pixel_count * 4);
    if (format->bayer_bits != 0 && cache != NULL) {
        rgb_cache = (double *)malloc(pixel_count * 3 * sizeof(*rgb_cache));
    }
    if (source == NULL || pixels == NULL ||
        (format->bayer_bits != 0 && cache != NULL && rgb_cache == NULL)) {
        free(source);
        free(pixels);
        free(rgb_cache);
        CloseHandle(file);
        set_error(error, error_count, L"Not enough memory to decode the RAW image.");
        return 0;
    }
    if (!ReadFile(file, source, (DWORD)source_size, &read_size, NULL) ||
        read_size != (DWORD)source_size) {
        free(source);
        free(pixels);
        free(rgb_cache);
        CloseHandle(file);
        set_error(error, error_count, L"Could not read the RAW frame.");
        return 0;
    }
    CloseHandle(file);
    if (yuv != YUV_NONE) {
        yuv_decode(yuv, params->width, params->height, source, pixels, gain);
    } else if (format->bayer_bits != 0) {
        bayer_decode(source, pixels, params, format->bayer_pattern, format->bayer_bits, gain, ccm, rgb_cache);
    } else {
        for (i = 0; i < pixel_count; ++i) {
            format->decode(source + i * format->bytes_per_pixel, pixels + i * 4, params);
        }
    }
    image->width = params->width;
    image->height = params->height;
    image->stride = (size_t)params->width * 4;
    image->pixels = pixels;
    image->gray_raw = format->decode == decode_gray8 || format->decode == decode_gray16 ? source : NULL;
    if (image->gray_raw == NULL) free(source);
    if (cache != NULL) {
        raw_bayer_cache_free(cache);
        cache->rgb = rgb_cache;
        cache->pixel_count = rgb_cache != NULL ? pixel_count : 0;
        cache->maximum = rgb_cache != NULL ? (1u << format->bayer_bits) - 1u : 0;
    }
    if (error != NULL && error_count > 0) error[0] = L'\0';
    return 1;
}

int raw_load_file_processed(const wchar_t *path, const RawParams *params, Image *image,
                            const double gain[3], const double ccm[9],
                            wchar_t *error, size_t error_count)
{
    return raw_load_file_cached(path, params, image, gain, ccm, NULL, error, error_count);
}

int raw_load_file_with_gain(const wchar_t *path, const RawParams *params, Image *image,
                            const double gain[3], wchar_t *error, size_t error_count)
{
    return raw_load_file_processed(path, params, image, gain, RAW_IDENTITY_CCM, error, error_count);
}

int raw_load_file(const wchar_t *path, const RawParams *params, Image *image,
                  wchar_t *error, size_t error_count)
{
    const double gain[3] = {1.0, 1.0, 1.0};
    return raw_load_file_with_gain(path, params, image, gain, error, error_count);
}
