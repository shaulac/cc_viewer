#ifndef EAGLE_VIEWER_BAYER_H
#define EAGLE_VIEWER_BAYER_H

#include "raw.h"

typedef enum BayerPattern {
    BAYER_RGGB,
    BAYER_GRBG,
    BAYER_BGGR,
    BAYER_GBRG
} BayerPattern;

void bayer_decode(const unsigned char *source, unsigned char *pixels,
                  const RawParams *params, BayerPattern pattern, unsigned bits,
                  const double gain[3], const double ccm[9], double *rgb_cache);
void bayer_apply_ccm(const double *rgb, unsigned char *pixels, size_t count,
                     unsigned maximum, const double ccm[9]);

#endif
