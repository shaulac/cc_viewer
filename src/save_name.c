#include "save_name.h"

#include <wchar.h>

int make_save_name(wchar_t *name, size_t count, int width, int height,
                   const wchar_t *raw_format, const wchar_t *source_path,
                   const SYSTEMTIME *time)
{
    const wchar_t *format = raw_format;
    wchar_t safe_format[32];
    size_t i;
    int written;

    if (name == NULL || count == 0 || width <= 0 || height <= 0 || time == NULL) return 0;
    if (format == NULL && source_path != NULL) {
        const wchar_t *dot = wcsrchr(source_path, L'.');
        const wchar_t *backslash = wcsrchr(source_path, L'\\');
        const wchar_t *slash = wcsrchr(source_path, L'/');
        if (dot != NULL && (backslash == NULL || dot > backslash) &&
            (slash == NULL || dot > slash) && dot[1] != L'\0') format = dot + 1;
    }
    if (format == NULL) format = source_path == NULL ? L"WINDOW" : L"IMAGE";
    for (i = 0; format[i] != L'\0' && i + 1 < sizeof(safe_format) / sizeof(safe_format[0]); ++i) {
        wchar_t ch = format[i];
        if (ch >= L'a' && ch <= L'z') ch -= L'a' - L'A';
        if (!((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9'))) break;
        safe_format[i] = ch;
    }
    if (i == 0 || format[i] != L'\0') {
        wcscpy(safe_format, L"IMAGE");
    } else {
        safe_format[i] = L'\0';
    }
    written = swprintf(name, count, L"%dx%d_%ls_%04u%02u%02u_%02u%02u%02u.bmp",
                       width, height, safe_format, (unsigned)time->wYear, (unsigned)time->wMonth,
                       (unsigned)time->wDay, (unsigned)time->wHour, (unsigned)time->wMinute,
                       (unsigned)time->wSecond);
    return written >= 0 && (size_t)written < count;
}
