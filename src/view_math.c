#include "view_math.h"

double view_fit_scale(int viewport_width, int viewport_height, int image_width, int image_height)
{
    double x;
    double y;
    if (viewport_width <= 0 || viewport_height <= 0 || image_width <= 0 || image_height <= 0) {
        return 0.0;
    }
    x = (double)viewport_width / image_width;
    y = (double)viewport_height / image_height;
    return x < y ? x : y;
}

double view_zoom_scale(double current, double factor, double fit)
{
    double maximum = fit > 64.0 ? fit : 64.0;
    double result = current * factor;
    if (result <= fit) return fit;
    if (result > maximum) return maximum;
    return result;
}
