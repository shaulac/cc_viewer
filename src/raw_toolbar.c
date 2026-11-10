#include "raw_toolbar.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>

typedef struct Resolution {
    const wchar_t *label;
    int width;
    int height;
} Resolution;

static const Resolution presets[] = {
    {L"CGA: 320x200", 320, 200}, {L"QVGA: 320x240", 320, 240},
    {L"VGA: 640x480", 640, 480}, {L"PAL: 768x576", 768, 576},
    {L"WVGA: 800x480", 800, 480}, {L"SVGA: 800x600", 800, 600},
    {L"XGA: 1024x768", 1024, 768}, {L"SXGA: 1280x1024", 1280, 1024},
    {L"WXGA: 1280x768", 1280, 768}, {L"720p: 1280x720", 1280, 720},
    {L"720p: 720x1280", 720, 1280}, {L"1080p: 1920x1080", 1920, 1080},
    {L"1080p: 1080x1920", 1080, 1920}, {L"5M: 2592x1944", 2592, 1944},
    {L"8M: 3264x2448", 3264, 2448}, {L"4K UHD: 3840x2160", 3840, 2160},
    {L"4K DCI: 4096x2160", 4096, 2160}
};

#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))
#define SLOT_COUNT 5

static void error(HWND parent, const wchar_t *text)
{
    MessageBoxW(parent, text, L"cc_viewer", MB_OK | MB_ICONERROR);
}

static HWND control(HWND parent, const wchar_t *class_name, const wchar_t *text,
                    DWORD style, int x, int width, int height, int id)
{
    HWND result = CreateWindowExW(0, class_name, text, WS_CHILD | WS_VISIBLE | style,
                                  x, 6, width, height, parent, (HMENU)(INT_PTR)id,
                                  GetModuleHandleW(NULL), NULL);
    return result;
}

static BOOL CALLBACK set_control_font(HWND child, LPARAM font)
{
    SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
    return TRUE;
}

static int positive(const wchar_t *text, int *out)
{
    wchar_t *end;
    unsigned long value;
    if (*text < L'0' || *text > L'9') return 0;
    errno = 0;
    value = wcstoul(text, &end, 10);
    if (errno || *end != L'\0' || value == 0 || value > INT_MAX) return 0;
    *out = (int)value;
    return 1;
}

static int read_number(HWND control_window, int *out)
{
    wchar_t text[32];
    if (GetWindowTextLengthW(control_window) >= (int)(sizeof(text) / sizeof(text[0]))) return 0;
    GetWindowTextW(control_window, text, sizeof(text) / sizeof(text[0]));
    return positive(text, out);
}

static void set_number(HWND control_window, int value)
{
    wchar_t text[32];
    swprintf(text, sizeof(text) / sizeof(text[0]), L"%d", value);
    SetWindowTextW(control_window, text);
}

static int add_item(HWND combo, const wchar_t *text)
{
    LRESULT result = SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)text);
    return result != CB_ERR && result != CB_ERRSPACE;
}

static void refresh_slot(RawToolbar *toolbar, int slot)
{
    wchar_t text[64];
    size_t index = PRESET_COUNT + (size_t)slot;
    if (toolbar->slot_width[slot] > 0) {
        swprintf(text, 64, L"%dx%d", toolbar->slot_width[slot], toolbar->slot_height[slot]);
    } else {
        wcscpy(text, L"(empty)");
    }
    SendMessageW(toolbar->preset, CB_DELETESTRING, index, 0);
    SendMessageW(toolbar->preset, CB_INSERTSTRING, index, (LPARAM)text);
}

static const wchar_t *find_format(const wchar_t *name)
{
    size_t i;
    for (i = 0; i < raw_format_count(); ++i) {
        if (_wcsicmp(name, raw_format_name(i)) == 0) return raw_format_name(i);
    }
    return NULL;
}

