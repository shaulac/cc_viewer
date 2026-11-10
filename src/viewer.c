#include "viewer.h"

#include "image.h"
#include "pixel_render.h"
#include "raw_toolbar.h"
#include "resource.h"
#include "save_name.h"
#include "view_math.h"

#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#define WINDOW_CLASS_NAME L"cc_viewer_Window"
#define MENU_OPEN 1001
#define MENU_FIT 1002
#define MENU_EXIT 1003
#define MENU_RELOAD 1004
#define MENU_SAVE_IMAGE 1005
#define MENU_HELP 1007
#define MENU_CCM 1008
#define SAVE_STATUS_TIMER 1
#define SAVE_STATUS_DURATION_MS 5000

typedef struct ViewerState {
    Image image;
    RawBayerCache bayer_cache;
    double zoom;
    double pan_x;
    double pan_y;
    int dragging;
    int drag_x;
    int drag_y;
    int cursor_x;
    int cursor_y;
    int cursor_valid;
    wchar_t *path;
    int selecting;
    int select_start_x;
    int select_start_y;
    int select_end_x;
    int select_end_y;
    RawParams raw;
    double gain[3];
    double ccm[9];
    int raw_mode;
    wchar_t *pending_raw;
    RawToolbar toolbar;
    HMENU file_menu;
    int save_status_visible;
} ViewerState;

typedef struct ImageLayout {
    double scale;
    double left;
    double top;
    double width;
    double height;
    int client_width;
    int client_height;
} ImageLayout;

static void show_error(HWND window, const wchar_t *message)
{
    MessageBoxW(window, message, L"cc_viewer", MB_OK | MB_ICONERROR);
}

static void update_title(HWND window, const wchar_t *path)
{
    SetWindowTextW(window, path != NULL ? path : L"cc_viewer");
}

static int load_image(HWND window, ViewerState *state, const wchar_t *path,
                      const RawParams *raw, const double gain[3], const double ccm[9])
{
    Image loaded = {0};
    RawBayerCache cache = {0};
    RawParams selected = {0};
    wchar_t error[256];
    wchar_t *new_path;
    size_t path_length = wcslen(path);

    new_path = (wchar_t *)malloc((path_length + 1) * sizeof(*new_path));
    if (new_path == NULL) {
        show_error(window, L"Not enough memory to store the image path.");
        return 0;
    }
    wcscpy(new_path, path);

    if (raw != NULL) {
        selected = *raw;
    }
    if (!(raw != NULL ? raw_load_file_cached(path, raw, &loaded, gain, ccm, &cache, error, sizeof(error) / sizeof(error[0])) :
                 image_load_file(path, &loaded, error, sizeof(error) / sizeof(error[0])))) {
        free(new_path);
        show_error(window, error);
        return 0;
    }

    image_free(&state->image);
    raw_bayer_cache_free(&state->bayer_cache);
    state->bayer_cache = cache;
    free(state->path);
    state->path = new_path;
    if (raw != NULL) {
        state->raw = selected;
        state->raw_mode = 1;
        if (gain != NULL) {
            state->gain[0] = gain[0];
            state->gain[1] = gain[1];
            state->gain[2] = gain[2];
        }
        if (ccm != NULL) {
            size_t i;
            for (i = 0; i < 9; ++i) state->ccm[i] = ccm[i];
        }
        raw_toolbar_set(&state->toolbar, raw);
        if ((raw_is_bayer(raw->format) || raw_is_yuv(raw->format)) && gain != NULL) {
            raw_toolbar_set_gain(&state->toolbar, gain);
        }
    } else {
        state->raw_mode = 0;
    }
    state->image = loaded;
    state->zoom = 0.0;
    state->pan_x = 0.0;
    state->pan_y = 0.0;
    state->cursor_valid = 0;
    update_title(window, state->path);
    InvalidateRect(window, NULL, TRUE);
    return 1;
}

