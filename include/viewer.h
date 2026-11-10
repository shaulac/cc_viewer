#ifndef EAGLE_VIEWER_VIEWER_H
#define EAGLE_VIEWER_VIEWER_H

#include <windows.h>
#include "raw.h"

int viewer_run(HINSTANCE instance, int show_command, const wchar_t *file, const RawParams *raw);

#endif