int raw_toolbar_create(RawToolbar *toolbar, HWND parent, const wchar_t *config_path)
{
    DWORD length;
    wchar_t *slash;
    wchar_t value[128];
    RawParams defaults = {640, 480, L"gray8", 0, RAW_ALIGN_LSB};
    int malformed = 0;
    NONCLIENTMETRICSW metrics = {0};
    size_t i;

    toolbar->parent = parent;
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) ||
        (toolbar->font = CreateFontIndirectW(&metrics.lfMessageFont)) == NULL) {
        error(parent, L"Could not create the toolbar font.");
        return 0;
    }
    if (config_path != NULL) {
        if (wcslen(config_path) >= sizeof(toolbar->config_path) / sizeof(toolbar->config_path[0])) {
            error(parent, L"The settings file path is too long.");
            return 0;
        }
        wcscpy(toolbar->config_path, config_path);
    } else {
        length = GetModuleFileNameW(NULL, toolbar->config_path,
                                sizeof(toolbar->config_path) / sizeof(toolbar->config_path[0]));
        if (length == 0 || length >= sizeof(toolbar->config_path) / sizeof(toolbar->config_path[0])) {
            error(parent, L"Could not determine the executable path for settings.");
            return 0;
        }
        slash = wcsrchr(toolbar->config_path, L'\\');
        if (slash == NULL || (size_t)(slash - toolbar->config_path) + 18 >=
                             sizeof(toolbar->config_path) / sizeof(toolbar->config_path[0])) {
            error(parent, L"Could not determine the settings file path.");
            return 0;
        }
        wcscpy(slash + 1, L"eagle_viewer.ini");
    }

    if (!control(parent, L"BUTTON", L"File", BS_PUSHBUTTON, 4, 43, 24, RAW_COMMAND_FILE) ||
        !control(parent, L"STATIC", L"Width:", SS_CENTERIMAGE, 51, 42, 24, 0) ||
        !(toolbar->width = control(parent, L"EDIT", L"", WS_BORDER | ES_NUMBER | WS_TABSTOP, 93, 57, 24, RAW_CONTROL_WIDTH)) ||
        !control(parent, L"STATIC", L"Height:", SS_CENTERIMAGE, 155, 46, 24, 0) ||
        !(toolbar->height = control(parent, L"EDIT", L"", WS_BORDER | ES_NUMBER | WS_TABSTOP, 201, 57, 24, RAW_CONTROL_HEIGHT)) ||
        !(toolbar->preset = control(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                                   260, 24, 500, RAW_COMMAND_PRESET)) ||
        !(toolbar->format = control(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 289, 83, 500, RAW_CONTROL_FORMAT)) ||
        !control(parent, L"STATIC", L"Endian:", SS_CENTERIMAGE, 377, 48, 24, 0) ||
        !(toolbar->endian = control(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 425, 68, 100, RAW_CONTROL_ENDIAN)) ||
        !(toolbar->alignment = control(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 498, 60, 100, RAW_CONTROL_ALIGNMENT)) ||
        !(toolbar->force_caption = control(parent, L"STATIC", L"Force:", SS_CENTERIMAGE, 563, 43, 24, 0)) ||
        !(toolbar->force_labels[0] = control(parent, L"STATIC", L"Y", SS_CENTERIMAGE, 608, 13, 24, 0)) ||
        !(toolbar->force_values[0] = control(parent, L"EDIT", L"1.00", WS_BORDER | WS_DISABLED | ES_CENTER | WS_TABSTOP, 622, 40, 24, RAW_CONTROL_Y_GAIN)) ||
        !(toolbar->force_labels[1] = control(parent, L"STATIC", L"U", SS_CENTERIMAGE, 667, 13, 24, 0)) ||
        !(toolbar->force_values[1] = control(parent, L"EDIT", L"1.00", WS_BORDER | WS_DISABLED | ES_CENTER | WS_TABSTOP, 681, 40, 24, RAW_CONTROL_U_GAIN)) ||
        !(toolbar->force_labels[2] = control(parent, L"STATIC", L"V", SS_CENTERIMAGE, 726, 13, 24, 0)) ||
        !(toolbar->force_values[2] = control(parent, L"EDIT", L"1.00", WS_BORDER | WS_DISABLED | ES_CENTER | WS_TABSTOP, 740, 40, 24, RAW_CONTROL_V_GAIN)) ||
        !control(parent, L"BUTTON", L"View", BS_PUSHBUTTON | WS_TABSTOP, 785, 48, 24, RAW_COMMAND_VIEW) ||
        !(toolbar->save_button = control(parent, L"BUTTON", L"Save", BS_PUSHBUTTON | WS_TABSTOP, 0, 72, 24, RAW_COMMAND_SAVE))) {
        error(parent, L"Could not create RAW toolbar controls.");
        return 0;
    }
    {
        RECT client;
        if (GetClientRect(parent, &client)) {
            raw_toolbar_layout_status(toolbar, client.right, client.bottom);
        }
    }
    EnumChildWindows(parent, set_control_font, (LPARAM)toolbar->font);
    SendMessageW(toolbar->preset, CB_SETDROPPEDWIDTH, 180, 0);
    SendMessageW(toolbar->format, CB_SETDROPPEDWIDTH, 150, 0);
    for (i = 0; i < PRESET_COUNT; ++i) if (!add_item(toolbar->preset, presets[i].label)) return 0;
    for (i = 0; i < SLOT_COUNT; ++i) {
        wchar_t key[16];
        int width, height;
        wchar_t tail;
        swprintf(key, 16, L"slot%u", (unsigned)i + 1);
        GetPrivateProfileStringW(L"presets", key, L"", value, 128, toolbar->config_path);
        if (value[0] != L'\0') {
            if (swscanf(value, L"%d,%d%lc", &width, &height, &tail) == 2 && width > 0 && height > 0) {
                toolbar->slot_width[i] = width;
                toolbar->slot_height[i] = height;
            } else {
                malformed = 1;
            }
        }
        if (!add_item(toolbar->preset, L"")) return 0;
        refresh_slot(toolbar, (int)i);
    }
    for (i = 0; i < raw_format_count(); ++i) {
        const wchar_t *name = raw_format_name(i);
        size_t length = wcslen(name);
        wchar_t *label = (wchar_t *)malloc((length + 1) * sizeof(*label));
        int added;
        if (label == NULL) {
            error(parent, L"Could not allocate the RAW format label.");
            return 0;
        }
        wcscpy(label, name);
        CharUpperBuffW(label, (DWORD)length);
        added = add_item(toolbar->format, label);
        free(label);
        if (!added) {
            error(parent, L"Could not populate the RAW format list.");
            return 0;
        }
    }
    if (!add_item(toolbar->endian, L"Little") || !add_item(toolbar->endian, L"Big") ||
        !add_item(toolbar->alignment, L"LSB") || !add_item(toolbar->alignment, L"MSB")) return 0;