static void open_path(HWND window, ViewerState *state, const wchar_t *path)
{
    wchar_t detection_error[256];
    ImageFileType type = image_detect_file(path, detection_error, sizeof(detection_error) / sizeof(detection_error[0]));
    if (type == IMAGE_FILE_ERROR) {
        show_error(window, detection_error);
    } else if (type == IMAGE_FILE_RAW) {
        size_t length = wcslen(path);
        wchar_t *pending = (wchar_t *)malloc((length + 1) * sizeof(*pending));
        RawParams params;
        double gain[3] = {1.0, 1.0, 1.0};
        double ccm[9];
        if (pending == NULL) {
            show_error(window, L"Could not allocate the RAW file path.");
            return;
        }
        wcscpy(pending, path);
        free(state->pending_raw);
        state->pending_raw = pending;
        if (raw_toolbar_get(&state->toolbar, &params) &&
            (!(raw_is_bayer(params.format) || raw_is_yuv(params.format)) ||
             raw_toolbar_get_gain(&state->toolbar, gain)) &&
            (!raw_is_bayer(params.format) || raw_toolbar_get_ccm(&state->toolbar, ccm))) {
            load_image(window, state, state->pending_raw, &params, gain,
                       raw_is_bayer(params.format) ? ccm : NULL);
        }
    } else if (load_image(window, state, path, NULL, NULL, NULL)) {
        free(state->pending_raw);
        state->pending_raw = NULL;
    }
}

static void save_bmp(HWND window, const Image *image, const wchar_t *title,
                     const ViewerState *state)
{
    static const wchar_t filter[] = L"Bitmap image (*.bmp)\0*.bmp\0\0";
    OPENFILENAMEW dialog = {0};
    wchar_t path[32768] = L"";
    wchar_t error[256];
    SYSTEMTIME now;
    const Image *named_image = state->image.pixels != NULL ? &state->image : image;

    GetLocalTime(&now);
    if (!make_save_name(path, sizeof(path) / sizeof(path[0]), named_image->width, named_image->height,
                        state->raw_mode ? state->raw.format : NULL, state->path, &now)) {
        show_error(window, L"Could not generate the default BMP file name.");
        return;
    }

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path;
    dialog.nMaxFile = (DWORD)(sizeof(path) / sizeof(path[0]));
    dialog.lpstrDefExt = L"bmp";
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetSaveFileNameW(&dialog)) {
        if (!image_save_bmp(path, image, error, sizeof(error) / sizeof(error[0]))) {
            show_error(window, error);
        }
    } else if (CommDlgExtendedError() != 0) {
        show_error(window, L"The file save dialog could not be displayed.");
    }
}

static void open_image_dialog(HWND window, ViewerState *state)
{
    static const wchar_t filter[] =
        L"Image files\0*.bmp;*.gif;*.jpg;*.jpeg;*.png;*.tga;*.psd;*.pic;*.pnm;*.pgm;*.ppm;*.raw\0"
        L"RAW files\0*.raw\0"
        L"All files\0*.*\0\0";
    OPENFILENAMEW dialog = {0};
    wchar_t path[32768] = L"";

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path;
    dialog.nMaxFile = (DWORD)(sizeof(path) / sizeof(path[0]));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    dialog.lpstrTitle = L"Open image";

    if (GetOpenFileNameW(&dialog)) {
        open_path(window, state, path);
    } else if (CommDlgExtendedError() != 0) {
        show_error(window, L"The file open dialog could not be displayed.");
    }
}

static ImageLayout get_layout(HWND window, const ViewerState *state)
{
    RECT client;
    ImageLayout layout = {0};

    GetClientRect(window, &client);
    layout.client_width = client.right - client.left;
    layout.client_height = client.bottom - client.top - RAW_STATUS_HEIGHT - RAW_TOOLBAR_HEIGHT;
    if (layout.client_width < 1) {
        layout.client_width = 1;
    }
    if (layout.client_height < 1) {
        layout.client_height = 1;
    }

    if (state->image.pixels == NULL) {
        return layout;
    }

    layout.scale = view_fit_scale(layout.client_width, layout.client_height,
                                  state->image.width, state->image.height);
    if (state->zoom > layout.scale) layout.scale = state->zoom;

    layout.width = state->image.width * layout.scale;
    layout.height = state->image.height * layout.scale;
    layout.left = (layout.client_width - layout.width) / 2.0 + state->pan_x;
    layout.top = RAW_TOOLBAR_HEIGHT + (layout.client_height - layout.height) / 2.0 + state->pan_y;
    return layout;
}

static int bounded_coordinate(double value)
{
    if (value > 1000000000.0) {
        return 1000000000;
    }
    if (value < -1000000000.0) {
        return -1000000000;
    }
    return (int)value;
}

