#ifndef EAGLE_VIEWER_SAVE_NAME_H
#define EAGLE_VIEWER_SAVE_NAME_H

#include <stddef.h>
#include <wchar.h>
#include <windows.h>

int make_save_name(wchar_t *name, size_t count, int width, int height,
                   const wchar_t *raw_format, const wchar_t *source_path,
                   const SYSTEMTIME *time);

#endif