#define READ_NUMBER(key, member) \
    GetPrivateProfileStringW(L"default", key, L"", value, 128, toolbar->config_path); \
    if (value[0] && !positive(value, &defaults.member)) malformed = 1
    READ_NUMBER(L"width", width);
    READ_NUMBER(L"height", height);
#undef READ_NUMBER
    GetPrivateProfileStringW(L"default", L"format", L"gray8", value, 128, toolbar->config_path);
    defaults.format = find_format(value);
    if (defaults.format == NULL) { defaults.format = L"gray8"; malformed = 1; }
    GetPrivateProfileStringW(L"default", L"endian", L"little", value, 128, toolbar->config_path);
    if (_wcsicmp(value, L"big") == 0) defaults.big_endian = 1;
    else if (_wcsicmp(value, L"little") != 0) malformed = 1;
    GetPrivateProfileStringW(L"default", L"alignment", L"lsb", value, 128, toolbar->config_path);
    if (_wcsicmp(value, L"msb") == 0) defaults.alignment = RAW_ALIGN_MSB;
    else if (_wcsicmp(value, L"lsb") != 0) malformed = 1;
    for (i = 0; i < 6; ++i) {
        static const wchar_t *keys[] = {L"gain_r", L"gain_g", L"gain_b",
                                        L"gain_y", L"gain_u", L"gain_v"};
        wchar_t *end;
        double gain;
        GetPrivateProfileStringW(L"default", keys[i], L"1.00", value, 128, toolbar->config_path);
        errno = 0;
        gain = wcstod(value, &end);
        if (end == value || *end != L'\0' || errno == ERANGE || !isfinite(gain) || gain < 0.0 ||
            wcslen(value) >= 64) {
            malformed = 1;
            wcscpy(toolbar->force_text[i / 3][i % 3], L"1.00");
        } else {
            wcscpy(toolbar->force_text[i / 3][i % 3], value);
        }
    }
    {
        wchar_t array[1024];
        const wchar_t missing[] = L"__ccm_array_missing__";
        int has_array;
        DWORD copied = GetPrivateProfileStringW(L"ccm", L"matrix", missing, array,
                                                1024, toolbar->config_path);
        has_array = wcscmp(array, missing) != 0;
        if (!has_array) {
            wchar_t alternate[1024];
            GetPrivateProfileStringW(L"ccm", L"matrix", L"__ccm_array_absent__",
                                     alternate, 1024, toolbar->config_path);
            has_array = wcscmp(alternate, missing) == 0;
        }
        if (has_array) {
            double coefficients[9];
            if (copied == 1023 || !raw_toolbar_parse_ccm_array(array, coefficients)) {
                malformed = 1;
                for (i = 0; i < 9; ++i) coefficients[i] = RAW_IDENTITY_CCM[i];
            }
            for (i = 0; i < 9; ++i) {
                swprintf(toolbar->ccm_text[i], 64, L"%.17g", coefficients[i]);
            }
        } else {
            for (i = 0; i < 9; ++i) {
                wchar_t key[16], *end;
                double coefficient;
                swprintf(key, 16, L"m%u%u", (unsigned)(i / 3), (unsigned)(i % 3));
                GetPrivateProfileStringW(L"ccm", key, i / 3 == i % 3 ? L"1" : L"0",
                                         value, 128, toolbar->config_path);
                errno = 0;
                coefficient = wcstod(value, &end);
                if (end == value || *end != L'\0' || errno == ERANGE ||
                    !isfinite(coefficient) || fabs(coefficient) > DBL_MAX / (3.0 * 65535.0) ||
                    wcslen(value) >= 64) {
                    malformed = 1;
                    wcscpy(toolbar->ccm_text[i], i / 3 == i % 3 ? L"1" : L"0");
                } else {
                    wcscpy(toolbar->ccm_text[i], value);
                }
            }
        }
    }
    raw_toolbar_set(toolbar, &defaults);
    if (malformed) error(parent, L"Some saved RAW settings are invalid. Built-in defaults were used for those entries.");
    return 1;
}