static int paint_nearest(HDC dc, const RECT *area, const ImageLayout *layout, const Image *image)
{
    double left = fmax((double)area->left, ceil(layout->left));
    double top = fmax((double)area->top, ceil(layout->top));
    double right = fmin((double)area->right, ceil(layout->left + layout->width));
    double bottom = fmin((double)area->bottom, ceil(layout->top + layout->height));
    int width, height, result;
    size_t stride;
    unsigned char *pixels;
    BITMAPINFO bitmap = {0};

    if (left >= right || top >= bottom) return 1;
    width = (int)(right - left);
    height = (int)(bottom - top);
    if ((size_t)width > SIZE_MAX / 4) return 0;
    stride = (size_t)width * 4;
    if ((size_t)height > SIZE_MAX / stride) return 0;
    pixels = (unsigned char *)malloc(stride * (size_t)height);
    if (pixels == NULL) return 0;

    render_nearest_pixels(pixels, stride, width, height, (int)left, (int)top,
                          layout->left, layout->top, layout->scale, image);

    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = width;
    bitmap.bmiHeader.biHeight = -height;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    result = SetDIBitsToDevice(dc, (int)left, (int)top, width, height,
                               0, 0, 0, height, pixels, &bitmap, DIB_RGB_COLORS);
    free(pixels);
    return result != 0;
}

