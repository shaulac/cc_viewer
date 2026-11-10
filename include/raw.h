#ifndef EAGLE_VIEWER_RAW_H
#define EAGLE_VIEWER_RAW_H

#include "image.h"

typedef enum RawAlignment {
    RAW_ALIGN_LSB,
    RAW_ALIGN_MSB
} RawAlignment;

typedef struct RawParams {
    int width;
    int height;
    const wchar_t *format;
    int big_endian;
    RawAlignment alignment;
} RawParams;

typedef struct RawBayerCache {
    double *rgb;
    size_t pixel_count;
    unsigned maximum;
} RawBayerCache;

typedef enum RawViewAction {
    RAW_VIEW_RELOAD,
    RAW_VIEW_REUSE,
    RAW_VIEW_APPLY_CCM
} RawViewAction;

size_t raw_format_count(void);
const wchar_t *raw_format_name(size_t index);
int raw_is_bayer(const wchar_t *format);
int raw_is_yuv(const wchar_t *format);
int raw_gray_value(const Image *image, const RawParams *params, int x, int y, unsigned *value);
extern const double RAW_IDENTITY_CCM[9];
int raw_load_file(const wchar_t *path, const RawParams *params, Image *image,
                  wchar_t *error, size_t error_count);
int raw_load_file_with_gain(const wchar_t *path, const RawParams *params, Image *image,
                            const double gain[3], wchar_t *error, size_t error_count);
int raw_load_file_processed(const wchar_t *path, const RawParams *params, Image *image,
                            const double gain[3], const double ccm[9],
                            wchar_t *error, size_t error_count);
/* Initialize cache to zero; on failure its existing contents are preserved. */
int raw_load_file_cached(const wchar_t *path, const RawParams *params, Image *image,
                         const double gain[3], const double ccm[9], RawBayerCache *cache,
                         wchar_t *error, size_t error_count);
int raw_bayer_apply_ccm(const RawBayerCache *cache, Image *image, const double ccm[9],
                        wchar_t *error, size_t error_count);
void raw_bayer_cache_free(RawBayerCache *cache);
RawViewAction raw_view_action(const wchar_t *current_path, const RawParams *current,
                              const double current_gain[3], const double current_ccm[9],
                              const RawBayerCache *cache, const wchar_t *path,
                              const RawParams *params, const double gain[3], const double ccm[9]);

#endif