void raw_toolbar_destroy(RawToolbar *toolbar)
{
    if (toolbar->ccm_window != NULL) DestroyWindow(toolbar->ccm_window);
    if (toolbar->font != NULL) {
        DeleteObject(toolbar->font);
        toolbar->font = NULL;
    }
}

void raw_toolbar_layout_status(RawToolbar *toolbar, int width, int height)
{
    int top = height - RAW_STATUS_HEIGHT + 1;
    if (toolbar->save_button == NULL) return;
    if (!SetWindowPos(toolbar->save_button, NULL, width - RAW_STATUS_BUTTONS_WIDTH,
                      top, 72, 24, SWP_NOZORDER | SWP_NOACTIVATE)) {
        OutputDebugStringW(L"cc_viewer: could not position the Save button.\n");
    }
}

int raw_toolbar_read(RawToolbar *toolbar, RawParams *params, const wchar_t **message)
{
    LRESULT format = SendMessageW(toolbar->format, CB_GETCURSEL, 0, 0);
    LRESULT endian = SendMessageW(toolbar->endian, CB_GETCURSEL, 0, 0);
    LRESULT alignment = SendMessageW(toolbar->alignment, CB_GETCURSEL, 0, 0);
    if (!read_number(toolbar->width, &params->width)) {
        *message = L"Width must be a positive whole number (1 to 2147483647).";
        return 0;
    }
    if (!read_number(toolbar->height, &params->height)) {
        *message = L"Height must be a positive whole number (1 to 2147483647).";
        return 0;
    }
    if (format < 0 || endian < 0 || alignment < 0 ||
        (params->format = raw_format_name((size_t)format)) == NULL) {
        *message = L"Select a RAW format, byte order and bit alignment.";
        return 0;
    }
    params->big_endian = endian == 1;
    params->alignment = alignment == 1 ? RAW_ALIGN_MSB : RAW_ALIGN_LSB;
    *message = NULL;
    return 1;
}

