#ifndef EAGLE_VIEWER_VIEW_MATH_H
#define EAGLE_VIEWER_VIEW_MATH_H

double view_fit_scale(int viewport_width, int viewport_height, int image_width, int image_height);
double view_zoom_scale(double current, double factor, double fit);

#endif
