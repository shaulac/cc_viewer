#include "raw_toolbar.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#define CCM_CLASS L"cc_viewer_CCM"
#define CCM_EDIT_FIRST 3100
#define CCM_RESET 3110

int raw_toolbar_parse_ccm_array(const wchar_t *text, double ccm[9])
{
    double values[9];
    int i;
    if (text == NULL || ccm == NULL) return 0;
    while (iswspace(*text)) ++text;
    if (*text++ != L'[') return 0;
    for (i = 0; i < 9; ++i) {
        wchar_t *end;
        while (iswspace(*text)) ++text;
        errno = 0;
        values[i] = wcstod(text, &end);
        if (end == text || errno == ERANGE || !isfinite(values[i]) ||
            fabs(values[i]) > DBL_MAX / (3.0 * 65535.0)) return 0;
        text = end;
        while (iswspace(*text)) ++text;
        if (*text++ != (i == 8 ? L']' : L',')) return 0;
    }
    while (iswspace(*text)) ++text;
    if (*text != L'\0') return 0;
    for (i = 0; i < 9; ++i) ccm[i] = values[i];
    return 1;
}

static LRESULT CALLBACK ccm_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    RawToolbar *toolbar = (RawToolbar *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        toolbar = (RawToolbar *)create->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)toolbar);
    }
    if (message == WM_COMMAND) {
        int id = LOWORD(wparam);
        if (id >= CCM_EDIT_FIRST && id < CCM_EDIT_FIRST + 9 && HIWORD(wparam) == EN_CHANGE) {
            GetWindowTextW((HWND)lparam, toolbar->ccm_text[id - CCM_EDIT_FIRST], 64);
            return 0;
        }
        if (id == CCM_RESET) {
            int i;
            for (i = 0; i < 9; ++i) {
                SetWindowTextW(toolbar->ccm_values[i], i / 3 == i % 3 ? L"1" : L"0");
            }
            return 0;
        }
    }
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_NCDESTROY) {
        int i;
        toolbar->ccm_window = NULL;
        for (i = 0; i < 9; ++i) toolbar->ccm_values[i] = NULL;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int raw_toolbar_get_ccm(RawToolbar *toolbar, double ccm[9])
{
    int i;
    for (i = 0; i < 9; ++i) {
        wchar_t *end;
        errno = 0;
        ccm[i] = wcstod(toolbar->ccm_text[i], &end);
        if (end == toolbar->ccm_text[i] || *end != L'\0' || errno == ERANGE ||
            !isfinite(ccm[i]) || fabs(ccm[i]) > DBL_MAX / (3.0 * 65535.0)) {
            MessageBoxW(toolbar->parent, L"Enter finite CCM coefficients within the supported range.",
                        L"cc_viewer", MB_OK | MB_ICONERROR);
            return 0;
        }
    }
    return 1;
}

int raw_toolbar_show_ccm(RawToolbar *toolbar)
{
    WNDCLASSEXW cls = {0};
    HWND window, label, reset;
    int i;
    if (toolbar->ccm_window != NULL) {
        ShowWindow(toolbar->ccm_window, SW_SHOWNORMAL);
        SetForegroundWindow(toolbar->ccm_window);
        return 1;
    }
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = ccm_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.lpszClassName = CCM_CLASS;
    if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 0;
    window = CreateWindowExW(WS_EX_TOOLWINDOW, CCM_CLASS, L"CCM",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT, 310, 190,
                             toolbar->parent, NULL, cls.hInstance, toolbar);
    if (window == NULL) return 0;
    toolbar->ccm_window = window;
    label = CreateWindowExW(0, L"STATIC", L"RGB output = CCM x RGB input",
                            WS_CHILD | WS_VISIBLE, 12, 8, 270, 20,
                            window, NULL, cls.hInstance, NULL);
    reset = CreateWindowExW(0, L"BUTTON", L"Identity", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            12, 118, 90, 25, window, (HMENU)(INT_PTR)CCM_RESET,
                            cls.hInstance, NULL);
    if (label == NULL || reset == NULL) {
        DestroyWindow(window);
        return 0;
    }
    SendMessageW(label, WM_SETFONT, (WPARAM)toolbar->font, TRUE);
    SendMessageW(reset, WM_SETFONT, (WPARAM)toolbar->font, TRUE);
    for (i = 0; i < 9; ++i) {
        HWND edit = CreateWindowExW(0, L"EDIT", toolbar->ccm_text[i],
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                    12 + (i % 3) * 92, 33 + (i / 3) * 27, 86, 23,
                                    window, (HMENU)(INT_PTR)(CCM_EDIT_FIRST + i),
                                    cls.hInstance, NULL);
        if (edit == NULL) {
            DestroyWindow(window);
            return 0;
        }
        toolbar->ccm_values[i] = edit;
        SendMessageW(edit, EM_SETLIMITTEXT, 63, 0);
        SendMessageW(edit, WM_SETFONT, (WPARAM)toolbar->font, TRUE);
    }
    ShowWindow(window, SW_SHOWNORMAL);
    return 1;
}