int raw_toolbar_get(RawToolbar *toolbar, RawParams *params)
{
    const wchar_t *message;
    if (!raw_toolbar_read(toolbar, params, &message)) {
        error(toolbar->parent, message);
        return 0;
    }
    return 1;
}

void raw_toolbar_set(RawToolbar *toolbar, const RawParams *params)
{
    size_t i;
    LRESULT selected = SendMessageW(toolbar->preset, CB_GETCURSEL, 0, 0);
    int current_width, current_height;
    int keep_slot = selected >= (LRESULT)PRESET_COUNT &&
                    selected < (LRESULT)(PRESET_COUNT + SLOT_COUNT) &&
                    read_number(toolbar->width, &current_width) &&
                    read_number(toolbar->height, &current_height) &&
                    current_width == params->width && current_height == params->height;
    set_number(toolbar->width, params->width);
    set_number(toolbar->height, params->height);
    for (i = 0; i < raw_format_count(); ++i) {
        if (_wcsicmp(params->format, raw_format_name(i)) == 0) {
            SendMessageW(toolbar->format, CB_SETCURSEL, i, 0);
            break;
        }
    }
    SendMessageW(toolbar->endian, CB_SETCURSEL, params->big_endian ? 1 : 0, 0);
    SendMessageW(toolbar->alignment, CB_SETCURSEL, params->alignment == RAW_ALIGN_MSB ? 1 : 0, 0);
    raw_toolbar_update_force(toolbar);
    if (keep_slot) return;
    SendMessageW(toolbar->preset, CB_SETCURSEL, (WPARAM)-1, 0);
    for (i = 0; i < PRESET_COUNT; ++i) {
        if (presets[i].width == params->width && presets[i].height == params->height) {
            SendMessageW(toolbar->preset, CB_SETCURSEL, i, 0);
            return;
        }
    }
    for (i = 0; i < SLOT_COUNT; ++i) {
        if (toolbar->slot_width[i] == params->width && toolbar->slot_height[i] == params->height) {
            SendMessageW(toolbar->preset, CB_SETCURSEL, PRESET_COUNT + i, 0);
            return;
        }
    }
}

void raw_toolbar_update_force(RawToolbar *toolbar)
{
    LRESULT selected = SendMessageW(toolbar->format, CB_GETCURSEL, 0, 0);
    const wchar_t *format = selected >= 0 ? raw_format_name((size_t)selected) : NULL;
    int bayer = raw_is_bayer(format);
    int yuv = raw_is_yuv(format);
    int mode = bayer ? 1 : yuv ? 2 : 0;
    const wchar_t *labels[3] = {bayer ? L"R" : L"Y", bayer ? L"G" : L"U", bayer ? L"B" : L"V"};
    int i;
    SetWindowTextW(toolbar->force_caption, bayer ? L"WB:" : L"Force:");
    if (toolbar->force_mode != mode) {
        if (toolbar->force_mode != 0) {
            for (i = 0; i < 3; ++i) {
                GetWindowTextW(toolbar->force_values[i],
                               toolbar->force_text[toolbar->force_mode - 1][i], 64);
            }
        }
        if (mode != 0) {
            for (i = 0; i < 3; ++i) {
                SetWindowTextW(toolbar->force_values[i], toolbar->force_text[mode - 1][i]);
            }
        }
        toolbar->force_mode = mode;
    }
    for (i = 0; i < 3; ++i) {
        SetWindowTextW(toolbar->force_labels[i], labels[i]);
        EnableWindow(toolbar->force_values[i], bayer || yuv);
    }
}

