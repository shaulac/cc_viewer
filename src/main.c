#include "viewer.h"

#include <shellapi.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

static int parse_positive(const wchar_t *text, int *value)
{
    wchar_t *end;
    long number;
    errno = 0;
    number = wcstol(text, &end, 10);
    if (errno != 0 || end == text || *end != L'\0' || number <= 0 || number > INT_MAX) {
        return 0;
    }
    *value = (int)number;
    return 1;
}

static const wchar_t usage[] =
    L"Usage:\n"
    L"cc_viewer.exe [image-file]\n"
    L"cc_viewer.exe -f file -w width -h height -m format [-b] [--align lsb|msb]\n\n"
    L"RAW formats: gray8, gray10, gray12, gray16, rgb555, rgb565, rgb888.\n"
    L"Bayer: rggb, grbg, bggr, gbrg followed by 8, 10, 12 or 16.\n"
    L"YUV: uyvy, vyuy, yvyu, yuyv, yv12, i420, nv12, nv21.\n"
    L"-b selects big endian. Default is little endian.\n"
    L"--align selects 10/12-bit alignment (default: lsb).\n"
    L"-H or --help shows this help.";

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line,
                   int show_command)
{
    int argument_count;
    wchar_t **arguments;
    int result;
    int i;
    const wchar_t *file = NULL;
    RawParams raw = {0};
    int raw_requested = 0;

    (void)previous_instance;
    (void)command_line;

    arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments == NULL) {
        MessageBoxW(NULL, L"Could not read the command line.", L"cc_viewer",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    for (i = 1; i < argument_count; ++i) {
        const wchar_t *arg = arguments[i];
        if (wcscmp(arg, L"-H") == 0 || wcscmp(arg, L"--help") == 0) {
            MessageBoxW(NULL, usage, L"cc_viewer Help", MB_OK | MB_ICONINFORMATION);
            LocalFree(arguments);
            return 0;
        }
        if (wcscmp(arg, L"-b") == 0) {
            raw.big_endian = 1;
            raw_requested = 1;
        } else if (wcscmp(arg, L"-f") == 0 && i + 1 < argument_count && file == NULL) {
            file = arguments[++i];
        } else if (wcscmp(arg, L"-w") == 0 && i + 1 < argument_count) {
            raw_requested = 1;
            if (!parse_positive(arguments[++i], &raw.width)) break;
        } else if (wcscmp(arg, L"-h") == 0 && i + 1 < argument_count) {
            raw_requested = 1;
            if (!parse_positive(arguments[++i], &raw.height)) break;
        } else if (wcscmp(arg, L"-m") == 0 && i + 1 < argument_count) {
            raw_requested = 1;
            raw.format = arguments[++i];
        } else if (wcscmp(arg, L"--align") == 0 && i + 1 < argument_count) {
            raw_requested = 1;
            ++i;
            if (wcscmp(arguments[i], L"lsb") == 0) {
                raw.alignment = RAW_ALIGN_LSB;
            } else if (wcscmp(arguments[i], L"msb") == 0) {
                raw.alignment = RAW_ALIGN_MSB;
            } else {
                break;
            }
        } else if (arg[0] != L'-' && file == NULL) {
            file = arg;
        } else {
            break;
        }
    }
    if (i != argument_count || (raw_requested &&
        (file == NULL || raw.width == 0 || raw.height == 0 || raw.format == NULL))) {
        MessageBoxW(NULL, usage, L"Invalid parameters", MB_OK | MB_ICONERROR);
        LocalFree(arguments);
        return 1;
    }

    result = viewer_run(instance, show_command, file, raw_requested ? &raw : NULL);
    LocalFree(arguments);
    return result;
}
