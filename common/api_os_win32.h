#ifndef API_OS_WIN32_H
#define API_OS_WIN32_H

#include "common.h"
#include "api_os.h"

struct HWND__ typedef *HWND;

API HWND     OS_W32_HwndFromWindow(OS_Window window);
API String   OS_W32_StringFromHr(long hr, Arena* output_arena);
API wchar_t* OS_W32_StringToWide(String str, Arena* output_arena);
API String   OS_W32_WideToString(wchar_t const* wstr, Arena* output_arena);
API bool     OS_W32_MFStartup(OS_Error* out_err);
API bool     OS_W32_MFShutdown(OS_Error* out_err);

#endif