int raw_toolbar_get_gain(RawToolbar *toolbar, double gain[3])
{
    int i;
    for (i = 0; i < 3; ++i) {
        wchar_t text[64], *end;
        if (GetWindowTextLengthW(toolbar->force_values[i]) >= 64 ||
            GetWindowTextW(toolbar->force_values[i], text, 64) == 0) {
            error(toolbar->parent, L"Enter a non-negative number for each channel gain.");
            return 0;
        }
        errno = 0;
        gain[i] = wcstod(text, &end);
        if (end == text || *end != L'\0' || errno == ERANGE || !isfinite(gain[i]) || gain[i] < 0.0) {
            error(toolbar->parent, L"Enter a non-negative finite number for each channel gain.");
            return 0;
        }
    }
    return 1;
}

void raw_toolbar_set_gain(RawToolbar *toolbar, const double gain[3])
{
    int i;
    for (i = 0; i < 3; ++i) {
        wchar_t text[64];
        swprintf(text, 64, L"%.17g", gain[i]);
        SetWindowTextW(toolbar->force_values[i], text);
    }
}

void raw_toolbar_preset_changed(RawToolbar *toolbar)
{
    LRESULT selected = SendMessageW(toolbar->preset, CB_GETCURSEL, 0, 0);
    int width = 0, height = 0;
    if (selected >= 0 && (size_t)selected < PRESET_COUNT) {
        width = presets[selected].width;
        height = presets[selected].height;
    } else if (selected >= 0 && (size_t)selected < PRESET_COUNT + SLOT_COUNT) {
        size_t slot = (size_t)selected - PRESET_COUNT;
        width = toolbar->slot_width[slot];
        height = toolbar->slot_height[slot];
    }
    if (width > 0 && height > 0) {
        set_number(toolbar->width, width);
        set_number(toolbar->height, height);
    }
}

