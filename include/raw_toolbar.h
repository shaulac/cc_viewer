#ifndef EAGLE_VIEWER_RAW_TOOLBAR_H
#define EAGLE_VIEWER_RAW_TOOLBAR_H

#include "raw.h"
#include <windows.h>

#define RAW_TOOLBAR_HEIGHT 36
#define RAW_STATUS_HEIGHT 26
#define RAW_STATUS_BUTTONS_WIDTH 76
#define RAW_TOOLBAR_MIN_WIDTH 837
#define RAW_COMMAND_FILE 2001
#define RAW_COMMAND_PRESET 2002
#define RAW_COMMAND_VIEW 2003
#define RAW_COMMAND_SAVE 2004
#define RAW_CONTROL_WIDTH 2010
#define RAW_CONTROL_HEIGHT 2011
#define RAW_CONTROL_FORMAT 2012
#define RAW_CONTROL_ENDIAN 2013
#define RAW_CONTROL_ALIGNMENT 2014
#define RAW_CONTROL_Y_GAIN 2015
#define RAW_CONTROL_U_GAIN 2016
#define RAW_CONTROL_V_GAIN 2017

typedef struct RawToolbar {
    HWND parent;
    HWND preset;
    HWND width;
    HWND height;
    HWND format;
    HWND endian;
    HWND alignment;
    HWND force_caption;
    HWND save_button;
    HWND ccm_window;
    HWND ccm_values[9];
    wchar_t ccm_text[9][64];
    HWND force_labels[3];
    HWND force_values[3];
    int force_mode;
    wchar_t force_text[2][3][64];
    HFONT font;
    wchar_t config_path[32768];
    int slot_width[5];
    int slot_height[5];
} RawToolbar;

int raw_toolbar_create(RawToolbar *toolbar, HWND parent, const wchar_t *config_path);
void raw_toolbar_destroy(RawToolbar *toolbar);
void raw_toolbar_layout_status(RawToolbar *toolbar, int width, int height);
int raw_toolbar_read(RawToolbar *toolbar, RawParams *params, const wchar_t **message);
int raw_toolbar_get(RawToolbar *toolbar, RawParams *params);
void raw_toolbar_set(RawToolbar *toolbar, const RawParams *params);
void raw_toolbar_update_force(RawToolbar *toolbar);
int raw_toolbar_get_gain(RawToolbar *toolbar, double gain[3]);
void raw_toolbar_set_gain(RawToolbar *toolbar, const double gain[3]);
int raw_toolbar_get_ccm(RawToolbar *toolbar, double ccm[9]);
int raw_toolbar_parse_ccm_array(const wchar_t *text, double ccm[9]);
int raw_toolbar_show_ccm(RawToolbar *toolbar);
void raw_toolbar_preset_changed(RawToolbar *toolbar);
int raw_toolbar_save(RawToolbar *toolbar, const RawParams *params,
                     const double gain[3], const double ccm[9], const wchar_t **message);

#endif