static void paint_image(HWND window, HDC dc, const ViewerState *state)
{
    RECT client;
    RECT image_area;
    RECT status_area;
    ImageLayout layout = get_layout(window, state);

    GetClientRect(window, &client);
    {
        RECT toolbar_area = client;
        toolbar_area.bottom = RAW_TOOLBAR_HEIGHT;
        FillRect(dc, &toolbar_area, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }
    image_area = client;
    image_area.top += RAW_TOOLBAR_HEIGHT;
    image_area.bottom -= RAW_STATUS_HEIGHT;
    if (image_area.bottom < image_area.top) {
        image_area.bottom = image_area.top;
    }

    FillRect(dc, &image_area, (HBRUSH)GetStockObject(WHITE_BRUSH));

    if (state->image.pixels != NULL) {
        BITMAPINFO bitmap = {0};
        int saved = SaveDC(dc);
        int destination_width = bounded_coordinate(layout.width + 0.5);
        int destination_height = bounded_coordinate(layout.height + 0.5);

        if (destination_width < 1) {
            destination_width = 1;
        }
        if (destination_height < 1) {
            destination_height = 1;
        }

        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = state->image.width;
        bitmap.bmiHeader.biHeight = -state->image.height;
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;

        IntersectClipRect(dc, image_area.left, image_area.top, image_area.right, image_area.bottom);
        if (layout.scale >= 1.0) {
            if (!paint_nearest(dc, &image_area, &layout, &state->image)) {
                OutputDebugStringW(L"cc_viewer: could not render the magnified image.\n");
                DrawTextW(dc, L"Could not render the magnified image.", -1, &image_area,
                          DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }
        } else {
            SetStretchBltMode(dc, HALFTONE);
            if (StretchDIBits(dc,
                      bounded_coordinate(layout.left), bounded_coordinate(layout.top),
                      destination_width, destination_height,
                      0, 0, state->image.width, state->image.height,
                      state->image.pixels, &bitmap, DIB_RGB_COLORS, SRCCOPY) == (int)GDI_ERROR) {
                OutputDebugStringW(L"cc_viewer: could not render the image.\n");
                DrawTextW(dc, L"Could not render the image.", -1, &image_area,
                          DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }
        }
        if (saved != 0) RestoreDC(dc, saved);
    }

    if (state->selecting) {
        RECT selection;
        int saved = SaveDC(dc);
        selection.left = state->select_start_x < state->select_end_x ?
                         state->select_start_x : state->select_end_x;
        selection.right = state->select_start_x > state->select_end_x ?
                          state->select_start_x : state->select_end_x;
        selection.top = state->select_start_y < state->select_end_y ?
                        state->select_start_y : state->select_end_y;
        selection.bottom = state->select_start_y > state->select_end_y ?
                           state->select_start_y : state->select_end_y;
        IntersectClipRect(dc, image_area.left, image_area.top, image_area.right, image_area.bottom);
        DrawFocusRect(dc, &selection);
        if (saved != 0) RestoreDC(dc, saved);
    }

    status_area = client;
    status_area.top = status_area.bottom - RAW_STATUS_HEIGHT;
    if (status_area.top < status_area.bottom) {
        wchar_t status[256];

        FillRect(dc, &status_area, (HBRUSH)(COLOR_BTNFACE + 1));
        if (state->save_status_visible) {
            wcscpy(status, L"RAW defaults and resolution preset saved.");
        } else if (state->image.pixels == NULL) {
            wcscpy(status, L"Open an image with Ctrl+O or drop a file onto this window.");
        } else if (state->cursor_valid) {
            const unsigned char *pixel = state->image.pixels +
                (size_t)state->cursor_y * state->image.stride + (size_t)state->cursor_x * 4;
            unsigned gray_value;
            swprintf(status, sizeof(status) / sizeof(status[0]),
                     L"%d x %d   Zoom: %.0f%%   Pixel: (%d, %d)   RGBA: (%u, %u, %u, %u)",
                     state->image.width, state->image.height, layout.scale * 100.0,
                     state->cursor_x, state->cursor_y,
                     (unsigned)pixel[2], (unsigned)pixel[1],
                     (unsigned)pixel[0], (unsigned)pixel[3]);
            if (state->raw_mode && raw_gray_value(&state->image, &state->raw,
                                                  state->cursor_x, state->cursor_y, &gray_value)) {
                size_t used = wcslen(status);
                swprintf(status + used, sizeof(status) / sizeof(status[0]) - used,
                         L"   RAW: 0x%X", gray_value);
            }
        } else {
            swprintf(status, sizeof(status) / sizeof(status[0]),
                     L"%d x %d   Zoom: %.0f%%   Wheel: zoom   Right-drag: pan",
                     state->image.width, state->image.height, layout.scale * 100.0);
        }

        status_area.right -= RAW_STATUS_BUTTONS_WIDTH + 4;
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, status, -1, &status_area,
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    }
}

static void zoom_selection(HWND window, ViewerState *state)
{
    ImageLayout layout = get_layout(window, state);
    double left = state->select_start_x < state->select_end_x ? state->select_start_x : state->select_end_x;
    double right = state->select_start_x > state->select_end_x ? state->select_start_x : state->select_end_x;
    double top = state->select_start_y < state->select_end_y ? state->select_start_y : state->select_end_y;
    double bottom = state->select_start_y > state->select_end_y ? state->select_start_y : state->select_end_y;
    double scale_x;
    double scale_y;
    double center_x;
    double center_y;
    double new_scale;

    if (layout.scale <= 0.0) return;
    if (left < 0.0) left = 0.0;
    if (top < RAW_TOOLBAR_HEIGHT) top = RAW_TOOLBAR_HEIGHT;
    if (right > layout.client_width) right = layout.client_width;
    if (bottom > RAW_TOOLBAR_HEIGHT + layout.client_height) bottom = RAW_TOOLBAR_HEIGHT + layout.client_height;
    if (left < layout.left) left = layout.left;
    if (top < layout.top) top = layout.top;
    if (right > layout.left + layout.width) right = layout.left + layout.width;
    if (bottom > layout.top + layout.height) bottom = layout.top + layout.height;
    if (right - left < 2.0 || bottom - top < 2.0) return;
    center_x = ((left + right) / 2.0 - layout.left) / layout.scale;
    center_y = ((top + bottom) / 2.0 - layout.top) / layout.scale;
    scale_x = layout.client_width * layout.scale / (right - left);
    scale_y = layout.client_height * layout.scale / (bottom - top);
    new_scale = scale_x < scale_y ? scale_x : scale_y;
    {
        double fit = view_fit_scale(layout.client_width, layout.client_height,
                                    state->image.width, state->image.height);
        new_scale = view_zoom_scale(layout.scale, new_scale / layout.scale, fit);
    }
    state->zoom = new_scale;
    state->pan_x = (state->image.width / 2.0 - center_x) * new_scale;
    state->pan_y = (state->image.height / 2.0 - center_y) * new_scale;
}

static void zoom_at(HWND window, ViewerState *state, int x, int y, double factor)
{
    ImageLayout before = get_layout(window, state);
    double image_x;
    double image_y;
    double new_zoom;
    double fit;
    double centered_left;
    double centered_top;

    if (state->image.pixels == NULL || before.scale <= 0.0) {
        return;
    }

    image_x = (x - before.left) / before.scale;
    image_y = (y - before.top) / before.scale;
    fit = view_fit_scale(before.client_width, before.client_height,
                         state->image.width, state->image.height);
    new_zoom = view_zoom_scale(before.scale, factor, fit);
    if (new_zoom == fit && factor < 1.0) {
        state->zoom = 0.0;
        state->pan_x = 0.0;
        state->pan_y = 0.0;
        InvalidateRect(window, NULL, FALSE);
        return;
    }

    state->zoom = new_zoom;
    centered_left = (before.client_width - state->image.width * new_zoom) / 2.0;
    centered_top = RAW_TOOLBAR_HEIGHT + (before.client_height - state->image.height * new_zoom) / 2.0;
    state->pan_x = x - centered_left - image_x * new_zoom;
    state->pan_y = y - centered_top - image_y * new_zoom;
    InvalidateRect(window, NULL, FALSE);
}

static void update_cursor(HWND window, ViewerState *state, int x, int y)
{
    ImageLayout layout = get_layout(window, state);
    int was_valid = state->cursor_valid;

    state->cursor_valid = 0;
    if (state->image.pixels != NULL && layout.scale > 0.0 &&
        x >= layout.left && y >= layout.top &&
        x < layout.left + layout.width && y < layout.top + layout.height) {
        state->cursor_x = (int)((x - layout.left) / layout.scale);
        state->cursor_y = (int)((y - layout.top) / layout.scale);
        state->cursor_valid = state->cursor_x >= 0 &&
                              state->cursor_x < state->image.width &&
                              state->cursor_y >= 0 &&
                              state->cursor_y < state->image.height;
    }

    if (state->cursor_valid && state->save_status_visible) {
        state->save_status_visible = 0;
        KillTimer(window, SAVE_STATUS_TIMER);
    }

    if (was_valid != state->cursor_valid || state->cursor_valid) {
        RECT client;
        GetClientRect(window, &client);
        client.top = client.bottom - RAW_STATUS_HEIGHT;
        InvalidateRect(window, &client, FALSE);
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    ViewerState *state = (ViewerState *)GetWindowLongPtrW(window, GWLP_USERDATA);

    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        state = (ViewerState *)create->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)state);
    }

    switch (message) {
    case WM_CREATE:
        DragAcceptFiles(window, TRUE);
        if (!raw_toolbar_create(&state->toolbar, window, NULL)) {
            raw_toolbar_destroy(&state->toolbar);
            return -1;
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == RAW_COMMAND_FILE) {
            RECT button;
            GetWindowRect(GetDlgItem(window, RAW_COMMAND_FILE), &button);
            TrackPopupMenu(state->file_menu, TPM_LEFTALIGN | TPM_TOPALIGN,
                           button.left, button.bottom, 0, window, NULL);
            return 0;
        }
        if (LOWORD(wparam) == RAW_COMMAND_PRESET && HIWORD(wparam) == CBN_SELCHANGE) {
            raw_toolbar_preset_changed(&state->toolbar);
            return 0;
        }
        if (LOWORD(wparam) == RAW_CONTROL_FORMAT && HIWORD(wparam) == CBN_SELCHANGE) {
            raw_toolbar_update_force(&state->toolbar);
            return 0;
        }
        if (LOWORD(wparam) == RAW_COMMAND_SAVE) {
            RawParams params;
            const wchar_t *message;
            double gain[3] = {1.0, 1.0, 1.0};
            double ccm[9];
            if (raw_toolbar_get(&state->toolbar, &params) &&
                (!(raw_is_bayer(params.format) || raw_is_yuv(params.format)) ||
                 raw_toolbar_get_gain(&state->toolbar, gain)) &&
                (!raw_is_bayer(params.format) || raw_toolbar_get_ccm(&state->toolbar, ccm))) {
                if (raw_toolbar_save(&state->toolbar, &params, gain, ccm, &message)) {
                    RECT client;
                    state->save_status_visible = 1;
                    if (SetTimer(window, SAVE_STATUS_TIMER, SAVE_STATUS_DURATION_MS, NULL) == 0) {
                        OutputDebugStringW(L"cc_viewer: could not start the status message timer.\n");
                    }
                    GetClientRect(window, &client);
                    client.top = client.bottom - RAW_STATUS_HEIGHT;
                    InvalidateRect(window, &client, FALSE);
                } else {
                    show_error(window, message);
                }
            }
            return 0;
        }
        if (LOWORD(wparam) == RAW_COMMAND_VIEW) {
            RawParams params;
            double gain[3] = {1.0, 1.0, 1.0};
            double ccm[9];
            if (state->pending_raw == NULL) {
                show_error(window, L"Open a RAW file first.");
            } else if (raw_toolbar_get(&state->toolbar, &params) &&
                       (!(raw_is_bayer(params.format) || raw_is_yuv(params.format)) ||
                        raw_toolbar_get_gain(&state->toolbar, gain)) &&
                       (!raw_is_bayer(params.format) || raw_toolbar_get_ccm(&state->toolbar, ccm))) {
                RawViewAction action = state->raw_mode ?
                    raw_view_action(state->path, &state->raw, state->gain, state->ccm,
                                    &state->bayer_cache, state->pending_raw, &params, gain,
                                    raw_is_bayer(params.format) ? ccm : NULL) : RAW_VIEW_RELOAD;
                if (action == RAW_VIEW_APPLY_CCM) {
                    wchar_t error[256];
                    if (raw_bayer_apply_ccm(&state->bayer_cache, &state->image, ccm,
                                            error, sizeof(error) / sizeof(error[0]))) {
                        size_t i;
                        for (i = 0; i < 9; ++i) state->ccm[i] = ccm[i];
                        InvalidateRect(window, NULL, FALSE);
                    } else {
                        show_error(window, error);
                    }
                } else if (action == RAW_VIEW_RELOAD) {
                    load_image(window, state, state->pending_raw, &params, gain,
                               raw_is_bayer(params.format) ? ccm : NULL);
                }
            }
            return 0;
        }
        if (LOWORD(wparam) == MENU_CCM) {
            if (!raw_toolbar_show_ccm(&state->toolbar)) {
                show_error(window, L"Could not open the CCM configuration window.");
            }
            return 0;
        }
        if (LOWORD(wparam) == MENU_OPEN) {
            open_image_dialog(window, state);
            return 0;
        }
        if (LOWORD(wparam) == MENU_FIT) {
            state->zoom = 0.0;
            state->pan_x = 0.0;
            state->pan_y = 0.0;
            InvalidateRect(window, NULL, TRUE);
            return 0;
        }
        if (LOWORD(wparam) == MENU_RELOAD) {
            if (state->path != NULL) {
                load_image(window, state, state->path, state->raw_mode ? &state->raw : NULL,
                           state->raw_mode ? state->gain : NULL,
                           state->raw_mode && raw_is_bayer(state->raw.format) ? state->ccm : NULL);
            }
            return 0;
        }
        if (LOWORD(wparam) == MENU_SAVE_IMAGE) {
            if (state->image.pixels != NULL) {
                save_bmp(window, &state->image, L"Save image", state);
            } else {
                show_error(window, L"Open an image before saving.");
            }
            return 0;
        }
        if (LOWORD(wparam) == MENU_HELP) {
            MessageBoxW(window,
                L"Ctrl+O: Open image\nS: Save image as BMP\n"
                L"R: Reload image\nF or Z: Fit to window\n"
                L"Mouse wheel: Zoom at cursor\nLeft drag: Zoom selection\n"
                L"Right drag: Pan\nF1: Help\n"
                L"Change RAW settings in the top toolbar, then click View.\n"
                L"Bayer uses R/G/B gains; YUV uses Y/U/V gains.",
                L"cc_viewer Help", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (LOWORD(wparam) == MENU_EXIT) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == 'O') {
            open_image_dialog(window, state);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (wparam == 'F' || wparam == 'Z')) {
            SendMessageW(window, WM_COMMAND, MENU_FIT, 0);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && wparam == 'R') {
            SendMessageW(window, WM_COMMAND, MENU_RELOAD, 0);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && wparam == 'S') {
            SendMessageW(window, WM_COMMAND, MENU_SAVE_IMAGE, 0);
            return 0;
        }
        if (wparam == VK_F1) {
            SendMessageW(window, WM_COMMAND, MENU_HELP, 0);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP bitmap = NULL;
        HGDIOBJ previous = NULL;
        GetClientRect(window, &client);
        if (buffer != NULL && client.right > 0 && client.bottom > 0) {
            bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
        }
        if (bitmap != NULL) previous = SelectObject(buffer, bitmap);
        if (previous != NULL && previous != HGDI_ERROR) {
            IntersectClipRect(buffer, paint.rcPaint.left, paint.rcPaint.top,
                              paint.rcPaint.right, paint.rcPaint.bottom);
            paint_image(window, buffer, state);
            if (!BitBlt(dc, paint.rcPaint.left, paint.rcPaint.top,
                        paint.rcPaint.right - paint.rcPaint.left,
                        paint.rcPaint.bottom - paint.rcPaint.top,
                        buffer, paint.rcPaint.left, paint.rcPaint.top, SRCCOPY)) {
                OutputDebugStringW(L"cc_viewer: buffered paint failed; using direct rendering.\n");
                paint_image(window, dc, state);
            }
            SelectObject(buffer, previous);
        } else {
            OutputDebugStringW(L"cc_viewer: could not create paint buffer; using direct rendering.\n");
            paint_image(window, dc, state);
        }
        if (bitmap != NULL) DeleteObject(bitmap);
        if (buffer != NULL) DeleteDC(buffer);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wparam == SAVE_STATUS_TIMER) {
            RECT client;
            KillTimer(window, SAVE_STATUS_TIMER);
            state->save_status_visible = 0;
            GetClientRect(window, &client);
            client.top = client.bottom - RAW_STATUS_HEIGHT;
            InvalidateRect(window, &client, FALSE);
            return 0;
        }
        break;
    case WM_SIZE:
        raw_toolbar_layout_status(&state->toolbar, LOWORD(lparam), HIWORD(lparam));
        if (state->image.pixels != NULL && state->zoom > 0.0) {
            ImageLayout layout = get_layout(window, state);
            double fit = view_fit_scale(layout.client_width, layout.client_height,
                                        state->image.width, state->image.height);
            if (state->zoom <= fit) {
                state->zoom = 0.0;
                state->pan_x = 0.0;
                state->pan_y = 0.0;
            }
        }
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_GETMINMAXINFO:
        {
            RECT min_client = {0, 0, RAW_TOOLBAR_MIN_WIDTH, 0};
            AdjustWindowRectEx(&min_client, (DWORD)GetWindowLongPtrW(window, GWL_STYLE),
                               FALSE, (DWORD)GetWindowLongPtrW(window, GWL_EXSTYLE));
            ((MINMAXINFO *)lparam)->ptMinTrackSize.x = min_client.right - min_client.left;
        }
        ((MINMAXINFO *)lparam)->ptMinTrackSize.y = 200;
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wparam, RGB(255, 255, 255));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    case WM_MOUSEWHEEL: {
        POINT cursor = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        double factor = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1.2 : 1.0 / 1.2;
        ScreenToClient(window, &cursor);
        zoom_at(window, state, cursor.x, cursor.y, factor);
        return 0;
    }
    case WM_RBUTTONDOWN:
        if (state->selecting) return 0;
        state->dragging = 1;
        state->drag_x = GET_X_LPARAM(lparam);
        state->drag_y = GET_Y_LPARAM(lparam);
        SetCapture(window);
        return 0;
    case WM_RBUTTONUP:
        if (state->dragging) {
            state->dragging = 0;
            ReleaseCapture();
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state->image.pixels != NULL && !state->dragging) {
            state->selecting = 1;
            state->select_start_x = state->select_end_x = GET_X_LPARAM(lparam);
            state->select_start_y = state->select_end_y = GET_Y_LPARAM(lparam);
            SetCapture(window);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state->selecting) {
            state->select_end_x = GET_X_LPARAM(lparam);
            state->select_end_y = GET_Y_LPARAM(lparam);
            zoom_selection(window, state);
            state->selecting = 0;
            ReleaseCapture();
            InvalidateRect(window, NULL, TRUE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        state->dragging = 0;
        state->selecting = 0;
        InvalidateRect(window, NULL, TRUE);
        return 0;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);

        if (state->dragging) {
            state->pan_x += x - state->drag_x;
            state->pan_y += y - state->drag_y;
            state->drag_x = x;
            state->drag_y = y;
            InvalidateRect(window, NULL, FALSE);
        }
        if (state->selecting) {
            state->select_end_x = x;
            state->select_end_y = y;
            InvalidateRect(window, NULL, TRUE);
        }
        update_cursor(window, state, x, y);
        return 0;
    }
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wparam;
        UINT length = DragQueryFileW(drop, 0, NULL, 0);
        wchar_t *path = NULL;

        if (length > 0 && length < 32768u) {
            path = (wchar_t *)malloc(((size_t)length + 1) * sizeof(wchar_t));
        }

        if (path != NULL && DragQueryFileW(drop, 0, path, length + 1) != 0) {
            open_path(window, state, path);
        } else {
            show_error(window, L"Could not read the dropped file path.");
        }
        free(path);
        DragFinish(drop);
        return 0;
    }
    case WM_DESTROY:
        image_free(&state->image);
        raw_bayer_cache_free(&state->bayer_cache);
        free(state->path);
        free(state->pending_raw);
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        raw_toolbar_destroy(&state->toolbar);
        break;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

int viewer_run(HINSTANCE instance, int show_command, const wchar_t *file, const RawParams *raw)
{
    WNDCLASSEXW window_class = {0};
    HMENU file_menu;
    HWND window;
    ViewerState state = {0};
    MSG message;
    RECT initial_client = {0, 0, RAW_TOOLBAR_MIN_WIDTH, 0};

    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                         IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                         GetSystemMetrics(SM_CYICON), LR_SHARED);
    window_class.hIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON),
                                           IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                           GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    if (window_class.hIcon == NULL || window_class.hIconSm == NULL) {
        show_error(NULL, L"Could not load the application icon.");
        return 1;
    }
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = WINDOW_CLASS_NAME;
    if (RegisterClassExW(&window_class) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        show_error(NULL, L"Could not register the application window.");
        return 1;
    }

    file_menu = CreatePopupMenu();
    if (file_menu == NULL) {
        if (file_menu != NULL) {
            DestroyMenu(file_menu);
        }
        show_error(NULL, L"Could not create the application menu.");
        return 1;
    }

    if (!AppendMenuW(file_menu, MF_STRING, MENU_OPEN, L"&Open...\tCtrl+O") ||
        !AppendMenuW(file_menu, MF_STRING, MENU_CCM, L"&CCM...") ||
        !AppendMenuW(file_menu, MF_STRING, MENU_RELOAD, L"&Reload\tR") ||
        !AppendMenuW(file_menu, MF_STRING, MENU_SAVE_IMAGE, L"Save &image as BMP...\tS") ||
        !AppendMenuW(file_menu, MF_STRING, MENU_FIT, L"&Fit to window\tF / Z") ||
        !AppendMenuW(file_menu, MF_STRING, MENU_HELP, L"&Help\tF1") ||
        !AppendMenuW(file_menu, MF_SEPARATOR, 0, NULL) ||
        !AppendMenuW(file_menu, MF_STRING, MENU_EXIT, L"E&xit")) {
        DestroyMenu(file_menu);
        show_error(NULL, L"Could not populate the application menu.");
        return 1;
    }

    state.file_menu = file_menu;
    if (!AdjustWindowRectEx(&initial_client, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, FALSE, 0)) {
        DestroyMenu(file_menu);
        show_error(NULL, L"Could not calculate the initial window size.");
        return 1;
    }
    window = CreateWindowExW(0, WINDOW_CLASS_NAME, L"cc_viewer",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                             initial_client.right - initial_client.left, 700,
                             NULL, NULL, instance, &state);
    if (window == NULL) {
        DestroyMenu(file_menu);
        show_error(NULL, L"Could not create the application window.");
        return 1;
    }

    ShowWindow(window, show_command == SW_HIDE ? SW_SHOWNORMAL : show_command);
    UpdateWindow(window);

    if (file != NULL) {
        if (raw != NULL) {
            size_t length = wcslen(file);
            state.pending_raw = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
            if (state.pending_raw != NULL) {
                wcscpy(state.pending_raw, file);
                raw_toolbar_set(&state.toolbar, raw);
                {
                    double gain[3] = {1.0, 1.0, 1.0};
                    double ccm[9];
                    if (!(raw_is_bayer(raw->format) || raw_is_yuv(raw->format)) ||
                        raw_toolbar_get_gain(&state.toolbar, gain)) {
                        if (!raw_is_bayer(raw->format) || raw_toolbar_get_ccm(&state.toolbar, ccm)) {
                            load_image(window, &state, state.pending_raw, raw, gain,
                                       raw_is_bayer(raw->format) ? ccm : NULL);
                        }
                    }
                }
            } else {
                show_error(window, L"Could not allocate the RAW file path.");
            }
        } else {
            open_path(window, &state, file);
        }
    }

    {
        int message_result;

        while ((message_result = (int)GetMessageW(&message, NULL, 0, 0)) > 0) {
            if (state.toolbar.ccm_window != NULL &&
                IsDialogMessageW(state.toolbar.ccm_window, &message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (message_result == -1) {
            show_error(window, L"An error occurred while processing window messages.");
            return 1;
        }
    }

    DestroyMenu(file_menu);
    return (int)message.wParam;
}