int raw_toolbar_save(RawToolbar *toolbar, const RawParams *params, const double gain[3],
                     const double ccm[9], const wchar_t **message)
{
    wchar_t width[32], height[32];
    wchar_t red[64], green[64], blue[64];
    LRESULT selected = SendMessageW(toolbar->preset, CB_GETCURSEL, 0, 0);
    int slot = -1;
    size_t i;
    const wchar_t *key0 = raw_is_yuv(params->format) ? L"gain_y" : L"gain_r";
    const wchar_t *key1 = raw_is_yuv(params->format) ? L"gain_u" : L"gain_g";
    const wchar_t *key2 = raw_is_yuv(params->format) ? L"gain_v" : L"gain_b";
    *message = NULL;
    if (raw_is_bayer(params->format)) {
        if (ccm == NULL) {
            *message = L"Could not save CCM: matrix is missing.";
            return 0;
        }
        for (i = 0; i < 9; ++i) {
            if (!isfinite(ccm[i]) || fabs(ccm[i]) > DBL_MAX / (3.0 * 65535.0)) {
                *message = L"Could not save CCM: coefficients must be finite and within the supported range.";
                return 0;
            }
        }
    }
    if (selected >= (LRESULT)PRESET_COUNT && selected < (LRESULT)(PRESET_COUNT + SLOT_COUNT)) {
        slot = (int)((size_t)selected - PRESET_COUNT);
    } else {
        for (i = 0; i < PRESET_COUNT; ++i) {
            if (presets[i].width == params->width && presets[i].height == params->height) break;
        }
        if (i == PRESET_COUNT) {
            for (i = 0; i < SLOT_COUNT; ++i) {
                if (toolbar->slot_width[i] == params->width && toolbar->slot_height[i] == params->height) {
                    slot = (int)i;
                    break;
                }
            }
            if (slot < 0) {
                for (i = 0; i < SLOT_COUNT; ++i) {
                    if (toolbar->slot_width[i] == 0) { slot = (int)i; break; }
                }
                if (slot < 0) {
                    *message = L"All custom resolution slots are full. Select a custom slot to replace it.";
                    return 0;
                }
            }
        }
    }
    if (slot >= 0) {
        wchar_t key[16], value[64];
        swprintf(key, 16, L"slot%d", slot + 1);
        swprintf(value, 64, L"%d,%d", params->width, params->height);
        if (!WritePrivateProfileStringW(L"presets", key, value, toolbar->config_path)) {
            *message = L"Could not save the resolution preset. Check directory permissions.";
            return 0;
        }
        toolbar->slot_width[slot] = params->width;
        toolbar->slot_height[slot] = params->height;
        refresh_slot(toolbar, slot);
        SendMessageW(toolbar->preset, CB_SETCURSEL, PRESET_COUNT + (size_t)slot, 0);
    }
    swprintf(width, 32, L"%d", params->width);
    swprintf(height, 32, L"%d", params->height);
    swprintf(red, 64, L"%.17g", gain[0]);
    swprintf(green, 64, L"%.17g", gain[1]);
    swprintf(blue, 64, L"%.17g", gain[2]);
    if (!WritePrivateProfileStringW(L"default", L"width", width, toolbar->config_path) ||
        !WritePrivateProfileStringW(L"default", L"height", height, toolbar->config_path) ||
        !WritePrivateProfileStringW(L"default", L"format", params->format, toolbar->config_path) ||
        !WritePrivateProfileStringW(L"default", L"endian", params->big_endian ? L"big" : L"little", toolbar->config_path) ||
        !WritePrivateProfileStringW(L"default", L"alignment", params->alignment == RAW_ALIGN_MSB ? L"msb" : L"lsb", toolbar->config_path) ||
        ((raw_is_bayer(params->format) || raw_is_yuv(params->format)) &&
         (!WritePrivateProfileStringW(L"default", key0, red, toolbar->config_path) ||
          !WritePrivateProfileStringW(L"default", key1, green, toolbar->config_path) ||
          !WritePrivateProfileStringW(L"default", key2, blue, toolbar->config_path)))) {
        *message = L"Could not save all defaults. A custom preset may already have been saved; check directory permissions.";
        return 0;
    }
    if (raw_is_bayer(params->format)) {
        wchar_t array[640];
        size_t used = 0;
        array[used++] = L'[';
        for (i = 0; i < 9; ++i) {
            int written;
            written = swprintf(array + used, 640 - used, i == 0 ? L"%.17g" : L", %.17g", ccm[i]);
            if (written < 0 || (size_t)written >= 640 - used) {
                *message = L"Could not format the CCM matrix.";
                return 0;
            }
            used += (size_t)written;
        }
        array[used++] = L']';
        array[used] = L'\0';
        if (!WritePrivateProfileStringW(L"ccm", L"matrix", array, toolbar->config_path)) {
            *message = L"Could not save the CCM matrix. Check directory permissions.";
            return 0;
        }
        for (i = 0; i < 9; ++i) {
            wchar_t key[16];
            swprintf(key, 16, L"m%u%u", (unsigned)(i / 3), (unsigned)(i % 3));
            if (!WritePrivateProfileStringW(L"ccm", key, NULL, toolbar->config_path)) {
                *message = L"CCM was saved, but old CCM entries could not be removed. Check directory permissions.";
                return 0;
            }
        }
    }
    return 1;
}
