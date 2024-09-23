/*
*   Implementation of api_os*.h, C translation unit.
*   Most stuff is implemented here.
*/

#include "common.h"
#include "api_os.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#define DIRECTINPUT_VERSION 0x0800
#define INITGUID
//#define _WIN32_WINNT 0x0601
#define near
#define far

#include <windows.h>
#include <dwmapi.h>
#include <versionhelpers.h>
#include <dbt.h>
#include <stdlib.h>
#include <time.h>
#include <synchapi.h>
#include <shellscalingapi.h>
#include <xinput.h>
#include <dinput.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <dxgi1_2.h>
#include <d3d11_1.h>
#include <Objbase.h>
#include <shellapi.h>
#include <immintrin.h>
#include <windowsx.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <mfapi.h>
#include <codecapi.h>

#include "third_party/guid_utils.h"
#include "api_os_d3d11.h"
#include "api_os_opengl.h"
#include "api_os_win32.h"
#include "os_win32.h"

#include <dxgi1_6.h>

#ifdef CONFIG_DEBUG
#include <dxgidebug.h>
#endif

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "propsys")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfplat")

#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000

#define XINPUT_DEVSUBTYPE_UNKNOWN           0x00
#define XINPUT_DEVSUBTYPE_WHEEL             0x02
#define XINPUT_DEVSUBTYPE_ARCADE_STICK      0x03
#define XINPUT_DEVSUBTYPE_FLIGHT_STICK      0x04
#define XINPUT_DEVSUBTYPE_DANCE_PAD         0x05
#define XINPUT_DEVSUBTYPE_GUITAR            0x06
#define XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE  0x07
#define XINPUT_DEVSUBTYPE_DRUM_KIT          0x08
#define XINPUT_DEVSUBTYPE_GUITAR_BASS       0x0B
#define XINPUT_DEVSUBTYPE_ARCADE_PAD        0x13

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_TYPE_RGBA_ARB                       0x202B
#define WGL_COLOR_BITS_ARB                      0x2014
#define WGL_DEPTH_BITS_ARB                      0x2022
#define WGL_STENCIL_BITS_ARB                    0x2023
#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x0001
#define WGL_CONTEXT_FLAGS                       0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB               0x0001

typedef PROC(WINAPI* PFNWGLGETPROCADDRESSPROC)(LPCSTR);
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTPROC)(HDC);
typedef BOOL(WINAPI* PFNWGLMAKECURRENTPROC)(HDC, HGLRC);
typedef BOOL(WINAPI* PFNWGLDELETECONTEXTPROC)(HGLRC);
typedef BOOL(WINAPI* PFNWGLSWAPLAYERBUFFERSPROC)(HDC, UINT);
typedef BOOL(WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef BOOL(WINAPI* PFNWGLMAKECONTEXTCURRENTARBPROC)(HDC, HDC, HGLRC);
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int);

#define CLSID_MMDeviceEnumerator _def_CLSID_MMDeviceEnumerator
#define IID_IMMDeviceEnumerator _def_IID_IMMDeviceEnumerator
#define IID_IAudioClient _def_IID_IAudioClient
#define IID_IAudioRenderClient _def_IID_IAudioRenderClient
#define c_dfDIJoystick _def_c_dfDIJoystick
#define c_dfDIJoystick2 _def_c_dfDIJoystick2

static const CLSID CLSID_MMDeviceEnumerator;
static const IID IID_IMMDeviceEnumerator;
static const IID IID_IAudioClient;
static const IID IID_IAudioRenderClient;
static const DIDATAFORMAT c_dfDIJoystick;
static const DIDATAFORMAT c_dfDIJoystick2;

extern __declspec(dllimport) LONG WINAPI RtlGetVersion(RTL_OSVERSIONINFOW*);

#if 1
int __declspec(dllexport) NvOptimusEnablement = 1;
int __declspec(dllexport) AmdPowerXpressRequestHighPerformance = 1;
#endif

struct AudioDevice_
{
	IMMDevice* immdevice;
	LPWSTR dev_id;
	uint32 id;
	uint8 name[64];
	uint8 interface_name[64];
	uint8 description[64];
}
typedef AudioDevice_;

enum GamepadApi_
{
	GamepadApi_Null = 0,
	GamepadApi_XInput,
	GamepadApi_DirectInput,
}
typedef GamepadApi_;

struct Gamepad_
{
	GamepadApi_ api;
	bool connected;
	OS_GamepadState data;
	
	union
	{
		struct
		{
			DWORD index;
		} xinput;
		
		struct
		{
			IDirectInputDevice8W* device;
			GUID guid;
			
			int32 buttons[32];
			int32 button_count;
			int32 axes[16];
			int32 axis_count;
			int32 slider_count;
			int32 povs[8];
			int32 pov_count;
			
			intsize map_index;
		} dinput;
	};
}
typedef Gamepad_;

struct
{
	bool inited_graphics : 1;
	bool inited_sleep : 1;
	bool inited_audio : 1;
	
	HINSTANCE instance;
	HWND message_hwnd;
	uint64 time_frequency;
	uint64 time_started;
	SYSTEM_INFO system_info;
	RTL_OSVERSIONINFOW version_info;
	DWORD main_thread_id;
	Arena* polling_event_output_arena;
	OS_Capabilities caps;
	bool is_darkmode_enabled : 1;
	
	HRESULT (WINAPI* set_thread_description)(HANDLE hThread, PCWSTR lpThreadDescription);
	
	//- OS_Init_Graphics
	LPWSTR class_name;
	HCURSOR default_cursor;

	// TODO(ljre): Make this thread_local
	WindowData_* windows;
	
	//- OS_Init_Audio
	IMMDeviceEnumerator* device_enumerator;
	// NOTE(ljre): If the device we're outputting sound to is removed, this boolean is set. It means that we
	//             should output to the default device provided by IMMDeviceEnumerator_GetDefaultAudioEndpoint.
	//
	//             We probably need a way to tell if the device we were outputting to is back, and so set this
	//             to false.
	bool should_try_to_keep_on_default_endpoint;
	uint32 device_id_counter; // NOTE(ljre): Generative counter for device's IDs.
	uint32 default_device_id;
	int32 active_device_index;
	int32 device_count;
	AudioDevice_ devices[OS_Limits_MaxAudioDeviceCount];
	HANDLE audio_render_thread;
	IAudioClient* audio_client;
	HANDLE audio_empty_event;
	
	SRWLOCK client_lock;
	// NOTE(ljre): All of these will change if the output device has changed. The audio thread might be sad for
	//             a bit because the event it was waiting on was suddenly closed, but then it should try again
	//             with the new event handle set by the main thread.
	alignas(64) struct
	{
		HANDLE volatile event;
		int32 volatile sample_rate;
		int32 volatile channels;
		int32 volatile frame_pull_rate;
		IAudioRenderClient* volatile render_client;
		void (*volatile proc)(OS_AudioProcArgs const* args);
		void* volatile user_data;
	} audio_shared_stuff;
	
	//- OS_Init_Input
	Gamepad_ gamepads[OS_Limits_MaxGamepadCount];
	intsize free_gamepads[OS_Limits_MaxGamepadCount];
	intsize free_gamepad_count;
	uint32 connected_gamepads;
	OS_GamepadMapping const* gamepad_mappings;
	intsize gamepad_mappings_count;
	IDirectInput8W* dinput;
	HMODULE dinput_module;
	HMODULE xinput_module;
	DWORD (WINAPI* xinput_get_state)(DWORD index, XINPUT_STATE* state);
	DWORD (WINAPI* xinput_set_state)(DWORD index, XINPUT_VIBRATION* vibration);
	DWORD (WINAPI* xinput_get_capabilities)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
	HRESULT (WINAPI* dinput_create)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
}
static g_win32;

static OS_KeyboardKey const g_keyboard_key_table[256] = {
	[0] = OS_KeyboardKey_Any,
	
	[VK_ESCAPE] = OS_KeyboardKey_Escape,
	[VK_CONTROL] = OS_KeyboardKey_Control,
	[VK_SHIFT] = OS_KeyboardKey_Shift,
	[VK_MENU] = OS_KeyboardKey_Alt,
	[VK_TAB] = OS_KeyboardKey_Tab,
	[VK_RETURN] = OS_KeyboardKey_Enter,
	[VK_BACK] = OS_KeyboardKey_Backspace,
	[VK_UP] = OS_KeyboardKey_Up,
	[VK_DOWN] = OS_KeyboardKey_Down,
	[VK_LEFT] = OS_KeyboardKey_Left,
	[VK_RIGHT] = OS_KeyboardKey_Right,
	[VK_LCONTROL] = OS_KeyboardKey_LeftControl,
	[VK_RCONTROL] = OS_KeyboardKey_RightControl,
	[VK_LSHIFT] = OS_KeyboardKey_LeftShift,
	[VK_RSHIFT] = OS_KeyboardKey_RightShift,
	[VK_PRIOR] = OS_KeyboardKey_PageUp,
	[VK_NEXT] = OS_KeyboardKey_PageDown,
	[VK_END] = OS_KeyboardKey_End,
	[VK_HOME] = OS_KeyboardKey_Home,
	
	[VK_SPACE] = OS_KeyboardKey_Space,
	
	['0'] = OS_KeyboardKey_0,
	OS_KeyboardKey_1, OS_KeyboardKey_2, OS_KeyboardKey_3, OS_KeyboardKey_4,
	OS_KeyboardKey_5, OS_KeyboardKey_6, OS_KeyboardKey_7, OS_KeyboardKey_8,
	OS_KeyboardKey_9,
	
	['A'] = OS_KeyboardKey_A,
	OS_KeyboardKey_B, OS_KeyboardKey_C, OS_KeyboardKey_D, OS_KeyboardKey_E,
	OS_KeyboardKey_F, OS_KeyboardKey_G, OS_KeyboardKey_H, OS_KeyboardKey_I,
	OS_KeyboardKey_J, OS_KeyboardKey_K, OS_KeyboardKey_L, OS_KeyboardKey_M,
	OS_KeyboardKey_N, OS_KeyboardKey_O, OS_KeyboardKey_P, OS_KeyboardKey_Q,
	OS_KeyboardKey_R, OS_KeyboardKey_S, OS_KeyboardKey_T, OS_KeyboardKey_U,
	OS_KeyboardKey_V, OS_KeyboardKey_W, OS_KeyboardKey_X, OS_KeyboardKey_Y,
	OS_KeyboardKey_Z,

	[VK_OEM_COMMA] = OS_KeyboardKey_Comma,
	[VK_OEM_PERIOD] = OS_KeyboardKey_Period,
	
	[VK_F1] = OS_KeyboardKey_F1,
	OS_KeyboardKey_F2, OS_KeyboardKey_F3,  OS_KeyboardKey_F4,  OS_KeyboardKey_F4,
	OS_KeyboardKey_F5, OS_KeyboardKey_F6,  OS_KeyboardKey_F7,  OS_KeyboardKey_F8,
	OS_KeyboardKey_F9, OS_KeyboardKey_F10, OS_KeyboardKey_F11, OS_KeyboardKey_F12,
	OS_KeyboardKey_F13,
	
	[VK_NUMPAD0] = OS_KeyboardKey_Numpad0,
	OS_KeyboardKey_Numpad1, OS_KeyboardKey_Numpad2, OS_KeyboardKey_Numpad3, OS_KeyboardKey_Numpad4,
	OS_KeyboardKey_Numpad5, OS_KeyboardKey_Numpad6, OS_KeyboardKey_Numpad7, OS_KeyboardKey_Numpad8,
	OS_KeyboardKey_Numpad9,
};

static inline wchar_t*
StringToWide_(Arena* output_arena, String str)
{
	Trace();
	SafeAssert(str.size < INT32_MAX);
	
	int32 size = MultiByteToWideChar(CP_UTF8, 0, (char const*)str.data, (int32)str.size, NULL, 0);
	if (size == 0)
		return NULL;
	wchar_t* wide = ArenaPushDirtyAligned(output_arena, (size + 1) * SignedSizeof(wchar_t), alignof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, (char const*)str.data, (int32)str.size, wide, size);
	wide[size] = 0;
	
	return wide;
}

static inline String
WideToString_(Arena* output_arena, LPCWSTR wide)
{
	Trace();
	
	int32 size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
	if (size == 0)
		return StrNull;
	--size;
	uint8* str = ArenaPushDirtyAligned(output_arena, size, 1);
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, (char*)str, size, NULL, NULL);
	
	return StrMake(size, str);
}

static inline wchar_t*
StringToWideSingle_(SingleAllocator alloc, String str, AllocatorError* out_err)
{
	Trace();
	SafeAssert(str.size < INT32_MAX);
	
	int32 size = MultiByteToWideChar(CP_UTF8, 0, (char const*)str.data, (int32)str.size, NULL, 0);
	if (size == 0)
		return NULL;
	wchar_t* wide = AllocatorAllocArray(&alloc, (size + 1), SignedSizeof(wchar_t), alignof(wchar_t), out_err);
	if (!wide)
		return NULL;
	MultiByteToWideChar(CP_UTF8, 0, (char const*)str.data, (int32)str.size, wide, size);
	wide[size] = 0;
	
	return wide;
}

static inline String
WideToStringSingle_(SingleAllocator alloc, LPCWSTR wide, AllocatorError* out_err)
{
	Trace();
	
	int32 size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
	if (size == 0)
		return StrNull;
	--size;
	uint8* str = AllocatorAlloc(&alloc, size, 1, out_err);
	if (!str)
		return StrNull;
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, (char*)str, size, NULL, NULL);
	
	return StrMake(size, str);
}

static bool
IsSameWide_(LPWSTR left, LPWSTR right)
{
	while (*left == *right)
	{
		if (!*left)
			return true;
		
		++left;
		++right;
	}
	
	return false;
}

static bool
IsSameGuid_(GUID const* a, GUID const* b)
{
	return MemoryCompare(a, b, sizeof (GUID)) == 0;
}

static uint64
FileTimeToPosixTime_(FILETIME file_time)
{
	FILETIME posix_time;
	SYSTEMTIME posix_system_time = {
		.wYear = 1970,
		.wMonth = 1,
		.wDayOfWeek = 4,
		.wDay = 1,
		.wHour = 0,
		.wMinute = 0,
		.wSecond = 0,
		.wMilliseconds = 0,
	};
	SystemTimeToFileTime(&posix_system_time, &posix_time);
	
	uint64 result = 0;
	result +=  file_time.dwLowDateTime | (uint64) file_time.dwHighDateTime << 32;
	result -= posix_time.dwLowDateTime | (uint64)posix_time.dwHighDateTime << 32;
	
	return result;
}

static bool
FillOsErr_(OS_Error* out_err, DWORD code)
{
	OS_Error err = {
		.ok = false,
		.is_system_err = true,
		.code = code,
		.what = StrInit("OK"),
	};
	
	switch (code)
	{
		default: err.what = Str("Unknown error"); break;
		case 0: err.ok = true; err.is_system_err = false; break;
		case ERROR_FILE_NOT_FOUND: err.what = Str("The system cannot find the file specified."); break;
		case ERROR_PATH_NOT_FOUND: err.what = Str("The system cannot find the path specified."); break;
		case ERROR_ACCESS_DENIED: err.what = Str("Access is denied."); break;
		case ERROR_INVALID_HANDLE: err.what = Str("The handle is invalid."); break;
		case ERROR_NOT_ENOUGH_MEMORY: err.what = Str("Not enough storage is available to process this command."); break;
		case ERROR_OUTOFMEMORY: err.what = Str("Not enough storage is available to complete this operation."); break;
		case ERROR_INVALID_DRIVE: err.what = Str("The system cannot find the drive specified."); break;
		case ERROR_CURRENT_DIRECTORY: err.what = Str("The directory cannot be removed."); break;
		case ERROR_NOT_SAME_DEVICE: err.what = Str("The system cannot move the file to a different disk drive."); break;
		case ERROR_WRITE_PROTECT: err.what = Str("The media is write-protected."); break;
		case ERROR_HANDLE_DISK_FULL: err.what = Str("The disk is full."); break;
		case ERROR_NOT_SUPPORTED: err.what = Str("The request is not supported."); break;
		case ERROR_NETWORK_ACCESS_DENIED: err.what = Str("Network access is denied."); break;
		case ERROR_FILE_EXISTS: err.what = Str("The file exists."); break;
		case ERROR_CANNOT_MAKE: err.what = Str("The directory or file cannot be created."); break;
		case ERROR_INVALID_PARAMETER: err.what = Str("The parameter is incorrect."); break;
		case ERROR_BROKEN_PIPE: err.what = Str("The pipe has been ended."); break;
		case ERROR_OPEN_FAILED: err.what = Str("The system cannot open the device or file specified."); break;
		case ERROR_BUFFER_OVERFLOW: err.what = Str("The file name is too long."); break;
		case ERROR_DISK_FULL: err.what = Str("There is not enough space on the disk."); break;
		case ERROR_CALL_NOT_IMPLEMENTED: err.what = Str("This function is not supported on this system."); break;
		case ERROR_SEM_TIMEOUT: err.what = Str("The semaphore time-out period has expired."); break;
		case ERROR_INSUFFICIENT_BUFFER: err.what = Str("The data area passed to a system call is too small."); break;
		case ERROR_INVALID_NAME: err.what = Str("The file name, directory name, or volume label syntax is incorrect."); break;
		case ERROR_MOD_NOT_FOUND: err.what = Str("The specified module could not be found."); break;
		case ERROR_PROC_NOT_FOUND: err.what = Str("The specified procedure could not be found."); break;
		case ERROR_DIR_NOT_EMPTY: err.what = Str("The directory is not empty."); break;
		case ERROR_PATH_BUSY: err.what = Str("The path specified cannot be used at this time."); break;
		case ERROR_TOO_MANY_TCBS: err.what = Str("Cannot create another thread."); break;
		case ERROR_BAD_ARGUMENTS: err.what = Str("One or more arguments are not correct."); break;
		case ERROR_BAD_PATHNAME: err.what = Str("The specified path is invalid."); break;
		case ERROR_MAX_THRDS_REACHED: err.what = Str("No more threads can be created in the system."); break;
		case ERROR_LOCK_FAILED: err.what = Str("Unable to lock a region of a file."); break;
		case ERROR_BUSY: err.what = Str("The requested resource is in use."); break;
		case ERROR_ALREADY_EXISTS: err.what = Str("Cannot create a file when that file already exists."); break;
		case ERROR_INVALID_FLAG_NUMBER: err.what = Str("The flag passed is not correct."); break;
		case WAIT_TIMEOUT: err.what = Str("The wait operation timed out."); break;
		case ERROR_DIRECTORY: err.what = Str("The directory name is invalid."); break;
		case ERROR_DELETE_PENDING: err.what = Str("The file cannot be opened because it is in the process of being deleted."); break;
		case ERROR_INVALID_ADDRESS: err.what = Str("Attempt to access invalid address."); break;
		case ERROR_ILLEGAL_CHARACTER: err.what = Str("An illegal character was encountered. For a multibyte character set, this includes a lead byte without a succeeding trail byte. For the Unicode character set, this includes the characters 0xFFFF and 0xFFFE."); break;
		case ERROR_UNDEFINED_CHARACTER: err.what = Str("The Unicode character is not defined in the Unicode character set installed on the system."); break;
		//case ERROR_ACCESS_DENIED: err.what = Str(""); break;
	}
	
	//if (code != 0)
	//OS_LogErr("[WARN] win32: returned error code:\n\terr.code = %x\n\terr.what = \"%S\"\n", err.code, err.what);
	
	if (out_err)
		*out_err = err;
	else
		SafeAssert(code == 0);
	return code == 0;
}

static STARTUPINFOW
ChildProcessDescToStartupinfow_(OS_ChildProcessDesc const* child_desc)
{
	STARTUPINFOW result = { sizeof(result) };
	if (child_desc)
	{
		HANDLE stdin_ = child_desc->stdin_file.ptr;
		HANDLE stdout_ = child_desc->stdout_file.ptr;
		HANDLE stderr_ = child_desc->stderr_file.ptr;
		if (!stdin_)
			stdin_ = INVALID_HANDLE_VALUE;
		if (!stdout_)
			stdout_ = INVALID_HANDLE_VALUE;
		if (!stderr_)
			stderr_ = INVALID_HANDLE_VALUE;
		result.dwFlags |= STARTF_USESTDHANDLES;
		result.hStdInput = stdin_;
		result.hStdOutput = stdout_;
		result.hStdError = stderr_;
	}
	return result;
}

static void
SetupThreadScratchMemory_(void)
{
	Trace();
	ThreadContext* thread_context = ThisThreadContext();
	if (!thread_context->scratch[0].memory)
	{
		intz scratch_size = 2<<20;
		uint8* mem = VirtualAlloc(NULL, (SIZE_T)(scratch_size * ArrayLength(thread_context->scratch)), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		SafeAssert(mem);
		for (intz i = 0; i < ArrayLength(thread_context->scratch); ++i)
			thread_context->scratch[i] = ArenaFromMemory(mem + scratch_size*i, scratch_size);
	}
}

static void
FreeThreadScratchMemory_(void)
{
	Trace();
	ThreadContext* thread_context = ThisThreadContext();
	if (thread_context->scratch[0].memory)
	{
		uint8* mem = thread_context->scratch[0].memory;
		BOOL ok = VirtualFree(mem, 0, MEM_RELEASE);
		SafeAssert(ok);
		MemoryZero(thread_context->scratch, sizeof(thread_context->scratch));
	}
}

//- Graphics
static void
D3d11PresentProc_(OS_D3D11Api* d3d11_api)
{
	Trace();
	
	IDXGISwapChain1_Present(d3d11_api->swapchain1, 1, 0);
}

static void
D3d11ResizeBuffersProc_(OS_D3D11Api* d3d11_api)
{
	Trace();
	ID3D11DeviceContext_ClearState(d3d11_api->context);
	ID3D11RenderTargetView_Release(d3d11_api->target);
	d3d11_api->target = NULL;
	
	HRESULT hr;
	
	hr = IDXGISwapChain_ResizeBuffers(d3d11_api->swapchain, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
	SafeAssert(SUCCEEDED(hr));
	
	ID3D11Texture2D* backbuffer;
	hr = IDXGISwapChain_GetBuffer(d3d11_api->swapchain, 0, &IID_ID3D11Texture2D, (void**)&backbuffer);
	SafeAssert(SUCCEEDED(hr));
	
	D3D11_TEXTURE2D_DESC backbuffer_desc;
	ID3D11Texture2D_GetDesc(backbuffer, &backbuffer_desc);
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
	};
	hr = ID3D11Device_CreateRenderTargetView(d3d11_api->device, (ID3D11Resource*)backbuffer, &rtv_desc, &d3d11_api->target);
	SafeAssert(SUCCEEDED(hr));
	
	ID3D11Resource_Release(backbuffer);
	
	ID3D11DepthStencilView_Release(d3d11_api->depth_stencil);
	d3d11_api->depth_stencil = NULL;
	
	D3D11_TEXTURE2D_DESC depth_stencil_desc = {
		.Width = backbuffer_desc.Width,
		.Height = backbuffer_desc.Height,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0,
		},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_DEPTH_STENCIL,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
	};
	
	ID3D11Texture2D* depth_stencil;
	hr = ID3D11Device_CreateTexture2D(d3d11_api->device, &depth_stencil_desc, NULL, &depth_stencil);
	SafeAssert(SUCCEEDED(hr));
	hr = ID3D11Device_CreateDepthStencilView(d3d11_api->device, (ID3D11Resource*)depth_stencil, NULL, &d3d11_api->depth_stencil);
	SafeAssert(SUCCEEDED(hr));
	
	ID3D11Texture2D_Release(depth_stencil);
}

static void*
OglGetProc_(PFNWGLGETPROCADDRESSPROC wglGetProcAddress, HMODULE library, char const* name)
{
	void* result = NULL;
	
	if (wglGetProcAddress)
		result = wglGetProcAddress(name);
	
	if (!result ||
		result == (void *)0x1 || result == (void *)0x2 || result == (void *)0x3 ||
		result == (void *)-1)
	{
		result = GetProcAddress(library, name);
	}
	
	return result;
}

static void
OglPresentProc_(OS_OpenGLApi* api)
{
	WindowData_* window_data = api->window.ptr;
	SafeAssert(window_data && window_data->acquired_hdc);
	SwapBuffers(window_data->acquired_hdc);
}

static void
OglResizeBuffersProc_(OS_OpenGLApi* api)
{
	(void)api;
	// NOTE(ljre): OpenGL already resizes it's framebuffer when the window resizes
}

//-
typedef HRESULT WINAPI GetScaleFactorForMonitorProc_(HMONITOR, DEVICE_SCALE_FACTOR*);

struct MonitorEnumProcArgs_
{
	OS_Capabilities* caps;
	GetScaleFactorForMonitorProc_* get_scale_factor_for_monitor;
	int32 index;
}
typedef MonitorEnumProcArgs_;

static WINAPI BOOL
MonitorEnumProc_(HMONITOR monitor, HDC hdc, LPRECT rect, LPARAM lparam)
{
	MonitorEnumProcArgs_* args = (MonitorEnumProcArgs_*)lparam;
	if (args->index >= ArrayLength(args->caps->monitors))
		return FALSE;
	
	int32 index = args->index++;
	args->caps->monitors[index].virtual_x = rect->left;
	args->caps->monitors[index].virtual_y = rect->top;
	args->caps->monitors[index].virtual_width = rect->right - rect->left;
	args->caps->monitors[index].virtual_height = rect->bottom - rect->top;
	
	DEVICE_SCALE_FACTOR scale_factor = 100;
	if (args->get_scale_factor_for_monitor && SUCCEEDED(args->get_scale_factor_for_monitor(monitor, &scale_factor)))
		args->caps->monitors[index].scaling_percentage = (int32)scale_factor;
	
	return TRUE;
}

//- Input
static intsize
GenGamepadIndex_(void)
{
	SafeAssert(g_win32.free_gamepad_count > 0);
	return g_win32.free_gamepads[--g_win32.free_gamepad_count];
}

static void
ReleaseGamepadIndex_(intsize index)
{
	SafeAssert(g_win32.free_gamepad_count < ArrayLength(g_win32.free_gamepads));
	g_win32.free_gamepads[g_win32.free_gamepad_count++] = index;
}

static String
GetXInputSubTypeString_(uint8 subtype)
{
	switch (subtype)
	{
		default:
		case XINPUT_DEVSUBTYPE_UNKNOWN:          return Str("Unknown");
		case XINPUT_DEVSUBTYPE_GAMEPAD:          return Str("Gamepad");
		case XINPUT_DEVSUBTYPE_WHEEL:            return Str("Racing Wheel");
		case XINPUT_DEVSUBTYPE_ARCADE_STICK:     return Str("Arcade Stick");
		case XINPUT_DEVSUBTYPE_FLIGHT_STICK:     return Str("Flight Stick");
		case XINPUT_DEVSUBTYPE_DANCE_PAD:        return Str("Dance Pad");
		case XINPUT_DEVSUBTYPE_GUITAR:           return Str("Guitar");
		case XINPUT_DEVSUBTYPE_GUITAR_ALTERNATE: return Str("Guitar Alternate");
		case XINPUT_DEVSUBTYPE_GUITAR_BASS:      return Str("Guitar Bass");
		case XINPUT_DEVSUBTYPE_DRUM_KIT:         return Str("Drum");
		case XINPUT_DEVSUBTYPE_ARCADE_PAD:       return Str("Arcade Pad");
	}
}

static inline void
NormalizeAxis_(float32 axis[2])
{
	float32 len2 = axis[0]*axis[0] + axis[1]*axis[1];
	float32 const deadzone = 0.3f;
	
	if (len2 < deadzone*deadzone)
	{
		axis[0] = 0.0f;
		axis[1] = 0.0f;
	}
	else
	{
		float32 invlen = 1.0f / _mm_cvtss_f32(_mm_sqrt_ss(_mm_set1_ps(len2)));
		
		if (axis[0] < -0.05f || axis[0] > 0.05f)
			axis[0] *= invlen;
		else
			axis[0] = 0.0f;
		
		if (axis[1] < -0.05f || axis[1] > 0.05f)
			axis[1] *= invlen;
		else
			axis[1] = 0.0f;
	}
}

static void
TranslateController_(OS_GamepadState* out, OS_GamepadMapping const* map,
	bool const buttons[32], float32 const axes[16], int32 const povs[8])
{
	Trace();
	
	// Translate Buttons
	for (intsize i = 0; i < ArrayLength(map->buttons); ++i)
	{
		if (!map->buttons[i])
			continue;
		
		OS_GamepadButton as_button = -1;
		uint8 lower = map->buttons[i] & 255;
		uint8 higher = map->buttons[i] >> 8;
		(void)higher;
		
		switch (lower)
		{
			case OS_GamepadMap_Button_A: as_button = OS_GamepadButton_A; break;
			case OS_GamepadMap_Button_B: as_button = OS_GamepadButton_B; break;
			case OS_GamepadMap_Button_X: as_button = OS_GamepadButton_X; break;
			case OS_GamepadMap_Button_Y: as_button = OS_GamepadButton_Y; break;
			case OS_GamepadMap_Button_Left: as_button = OS_GamepadButton_Left; break;
			case OS_GamepadMap_Button_Right: as_button = OS_GamepadButton_Right; break;
			case OS_GamepadMap_Button_Up: as_button = OS_GamepadButton_Up; break;
			case OS_GamepadMap_Button_Down: as_button = OS_GamepadButton_Down; break;
			case OS_GamepadMap_Button_LeftShoulder: as_button = OS_GamepadButton_LB; break;
			case OS_GamepadMap_Button_RightShoulder: as_button = OS_GamepadButton_RB; break;
			case OS_GamepadMap_Button_LeftStick: as_button = OS_GamepadButton_LS; break;
			case OS_GamepadMap_Button_RightStick: as_button = OS_GamepadButton_RS; break;
			case OS_GamepadMap_Button_Start: as_button = OS_GamepadButton_Start; break;
			case OS_GamepadMap_Button_Back: as_button = OS_GamepadButton_Back; break;
			
			case OS_GamepadMap_Pressure_LeftTrigger: out->lt = (float32)(buttons[i] != 0); break;
			case OS_GamepadMap_Pressure_RightTrigger: out->rt = (float32)(buttons[i] != 0); break;
			
			// NOTE(ljre): Every other is invalid
			default: break;
		}
		
		if (as_button != -1)
		{
			bool is_down = buttons[i];
			out->buttons[as_button].changes += (out->buttons[as_button].is_down != is_down);
			out->buttons[as_button].is_down = is_down;
		}
	}
	
	// Translate Axes
	for (intsize i = 0; i < ArrayLength(map->axes); ++i)
	{
		if (!map->axes[i])
			continue;
		
		uint8 lower = map->axes[i] & 255;
		uint8 higher = map->axes[i] >> 8;
		float32 value = axes[i];
		
		if (higher & (1 << 7))
			value = -value;
		
		// Input Limit
		if (higher & (1 << 6))
			value = value * 0.5f + 1.0f;
		else if (higher & (1 << 5))
			value = value * 2.0f - 1.0f;
		
		value = Clamp(value, -1.0f, 1.0f);
		
		// Output Limit
		if (higher & (1 << 4))
			value = value * 2.0f - 1.0f;
		else if (higher & (1 << 3))
			value = value * 0.5f + 1.0f;
		
		value = Clamp(value, -1.0f, 1.0f);
		
		switch (lower)
		{
			case OS_GamepadMap_Axis_LeftX: out->left[0] = value; break;
			case OS_GamepadMap_Axis_LeftY: out->left[1] = value; break;
			case OS_GamepadMap_Axis_RightX: out->right[0] = value; break;
			case OS_GamepadMap_Axis_RightY: out->right[1] = value; break;
			
			case OS_GamepadMap_Pressure_LeftTrigger: out->lt = value; break;
			case OS_GamepadMap_Pressure_RightTrigger: out->rt = value; break;
			
			// NOTE(ljre): Every other is invalid
			default: break;
		}
	}
	
	// Translate Povs
	for (intsize i = 0; i < ArrayLength(map->povs); ++i)
	{
		if (!map->povs[i][0])
			continue;
		
		OS_GamepadButton as_button;
		int32 pov = povs[i];
		uint8 lower, higher;
		uint16 object;
		
		(void)higher;
		
		if (pov == -1)
			continue;
		
		as_button = -1;
		object = map->povs[i][pov >> 1];
		lower = object & 255;
		higher = object >> 8;
		switch (lower)
		{
			case OS_GamepadMap_Button_A: as_button = OS_GamepadButton_A; break;
			case OS_GamepadMap_Button_B: as_button = OS_GamepadButton_B; break;
			case OS_GamepadMap_Button_X: as_button = OS_GamepadButton_X; break;
			case OS_GamepadMap_Button_Y: as_button = OS_GamepadButton_Y; break;
			case OS_GamepadMap_Button_Left: as_button = OS_GamepadButton_Left; break;
			case OS_GamepadMap_Button_Right: as_button = OS_GamepadButton_Right; break;
			case OS_GamepadMap_Button_Up: as_button = OS_GamepadButton_Up; break;
			case OS_GamepadMap_Button_Down: as_button = OS_GamepadButton_Down; break;
			case OS_GamepadMap_Button_LeftShoulder: as_button = OS_GamepadButton_LB; break;
			case OS_GamepadMap_Button_RightShoulder: as_button = OS_GamepadButton_RB; break;
			case OS_GamepadMap_Button_LeftStick: as_button = OS_GamepadButton_LS; break;
			case OS_GamepadMap_Button_RightStick: as_button = OS_GamepadButton_RS; break;
			case OS_GamepadMap_Button_Start: as_button = OS_GamepadButton_Start; break;
			case OS_GamepadMap_Button_Back: as_button = OS_GamepadButton_Back; break;
			
			default: break;
		}
		
		if (as_button != -1)
		{
			bool is_down = buttons[i];
			out->buttons[as_button].changes += (out->buttons[as_button].is_down != is_down);
			out->buttons[as_button].is_down = is_down;
		}
		
		// NOTE(ljre): if it's not pointing to a diagonal, ignore next step
		if ((pov & 1) == 0)
			continue;
		
		as_button = -1;
		object = map->povs[i][(pov+1 & 7) >> 1];
		lower = object & 255;
		higher = object >> 8;
		switch (lower)
		{
			case OS_GamepadMap_Button_A: as_button = OS_GamepadButton_A; break;
			case OS_GamepadMap_Button_B: as_button = OS_GamepadButton_B; break;
			case OS_GamepadMap_Button_X: as_button = OS_GamepadButton_X; break;
			case OS_GamepadMap_Button_Y: as_button = OS_GamepadButton_Y; break;
			case OS_GamepadMap_Button_Left: as_button = OS_GamepadButton_Left; break;
			case OS_GamepadMap_Button_Right: as_button = OS_GamepadButton_Right; break;
			case OS_GamepadMap_Button_Up: as_button = OS_GamepadButton_Up; break;
			case OS_GamepadMap_Button_Down: as_button = OS_GamepadButton_Down; break;
			case OS_GamepadMap_Button_LeftShoulder: as_button = OS_GamepadButton_LB; break;
			case OS_GamepadMap_Button_RightShoulder: as_button = OS_GamepadButton_RB; break;
			case OS_GamepadMap_Button_LeftStick: as_button = OS_GamepadButton_LS; break;
			case OS_GamepadMap_Button_RightStick: as_button = OS_GamepadButton_RS; break;
			case OS_GamepadMap_Button_Start: as_button = OS_GamepadButton_Start; break;
			case OS_GamepadMap_Button_Back: as_button = OS_GamepadButton_Back; break;
			
			default: break;
		}
		
		if (as_button != -1)
		{
			bool is_down = buttons[i];
			out->buttons[as_button].changes += (out->buttons[as_button].is_down != is_down);
			out->buttons[as_button].is_down = is_down;
		}
	}
	
	// The end of your pain
	NormalizeAxis_(out->left);
	NormalizeAxis_(out->right);
}

static void
UpdateConnectedGamepad_(intsize index, bool accumulate)
{
	Trace();
	Gamepad_* gamepad = &g_win32.gamepads[index];
	
	if (!accumulate)
	{
		for (intsize i = 0; i < ArrayLength(gamepad->data.buttons); ++i)
			gamepad->data.buttons[i].changes = 0;
	}
	
	switch (gamepad->api)
	{
		//~ DirectInput
		case GamepadApi_DirectInput:
		{
			DIJOYSTATE2 state;
			IDirectInputDevice8_Poll(gamepad->dinput.device);
			
			HRESULT result = IDirectInputDevice8_GetDeviceState(gamepad->dinput.device, sizeof(state), &state);
			if (result == DIERR_NOTACQUIRED || result == DIERR_INPUTLOST)
			{
				IDirectInputDevice8_Acquire(gamepad->dinput.device);
				IDirectInputDevice8_Poll(gamepad->dinput.device);
				
				result = IDirectInputDevice8_GetDeviceState(gamepad->dinput.device, sizeof(state), &state);
			}
			
			if (result == DI_OK)
			{
				bool buttons[32];
				float32 axes[16];
				int32 povs[8];
				
				int32 axis_count = Min(gamepad->dinput.axis_count, ArrayLength(axes));
				int32 button_count = Min(gamepad->dinput.button_count, ArrayLength(buttons));
				int32 pov_count = Min(gamepad->dinput.pov_count, ArrayLength(povs));
				
				for (int32 i = 0; i < axis_count; ++i)
				{
					LONG raw = *(LONG*)((uint8*)&state + gamepad->dinput.axes[i]);
					axes[i] = ((float32)raw + 0.5f) / 32767.5f;
				}
				
				for (int32 i = 0; i < button_count; ++i)
				{
					LONG raw = *(LONG*)((uint8*)&state + gamepad->dinput.buttons[i]);
					buttons[i] = !!(raw & 128);
				}
				
				for (int32 i = 0; i < pov_count; ++i)
				{
					LONG raw = *(LONG*)((uint8*)&state + gamepad->dinput.povs[i]);
					if (raw == -1)
					{
						povs[i] = -1;
						continue;
					}
					
					// '(x%n + n) % n' is the same as 'x & (n-1)' when 'n' is a power of 2
					// negative values are going to wrap around
					raw = (raw / 4500) & 7;
					
					// Value of 'raw' is, assuming order Right, Up, Left, Down:
					// 0 = Right
					// 1 = Right & Up
					// 2 = Up
					// 3 = Up & Left
					// 4 = Left
					// 5 = Left & Down
					// 6 = Down
					// 7 = Down & Right
					povs[i] = (int32)raw;
				}
				
				OS_GamepadMapping mapping = { 0 };
				if (gamepad->dinput.map_index != -1 && gamepad->dinput.map_index < g_win32.gamepad_mappings_count)
					mapping = g_win32.gamepad_mappings[gamepad->dinput.map_index];
				
				TranslateController_(&gamepad->data, &mapping, buttons, axes, povs);
			}
			else
			{
				IDirectInputDevice8_Unacquire(gamepad->dinput.device);
				IDirectInputDevice8_Release(gamepad->dinput.device);
				
				gamepad->connected = false;
				gamepad->api = GamepadApi_Null;
				ReleaseGamepadIndex_(index);
			}
		} break;
		
		//~ XInput
		case GamepadApi_XInput:
		{
			XINPUT_STATE state;
			DWORD result = g_win32.xinput_get_state(gamepad->xinput.index, &state);
			
			if (result == ERROR_SUCCESS)
			{
				//- Sticks
				gamepad->data.left[0] = ((float32)state.Gamepad.sThumbLX + 0.5f) /  32767.5f;
				gamepad->data.left[1] = ((float32)state.Gamepad.sThumbLY + 0.5f) / -32767.5f;
				NormalizeAxis_(gamepad->data.left);
				
				gamepad->data.right[0] = ((float32)state.Gamepad.sThumbRX + 0.5f) /  32767.5f;
				gamepad->data.right[1] = ((float32)state.Gamepad.sThumbRY + 0.5f) / -32767.5f;
				NormalizeAxis_(gamepad->data.right);
				
				//- LR & RB
				gamepad->data.lt = (float32)state.Gamepad.bLeftTrigger / 255.0f;
				gamepad->data.rt = (float32)state.Gamepad.bRightTrigger / 255.0f;
				
				//- Buttons
				struct
				{
					OS_GamepadButton gpad;
					int32 xinput;
				}
				static const table[] = {
					{ OS_GamepadButton_Left, XINPUT_GAMEPAD_DPAD_LEFT },
					{ OS_GamepadButton_Right, XINPUT_GAMEPAD_DPAD_RIGHT },
					{ OS_GamepadButton_Up, XINPUT_GAMEPAD_DPAD_UP },
					{ OS_GamepadButton_Down, XINPUT_GAMEPAD_DPAD_DOWN },
					{ OS_GamepadButton_Start, XINPUT_GAMEPAD_START },
					{ OS_GamepadButton_Back, XINPUT_GAMEPAD_BACK },
					{ OS_GamepadButton_LS, XINPUT_GAMEPAD_LEFT_THUMB },
					{ OS_GamepadButton_RS, XINPUT_GAMEPAD_RIGHT_THUMB },
					{ OS_GamepadButton_LB, XINPUT_GAMEPAD_LEFT_SHOULDER },
					{ OS_GamepadButton_RB, XINPUT_GAMEPAD_RIGHT_SHOULDER },
					{ OS_GamepadButton_A, XINPUT_GAMEPAD_A },
					{ OS_GamepadButton_B, XINPUT_GAMEPAD_B },
					{ OS_GamepadButton_X, XINPUT_GAMEPAD_X },
					{ OS_GamepadButton_Y, XINPUT_GAMEPAD_Y },
				};
				
				for (int32 i = 0; i < ArrayLength(table); ++i)
				{
					bool is_down = state.Gamepad.wButtons & table[i].xinput;
					OS_ButtonState* button = &gamepad->data.buttons[table[i].gpad];
					button->changes += (button->is_down != is_down);
					button->is_down = is_down;
				}
			}
			else if (result == ERROR_DEVICE_NOT_CONNECTED)
			{
				gamepad->api = GamepadApi_Null;
				gamepad->connected = false;
				ReleaseGamepadIndex_(index);
			}
		} break;
		
		case GamepadApi_Null:
		{
			// NOTE(ljre): This should be unreachable... just to be safe
			gamepad->connected = false;
			ReleaseGamepadIndex_(index);
		} break;
	}
}

static BOOL CALLBACK
DirectInputEnumObjectsCallback_(DIDEVICEOBJECTINSTANCEW const* object, void* userdata)
{
	Gamepad_* gamepad = userdata;
	GUID const* object_guid = &object->guidType;
	
	if (DIDFT_GETTYPE(object->dwType) & DIDFT_AXIS)
	{
		int32 axis_offset;
		DIPROPRANGE range_property = {
			.diph = {
				.dwSize = sizeof(range_property),
				.dwHeaderSize = sizeof(range_property.diph),
				.dwObj = object->dwType,
				.dwHow = DIPH_BYID,
			},
			.lMin = INT16_MIN,
			.lMax = INT16_MAX,
		};
		
		if (IsSameGuid_(object_guid, &GUID_Slider))
		{
			axis_offset = (int32)DIJOFS_SLIDER(gamepad->dinput.slider_count);
			axis_offset |= 0x40000000;
		}
		else if (IsSameGuid_(object_guid, &GUID_XAxis )) axis_offset = (int32)DIJOFS_X;
		else if (IsSameGuid_(object_guid, &GUID_YAxis )) axis_offset = (int32)DIJOFS_Y;
		else if (IsSameGuid_(object_guid, &GUID_ZAxis )) axis_offset = (int32)DIJOFS_Z;
		else if (IsSameGuid_(object_guid, &GUID_RxAxis)) axis_offset = (int32)DIJOFS_RX;
		else if (IsSameGuid_(object_guid, &GUID_RyAxis)) axis_offset = (int32)DIJOFS_RY;
		else if (IsSameGuid_(object_guid, &GUID_RzAxis)) axis_offset = (int32)DIJOFS_RZ;
		else return DIENUM_CONTINUE;
		
		if (DI_OK != IDirectInputDevice8_SetProperty(gamepad->dinput.device, DIPROP_RANGE, &range_property.diph))
			return DIENUM_CONTINUE;
		
		if (axis_offset & 0x40000000)
			++gamepad->dinput.slider_count;
		
		gamepad->dinput.axes[gamepad->dinput.axis_count++] = axis_offset;
	}
	else if (DIDFT_GETTYPE(object->dwType) & DIDFT_BUTTON)
	{
		int32 button_offset = (int32)DIJOFS_BUTTON(gamepad->dinput.button_count);
		gamepad->dinput.buttons[gamepad->dinput.button_count++] = button_offset;
	}
	else if (DIDFT_GETTYPE(object->dwType) & DIDFT_POV)
	{
		int32 pov_offset = (int32)DIJOFS_POV(gamepad->dinput.pov_count);
		gamepad->dinput.povs[gamepad->dinput.pov_count++] = pov_offset;
	}
	
	return DIENUM_CONTINUE;
}

static void
SortGamepadObjectInt_(int32* array, intsize count)
{
	for (intsize i = 0; i < count; ++i)
	{
		for (intsize j = 0; j < count-1-i; ++j)
		{
			if (array[j] > array[j+1])
			{
				int32 tmp = array[j];
				array[j] = array[j+1];
				array[j+1] = tmp;
			}
		}
	}
}

static void
SortGamepadObjects_(Gamepad_* gamepad)
{
	SortGamepadObjectInt_(gamepad->dinput.buttons, gamepad->dinput.button_count);
	SortGamepadObjectInt_(gamepad->dinput.axes, gamepad->dinput.axis_count);
	SortGamepadObjectInt_(gamepad->dinput.povs, gamepad->dinput.pov_count);
	
	// Cleanup
	for (int32 i = 0; i < gamepad->dinput.axis_count; ++i)
		gamepad->dinput.axes[i] &= ~0x40000000;
}

static BOOL CALLBACK
DirectInputEnumDevicesCallback_(DIDEVICEINSTANCEW const* instance, void* userdata)
{
	Trace();
	
	if (g_win32.free_gamepad_count <= 0)
		return DIENUM_STOP;
	
	GUID const* guid = &instance->guidInstance;
	if (g_win32.xinput_get_state && IsXInputDevice(&instance->guidProduct))
		return DIENUM_CONTINUE;
	
	// Check if device is already added
	for (int32 i = 0; i < ArrayLength(g_win32.gamepads); ++i)
	{
		if (g_win32.gamepads[i].connected &&
			g_win32.gamepads[i].api == GamepadApi_DirectInput &&
			IsSameGuid_(guid, &g_win32.gamepads[i].dinput.guid))
		{
			return DIENUM_CONTINUE;
		}
	}
	
	// It's not! Add it!
	intsize index = GenGamepadIndex_();
	Gamepad_* gamepad = &g_win32.gamepads[index];
	gamepad->dinput.axis_count = 0;
	gamepad->dinput.slider_count = 0;
	gamepad->dinput.button_count = 0;
	gamepad->dinput.pov_count = 0;
	
	DIDEVCAPS capabilities = {
		.dwSize = sizeof(capabilities),
	};
	DIPROPDWORD word_property = {
		.diph = {
			.dwSize = sizeof(word_property),
			.dwHeaderSize = sizeof(word_property.diph),
			.dwObj = 0,
			.dwHow = DIPH_DEVICE,
		},
		.dwData = DIPROPAXISMODE_ABS,
	};
	
	if (DI_OK != IDirectInput8_CreateDevice(g_win32.dinput, guid, &gamepad->dinput.device, NULL))
		goto _failure;
	
	if (DI_OK != IDirectInputDevice8_SetCooperativeLevel(gamepad->dinput.device, NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))
		goto _failure;
	
	if (DI_OK != IDirectInputDevice8_SetDataFormat(gamepad->dinput.device, &c_dfDIJoystick2))
		goto _failure;
	
	if (DI_OK != IDirectInputDevice8_GetCapabilities(gamepad->dinput.device, &capabilities))
		goto _failure;
	
	if (DI_OK != IDirectInputDevice8_SetProperty(gamepad->dinput.device, DIPROP_AXISMODE, &word_property.diph))
		goto _failure;
	
	if (DI_OK != IDirectInputDevice8_EnumObjects(gamepad->dinput.device, DirectInputEnumObjectsCallback_, gamepad, DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV))
		goto _failure;
	
	if (DI_OK != IDirectInputDevice8_Acquire(gamepad->dinput.device))
		goto _failure;
	
	// Success
	gamepad->connected = true;
	gamepad->api = GamepadApi_DirectInput;
	gamepad->dinput.guid = *guid;
	gamepad->dinput.map_index = -1;
	SortGamepadObjects_(gamepad);
	
	int32 index_of_map_being_used = -1;
	
	// NOTE(ljre): Look for a mapping for this gamepad
	char guid_str[64];
	
	if (ConvertGuidToSDLGuid(instance, guid_str, sizeof(guid_str)))
	{
		for (intsize i = 0; i < g_win32.gamepad_mappings_count; ++i)
		{
			OS_GamepadMapping const* map = &g_win32.gamepad_mappings[i];
			
			if (MemoryCompare(guid_str, map->guid, ArrayLength(map->guid)) == 0)
			{
				gamepad->dinput.map_index = i;
				index_of_map_being_used = i;
				break;
			}
		}
	}
	
	(void)index_of_map_being_used;
	OS_LogErr(
		"[INFO] win32: directinput device connected:\n"
		"\tOS Index: %Z\n"
		"\tGUID Instance: %08x-%04x-%04x-%04x-%04x-%08x\n"
		"\tGUID Product: %08x-%04x-%04x-%04x-%04x-%08x\n"
		"\tMappings Index: %i\n",
		index,
		guid->Data1, guid->Data2, guid->Data3, *(uint16*)guid->Data4, ((uint16*)guid->Data4)[1], *(uint32*)guid->Data4,
		instance->guidProduct.Data1, instance->guidProduct.Data2, instance->guidProduct.Data3, *(uint16*)instance->guidProduct.Data4, ((uint16*)instance->guidProduct.Data4)[1], *(uint32*)instance->guidProduct.Data4,
		index_of_map_being_used);
	
	return DIENUM_CONTINUE;
	
	//-
	_failure:
	{
		gamepad->connected = false;
		gamepad->api = GamepadApi_Null;
		IDirectInputDevice8_Release(gamepad->dinput.device);
		gamepad->dinput.device = NULL;
		ReleaseGamepadIndex_(index);
		
		return DIENUM_CONTINUE;
	}
}

static void
CheckForGamepads_(void)
{
	Trace();
	
	// NOTE(ljre): Don't check for more devices if there isn't more space
	if (g_win32.free_gamepad_count <= 0)
		return;
	
	// Direct Input
	if (g_win32.dinput)
		IDirectInput8_EnumDevices(g_win32.dinput, DI8DEVCLASS_GAMECTRL, DirectInputEnumDevicesCallback_, NULL, DIEDFL_ATTACHEDONLY);
	
	// XInput
	if (g_win32.xinput_get_capabilities)
	{
		for (DWORD i = 0; i < 4 && g_win32.free_gamepad_count > 0; ++i)
		{
			bool already_exists = false;
			
			for (int32 index = 0; index < ArrayLength(g_win32.gamepads); ++index)
			{
				if (g_win32.gamepads[index].connected &&
					g_win32.gamepads[index].api == GamepadApi_XInput &&
					g_win32.gamepads[index].xinput.index == i)
				{
					already_exists = true;
				}
			}
			
			XINPUT_CAPABILITIES cap;
			if (!already_exists && g_win32.xinput_get_capabilities(i, 0, &cap) == ERROR_SUCCESS)
			{
				intsize index = GenGamepadIndex_();
				Gamepad_* gamepad = &g_win32.gamepads[index];
				
				gamepad->connected = true;
				gamepad->api = GamepadApi_XInput;
				gamepad->xinput.index = i;
				
				OS_LogErr(
					"[INFO] win32: xinput device connected:\n"
					"\tOS Index: %i\n"
					"\tXInput Index: %i\n"
					"\tSubtype: %S\n",
					index, i, GetXInputSubTypeString_(cap.SubType));
			}
		}
	}
}

//- Audio
static bool
EnumerateAudioEndpoints_(void)
{
	Trace();
	bool result = true;
	HRESULT hr;
	
	//- Check if the devices we already know are still available
	for (int32 i = 0; i < g_win32.device_count; ++i)
	{
		IMMDevice* immdevice = g_win32.devices[i].immdevice;
		LPWSTR dev_id = g_win32.devices[i].dev_id;
		bool should_remove = false;
		
		// NOTE(ljre): If a device was unplugged, IMMDevice_GetState() on that device will segfault.
		//             Getting a new IMMDevice from the enumerator _and then_ calling IMMDevice_GetState()
		//             works as expected. I don't know why this happens.
		
		IMMDevice* tmp;
		if (!SUCCEEDED(IMMDeviceEnumerator_GetDevice(g_win32.device_enumerator, dev_id, &tmp)))
			should_remove = true;
		else
		{
			DWORD state;
			hr = IMMDevice_GetState(tmp, &state);
			
			if (!SUCCEEDED(hr) || state != DEVICE_STATE_ACTIVE)
				should_remove = true;
			
			IMMDevice_Release(tmp);
		}
		
		if (should_remove)
		{
			// NOTE(ljre): If uncommented, segfault. Maybe the string is already free'd when the device is
			//             unplugged? Either way, this code should run so infrequently that it shouldn't be
			//             a problem.
			
			//CoTaskMemFree(dev_id);
			
			IMMDevice_Release(immdevice);
			
			if (g_win32.active_device_index == i)
			{
				g_win32.active_device_index = -1;
				g_win32.should_try_to_keep_on_default_endpoint = true;
			}
			else if (g_win32.active_device_index > i)
				--g_win32.active_device_index;
			
			intsize remaining = g_win32.device_count - i - 1;
			
			if (remaining > 0)
				MemoryMove(&g_win32.devices[i], &g_win32.devices[i + 1], SignedSizeof(g_win32.devices[0]) * remaining);
			
			--g_win32.device_count;
			--i;
		}
	}
	
	//- Enum available devices
	IMMDeviceCollection* collection;
	hr = IMMDeviceEnumerator_EnumAudioEndpoints(g_win32.device_enumerator, eRender, DEVICE_STATE_ACTIVE, &collection);
	
	if (!SUCCEEDED(hr))
		result = false;
	else
	{
		uint32 count;
		if (!SUCCEEDED(IMMDeviceCollection_GetCount(collection, &count)))
			count = 0;
		
		for (uint32 i = 0; i < count; ++i)
		{
			IMMDevice* immdevice = NULL;
			LPWSTR dev_id;
			
			if (!SUCCEEDED(IMMDeviceCollection_Item(collection, i, &immdevice)) ||
				!SUCCEEDED(IMMDevice_GetId(immdevice, &dev_id)))
			{
				if (immdevice)
					IMMDevice_Release(immdevice);
				
				result = false;
				break;
			}
			
			int32 found_index = -1;
			for (int32 j = 0; j < g_win32.device_count; ++j)
			{
				bool are_same_device = IsSameWide_(dev_id, g_win32.devices[j].dev_id);
				
				if (are_same_device)
				{
					found_index = j;
					break;
				}
			}
			
			bool successfully_added = false;
			
			if (found_index == -1 && g_win32.device_count < ArrayLength(g_win32.devices))
			{
				AudioDevice_ this_device = { 0 };
				
				IPropertyStore* property_store;
				hr = IMMDevice_OpenPropertyStore(immdevice, STGM_READ, &property_store);
				if (SUCCEEDED(hr))
				{
					PROPVARIANT property_interface_name = { 0 };
					PROPVARIANT property_description = { 0 };
					PROPVARIANT property_name = { 0 };
					PropVariantInit(&property_interface_name);
					PropVariantInit(&property_description);
					PropVariantInit(&property_name);
					
					if (SUCCEEDED(IPropertyStore_GetValue(property_store, &PKEY_DeviceInterface_FriendlyName, &property_interface_name)) &&
						SUCCEEDED(IPropertyStore_GetValue(property_store, &PKEY_Device_DeviceDesc, &property_description)) &&
						SUCCEEDED(IPropertyStore_GetValue(property_store, &PKEY_Device_FriendlyName, &property_name)))
					{
						WideCharToMultiByte(CP_UTF8, 0, property_interface_name.pwszVal, -1, (char*)this_device.interface_name, ArrayLength(this_device.interface_name), NULL, NULL);
						WideCharToMultiByte(CP_UTF8, 0, property_description.pwszVal, -1, (char*)this_device.description, ArrayLength(this_device.description), NULL, NULL);
						WideCharToMultiByte(CP_UTF8, 0, property_name.pwszVal, -1, (char*)this_device.name, ArrayLength(this_device.name), NULL, NULL);
						
						this_device.immdevice = immdevice;
						this_device.id = ++g_win32.device_id_counter;
						this_device.dev_id = dev_id;
						g_win32.devices[g_win32.device_count++] = this_device;
						successfully_added = true;
					}
					
					PropVariantClear(&property_interface_name);
					PropVariantClear(&property_description);
					PropVariantClear(&property_name);
					IPropertyStore_Release(property_store);
				}
			}
			
			if (!successfully_added)
			{
				CoTaskMemFree(dev_id);
				IMMDevice_Release(immdevice);
			}
		}
		
		IMMDeviceCollection_Release(collection);
		collection = NULL;
	}
	
	//- Find default device endpoint
	IMMDevice* immdevice;
	hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(g_win32.device_enumerator, eRender, eConsole, &immdevice);
	if (SUCCEEDED(hr))
	{
		LPWSTR dev_id;
		hr = IMMDevice_GetId(immdevice, &dev_id);
		
		if (SUCCEEDED(hr))
		{
			for (int32 i = 0; i < g_win32.device_count; ++i)
			{
				bool is_same_device = IsSameWide_(dev_id, g_win32.devices[i].dev_id);
				if (is_same_device)
				{
					g_win32.default_device_id = g_win32.devices[i].id;
					break;
				}
			}
			
			CoTaskMemFree(dev_id);
		}
		
		IMMDevice_Release(immdevice);
	}
	
	return result;
}

static bool
ChangeAudioEndpoint_(uint32 id, OS_AudioProc* proc, void* user_data)
{
	Trace();
	bool result = true;
	
	IAudioClient* audio_client = NULL;
	IAudioRenderClient* audio_render_client = NULL;
	int32 device_index = -1;
	int32 new_frame_pull_rate = 0;
	int32 new_sample_rate = 0;
	int32 new_channels = 0;
	HANDLE event = NULL;
	
	//- Try to find the device by the ID
	for (int32 i = 0; i < g_win32.device_count; ++i)
	{
		if (g_win32.devices[i].id == id)
		{
			device_index = i;
			break;
		}
	}
	
	//- Try to create the client and render client interfaces
	if (device_index == -1)
		result = false;
	else
	{
		IMMDevice* immdevice = g_win32.devices[device_index].immdevice;
		WAVEFORMATEX* wave_format = NULL;
		
		HRESULT hr = IMMDevice_Activate(immdevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audio_client);
		if (!SUCCEEDED(hr))
			goto lbl_error;
		
		hr = IAudioClient_GetMixFormat(audio_client, &wave_format);
		if (!SUCCEEDED(hr))
			goto lbl_error;
		
		uint16 channels = wave_format->nChannels;
		uint32 sample_rate = wave_format->nSamplesPerSec;
		
		const REFERENCE_TIME requested_sound_duration = 10000000;
		uint16 bytes_per_sample = sizeof(int16);
		uint16 bits_per_sample = bytes_per_sample * 8;
		uint16 block_align = channels * bytes_per_sample;
		uint32 average_bytes_per_second = block_align * sample_rate;
		
		WAVEFORMATEX new_wave_format = {
			.wFormatTag = WAVE_FORMAT_PCM,
			.nChannels = channels,
			.nSamplesPerSec = sample_rate,
			.nAvgBytesPerSec = average_bytes_per_second,
			.nBlockAlign = block_align,
			.wBitsPerSample = bits_per_sample,
			.cbSize = 0,
		};
		
		hr = IAudioClient_Initialize(
			audio_client, AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_RATEADJUST |
			AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
			requested_sound_duration, 0, &new_wave_format, NULL);
		if (!SUCCEEDED(hr))
			goto lbl_error;
		
		hr = IAudioClient_GetService(audio_client, &IID_IAudioRenderClient, (void**)&audio_render_client);
		if (!SUCCEEDED(hr))
			goto lbl_error;
		
		REFERENCE_TIME reftime;
		hr = IAudioClient_GetDevicePeriod(audio_client, &reftime, NULL);
		if (!SUCCEEDED(hr))
			goto lbl_error;
		
		new_frame_pull_rate = 2 * (int32)(reftime * (int64)(sample_rate / channels) / 10000000);
		new_sample_rate = (int32)sample_rate;
		new_channels = (int32)channels;
		
		event = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!event)
			goto lbl_error;
		
		hr = IAudioClient_SetEventHandle(audio_client, event);
		if (!SUCCEEDED(hr))
			goto lbl_error;
		
		hr = IAudioClient_Start(audio_client);
		if (!SUCCEEDED(hr))
			goto lbl_error;
		
		CoTaskMemFree(wave_format);
		
		//- Error
		if (0) lbl_error:
		{
			result = false;
			
			if (event)
				CloseHandle(event);
			if (wave_format)
				CoTaskMemFree(wave_format);
			if (audio_render_client)
				IAudioRenderClient_Release(audio_render_client);
			if (audio_client)
				IAudioClient_Release(audio_client);
		}
	}
	
	//- Apply changes if needed.
	if (result)
	{
		AcquireSRWLockExclusive(&g_win32.client_lock);
		
		if (g_win32.audio_shared_stuff.render_client)
			IAudioClient_Release(g_win32.audio_shared_stuff.render_client);
		
		if (g_win32.audio_client)
		{
			IAudioClient_Stop(g_win32.audio_client);
			IAudioClient_Release(g_win32.audio_client);
		}
		
		g_win32.active_device_index = device_index;
		g_win32.audio_client = audio_client;
		g_win32.audio_shared_stuff.render_client = audio_render_client;
		g_win32.audio_shared_stuff.frame_pull_rate = new_frame_pull_rate;
		g_win32.audio_shared_stuff.sample_rate = new_sample_rate;
		g_win32.audio_shared_stuff.channels = new_channels;
		if (proc)
		{
			g_win32.audio_shared_stuff.proc = proc;
			g_win32.audio_shared_stuff.user_data = user_data;
		}
		
		// NOTE(ljre): Making sure there's always a valid event assigned to 'g_audio.event'...
		HANDLE old_event = g_win32.audio_shared_stuff.event;
		g_win32.audio_shared_stuff.event = event;
		_ReadWriteBarrier();
		
		if (old_event != g_win32.audio_empty_event)
			CloseHandle(old_event);
		else
			SetEvent(g_win32.audio_empty_event);
		
		ReleaseSRWLockExclusive(&g_win32.client_lock);
	}
	
	if (!result)
		OS_LogErr("[WARN] win32: failed to change audio endpoint to %u\n", id);
	
	return result;
}

static void
CloseCurrentAudioEndpoint_(void)
{
	Trace();
	
	if (g_win32.inited_audio && g_win32.audio_client)
	{
		AcquireSRWLockExclusive(&g_win32.client_lock);
		
		IAudioClient_Release(g_win32.audio_shared_stuff.render_client);
		g_win32.audio_shared_stuff.render_client = NULL;
		
		IAudioClient_Stop(g_win32.audio_client);
		IAudioClient_Release(g_win32.audio_client);
		g_win32.audio_client = NULL;
		
		HANDLE old_event = g_win32.audio_shared_stuff.event;
		g_win32.audio_shared_stuff.event = g_win32.audio_empty_event;
		_ReadWriteBarrier();
		
		if (old_event != g_win32.audio_empty_event)
			CloseHandle(old_event);
		
		ReleaseSRWLockExclusive(&g_win32.client_lock);
	}
}

static void
UpdateAudioEndpointIfNeeded_(void)
{
	Trace();
	
	if (g_win32.inited_audio)
	{
		EnumerateAudioEndpoints_();
		
		if (g_win32.active_device_index == -1 ||
			g_win32.should_try_to_keep_on_default_endpoint && g_win32.devices[g_win32.active_device_index].id != g_win32.default_device_id)
		{
			ChangeAudioEndpoint_(g_win32.default_device_id, NULL, NULL);
		}
	}
}

//- Callbacks
static DWORD WINAPI
ThreadProc_(LPVOID arg)
{
	Trace();
	OS_ThreadDesc thread_desc = *(OS_ThreadDesc*)arg;
	OS_HeapFree(arg);
	SetupThreadScratchMemory_();

	ThreadContext* thread_context = ThisThreadContext();
	thread_context->logger = OS_DefaultLogger();
	thread_context->assertion_failure_proc = OS_DefaultAssertionFailureProc;
	
	int32 result = thread_desc.proc(thread_desc.user_data);
	
	FreeThreadScratchMemory_();
	return (DWORD)result;
}

static DWORD WINAPI
AudioThreadProc_(void* user_data)
{
	if (g_win32.set_thread_description)
		g_win32.set_thread_description(GetCurrentThread(), L"Win32AudioThreadProc");
	
	SetupThreadScratchMemory_();

	HANDLE event;
	while (event = g_win32.audio_shared_stuff.event, event)
	{
		DWORD result = WaitForSingleObject(event, 2000);
		if (result != WAIT_OBJECT_0 || event == g_win32.audio_empty_event)
			continue;
		
		Trace();
		AcquireSRWLockExclusive(&g_win32.client_lock);
		
		int32 frame_count = g_win32.audio_shared_stuff.frame_pull_rate;
		int32 channels = g_win32.audio_shared_stuff.channels;
		int32 sample_rate = g_win32.audio_shared_stuff.sample_rate;
		int32 sample_count = frame_count * channels;
		IAudioRenderClient* render_client = g_win32.audio_shared_stuff.render_client;
		OS_AudioProc* proc = g_win32.audio_shared_stuff.proc;
		void* user_data = g_win32.audio_shared_stuff.user_data;
		
		if (render_client && proc && frame_count)
		{
			BYTE* buffer;
			HRESULT hr = IAudioRenderClient_GetBuffer(render_client, frame_count, &buffer);
			
			if (Likely(SUCCEEDED(hr)))
			{
				proc(&(OS_AudioProcArgs) {
					.user_data = user_data,
					.samples = (int16*)buffer,
					.sample_count = sample_count,
					.sample_rate = sample_rate,
					.channel_count = channels,
				});
				IAudioRenderClient_ReleaseBuffer(render_client, frame_count, 0);
			}
		}
		
		ReleaseSRWLockExclusive(&g_win32.client_lock);
	}

	FreeThreadScratchMemory_();
	
	return 0;
}

static LRESULT CALLBACK
WindowProc_(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	Trace();
	SafeAssert(GetCurrentThreadId() == g_win32.main_thread_id);
	
	LRESULT result = 0;
	WindowData_* window_data = (WindowData_*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	OS_Event os_event = {
		.window_handle = { window_data },
	};
	
	if (!window_data)
		return DefWindowProcW(hwnd, message, wparam, lparam);
	
	switch (message)
	{
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_QUIT:
		{
			os_event.kind = OS_EventKind_WindowClose;
		} break;
		
		case WM_ACTIVATE:
		{
			os_event.kind = (wparam != WA_INACTIVE) ? OS_EventKind_WindowGotFocus : OS_EventKind_WindowLostFocus;
			if (window_data->mouse_lock == OS_MouseLockKind_LockInWindowRegion)
			{
				if (wparam != WA_INACTIVE)
				{
					union
					{
						RECT rect;
						struct { POINT top_left, bottom_right; };
					} clip;
					GetClientRect(hwnd, &clip.rect);
					ClientToScreen(hwnd, &clip.top_left);
					ClientToScreen(hwnd, &clip.bottom_right);
					ClipCursor(&clip.rect);
				}
				else
					ClipCursor(NULL);
			}
		} break;
		
		case WM_SIZE:
		{
			os_event.kind = OS_EventKind_WindowResize;
			os_event.window_resize.user_width  = LOWORD(lparam);
			os_event.window_resize.user_height = HIWORD(lparam);
			
			RECT rect;
			if (GetWindowRect(hwnd, &rect))
			{
				os_event.window_resize.total_width = rect.right - rect.left;
				os_event.window_resize.total_height = rect.bottom - rect.top;
			}
		} break;
		
		case WM_UNICHAR:
		{
			if (wparam == UNICODE_NOCHAR)
			{
				result = TRUE;
				break;
			}
			if ((wparam < 32 && wparam != '\r' && wparam != '\t' && wparam != '\b') || (wparam >= 0x7f && wparam <= 0xa0))
				break;
			
			uint32 codepoint = (uint32)wparam;
			int32 repeat_count = (lparam & 0xffff);
			
			os_event.kind = OS_EventKind_WindowTyping;
			os_event.window_typing.codepoint = codepoint;
			os_event.window_typing.is_repeat = (repeat_count > 1);
		} break;

		case WM_CHAR:
		{
			uint32 codepoint = (uint32)wparam;
			int32 repeat_count = (lparam & 0xffff);

			if (IS_HIGH_SURROGATE(wparam) || IS_LOW_SURROGATE(wparam))
			{
				if (IS_HIGH_SURROGATE(wparam))
					window_data->high_surrogate = wparam;
				else if (IS_LOW_SURROGATE(wparam))
					window_data->low_surrogate = wparam;

				if (window_data->high_surrogate && window_data->low_surrogate)
				{
					codepoint = 0;
					if (IS_SURROGATE_PAIR(window_data->high_surrogate, window_data->low_surrogate))
					{
						codepoint = 0x10000;
						codepoint += (window_data->high_surrogate & 0x3FF) << 10;
						codepoint += (window_data->low_surrogate & 0x3FF);
					}
				}
				else
					break;
			}
			window_data->high_surrogate = 0;
			window_data->low_surrogate = 0;

			if (codepoint < 32 || (codepoint >= 0x7f && codepoint <= 0xa0))
				break;
			os_event.kind = OS_EventKind_WindowTyping;
			os_event.window_typing.codepoint = codepoint;
			os_event.window_typing.is_repeat = repeat_count;
			os_event.window_typing.ctrl = GetKeyState(VK_CONTROL) & 0x8000;
			os_event.window_typing.shift = GetKeyState(VK_SHIFT) & 0x8000;
			os_event.window_typing.alt = GetKeyState(VK_MENU) & 0x8000;
		} break;
		
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			if (wparam > 0 && wparam < ArrayLength(g_keyboard_key_table))
			{
				OS_KeyboardKey key = g_keyboard_key_table[wparam];
				if (key)
				{
					int32 repeat_count = (lparam & 0xffff);

					os_event.kind = (message == WM_KEYDOWN) ? OS_EventKind_WindowKeyPressed : OS_EventKind_WindowKeyReleased;
					os_event.window_key.key = key;
					os_event.window_key.is_repeat = (repeat_count > 0);
					os_event.window_key.ctrl = GetKeyState(VK_CONTROL) & 0x8000;
					os_event.window_key.shift = GetKeyState(VK_SHIFT) & 0x8000;
					os_event.window_key.alt = GetKeyState(VK_MENU) & 0x8000;
				}
			}

			result = DefWindowProcW(hwnd, message, wparam, lparam);
		} break;

		case WM_SYSCOMMAND:
		{
			if (wparam != SC_KEYMENU || (lparam >> 16) <= 0)
				result = DefWindowProcW(hwnd, message, wparam, lparam);
		} break;

		case WM_MOUSEWHEEL:
		{
			int32 delta = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
			POINT px = {
				GET_X_LPARAM(lparam),
				GET_Y_LPARAM(lparam),
			};
			ScreenToClient(hwnd, &px);

			os_event.kind = OS_EventKind_WindowMouseWheel;
			os_event.window_mouse_wheel.delta = delta;
			os_event.window_mouse_wheel.mouse_x = px.x;
			os_event.window_mouse_wheel.mouse_y = px.y;
		} break;
		
		case WM_DEVICECHANGE:
		{
			CheckForGamepads_();
			UpdateAudioEndpointIfNeeded_();
		} break;

		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN:
		{
			OS_MouseButton index = -1;
			bool down = false;
			switch (message)
			{
				case WM_LBUTTONUP:   index = OS_MouseButton_Left; break;
				case WM_LBUTTONDOWN: index = OS_MouseButton_Left; down = true; break;
				case WM_MBUTTONUP:   index = OS_MouseButton_Middle; break;
				case WM_MBUTTONDOWN: index = OS_MouseButton_Middle; down = true; break;
				case WM_RBUTTONUP:   index = OS_MouseButton_Right; break;
				case WM_RBUTTONDOWN: index = OS_MouseButton_Right; down = true; break;
			}

			if (index != -1)
			{
				OS_LockExclusive(&window_data->mouse_state_lock);
				OS_ButtonState* btn = &window_data->mouse_state.buttons[index];
				if (btn->is_down != down)
					btn->changes += 1;
				btn->is_down = down;
				OS_UnlockExclusive(&window_data->mouse_state_lock);

				os_event.kind = (down) ? OS_EventKind_WindowMouseClick : OS_EventKind_WindowMouseRelease;
				os_event.window_mouse_click.button = index;
				os_event.window_mouse_click.mouse_x = GET_X_LPARAM(lparam);
				os_event.window_mouse_click.mouse_y = GET_Y_LPARAM(lparam);
			}
		} break;

		//case WM_MOUSEMOVE:
		//{
		//	POINT point;
		//	point.x = GET_X_LPARAM(lparam);
		//	point.y = GET_Y_LPARAM(lparam);
		//	ScreenToClient(hwnd, &point);
//
		//	OS_LockExclusive(&window_data->mouse_state_lock);
		//	window_data->mouse_state.pos[0] = (float32)point.x;
		//	window_data->mouse_state.pos[1] = (float32)point.y;
		//	OS_UnlockExclusive(&window_data->mouse_state_lock);
		//} break;
		
		case WM_SETCURSOR:
		{
			if (LOWORD(lparam) == HTCLIENT)
			{
				HCURSOR cursor = g_win32.default_cursor;
				if (window_data->mouse_lock == OS_MouseLockKind_Hide ||
					window_data->mouse_lock == OS_MouseLockKind_HideAndClip)
					cursor = NULL;
				SetCursor(cursor);
				result = TRUE;
			}
			else
				result = DefWindowProcW(hwnd, message, wparam, lparam);
		} break;
		
		default: result = DefWindowProcW(hwnd, message, wparam, lparam); break;
	}
	
	if (os_event.kind && g_win32.polling_event_output_arena)
		ArenaPushStructData(g_win32.polling_event_output_arena, OS_Event, &os_event);
	return result;
}

/*
#ifdef CONFIG_DONT_USE_CRT
#ifdef CONFIG_WINDOWS_SUBSYSTEM
void WINAPI
WinMainCRTStartup(void)
#else //CONFIG_WINDOWS_SUBSYSTEM
void WINAPI
mainCRTStartup(void)
#endif
#else //CONFIG_DONT_USE_CRT
*/

#ifdef CONFIG_WINDOWS_SUBSYSTEM
int WINAPI
wWinMain(HINSTANCE instance_, HINSTANCE previous_, LPWSTR args_, int cmd_show_)
#else //CONFIG_WINDOWS_SUBSYSTEM
int
wmain(int argc_, wchar_t** argv_)
#endif
{
	Trace();
	
	// basics
	g_win32.instance = GetModuleHandleW(NULL);
	g_win32.class_name = L"ThisClassName";
	g_win32.main_thread_id = GetCurrentThreadId();
	QueryPerformanceCounter((LARGE_INTEGER*)&g_win32.time_started);
	QueryPerformanceFrequency((LARGE_INTEGER*)&g_win32.time_frequency);
	GetNativeSystemInfo(&g_win32.system_info);
	g_win32.version_info = (RTL_OSVERSIONINFOW) { sizeof(RTL_OSVERSIONINFOW) };
	RtlGetVersion(&g_win32.version_info);

	{
		SetupThreadScratchMemory_();
		ThreadContext* thread_context = ThisThreadContext();
		thread_context->logger = OS_DefaultLogger();
		thread_context->assertion_failure_proc = OS_DefaultAssertionFailureProc;
	}

	// argc & argv
	int32 argc;
	char** argv;
	{
		LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
		Assert(argc >= 0);
		
		intsize needed_size = 0;
		SafeAssert(wargv);
		for (intsize i = 0; i < argc; ++i)
		{
			int32 size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
			SafeAssert(size >= 0 && "got bad string from CommandLineToArgvW????");
			needed_size += size;
		}
		
		char* buffer = OS_HeapAlloc(needed_size + (argc + 1)*SignedSizeof(char*));
		char* end_buffer = buffer + needed_size + (argc + 1)*SignedSizeof(char*);
		argv = (char**)buffer;
		buffer += (argc + 1) * SignedSizeof(char*);
		for (intsize i = 0; i < argc; ++i)
		{
			argv[i] = buffer;
			int32 remaining = (int32)(end_buffer-buffer);
			int32 size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, (char*)buffer, remaining, NULL, NULL);
			buffer += size;
		}
		argv[argc] = NULL;
		LocalFree((HLOCAL)wargv);
	}
	
	// entry point
	int32 result = EntryPoint(argc, (char const* const*)argv);
	
	// cleanup
	if (g_win32.inited_audio)
	{
		TerminateThread(g_win32.audio_render_thread, 0);
		CloseHandle(g_win32.audio_render_thread);
	}
	FreeThreadScratchMemory_();
	
	return result;
}

//~ Implement OS API
API void
OS_Init(int32 systems)
{
	Trace();
	SafeAssert(GetCurrentThreadId() == g_win32.main_thread_id);
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	
	g_win32.message_hwnd = CreateWindowExW(0, NULL, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
	DEV_BROADCAST_DEVICEINTERFACE notification_filter = {
		sizeof(notification_filter),
		DBT_DEVTYP_DEVICEINTERFACE
	};
	if (!RegisterDeviceNotificationW(g_win32.message_hwnd, &notification_filter, DEVICE_NOTIFY_WINDOW_HANDLE))
		OS_LogErr("[WARN] win32: RegisterDeviceNotificationW returned NULL\n");

	//------------------------------------------------------------------------
	if (systems & OS_Init_WindowAndGraphics)
	{
		//- Set DPI awareness
		{
			HMODULE library = LoadLibraryA("shcore.dll");
			bool ok = false;
			
			if (library)
			{
				HRESULT (WINAPI* set_process_dpi_aware)(PROCESS_DPI_AWARENESS value);
				
				set_process_dpi_aware = (void*)GetProcAddress(library, "SetProcessDpiAwareness");
				if (set_process_dpi_aware)
					ok = (S_OK == set_process_dpi_aware(PROCESS_PER_MONITOR_DPI_AWARE));
			}
			
			if (!ok)
				ok = SetProcessDPIAware();
			if (!ok)
				OS_LogErr("[WARN] win32: failed to call both SetProcessDpiAwareness and SetProcessDPIAware\n");
		}
		
		//- Window class
		g_win32.default_cursor = LoadCursorA(NULL, IDC_ARROW);
		SafeAssert(RegisterClassW(&(WNDCLASSW) {
			.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = WindowProc_,
			.lpszClassName = g_win32.class_name,
			.hInstance = g_win32.instance,
			.hCursor = NULL,
			.hIcon = LoadIconA(g_win32.instance, MAKEINTRESOURCE(101)),
		}));
		
		//- Darkmode
		{
			DWORD data = 0;
			DWORD sizeof_data = sizeof(DWORD);
			LPCWSTR key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
			LPCWSTR name = L"SystemUsesLightTheme";
			if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_DWORD, NULL, &data, &sizeof_data) == ERROR_SUCCESS)
				g_win32.is_darkmode_enabled = (data == 0);
		}
		
		g_win32.inited_graphics = true;
	}
	
	//------------------------------------------------------------------------
	if (systems & OS_Init_Sleep)
		g_win32.inited_sleep = true;
	
	//------------------------------------------------------------------------
	if (systems & OS_Init_Audio)
	{
		InitializeSRWLock(&g_win32.client_lock);
		HRESULT hr = 0;
		
		hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&g_win32.device_enumerator);
		if (!SUCCEEDED(hr))
			goto lbl_audio_error;
		
		if (!EnumerateAudioEndpoints_())
			goto lbl_audio_error;
		
		g_win32.audio_empty_event = CreateEvent(NULL, 0, 0, NULL);
		if (!g_win32.audio_empty_event)
			goto lbl_audio_error;
		g_win32.audio_shared_stuff.event = g_win32.audio_empty_event;
		
		g_win32.audio_render_thread = CreateThread(NULL, 0, AudioThreadProc_, NULL, 0, NULL);
		if (!g_win32.audio_render_thread)
			goto lbl_audio_error;
		
		if (!SetThreadPriority(g_win32.audio_render_thread, THREAD_PRIORITY_TIME_CRITICAL))
			OS_LogErr("[WARN] win32: could not set render thread priority to THREAD_PRIORITY_TIME_CRITICAL\n");
		
		g_win32.inited_audio = true;
		
		// NOTE(ljre): There's no possible error after the audio render thread is created, so it's safe to
		//             simply release everything without holding a lock.
		if (0) lbl_audio_error:
		{
			if (g_win32.audio_shared_stuff.render_client)
				IAudioRenderClient_Release(g_win32.audio_shared_stuff.render_client), g_win32.audio_shared_stuff.render_client = NULL;
			if (g_win32.audio_empty_event)
				CloseHandle(g_win32.audio_empty_event), g_win32.audio_empty_event = NULL;
			if (g_win32.audio_shared_stuff.event)
				CloseHandle(g_win32.audio_shared_stuff.event), g_win32.audio_shared_stuff.event = NULL;
			if (g_win32.audio_client)
				IAudioClient_Release(g_win32.audio_client), g_win32.audio_client = NULL;
			if (g_win32.device_enumerator)
				IMMDeviceEnumerator_Release(g_win32.device_enumerator), g_win32.device_enumerator = NULL;
			for (int32 i = 0; i < g_win32.device_count; ++i)
			{
				if (g_win32.devices[i].immdevice)
					IMMDevice_Release(g_win32.devices[i].immdevice), g_win32.devices[i].immdevice = NULL;
			}
			g_win32.device_count = 0;
			g_win32.device_id_counter = 0;
			
			OS_LogErr("[ERROR] win32: failed to initialize audio\n");
		}
	}
	
	//------------------------------------------------------------------------
	if (systems & OS_Init_Gamepad)
	{
		char const* xinput_dll_names[] = { "xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll" };
		for (int32 i = 0; i < ArrayLength(xinput_dll_names); ++i)
		{
			HMODULE library = LoadLibraryA(xinput_dll_names[i]);
			if (library)
			{
				g_win32.xinput_module = library;
				g_win32.xinput_get_state = (void*)GetProcAddress(library, "XInputGetState");
				g_win32.xinput_set_state = (void*)GetProcAddress(library, "XInputSetState");
				g_win32.xinput_get_capabilities = (void*)GetProcAddress(library, "XInputGetCapabilities");
				break;
			}
		}
		
		char const* dinput_dll_names[] = { "dinput8.dll", /* "dinput.dll" */ };
		for (int32 i = 0; i < ArrayLength(dinput_dll_names); ++i)
		{
			HMODULE library = LoadLibraryA(dinput_dll_names[i]);
			
			if (library)
			{
				g_win32.dinput_module = library;
				g_win32.dinput_create = (void*)GetProcAddress(library, "DirectInput8Create");
				HRESULT result = g_win32.dinput_create(g_win32.instance, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void**)&g_win32.dinput, NULL);
				if (!SUCCEEDED(result))
					g_win32.dinput = NULL;
				break;
			}
		}
		
		g_win32.free_gamepad_count = OS_Limits_MaxGamepadCount;
		for (int32 i = 0; i < OS_Limits_MaxGamepadCount; ++i)
			g_win32.free_gamepads[OS_Limits_MaxGamepadCount - i - 1] = i;
		
		CheckForGamepads_();
	}

	//------------------------------------------------------------------------
	if (systems & OS_Init_MediaFoundation)
	{
		HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
		if (!SUCCEEDED(hr))
			OS_LogErr("[ERROR] win32: failed to initialize media foundation\n");
	}
	
	//------------------------------------------------------------------------
	{
		OS_Capabilities caps = {
			.has_mouse = true,
			.has_keyboard = true,
			.has_gamepad = (g_win32.dinput_module || g_win32.xinput_module),
			.has_gestures = false,
			.has_audio = (g_win32.inited_audio),
			.is_mobile_like = false,
		};
		GetScaleFactorForMonitorProc_* get_scale_factor_for_monitor = NULL;
		HMODULE shcore = LoadLibraryW(L"shcore.dll");
		if (shcore)
			get_scale_factor_for_monitor = (void*)GetProcAddress(shcore, "GetScaleFactorForMonitor");
		EnumDisplayMonitors(NULL, NULL, MonitorEnumProc_, (LPARAM)&(MonitorEnumProcArgs_) {
			.caps = &caps,
			.index = 0,
			.get_scale_factor_for_monitor = get_scale_factor_for_monitor
		});
		g_win32.caps = caps;

		HMODULE kernel32 = LoadLibraryW(L"kernel32.dll");
		if (kernel32)
			g_win32.set_thread_description = (void*)GetProcAddress(kernel32, "SetThreadDescription");
	}
}

API OS_Event*
OS_PollEvents(bool wait, Arena* output_arena, intsize* out_event_count)
{
	Trace();
	for (WindowData_* window_data = g_win32.windows; window_data; window_data = window_data->next)
	{
		OS_LockExclusive(&window_data->mouse_state_lock);
		window_data->mouse_state.old_pos[0] = window_data->mouse_state.pos[0];
		window_data->mouse_state.old_pos[1] = window_data->mouse_state.pos[1];
		for (intsize i = 0; i < ArrayLength(window_data->mouse_state.buttons); ++i)
			window_data->mouse_state.buttons[i].changes = 0;
		OS_UnlockExclusive(&window_data->mouse_state_lock);
	}
	
	//------------------------------------------------------------------------
	OS_Event* events = ArenaEndAligned(output_arena, alignof(OS_Event));
	g_win32.polling_event_output_arena = output_arena;
	
	if (wait)
	{
		MSG message;
		GetMessageW(&message, NULL, 0, 0);
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	
	MSG message;
	while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	
	g_win32.polling_event_output_arena = NULL;
	*out_event_count = (OS_Event*)ArenaEnd(output_arena) - events;
	
	//------------------------------------------------------------------------
	for (WindowData_* window_data = g_win32.windows; window_data; window_data = window_data->next)
	{
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(window_data->handle, &point);
		OS_LockExclusive(&window_data->mouse_state_lock);
		window_data->mouse_state.pos[0] = (float32)point.x;
		window_data->mouse_state.pos[1] = (float32)point.y;

		if (window_data->mouse_lock == OS_MouseLockKind_HideAndClip)
		{
			RECT rect;
			GetClientRect(window_data->handle, &rect);
			int32 middle_x = (rect.left + rect.right) / 2;
			int32 middle_y = (rect.top + rect.bottom) / 2;
			POINT screen = { middle_x, middle_y };
			ClientToScreen(window_data->handle, &screen);
			SetCursorPos(screen.x, screen.y);
			
			window_data->mouse_state.old_pos[0] = (float32)middle_x;
			window_data->mouse_state.old_pos[1] = (float32)middle_y;
		}

		OS_UnlockExclusive(&window_data->mouse_state_lock);
	}

	return events;
}

API void
OS_Sleep(uint64 ms)
{
	Trace();
	Sleep((DWORD)ms);
}

API void
OS_SleepPrecise(uint64 ns)
{
	Trace();
	uint64 end;
	QueryPerformanceCounter((LARGE_INTEGER*)&end);
	end += ns * (g_win32.time_frequency / 100000) / 10000;
	end -= 100;
	
	if (ns > 1*1000000)
	{
		ns -= 1*1000000;

		thread_local static bool initialized_timer;
		thread_local static HANDLE timer;

		if (!initialized_timer)
		{
			timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
			if (!timer)
			{
				timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
				OS_LogErr("[WARN] win32: failed to create waitable timer with flag CREATE_WAITABLE_TIMER_HIGH_RESOLUTION");
			}
			if (!timer)
				OS_LogErr("[WARN] win32: failed to create waitable timer\n");
		}

		if (timer)
		{
			LARGE_INTEGER large_int = { .QuadPart = -(int64)ns / 100 };
			if (!SetWaitableTimer(timer, &large_int, 0, NULL, NULL, 0))
				goto lbl_simple_sleep;
			
			{
				Trace(); TraceName(Str("WaitForSingleObject"));
				DWORD result = WaitForSingleObject(timer, (DWORD)(ns / 1000000) + 1);
				(void)result;
			}
		}
		else
		{
			lbl_simple_sleep:;
			Sleep((DWORD)(ns/1000000));
		}
	}
	
	{
		Trace(); TraceName(Str("spinlock"));
		uint64 now;
		do
			QueryPerformanceCounter((LARGE_INTEGER*)&now);
		while (now < end);
	}
}

API void
OS_ExitWithErrorMessage(char const* fmt, ...)
{
	Trace();
	// NOTE: we don't use OS_ScratchArena because the thread_local memory might not be setup yet.
	char buf[32<<10];
	Arena local_arena = ArenaFromMemory(buf, sizeof(buf));
	
	va_list args;
	va_start(args, fmt);
	String str = ArenaVPrintf(&local_arena, fmt, args);
	va_end(args);
	
	wchar_t* wstr = StringToWide_(&local_arena, str);
	MessageBoxW(NULL, wstr, L"Fatal Error!", MB_OK);
	ExitProcess(1);
}

API void
OS_MessageBox(String title, String message)
{
	Trace();
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	
	for ArenaTempScope(scratch_arena)
	{
		wchar_t* wtitle = StringToWide_(scratch_arena, title);
		wchar_t* wmessage = StringToWide_(scratch_arena, message);
		
		MessageBoxW(NULL, wmessage, wtitle, MB_OK);
	}
}

API Arena*
OS_ScratchArena(Arena* const* conflict, intsize conflict_count)
{
	Trace();
	Arena* arena = ScratchArena(conflict_count, conflict);
	SafeAssert(arena);
	return arena;
}

API void
OS_DefaultLoggerProc(ThreadContextLogger* logger, int32 level, String str)
{
	Trace();
	if (level < logger->minimum_level)
		return;

	String prefix = {};
	if (level >= LOG_FATAL)
		prefix = Str("[FATAL] ");
	else if (level >= LOG_ERROR)
		prefix = Str("[ERROR] ");
	else if (level >= LOG_WARN)
		prefix = Str("[WARN] ");
	else if (level >= LOG_INFO)
		prefix = Str("[INFO] ");
	else if (level >= LOG_DEBUG)
		prefix = Str("[DEBUG] ");

	OS_LogErr("%S%S\n", prefix, str);
}

API ThreadContextLogger
OS_DefaultLogger(void)
{
	Trace();
	return (ThreadContextLogger) {
		.proc = OS_DefaultLoggerProc,
		.user_data = NULL,
		.minimum_level = 0,
	};
}

API NO_RETURN void
OS_DefaultAssertionFailureProc(String expr, String func, String file, int32 line)
{
	OS_ExitWithErrorMessage("Assertion failure!\nFile: %S\nLine: %i\nFunc: %S\nExpr: %S", file, line, func, expr);
}

API uint64
OS_PosixTime(void)
{
	Trace();
	SYSTEMTIME system_time;
	FILETIME file_time;
	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	
	return FileTimeToPosixTime_(file_time);
}

API uint64
OS_Tick(void)
{
	Trace();
	uint64 now;
	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	return now;
}

API uint64
OS_TickRate(void)
{
	return g_win32.time_frequency;
}

API float64
OS_TimeInSeconds(void)
{
	Trace();
	uint64 freq = g_win32.time_frequency;
	uint64 now;
	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	return (float64)(now - g_win32.time_started) / (float64)freq;
}

API void
OS_LogOut(char const* fmt, ...)
{
	Trace();
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	
	for ArenaTempScope(scratch_arena)
	{
		va_list args;
		va_start(args, fmt);
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		ArenaPushData(scratch_arena, &""); // null terminator
		va_end(args);
		
		HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (handle && handle != INVALID_HANDLE_VALUE)
			WriteFile(handle, str.data, (DWORD)str.size, NULL, NULL);
		if (IsDebuggerPresent())
			OutputDebugStringA((const char*)str.data);
	}
}

API void
OS_LogErr(char const* fmt, ...)
{
	Trace();
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	
	for ArenaTempScope(scratch_arena)
	{
		va_list args;
		va_start(args, fmt);
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		ArenaPushData(scratch_arena, &""); // null terminator
		va_end(args);
		
		HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
		if (handle && handle != INVALID_HANDLE_VALUE)
			WriteFile(handle, str.data, (DWORD)str.size, NULL, NULL);
		if (IsDebuggerPresent())
			OutputDebugStringA((const char*)str.data);
	}
}

API bool
OS_IsValidImpl(void* handle_ptr)
{
	return handle_ptr != NULL && handle_ptr != INVALID_HANDLE_VALUE;
}

API OS_Capabilities
OS_GetSystemCapabilities(void)
{
	return g_win32.caps;
}

API OS_SystemTime
OS_GetSystemTime(void)
{
	Trace();
	SYSTEMTIME system_time;
	GetLocalTime(&system_time);
	
	OS_SystemTime result = {
		.year = system_time.wYear,
		.month = system_time.wMonth,
		.week_day = system_time.wDayOfWeek,
		.day = system_time.wDay,
		.hours = system_time.wHour,
		.minutes = system_time.wMinute,
		.seconds = system_time.wSecond,
		.milliseconds = system_time.wMilliseconds,
	};
	
	return result;
}

//~ Audio
API bool
OS_StartAudioProc(uint32 device_id, OS_AudioProc* proc, void* user_data, OS_Error* out_err)
{
	Trace();
	ChangeAudioEndpoint_(device_id ? device_id : g_win32.default_device_id, proc, user_data);
	FillOsErr_(out_err, 0);
	return true;
}

API bool
OS_StopAudioProc(OS_Error* out_err)
{
	Trace();
	CloseCurrentAudioEndpoint_();
	FillOsErr_(out_err, 0);
	return true;
}

API OS_RetrieveAudioDevicesResult
OS_RetrieveAudioDevices(void)
{
	Trace();
	OS_RetrieveAudioDevicesResult result = { 0 };
	
	for (int32 i = 0; i < g_win32.device_count; ++i)
	{
		result.devices[i].id = g_win32.devices[i].id;
		static_assert(sizeof(result.devices[i].name) == sizeof(g_win32.devices[i].name), "their sizes should be the same!");
		MemoryCopy(result.devices[i].name, g_win32.devices[i].name, sizeof(result.devices[i].name));
	}
	
	result.count = g_win32.device_count;
	result.default_device_id = g_win32.default_device_id;
	return result;
}

API uint32
OS_RetrieveCurrentAudioDevice(void)
{
	return g_win32.devices[g_win32.active_device_index].id;
}

//~ Threads
API OS_Thread
OS_CreateThread(OS_ThreadDesc const* desc)
{
	Trace();
	OS_ThreadDesc* on_heap = OS_HeapAlloc(sizeof(OS_ThreadDesc));
	*on_heap = *desc;
	
	HANDLE handle = CreateThread(NULL, 0, ThreadProc_, on_heap, 0, NULL);
	
	if (desc->name.size && g_win32.set_thread_description)
	{
		Arena* scratch_arena = OS_ScratchArena(NULL, 0);
		for ArenaTempScope(scratch_arena)
		{
			LPWSTR wname = StringToWide_(scratch_arena, desc->name);
			(void) g_win32.set_thread_description(handle, wname);
		}
	}
	
	return (OS_Thread) { handle };
}

API bool
OS_CheckThread(OS_Thread thread)
{
	Trace();
	HANDLE handle = thread.ptr;
	DWORD result;
	
	if (GetExitCodeThread(handle, &result))
		return result == STILL_ACTIVE;
	return false;
}

API int32
OS_JoinThread(OS_Thread thread)
{
	Trace();
	HANDLE handle = thread.ptr;
	DWORD wait_result;
	DWORD code;
	
	wait_result = WaitForSingleObject(handle, INFINITE);
	if (wait_result != WAIT_OBJECT_0)
		return INT32_MIN;
	if (!GetExitCodeThread(handle, &code))
		return INT32_MIN;
	CloseHandle(handle);
	
	return (int32)code;
}

API void
OS_DestroyThread(OS_Thread thread)
{
	Trace();
	HANDLE handle = thread.ptr;
	CloseHandle(handle);
}

//~ Window
API OS_Window
OS_CreateWindow(OS_WindowDesc const* desc)
{
	Trace();
	String title = StrInit("");
	LPCWSTR class_name = g_win32.class_name;
	DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	DWORD exstyle = 0;
	int32 x = 0;
	int32 y = 0;
	int32 width = 1280;
	int32 height = 720;
	HINSTANCE instance = g_win32.instance;
	
	int32 monitor_width = 0;
	int32 monitor_height = 0;
	int32 monitor_x = 0;
	int32 monitor_y = 0;
	bool got_monitor_info;
	{
		HMONITOR monitor = MonitorFromPoint((POINT) { 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO monitor_info = { sizeof(monitor_info) };
		got_monitor_info = GetMonitorInfoW(monitor, &monitor_info);
		if (got_monitor_info)
		{
			monitor_x = monitor_info.rcMonitor.left;
			monitor_y = monitor_info.rcMonitor.top;
			monitor_width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
			monitor_height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
		}
	}
	
	if (desc->resizeable)
		style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
	if (desc->x)
		x = desc->x;
	if (desc->y)
		y = desc->y;
	if (desc->width)
		width = desc->width;
	if (desc->height)
		height = desc->height;
	if (desc->title.size)
		title = desc->title;
	if (desc->fullscreen)
	{
		if (got_monitor_info)
		{
			x = monitor_x;
			y = monitor_y;
			width = monitor_width;
			height = monitor_height;
			style = WS_POPUP;
		}
	}
	else
	{
		bool should_do_usedefault = (x == 0 && y == 0);
		
		RECT rect = { x, y, x+width, y+height };
		if (AdjustWindowRectEx(&rect, style, false, exstyle))
		{
			x = rect.left;
			y = rect.top;
			width = rect.right - rect.left;
			height = rect.bottom - rect.top;
			if (should_do_usedefault && got_monitor_info)
			{
				x = monitor_x + (monitor_width - width)/2;
				y = monitor_y + (monitor_height - height)/2;
				should_do_usedefault = false;
			}
		}
		
		if (should_do_usedefault)
		{
			x = CW_USEDEFAULT;
			y = CW_USEDEFAULT;
		}
	}

	HWND hwnd;
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	
	// Window
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wtitle = StringToWide_(scratch_arena, title);
		hwnd = CreateWindowExW(exstyle, class_name, wtitle, style, x, y, width, height, NULL, NULL, instance, NULL);
		SafeAssert(hwnd);
	}
	
	if (g_win32.is_darkmode_enabled)
	{
		DWORD enable = g_win32.is_darkmode_enabled;
		if (DwmSetWindowAttribute(hwnd, 19, &enable, sizeof(DWORD)) != S_OK)
			DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable, sizeof(DWORD));
	}
	
	WindowData_* window_data = OS_HeapAlloc(sizeof(WindowData_));
	window_data->handle = hwnd;
	OS_InitRWLock(&window_data->mouse_state_lock);
	OS_PollMouseState(&window_data->mouse_state, false, (OS_Window) { window_data });
	window_data->mouse_state.old_pos[0] = window_data->mouse_state.pos[0];
	window_data->mouse_state.old_pos[1] = window_data->mouse_state.pos[1];
	
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window_data);
	ShowWindow(hwnd, SW_SHOW);
	
	window_data->next = g_win32.windows;
	g_win32.windows = window_data;

	return (OS_Window) { window_data };
}

API void
OS_UpdateWindow(OS_Window window, OS_WindowDesc const* desc)
{
	Trace();
	// TODO
	SafeAssert(false);
}

API void
OS_DestroyWindow(OS_Window window)
{
	Trace();
	WindowData_* window_data = window.ptr;
	
	// TODO: destroy d3d11 stuff
	DestroyWindow(window_data->handle);
}

API void
OS_GetWindowUserSize(OS_Window window, int32* out_width, int32* out_height)
{
	Trace();
	WindowData_* window_data = window.ptr;
	
	RECT rect;
	if (GetClientRect(window_data->handle, &rect))
	{
		*out_width = rect.right - rect.left;
		*out_height = rect.bottom - rect.top;
	}
	else
	{
		*out_width = 0;
		*out_height = 0;
	}
}

API void
OS_SetWindowMouseLock(OS_Window window, OS_MouseLockKind kind)
{
	Trace();
	WindowData_* window_data = window.ptr;
	HWND hwnd = window_data->handle;
	bool has_focus = (GetActiveWindow() == hwnd);
	OS_MouseLockKind prev_kind = window_data->mouse_lock;

	if (prev_kind == kind)
		return;

	bool prev_locks_cursor = (prev_kind == OS_MouseLockKind_LockInWindowRegion);
	bool curr_locks_cursor = (kind      == OS_MouseLockKind_LockInWindowRegion);

	if (has_focus && prev_locks_cursor != curr_locks_cursor)
	{
		if (curr_locks_cursor)
		{
			union
			{
				RECT rect;
				struct { POINT top_left, bottom_right; };
			} clip;
			GetClientRect(hwnd, &clip.rect);
			ClientToScreen(hwnd, &clip.top_left);
			ClientToScreen(hwnd, &clip.bottom_right);
			ClipCursor(&clip.rect);
		}
		else
			ClipCursor(NULL);
	}

	window_data->mouse_lock = kind;
}

API OS_MouseState
OS_GetWindowMouseState(OS_Window window)
{
	Trace();
	OS_MouseState result = { 0 };
	if (window.ptr)
	{
		WindowData_* window_data = window.ptr;
		OS_LockShared(&window_data->mouse_state_lock);
		MemoryCopy(&result, &window_data->mouse_state, sizeof(OS_MouseState));
		OS_UnlockShared(&window_data->mouse_state_lock);
	}
	return result;
}

//~ Gamepad
API void
OS_PollGamepadStates(OS_GamepadStates* states, bool accumulate)
{
	Trace();
	g_win32.connected_gamepads = 0;
	
	for (intsize i = 0; i < OS_Limits_MaxGamepadCount; ++i)
	{
		Gamepad_* gamepad = &g_win32.gamepads[i];
		if (gamepad->connected)
		{
			g_win32.connected_gamepads |= 1 << i;
			UpdateConnectedGamepad_(i, accumulate);
		}
		else
			MemoryZero(&g_win32.gamepads[i].data, sizeof(g_win32.gamepads[i].data));
		states->gamepads[i] = gamepad->data;
	}
	
	states->connected_bitset = g_win32.connected_gamepads;
}

API void
OS_PollKeyboardState(OS_KeyboardState* state, bool accumulate)
{
	Trace();
	for (intsize i = 0; i < ArrayLength(g_keyboard_key_table); ++i)
	{
		OS_KeyboardKey key = g_keyboard_key_table[i];
		if (key)
		{
			OS_ButtonState* btn = &state->buttons[key];
			if (!accumulate)
				btn->changes = 0;
			SHORT key_state = GetAsyncKeyState((int32)i);
			bool is_down = key_state & 0x8000;
			if (btn->is_down != is_down)
			{
				btn->changes += 1;
				btn->is_down = is_down;
			}
		}
	}
}

API void
OS_PollMouseState(OS_MouseState* state, bool accumulate, OS_Window optional_window)
{
	Trace();
	WindowData_* window_data = optional_window.ptr;

	struct
	{
		OS_MouseButton button : 16;
		DWORD vkey : 16;
	}
	static const table[] = {
		{ OS_MouseButton_Left,   VK_LBUTTON  },
		{ OS_MouseButton_Right,  VK_RBUTTON  },
		{ OS_MouseButton_Middle, VK_MBUTTON  },
		{ OS_MouseButton_Other0, VK_XBUTTON1 },
		{ OS_MouseButton_Other1, VK_XBUTTON2 },
	};
	for (intsize i = 0; i < ArrayLength(table); ++i)
	{
		OS_ButtonState* btn = &state->buttons[table[i].button];
		if (!accumulate)
			btn->changes = 0;
		SHORT key_state = GetAsyncKeyState(table[i].vkey);
		bool is_down = key_state & 0x8000;
		if (btn->is_down != is_down)
		{
			btn->changes += 1;
			btn->is_down = is_down;
		}
	}
	
	POINT pos;
	if (GetCursorPos(&pos))
	{
		if (!accumulate)
		{
			state->old_pos[0] = state->pos[0];
			state->old_pos[1] = state->pos[1];
		}
		if (window_data)
			ScreenToClient(window_data->handle, &pos);
		state->pos[0] = (float32)pos.x;
		state->pos[1] = (float32)pos.y;
	}
}

API void
OS_SetGamepadMappings(OS_GamepadMapping const* mappings, intsize mapping_count)
{
	Trace();
	SafeAssert(mapping_count >= 0);
	if (!mappings)
		SafeAssert(mapping_count == 0);
	
	g_win32.gamepad_mappings = mappings;
	g_win32.gamepad_mappings_count = mapping_count;
}

//~ Memory & file stuff
API void*
OS_HeapAlloc(intz size)
{
	Trace();
	SafeAssert(size >= 0);
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size);
}

API void*
OS_HeapRealloc(void* ptr, intz size)
{
	Trace();
	SafeAssert(size >= 0);
	void* result;
	if (ptr)
		result = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, (SIZE_T)size);
	else
		result = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size);
	return result;
}

API void
OS_HeapFree(void* ptr)
{
	Trace();
	if (ptr)
		HeapFree(GetProcessHeap(), 0, ptr);
}

API void*
OS_VirtualAlloc(void* address, intz size, uint32 flags)
{
	Trace();
	SafeAssert(size >= 0);
	
	DWORD type = MEM_RESERVE|MEM_COMMIT;
	DWORD protect = PAGE_READWRITE;
	
	if (flags & OS_VirtualFlags_ReserveOnly)
	{
		type = MEM_RESERVE;
		protect = 0;
	}
	
	void* result = VirtualAlloc(address, (SIZE_T)size, type, protect);
	return result;
}

API bool
OS_VirtualCommit(void* address, intz size)
{
	Trace();
	SafeAssert(size >= 0);
	
	void* result = VirtualAlloc(address, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE);
	return result;
}

API bool
OS_VirtualProtect(void* address, intz size, uint32 flags)
{
	Trace();
	SafeAssert(size >= 0);
	
	DWORD protect = PAGE_READWRITE;
	if (flags & OS_VirtualFlags_Read)
		protect = PAGE_READONLY;
	else if (flags & OS_VirtualFlags_Execute)
		protect = PAGE_EXECUTE;
	
	DWORD prev;
	bool result = VirtualProtect(address, (SIZE_T)size, protect, &prev);
	return result;
}

API bool
OS_VirtualFree(void* address, intz size)
{
	Trace();
	SafeAssert(size >= 0);
	
	bool result = VirtualFree(address, 0, MEM_RELEASE);
	return result;
}

API bool
OS_ArenaCommitMemoryProc(Arena* arena, intz needed_size)
{
	Trace();
	intz   commited = arena->size;
	intz   reserved = arena->reserved;
	uint8* memory   = arena->memory;
	Assert(commited < needed_size);
	
	intz rounded_needed_size = AlignUp(needed_size, (32<<20) - 1);
	if (rounded_needed_size > reserved)
		return false;
	
	intz amount_to_commit = rounded_needed_size - commited;
	SafeAssert(amount_to_commit >= 0);
	void* result = VirtualAlloc(memory + commited, (SIZE_T)amount_to_commit, MEM_COMMIT, PAGE_READWRITE);
	if (result != NULL)
	{
		arena->size = rounded_needed_size;
		return true;
	}
	return false;
}

API Arena
OS_VirtualAllocArena(intz total_reserved_size)
{
	Trace();
	SafeAssert(total_reserved_size >= 0);
	Arena result = { 0 };
	
	total_reserved_size = AlignUp(total_reserved_size, (32<<20) - 1);
	void* memory = VirtualAlloc(NULL, (SIZE_T)total_reserved_size, MEM_RESERVE, PAGE_READWRITE);
	if (memory)
	{
		if (VirtualAlloc(memory, 32<<20, MEM_COMMIT, PAGE_READWRITE))
		{
			result.memory = memory;
			result.size = 32<<20;
			result.offset = 0;
			result.reserved = total_reserved_size;
			result.commit_memory_proc = OS_ArenaCommitMemoryProc;
		}
		else
		{
			VirtualFree(memory, 0, MEM_RELEASE);
			memory = NULL;
		}
	}
	
	return result;
}

API bool
OS_ReadEntireFile(String path, Arena* output_arena, void** out_data, intz* out_size, OS_Error* out_err)
{
	Trace(); TraceText(path);
	Arena* scratch_arena = OS_ScratchArena(&output_arena, 1);
	HANDLE handle = INVALID_HANDLE_VALUE;
	
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wpath = StringToWide_(scratch_arena, path);
		if (wpath)
			handle = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	}
	
	if (handle == INVALID_HANDLE_VALUE)
		return FillOsErr_(out_err, GetLastError());
	
	LARGE_INTEGER large_int;
	if (!GetFileSizeEx(handle, &large_int))
	{
		DWORD errcode = GetLastError();
		CloseHandle(handle);
		return FillOsErr_(out_err, errcode);
	}
	
#ifndef _WIN64
	if (large_int.QuadPart > UINT32_MAX)
	{
		CloseHandle(handle);
		FillOsErr_(out_err, ERROR_FILE_TOO_LARGE);
		return false;
	}
#endif
	
	ArenaSavepoint output_arena_save = ArenaSave(output_arena);
	intz file_size = large_int.QuadPart;
	uint8* file_data = ArenaPushDirtyAligned(output_arena, file_size, 1);
	
	intz still_to_read = file_size;
	uint8* p = file_data;
	while (still_to_read > 0)
	{
		DWORD to_read = (DWORD)Min(still_to_read, UINT32_MAX);
		DWORD did_read;
		
		if (!ReadFile(handle, p, to_read, &did_read, NULL))
		{
			DWORD errcode = GetLastError();
			ArenaRestore(output_arena_save);
			CloseHandle(handle);
			return FillOsErr_(out_err, errcode);
		}
		
		still_to_read -= did_read;
		p += did_read;
	}
	
	*out_data = file_data;
	*out_size = file_size;
	
	CloseHandle(handle);
	FillOsErr_(out_err, 0);
	return true;
}

API bool
OS_WriteEntireFile(String path, void const* data, intz size, OS_Error* out_err)
{
	Trace(); TraceText(path);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	HANDLE handle = INVALID_HANDLE_VALUE;
	
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wpath = StringToWide_(scratch_arena, path);
		if (wpath)
			handle = CreateFileW(wpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	}
	
	if (handle == INVALID_HANDLE_VALUE)
		return FillOsErr_(out_err, GetLastError());
	
	intz total_size = size;
	const uint8* head = data;
	
	while (total_size > 0)
	{
		DWORD bytes_written = 0;
		DWORD to_write = (uint32)Min(total_size, UINT32_MAX);
		
		if (!WriteFile(handle, head, to_write, &bytes_written, NULL))
		{
			DWORD errcode = GetLastError();
			CloseHandle(handle);
			return FillOsErr_(out_err, errcode);
		}
		
		total_size -= bytes_written;
		head += bytes_written;
	}
	
	CloseHandle(handle);
	FillOsErr_(out_err, 0);
	return true;
}

API OS_FileInfo
OS_GetFileInfoFromPath(String path, OS_Error* out_err)
{
	Trace(); TraceText(path);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	HANDLE handle = INVALID_HANDLE_VALUE;
	OS_FileInfo result = { 0 };
	SetLastError(0);
	
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wpath = StringToWide_(scratch_arena, path);
		if (wpath)
			handle = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	}
	
	if (handle != INVALID_HANDLE_VALUE)
	{
		result.exists = true;
		
		BY_HANDLE_FILE_INFORMATION file_info;
		if (GetFileInformationByHandle(handle, &file_info))
		{
			result.read_only = !!(file_info.dwFileAttributes & FILE_ATTRIBUTE_READONLY);
			result.created_at = FileTimeToPosixTime_(file_info.ftCreationTime);
			result.modified_at = FileTimeToPosixTime_(file_info.ftLastWriteTime);
			result.size = (uint64)file_info.nFileSizeLow | (uint64)file_info.nFileSizeHigh<<32;
			SetLastError(0);
		}
		
		CloseHandle(handle);
	}
	
	FillOsErr_(out_err, GetLastError());
	return result;
}

API bool
OS_CopyFile(String from, String to, OS_Error* out_err)
{
	Trace(); TraceText(to);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	SetLastError(0);
	
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wfrom = StringToWide_(scratch_arena, from);
		if (wfrom)
		{
			LPWSTR wto = StringToWide_(scratch_arena, to);
			if (wto)
				CopyFileW(wfrom, wto, false);
		}
	}
	
	return FillOsErr_(out_err, GetLastError());
}

API bool
OS_DeleteFile(String path, OS_Error* out_err)
{
	Trace(); TraceText(path);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	SetLastError(0);
	
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wpath = StringToWide_(scratch_arena, path);
		if (wpath)
			DeleteFileW(wpath);
	}
	
	return FillOsErr_(out_err, GetLastError());
}

API bool
OS_MakeDirectory(String path, OS_Error* out_err)
{
	Trace(); TraceText(path);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	SetLastError(0);
	
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wpath = StringToWide_(scratch_arena, path);
		if (wpath)
			CreateDirectoryW(wpath, NULL);
	}
	
	return FillOsErr_(out_err, GetLastError());
}

API OS_Library
OS_LoadLibrary(String name, OS_Error* out_err)
{
	Trace(); TraceText(name);
	HMODULE module;
	
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wname = StringToWide_(scratch_arena, name);
		if (wname)
			module = LoadLibraryW(wname);
	}
	
	FillOsErr_(out_err, GetLastError());
	return (OS_Library) { module };
}

API void*
OS_LoadSymbol(OS_Library library, char const* symbol_name)
{
	Trace();
	HMODULE module = library.ptr;
	return (void*)GetProcAddress(module, symbol_name);
}

API void
OS_UnloadLibrary(OS_Library library)
{
	Trace();
	HMODULE module = library.ptr;
	FreeLibrary(module);
}

API OS_File
OS_OpenFile(String path, OS_OpenFileFlags flags, OS_Error* out_err)
{
	Trace(); TraceText(path);
	HANDLE handle = NULL;
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	DWORD desired_access = 0;
	DWORD share_mode = 0;
	DWORD creation_disposition = 0;
	DWORD attributes = 0;
	
	uint32 flags_rw = flags & (OS_OpenFileFlags_Read | OS_OpenFileFlags_Write);
	uint32 flags_fail = flags & (OS_OpenFileFlags_FailIfAlreadyExists | OS_OpenFileFlags_FailIfDoesntExist);
	
	switch (flags_rw)
	{
		case 0: Assert(false); break;
		case OS_OpenFileFlags_Read:
		{
			desired_access = GENERIC_READ;
			share_mode = FILE_SHARE_READ;
			attributes = FILE_ATTRIBUTE_READONLY;
		} break;
		case OS_OpenFileFlags_Write:
		{
			desired_access = GENERIC_WRITE;
			share_mode = 0;
			attributes = FILE_ATTRIBUTE_NORMAL;
		} break;
		case OS_OpenFileFlags_Read | OS_OpenFileFlags_Write:
		{
			desired_access = GENERIC_READ | GENERIC_WRITE;
			share_mode = 0;
			attributes = FILE_ATTRIBUTE_NORMAL;
		} break;
	}
	
	switch (flags_fail)
	{
		case 0: creation_disposition = !(flags & OS_OpenFileFlags_Read) ? CREATE_ALWAYS : OPEN_ALWAYS; break;
		case OS_OpenFileFlags_FailIfAlreadyExists: creation_disposition = CREATE_NEW; break;
		case OS_OpenFileFlags_FailIfDoesntExist: creation_disposition = OPEN_EXISTING; break;
		default: Assert(false); break;
	}
	
	for ArenaTempScope(scratch_arena)
	{
		LPWSTR wpath = StringToWide_(scratch_arena, path);
		if (wpath)
			handle = CreateFileW(wpath, desired_access, share_mode, NULL, creation_disposition, attributes, NULL);
	}
	
	if (handle == INVALID_HANDLE_VALUE)
		handle = NULL;
	else
		SetLastError(0);
	
	FillOsErr_(out_err, GetLastError());
	return (OS_File) { handle };
}

API OS_FileInfo
OS_GetFileInfo(OS_File file, OS_Error* out_err)
{
	Trace();
	HANDLE handle = file.ptr;
	OS_FileInfo result = { 0 };
	
	BY_HANDLE_FILE_INFORMATION file_info;
	if (GetFileInformationByHandle(handle, &file_info))
	{
		result.exists = true;
		result.read_only = !!(file_info.dwFileAttributes & FILE_ATTRIBUTE_READONLY);
		result.created_at = FileTimeToPosixTime_(file_info.ftCreationTime);
		result.modified_at = FileTimeToPosixTime_(file_info.ftLastWriteTime);
		result.size = (uint64)file_info.nFileSizeLow | (uint64)file_info.nFileSizeHigh<<32;
		SetLastError(0);
	}
	
	FillOsErr_(out_err, GetLastError());
	return result;
}

API intsize
OS_WriteFile(OS_File file, intsize size, void const* buffer, OS_Error* out_err)
{
	Trace();
	HANDLE handle = file.ptr;
	intsize written = 0;
	
	while (written < size)
	{
		DWORD api_written = 0;
		DWORD api_size = (DWORD)Min(UINT32_MAX, size);
		void const* mem = (uint8 const*)buffer + written;
		
		if (!WriteFile(handle, mem, api_size, &api_written, NULL))
			break;
		
		written += (intsize)api_written;
	}

	FillOsErr_(out_err, GetLastError());
	return written;
}

API intsize
OS_ReadFile(OS_File file, intsize size, void* buffer, OS_Error* out_err)
{
	Trace();
	HANDLE handle = file.ptr;
	intsize read = 0;
	
	while (read < size)
	{
		DWORD api_read = 0;
		DWORD api_size = (DWORD)Min(UINT32_MAX, size);
		void* mem = (uint8*)buffer + read;
		
		if (!ReadFile(handle, mem, api_size, &api_read, NULL))
			break;
		
		read += (intsize)api_read;
	}
	
	FillOsErr_(out_err, GetLastError());
	return read;
}

API int64
OS_TellFile(OS_File file, OS_Error* out_err)
{
	Trace();
	HANDLE handle = file.ptr;
	int64 result = -1;
	LARGE_INTEGER offset;
	
	if (SetFilePointerEx(handle, (LARGE_INTEGER) { 0 }, &offset, FILE_CURRENT))
		result = offset.QuadPart;
	
	FillOsErr_(out_err, GetLastError());
	return result;
}

API void
OS_SeekFile(OS_File file, int64 offset, bool relative, OS_Error* out_err)
{
	Trace();
	HANDLE handle = file.ptr;
	LARGE_INTEGER seek_pos = { .QuadPart = offset };
	DWORD move_method = FILE_BEGIN;
	if (relative)
		move_method = FILE_CURRENT;
	else if (offset < 0)
	{
		seek_pos.QuadPart = offset + 1;
		move_method = FILE_END;
	}
	
	SetLastError(0);
	SetFilePointerEx(handle, seek_pos, NULL, move_method);
	FillOsErr_(out_err, GetLastError());
}

API intsize
OS_WriteFileAt(OS_File file, int64 offset, intsize size, void const* buffer, OS_Error* out_err)
{
	Trace();
	HANDLE handle = file.ptr;
	intsize written = 0;
	
	LARGE_INTEGER seek_pos = { .QuadPart = offset };
	DWORD move_method = FILE_BEGIN;
	if (offset < 0)
	{
		seek_pos.QuadPart = offset + 1;
		move_method = FILE_END;
	}
	
	if (SetFilePointerEx(handle, seek_pos, NULL, move_method))
	{
		while (written < size)
		{
			DWORD api_written = 0;
			DWORD api_size = (DWORD)Min(UINT32_MAX, size);
			void const* mem = (uint8 const*)buffer + written;
			
			if (!WriteFile(handle, mem, api_size, &api_written, NULL))
				break;
			
			written += (intsize)api_written;
		}
	}
	
	FillOsErr_(out_err, GetLastError());
	return written;
}

API intsize
OS_ReadFileAt(OS_File file, int64 offset, intsize size, void* buffer, OS_Error* out_err)
{
	Trace();
	HANDLE handle = file.ptr;
	intsize read = 0;
	
	LARGE_INTEGER seek_pos = { .QuadPart = offset };
	DWORD move_method = FILE_BEGIN;
	if (offset < 0)
	{
		seek_pos.QuadPart = offset + 1;
		move_method = FILE_END;
	}
	
	if (SetFilePointerEx(handle, seek_pos, NULL, move_method))
	{
		while (read < size)
		{
			DWORD api_read = 0;
			DWORD api_size = (DWORD)Min(UINT32_MAX, size);
			void* mem = (uint8*)buffer + read;
			
			if (!ReadFile(handle, mem, api_size, &api_read, NULL))
				break;
			
			read += (intsize)api_read;
		}
	}
	
	FillOsErr_(out_err, GetLastError());
	return read;
}

API void
OS_CloseFile(OS_File file)
{
	Trace();
	HANDLE handle = file.ptr;
	
	CloseHandle(handle);
}

API OS_File
OS_GetStdout(void)
{
	Trace();
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (handle == INVALID_HANDLE_VALUE)
		handle = NULL;
	return (OS_File) { handle };
}

API OS_File
OS_GetStdin(void)
{
	Trace();
	HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
	if (handle == INVALID_HANDLE_VALUE)
		handle = NULL;
	return (OS_File) { handle };
}

API OS_File
OS_GetStderr(void)
{
	Trace();
	HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
	if (handle == INVALID_HANDLE_VALUE)
		handle = NULL;
	return (OS_File) { handle };
}

API OS_File
OS_CreatePipe(bool inheritable, OS_Error* out_err)
{
	Trace();
	return (OS_File) { 0 };
}

API bool
OS_IsFilePipe(OS_File file)
{
	Trace();
	return false;
}

API OS_FileMapping
OS_MapFileForReading(OS_File file, void const** out_buffer, intz* out_size, OS_Error* out_err)
{
	Trace();
	OS_FileMapping result = { 0 };
	HANDLE file_handle = file.ptr;
	HANDLE mapping;
	intz size;

	*out_buffer = NULL;
	*out_size = 0;

	{
		LARGE_INTEGER large_int;
		if (!GetFileSizeEx(file_handle, &large_int))
		{
			CloseHandle(file_handle);
			FillOsErr_(out_err, GetLastError());
			return result;
		}
		
		#ifndef _WIN64
		if (large_int.QuadPart > UINT32_MAX)
		{
			CloseHandle(file_handle);
			FillOsErr_(out_err, ERROR_FILE_TOO_LARGE);
			return result;
		}
		#endif
		
		size = large_int.QuadPart;
	}

	mapping = CreateFileMappingW(file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
	if (mapping)
	{
		void* base_address = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
		if (base_address)
		{
			FileMappingData_* data = OS_HeapAlloc(sizeof(FileMappingData_));
			data->handle = mapping;
			data->address = base_address;
			result.ptr = data;

			*out_buffer = base_address;
			*out_size = size;
		}
		else
			CloseHandle(mapping);
	}

	FillOsErr_(out_err, GetLastError());
	return result;
}

API void
OS_UnmapFile(OS_FileMapping mapping)
{
	Trace();
	FileMappingData_* data = mapping.ptr;

	UnmapViewOfFile(data->address);
	CloseHandle(data->handle);
	OS_HeapFree(data);
}

//------------------------------------------------------------------------
//~ Process stuff
API int32
OS_Exec(String cmd, OS_Error* out_err)
{
	Trace(); TraceText(cmd);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	int32 r = INT32_MIN;
	
	for ArenaTempScope(scratch_arena)
	{
		cmd = ArenaPrintf(scratch_arena, "cmd /c %S", cmd);
		if (!cmd.size)
			continue;
		LPWSTR wcmd = StringToWide_(scratch_arena, cmd);
		if (!wcmd)
			continue;
		STARTUPINFOW startup_info = {
			sizeof(startup_info),
		};
		PROCESS_INFORMATION process_info = { 0 };
		
		// NOTE(ljre): Remember that CreateProcessW may mutate it's second argument!
		BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, false, 0, NULL, NULL, &startup_info, &process_info);
		if (ok)
		{
			HANDLE handle = process_info.hProcess;
			DWORD wait = WaitForSingleObject(handle, INFINITE);
			if (wait == WAIT_OBJECT_0)
			{
				DWORD code;
				if (GetExitCodeProcess(handle, &code))
				{
					r = (int32)code;
					SetLastError(0);
				}
			}
			CloseHandle(process_info.hProcess);
			CloseHandle(process_info.hThread);
		}
	}
	
	FillOsErr_(out_err, GetLastError());
	return r;
}

API OS_ChildProcess
OS_ExecAsync(String cmd, OS_ChildProcessDesc const* child_desc, OS_Error* out_err)
{
	Trace(); TraceText(cmd);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	HANDLE handle = NULL;
	
	for ArenaTempScope(scratch_arena)
	{
		cmd = ArenaPrintf(scratch_arena, "cmd /c %S", cmd);
		if (!cmd.size)
			continue;
		LPWSTR wcmd = StringToWide_(scratch_arena, cmd);
		if (!wcmd)
			continue;
		STARTUPINFOW startup_info = ChildProcessDescToStartupinfow_(child_desc);
		PROCESS_INFORMATION process_info = { 0 };
		
		// NOTE(ljre): Remember that CreateProcessW may mutate it's second argument!
		BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, !!child_desc, 0, NULL, NULL, &startup_info, &process_info);
		if (ok)
		{
			handle = process_info.hProcess;
			if (handle)
				SetLastError(0);
		}
	}
	
	FillOsErr_(out_err, GetLastError());
	return (OS_ChildProcess) { handle };
}

API OS_ChildProcess
OS_CreateChildProcess(String executable, String args, OS_ChildProcessDesc const* child_desc, OS_Error* out_err)
{
	Trace(); TraceText(executable);
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	HANDLE handle = NULL;
	
	for ArenaTempScope(scratch_arena)
	{
		String cmd = ArenaPrintf(scratch_arena, "\"%S\" %S", executable, args);
		if (!cmd.size)
			continue;
		LPWSTR wexecutable = StringToWide_(scratch_arena, executable);
		if (!wexecutable)
			continue;
		LPWSTR wcmd = StringToWide_(scratch_arena, cmd);
		if (!wcmd)
			continue;
		STARTUPINFOW startup_info = ChildProcessDescToStartupinfow_(child_desc);
		PROCESS_INFORMATION process_info = { 0 };
		
		// NOTE(ljre): Remember that CreateProcessW may mutate it's second argument!
		BOOL ok = CreateProcessW(wexecutable, wcmd, NULL, NULL, !!child_desc, 0, NULL, NULL, &startup_info, &process_info);
		if (ok)
		{
			handle = process_info.hProcess;
			if (handle)
				SetLastError(0);
		}
	}
	
	FillOsErr_(out_err, GetLastError());
	return (OS_ChildProcess) { handle };
}

API void
OS_WaitChildProcess(OS_ChildProcess child, OS_Error* out_err)
{
	Trace();
	HANDLE handle = child.ptr;
	DWORD result = WaitForSingleObject(handle, INFINITE);
	if (result == WAIT_OBJECT_0)
		SetLastError(0);
	
	FillOsErr_(out_err, GetLastError());
}

API bool
OS_GetReturnCodeChildProcess(OS_ChildProcess child, int32* out_code, OS_Error* out_err)
{
	Trace();
	HANDLE handle = child.ptr;
	bool result = false;
	
	DWORD retcode;
	BOOL ok = GetExitCodeProcess(handle, &retcode);
	if (ok)
	{
		*out_code = (int32)retcode;
		result = true;
		SetLastError(0);
	}
	
	FillOsErr_(out_err, GetLastError());
	return result;
}

API void
OS_KillChildProcess(OS_ChildProcess child, OS_Error* out_err)
{
	Trace();
	HANDLE handle = child.ptr;
	BOOL ok = TerminateProcess(handle, (UINT)INT32_MIN);
	
	if (ok)
		SetLastError(0);
	
	FillOsErr_(out_err, GetLastError());
}

API void
OS_DestroyChildProcess(OS_ChildProcess child)
{
	Trace();
	HANDLE handle = child.ptr;
	
	CloseHandle(handle);
}

//~ Threading Stuff
static_assert(sizeof(OS_RWLock) == sizeof(SRWLOCK), "both should have the same size");
static_assert(sizeof(OS_ConditionVariable) == sizeof(CONDITION_VARIABLE), "both should have the same size");

API void
OS_InitRWLock(OS_RWLock* lock)
{
	Trace();
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	InitializeSRWLock(srwlock);
}

API void
OS_LockShared(OS_RWLock* lock)
{
	Trace();
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	AcquireSRWLockShared(srwlock);
}

API void
OS_LockExclusive(OS_RWLock* lock)
{
	Trace();
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	AcquireSRWLockExclusive(srwlock);
}

API bool
OS_TryLockShared(OS_RWLock* lock)
{
	Trace();
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	return TryAcquireSRWLockShared(srwlock);
}

API bool
OS_TryLockExclusive(OS_RWLock* lock)
{
	Trace();
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	return TryAcquireSRWLockExclusive(srwlock);
}

API void
OS_UnlockShared(OS_RWLock* lock)
{
	Trace();
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	ReleaseSRWLockShared(srwlock);
}

API void
OS_UnlockExclusive(OS_RWLock* lock)
{
	Trace();
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	ReleaseSRWLockExclusive(srwlock);
}

API void
OS_DeinitRWLock(OS_RWLock* lock)
{
	Trace();
}

API void
OS_InitSemaphore(OS_Semaphore* sem, int32 max_count)
{
	Trace();
	HANDLE handle = CreateSemaphoreA(NULL, max_count, max_count, NULL);
	sem->ptr = handle;
}

API bool
OS_WaitForSemaphore(OS_Semaphore* sem)
{
	Trace();
	Assert(sem);
	
	if (sem->ptr)
	{
		DWORD result = WaitForSingleObjectEx(sem->ptr, INFINITE, false);
		return result == WAIT_OBJECT_0;
	}
	
	return false;
}

API void
OS_SignalSemaphore(OS_Semaphore* sem, int32 count)
{
	Trace();
	Assert(sem);
	
	if (sem->ptr)
		ReleaseSemaphore(sem->ptr, count, NULL);
}

API void
OS_DeinitSemaphore(OS_Semaphore* sem)
{
	Trace();
	Assert(sem);
	
	if (sem->ptr)
	{
		CloseHandle(sem->ptr);
		sem->ptr = NULL;
	}
}

API void
OS_InitConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	CONDITION_VARIABLE* convar = (CONDITION_VARIABLE*)var;
	InitializeConditionVariable(convar);
}

API bool
OS_WaitConditionVariable(OS_ConditionVariable* var, OS_RWLock* lock, bool is_lock_shared, int32 ms)
{
	Trace();
	DWORD wait_time = (ms < 0) ? INFINITE : (DWORD)ms;
	DWORD flags = (is_lock_shared) ? CONDITION_VARIABLE_LOCKMODE_SHARED : 0;
	SRWLOCK* srwlock = (SRWLOCK*)lock;
	CONDITION_VARIABLE* convar = (CONDITION_VARIABLE*)var;
	
	BOOL result = SleepConditionVariableSRW(convar, srwlock, wait_time, flags);
	if (result == 0)
	{
		DWORD err = GetLastError();
		SafeAssert(err == ERROR_TIMEOUT);
	}
	
	return (result != 0);
}

API void
OS_SignalConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	CONDITION_VARIABLE* convar = (CONDITION_VARIABLE*)var;
	WakeConditionVariable(convar);
}

API void
OS_SignalAllConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	CONDITION_VARIABLE* convar = (CONDITION_VARIABLE*)var;
	WakeAllConditionVariable(convar);
}

API void
OS_DeinitConditionVariable(OS_ConditionVariable* var)
{
	Trace();
}

// ===========================================================================
// ===========================================================================
// Clipboard
API OS_ClipboardContents
OS_GetClipboard(SingleAllocator allocator, OS_ClipboardContentType allowed_types, OS_Window owner_window, OS_Error* out_err)
{
	Trace();
	OS_ClipboardContents result = {};
	AllocatorError alloc_err = 0;
	HWND hwnd = NULL;
	if (owner_window.ptr)
		hwnd = ((WindowData_*)owner_window.ptr)->handle;

	if (OpenClipboard(hwnd))
	{
		// unicode text
		if (!result.type && !alloc_err && (allowed_types & OS_ClipboardContentType_Text))
		{
			HANDLE clipboard_data = GetClipboardData(CF_UNICODETEXT);
			if (clipboard_data)
			{
				wchar_t* wstr = GlobalLock(clipboard_data);
				if (wstr)
				{
					String str = WideToStringSingle_(allocator, wstr, &alloc_err);
					if (str.size)
					{
						result.contents = str;
						result.type = OS_ClipboardContentType_Text;
					}
				}
				GlobalUnlock(clipboard_data);
			}
		}

		// ascii text
		if (!result.type && !alloc_err && (allowed_types & OS_ClipboardContentType_Text))
		{
			// TODO(ljre): This is actually ANSI, and not ASCII... but surely the path above will work
			//             and perform the conversion before this is reached, right?
			HANDLE clipboard_data = GetClipboardData(CF_TEXT);
			if (clipboard_data)
			{
				char* str = GlobalLock(clipboard_data);
				if (str)
				{
					intz size = MemoryStrlen(str);
					uint8* mem = AllocatorAlloc(&allocator, size, 1, &alloc_err);
					if (mem)
					{
						MemoryCopy(mem, str, size);
						result.contents = StrMake(size, mem);
						result.type = OS_ClipboardContentType_Text;
					}
				}
				GlobalUnlock(clipboard_data);
			}
		}

		CloseClipboard();
	}

	if (out_err)
	{
		if (alloc_err)
		{
			*out_err = (OS_Error) {
				.ok = (alloc_err == 0),
				.code = (uint32)alloc_err,
				.is_allocator_err = (alloc_err != 0),
			};
		}
		else
			FillOsErr_(out_err, GetLastError());
	}
	else
		SafeAssert(!alloc_err);

	return result;
}

API bool
OS_SetClipboard(OS_ClipboardContents contents, OS_Window owner_window, OS_Error* out_err)
{
	Trace();
	HWND hwnd = NULL;
	if (owner_window.ptr)
		hwnd = ((WindowData_*)owner_window.ptr)->handle;

	if (OpenClipboard(hwnd))
	{
		if (EmptyClipboard())
		{
			HANDLE clipboard_data = NULL;
			UINT data_type = 0;

			switch (contents.type)
			{
				default: break;
				case OS_ClipboardContentType_Text:
				{
					ArenaSavepoint scratch = ArenaSave(OS_ScratchArena(NULL, 0));
					wchar_t* wstr = StringToWide_(scratch.arena, contents.contents);
					intz size = 0;
					while (wstr[size++]);
					clipboard_data = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(size * SignedSizeof(wchar_t)));

					if (clipboard_data)
					{
						void* dst = GlobalLock(clipboard_data);
						MemoryCopy(dst, wstr, size * SignedSizeof(wchar_t));
						GlobalUnlock(clipboard_data);
						data_type = CF_UNICODETEXT;
					}
					ArenaRestore(scratch);
				} break;
			}

			if (clipboard_data)
				SetClipboardData(data_type, clipboard_data);
		}

		CloseClipboard();
	}

	return FillOsErr_(out_err, GetLastError());
}

//------------------------------------------------------------------------
//~ API D3D11
API bool
OS_MakeD3D11Api(OS_D3D11Api* out_api, OS_D3D11ApiDesc const* args)
{
	Trace();
	WindowData_* window_data = args->window.ptr;
	HWND hwnd = window_data->handle;
	
	ID3D11Device* d3d11_device = NULL;
	ID3D11Device1* d3d11_device1 = NULL;
	ID3D11DeviceContext* d3d11_context = NULL;
	ID3D11DeviceContext1* d3d11_context1 = NULL;
	IDXGISwapChain* d3d11_swapchain = NULL;
	IDXGISwapChain1* d3d11_swapchain1 = NULL;
	IDXGIFactory2* d3d11_factory2 = NULL;
	IDXGIAdapter* d3d11_adapter = NULL;
	IDXGIDevice1* d3d11_dxgi_device1 = NULL;
	ID3D11RenderTargetView* d3d11_target = NULL;
	ID3D11DepthStencilView* d3d11_depth_stencil = NULL;
	ID3D11Texture2D* backbuffer = NULL;
	ID3D11Texture2D* depth_stencil = NULL;
	
	// NOTE(ljre): Try to find dGPU adapter
	IDXGIAdapter* adapter = NULL;
	{
		IDXGIFactory* factory;
		if (SUCCEEDED(CreateDXGIFactory(&IID_IDXGIFactory, (void**)&factory)))
		{
			IDXGIFactory6* factory6;
			if (SUCCEEDED(IDXGIFactory_QueryInterface(factory, &IID_IDXGIFactory6, (void**)&factory6)))
			{
				DXGI_GPU_PREFERENCE type = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
				if (args->use_igpu_if_possible)
					type = DXGI_GPU_PREFERENCE_MINIMUM_POWER;
				
				if (FAILED(IDXGIFactory6_EnumAdapterByGpuPreference(factory6, 0, type, &IID_IDXGIAdapter, (void**)&adapter)))
					adapter = NULL;
				IDXGIFactory6_Release(factory6);
			}
			IDXGIFactory_Release(factory);
		}
	}
	
	bool is_win10_or_later = (g_win32.version_info.dwMajorVersion >= 10);
	UINT flags = 0;
	HRESULT hr;
	
#if defined(CONFIG_DEBUG) && !defined(__GNUC__)
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	
	D3D_FEATURE_LEVEL feature_levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};
	
	hr = D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL,
		flags, feature_levels, ArrayLength(feature_levels), D3D11_SDK_VERSION,
		&d3d11_device, NULL, &d3d11_context);
	if (adapter)
		IDXGIAdapter_Release(adapter);
	if (FAILED(hr))
		goto failure;
	
	hr = ID3D11Device_QueryInterface(d3d11_device, &IID_ID3D11Device1, (void**)&d3d11_device1);
	if (FAILED(hr))
		goto failure;
	
	hr = ID3D11DeviceContext_QueryInterface(d3d11_context, &IID_ID3D11DeviceContext1, (void**)&d3d11_context1);
	if (FAILED(hr))
		goto failure;
	
	hr = ID3D11Device1_QueryInterface(d3d11_device1, &IID_IDXGIDevice1, (void**)&d3d11_dxgi_device1);
	if (FAILED(hr))
		goto failure;
	
	hr = IDXGIDevice1_GetAdapter(d3d11_dxgi_device1, &d3d11_adapter);
	if (FAILED(hr))
		goto failure;
	
	hr = IDXGIAdapter_GetParent(d3d11_adapter, &IID_IDXGIFactory2, (void**)&d3d11_factory2);
	if (FAILED(hr))
		goto failure;
	
	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
		.Width = 0,
		.Height = 0,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		//.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.SampleDesc.Count = 1,
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 3,
		.SwapEffect = is_win10_or_later ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD,
		.Flags = 0,
	};
	
	hr = IDXGIFactory2_CreateSwapChainForHwnd(d3d11_factory2, (IUnknown*)d3d11_device1, hwnd, &swapchain_desc, NULL, NULL, &d3d11_swapchain1);
	if (FAILED(hr))
		goto failure;
	
	(void) IDXGIFactory2_MakeWindowAssociation(d3d11_factory2, hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
	
	hr = IDXGISwapChain1_QueryInterface(d3d11_swapchain1, &IID_IDXGISwapChain, (void**)&d3d11_swapchain);
	if (FAILED(hr))
		goto failure;
	
	// Make render target view and get backbuffer desc
	hr = IDXGISwapChain_GetBuffer(d3d11_swapchain, 0, &IID_ID3D11Texture2D, (void**)&backbuffer);
	if (FAILED(hr))
		goto failure;
	
	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
	};
	hr = ID3D11Device_CreateRenderTargetView(d3d11_device, (ID3D11Resource*)backbuffer, &rtv_desc, &d3d11_target);
	if (FAILED(hr))
		goto failure;
	
	D3D11_TEXTURE2D_DESC backbuffer_desc;
	ID3D11Texture2D_GetDesc(backbuffer, &backbuffer_desc);
	ID3D11Resource_Release(backbuffer);
	backbuffer = NULL;
	
	// Make depth stencil view
	D3D11_TEXTURE2D_DESC depth_stencil_desc = {
		.Width = backbuffer_desc.Width,
		.Height = backbuffer_desc.Height,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0,
		},
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_DEPTH_STENCIL,
		.CPUAccessFlags = 0,
		.MiscFlags = 0,
	};
	
	hr = ID3D11Device_CreateTexture2D(d3d11_device, &depth_stencil_desc, NULL, &depth_stencil);
	if (FAILED(hr))
		goto failure;
	hr = ID3D11Device_CreateDepthStencilView(d3d11_device, (ID3D11Resource*)depth_stencil, NULL, &d3d11_depth_stencil);
	if (FAILED(hr))
		goto failure;
	ID3D11Texture2D_Release(depth_stencil);
	depth_stencil = NULL;
	
	*out_api = (OS_D3D11Api) {
		.device = d3d11_device,
		.device1 = d3d11_device1,
		.context = d3d11_context,
		.context1 = d3d11_context1,
		.swapchain = d3d11_swapchain,
		.swapchain1 = d3d11_swapchain1,
		.factory2 = d3d11_factory2,
		.adapter = d3d11_adapter,
		.dxgi_device1 = d3d11_dxgi_device1,
		.target = d3d11_target,
		.depth_stencil = d3d11_depth_stencil,
		
		.present = D3d11PresentProc_,
		.resize_buffers = D3d11ResizeBuffersProc_,
	};
	return true;
	
	failure:
	{
		if (d3d11_device)
			ID3D11Device_Release(d3d11_device);
		if (d3d11_device1)
			ID3D11Device1_Release(d3d11_device1);
		if (d3d11_context)
			ID3D11DeviceContext_Release(d3d11_context);
		if (d3d11_context1)
			ID3D11DeviceContext1_Release(d3d11_context1);
		if (d3d11_swapchain)
			IDXGISwapChain_Release(d3d11_swapchain);
		if (d3d11_swapchain1)
			IDXGISwapChain1_Release(d3d11_swapchain1);
		if (d3d11_factory2)
			IDXGIFactory2_Release(d3d11_factory2);
		if (d3d11_adapter)
			IDXGIAdapter_Release(d3d11_adapter);
		if (d3d11_dxgi_device1)
			IDXGIDevice1_Release(d3d11_dxgi_device1);
		if (d3d11_target)
			ID3D11RenderTargetView_Release(d3d11_target);
		if (d3d11_depth_stencil)
			ID3D11DepthStencilView_Release(d3d11_depth_stencil);
		if (backbuffer)
			ID3D11Texture2D_Release(backbuffer);
		if (depth_stencil)
			ID3D11Texture2D_Release(depth_stencil);
		
		return false;
	}
}

API bool
OS_FreeD3D11Api(OS_D3D11Api* api)
{
	Trace();

	if (api->context)
		ID3D11DeviceContext_ClearState(api->context);

	if (api->device)
		ID3D11Device_Release(api->device);
	if (api->device1)
		ID3D11Device1_Release(api->device1);
	if (api->context)
		ID3D11DeviceContext_Release(api->context);
	if (api->context1)
		ID3D11DeviceContext1_Release(api->context1);
	if (api->swapchain)
		IDXGISwapChain_Release(api->swapchain);
	if (api->swapchain1)
		IDXGISwapChain1_Release(api->swapchain1);
	if (api->factory2)
		IDXGIFactory2_Release(api->factory2);
	if (api->adapter)
		IDXGIAdapter_Release(api->adapter);
	if (api->dxgi_device1)
		IDXGIDevice1_Release(api->dxgi_device1);
	if (api->target)
		ID3D11RenderTargetView_Release(api->target);
	if (api->depth_stencil)
		ID3D11DepthStencilView_Release(api->depth_stencil);

	MemoryZero(api, sizeof(*api));

	#ifdef CONFIG_DEBUG
	HMODULE module = GetModuleHandleW(L"Dxgidebug.dll");
	if (module)
	{
		HRESULT (WINAPI *get_debug_interface)(REFIID riid, void** ppDebug);
		get_debug_interface = (void*)GetProcAddress(module, "DXGIGetDebugInterface");
		
		IDXGIDebug* debug = NULL;
		if (get_debug_interface && SUCCEEDED(get_debug_interface(&IID_IDXGIDebug, (void**)&debug)))
		{
			IDXGIDebug_ReportLiveObjects(debug, DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			IDXGIDebug_Release(debug);
		}
	}
	#endif

	return true;
}

//------------------------------------------------------------------------
API bool
OS_MakeOpenGLApi(OS_OpenGLApi* out_api, OS_OpenGLApiDesc const* args)
{
	Trace();
	WindowData_* window_data = args->window.ptr;
	SafeAssert(window_data);
	
	HWND dummy_window = NULL;
	HDC dummy_hdc = NULL;
	HGLRC dummy_context = NULL;
	HMODULE library = NULL;
	HDC hdc = NULL;
	HGLRC context = NULL;
	
	dummy_window = CreateWindowExW(
		0, L"STATIC", L"dummy", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, g_win32.instance, NULL);
	if (!dummy_window)
		goto failure;
	
	dummy_hdc = GetDC(dummy_window);
	if (!dummy_hdc)
		goto failure;
	
	library = LoadLibraryW(L"opengl32.dll");
	if (!library)
		goto failure;
	
	PFNWGLGETPROCADDRESSPROC wglGetProcAddress = (void*)GetProcAddress(library, "wglGetProcAddress");
	PFNWGLCREATECONTEXTPROC wglCreateContext = (void*)GetProcAddress(library, "wglCreateContext");
	PFNWGLMAKECURRENTPROC wglMakeCurrent = (void*)GetProcAddress(library, "wglMakeCurrent");
	PFNWGLDELETECONTEXTPROC wglDeleteContext = (void*)GetProcAddress(library, "wglDeleteContext");
	PFNWGLSWAPLAYERBUFFERSPROC wglSwapLayerBuffers = (void*)GetProcAddress(library, "wglSwapLayerBuffers");
	if (!wglGetProcAddress || !wglCreateContext || !wglMakeCurrent || !wglDeleteContext || !wglSwapLayerBuffers)
		goto failure;
	
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		24,
		8,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};
	
	int32 pixel_format = ChoosePixelFormat(dummy_hdc, &pfd);
	if (!pixel_format)
		goto failure;
	
	SetPixelFormat(dummy_hdc, pixel_format, &pfd);
	dummy_context = wglCreateContext(dummy_hdc);
	if (!dummy_context)
		goto failure;
	wglMakeCurrent(dummy_hdc, dummy_context);
	
	PFNWGLCHOOSEPIXELFORMATARBPROC    wglChoosePixelFormatARB    = OglGetProc_(wglGetProcAddress, library, "wglChoosePixelFormatARB");
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = OglGetProc_(wglGetProcAddress, library, "wglCreateContextAttribsARB");
	PFNWGLMAKECONTEXTCURRENTARBPROC   wglMakeContextCurrentARB   = OglGetProc_(wglGetProcAddress, library, "wglMakeContextCurrentARB");
	PFNWGLSWAPINTERVALEXTPROC         wglSwapIntervalEXT         = OglGetProc_(wglGetProcAddress, library, "wglSwapIntervalEXT");
	if (!wglChoosePixelFormatARB || !wglCreateContextAttribsARB || !wglMakeContextCurrentARB || !wglSwapIntervalEXT)
		goto failure;
	
	hdc = GetDC(window_data->handle);
	if (!hdc)
		goto failure;
	
	UINT num_formats = 0;
	int32 attribs[] = {
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB, 32,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_STENCIL_BITS_ARB, 8,
		0
	};
	
	wglChoosePixelFormatARB(hdc, attribs, 0, 1, &pixel_format, &num_formats);
	if (!pixel_format || num_formats == 0)
		goto failure;
	
	pfd = (PIXELFORMATDESCRIPTOR) { .nSize = sizeof(pfd) };
	if (!DescribePixelFormat(hdc, pixel_format, sizeof(pfd), &pfd))
		goto failure;
	
	if (!SetPixelFormat(hdc, pixel_format, &pfd))
		goto failure;
	
	int32 context_attribs[] = {
		//WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		//WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef CONFIG_DEBUG
		WGL_CONTEXT_FLAGS, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
		0
	};
	
	context = wglCreateContextAttribsARB(hdc, 0, context_attribs);
	if (!context)
		goto failure;
	
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(dummy_context);
	ReleaseDC(dummy_window, dummy_hdc);
	DestroyWindow(dummy_window);
	dummy_context = NULL;
	dummy_hdc = NULL;
	dummy_window = NULL;
	
	wglMakeContextCurrentARB(hdc, hdc, context);
	wglSwapIntervalEXT(args->swap_interval);
	
	window_data->acquired_hdc = hdc;
	*out_api = (OS_OpenGLApi) {
		.is_es = false,
		.version = 33,
		.window = args->window,
		.library = library,
		.hdc = hdc,
		.context = context,
		.present = OglPresentProc_,
		.resize_buffers = OglResizeBuffersProc_,
#define X(Type, Name) .Name = OglGetProc_(wglGetProcAddress, library, #Name),
		_GL_FUNCTIONS_X_MACRO_ES20(X)
			_GL_FUNCTIONS_X_MACRO_ES30(X)
			_GL_FUNCTIONS_X_MACRO_ES31(X)
			_GL_FUNCTIONS_X_MACRO_ES32(X)
#undef X
	};
	
	return true;
	failure:
	{
		if (hdc)
			ReleaseDC(window_data->handle, hdc);
		if (dummy_context && wglMakeCurrent && wglDeleteContext)
		{
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(dummy_context);
		}
		if (dummy_hdc)
			ReleaseDC(dummy_window, dummy_hdc);
		if (dummy_window)
			DestroyWindow(dummy_window);
		if (library)
			FreeLibrary(library);
		
		return false;
	}
}

API bool
OS_FreeOpenGLApi(OS_OpenGLApi* api)
{
	Trace();
	WindowData_* window_data = api->window.ptr;
	HMODULE library = api->library;
	HDC hdc = api->hdc;
	HGLRC context = api->context;

	if (!window_data || !library || !context)
		return false;

	PFNWGLMAKECURRENTPROC wglMakeCurrent = (void*)GetProcAddress(library, "wglMakeCurrent");
	PFNWGLDELETECONTEXTPROC wglDeleteContext = (void*)GetProcAddress(library, "wglDeleteContext");

	if (!wglMakeCurrent || !wglDeleteContext)
		return false;

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(context);
	ReleaseDC(window_data->handle, hdc);
	FreeLibrary(library);
	MemoryZero(api, sizeof(*api));
	return true;
}

//------------------------------------------------------------------------
// API OS_W32_
API HWND
OS_W32_HwndFromWindow(OS_Window window)
{
	WindowData_* window_data = window.ptr;
	HWND result = 0;
	if (window_data)
		result = window_data->handle;
	return result;
}

API String
OS_W32_StringFromHr(HRESULT hr, Arena* output_arena)
{
	Trace();
	String result = { 0 };
	wchar_t* wtext = NULL;
	DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	DWORD len = FormatMessageW(flags, NULL, (DWORD)hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (void*)&wtext, 0, NULL);

	if (len > 0 && wtext)
	{
		result = WideToString_(output_arena, wtext);
		LocalFree(wtext);
	}

	return result;
}

API wchar_t*
OS_W32_StringToWide(String str, Arena* output_arena)
{ return StringToWide_(output_arena, str); }

API String
OS_W32_WideToString(LPCWSTR wstr, Arena* output_arena)
{ return WideToString_(output_arena, wstr); }

API bool
OS_W32_MFStartup(OS_Error *out_err)
{
	Trace();
	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
	return FillOsErr_(out_err, (DWORD)hr);
}

API bool
OS_W32_MFShutdown(OS_Error *out_err)
{
	Trace();
	HRESULT hr = MFShutdown();
	return FillOsErr_(out_err, (DWORD)hr);
}

#ifdef CONFIG_DEBUG
API void
OS_DebugMessageBox(const char* fmt, ...)
{
	Trace();
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	
	for ArenaTempScope(scratch_arena)
	{
		va_list args;
		va_start(args, fmt);
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		va_end(args);
		
		wchar_t* wstr = StringToWide_(scratch_arena, str);
		MessageBoxW(NULL, wstr, L"OS_DebugMessageBox", MB_OK);
	}
}

API void
OS_DebugLog(const char* fmt, ...)
{
	Trace();
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	
	for ArenaTempScope(scratch_arena)
	{
		va_list args;
		va_start(args, fmt);
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		ArenaPushData(scratch_arena, &""); // null terminator
		va_end(args);
		
		if (IsDebuggerPresent())
			OutputDebugStringA((const char*)str.data);
		else
			WriteFile(GetStdHandle(STD_ERROR_HANDLE), str.data, (DWORD)str.size, NULL, NULL);
	}
}
#endif //CONFIG_DEBUG

// Heap Allocator
static void*
HeapAllocatorProc_(void* instance, AllocatorMode mode, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	Trace();
	SafeAssert(size >= 0);
	SafeAssert(alignment >= 0);
	void* result = NULL;
	AllocatorError error = AllocatorError_Ok;

	bool is_invalid_alignment = (!alignment || (alignment & alignment-1) != 0);
	if (alignment < sizeof(void*))
		alignment = sizeof(void*);

	switch (mode)
	{
		case AllocatorMode_Alloc:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			result = _aligned_malloc((size_t)size, (size_t)alignment);
			if (!result)
				error = AllocatorError_OutOfMemory;
			else
				MemoryZero(result, size);
		} break;
		case AllocatorMode_Free:
		{
			if (!old_ptr)
				break; // free(NULL) has to be noop?
			_aligned_free(old_ptr);
		} break;
		case AllocatorMode_Resize:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			if (!old_ptr)
			{
				result = _aligned_malloc((size_t)size, (size_t)alignment);
				if (!result)
					error = AllocatorError_OutOfMemory;
				else
					MemoryZero(result, size);
				break;
			}

			result = _aligned_realloc(old_ptr, (size_t)size, (size_t)alignment);
			if (!result)
				error = AllocatorError_OutOfMemory;
			else if (size > old_size)
				MemoryZero((uint8*)result + old_size, size - old_size);
		} break;
		case AllocatorMode_AllocNonZeroed:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			result = _aligned_malloc((size_t)size, (size_t)alignment);
			if (!result)
				error = AllocatorError_OutOfMemory;
		} break;
		case AllocatorMode_ResizeNonZeroed:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			if (!old_ptr)
			{
				result = _aligned_malloc((size_t)size, (size_t)alignment);
				if (!result)
					error = AllocatorError_OutOfMemory;
				else
					MemoryZero(result, size);
				break;
			}

			result = _aligned_realloc(old_ptr, (size_t)size, (size_t)alignment);
			if (!result)
				error = AllocatorError_OutOfMemory;
		} break;
		case AllocatorMode_QueryFeatures:
		{
			SafeAssert(old_ptr && ((uintptr)old_ptr & sizeof(uint32)-1) == 0);
			uint32* features_ptr = (uint32*)old_ptr;
			*features_ptr =
				AllocatorMode_Alloc |
				AllocatorMode_AllocNonZeroed |
				AllocatorMode_Resize |
				AllocatorMode_ResizeNonZeroed |
				AllocatorMode_Free |
				AllocatorMode_QueryFeatures;
		} break;
		default:
		{
			error = AllocatorError_ModeNotImplemented;
		} break;
	}

	if (out_err)
		*out_err = error;
	return result;
}

API Allocator
OS_HeapAllocator(void)
{
	return (Allocator) {
		.proc = HeapAllocatorProc_,
		.instance = NULL,
	};
}

//~ NOTE(ljre): COM GUIDs
DisableWarnings();
static const CLSID CLSID_MMDeviceEnumerator = {0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};
static const IID IID_IMMDeviceEnumerator = {0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};
static const IID IID_IAudioClient = {0x1CB9AD4C,0xDBFA,0x4c32,{0xB1,0x78,0xC2,0xF5,0x68,0xA7,0x03,0xB2}};
static const IID IID_IAudioRenderClient = {0xF294ACFC,0x3146,0x4483,{0xA7,0xBF,0xAD,0xDC,0xA7,0xC2,0x60,0xE2}};
//- NOTE(ljre): Generated with https://github.com/elmindreda/c_dfDIJoystick2
#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL        0x80000000
#endif

static DIOBJECTDATAFORMAT c_dfDIJoystick_data_format[] = {
	{ &GUID_XAxis,DIJOFS_X,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_YAxis,DIJOFS_Y,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_ZAxis,DIJOFS_Z,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_RxAxis,DIJOFS_RX,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_RyAxis,DIJOFS_RY,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_RzAxis,DIJOFS_RZ,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_POV,DIJOFS_POV(0),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ &GUID_POV,DIJOFS_POV(1),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ &GUID_POV,DIJOFS_POV(2),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ &GUID_POV,DIJOFS_POV(3),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(0),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(1),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(2),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(3),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(4),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(5),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(6),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(7),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(8),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(9),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(10),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(11),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(12),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(13),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(14),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(15),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(16),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(17),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(18),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(19),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(20),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(21),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(22),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(23),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(24),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(25),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(26),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(27),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(28),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(29),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(30),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(31),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
};

static DIOBJECTDATAFORMAT c_dfDIJoystick2_data_format[] = {
	{ &GUID_XAxis,DIJOFS_X,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_YAxis,DIJOFS_Y,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_ZAxis,DIJOFS_Z,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_RxAxis,DIJOFS_RX,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_RyAxis,DIJOFS_RY,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_RzAxis,DIJOFS_RZ,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
	{ &GUID_POV,DIJOFS_POV(0),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ &GUID_POV,DIJOFS_POV(1),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ &GUID_POV,DIJOFS_POV(2),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ &GUID_POV,DIJOFS_POV(3),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(0),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(1),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(2),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(3),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(4),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(5),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(6),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(7),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(8),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(9),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(10),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(11),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(12),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(13),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(14),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(15),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(16),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(17),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(18),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(19),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(20),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(21),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(22),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(23),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(24),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(25),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(26),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(27),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(28),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(29),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(30),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(31),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(32),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(33),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(34),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(35),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(36),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(37),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(38),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(39),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(40),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(41),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(42),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(43),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(44),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(45),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(46),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(47),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(48),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(49),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(50),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(51),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(52),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(53),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(54),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(55),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(56),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(57),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(58),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(59),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(60),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(61),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(62),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(63),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(64),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(65),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(66),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(67),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(68),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(69),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(70),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(71),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(72),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(73),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(74),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(75),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(76),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(77),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(78),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(79),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(80),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(81),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(82),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(83),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(84),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(85),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(86),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(87),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(88),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(89),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(90),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(91),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(92),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(93),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(94),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(95),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(96),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(97),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(98),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(99),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(100),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(101),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(102),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(103),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(104),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(105),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(106),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(107),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(108),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(109),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(110),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(111),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(112),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(113),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(114),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(115),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(116),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(117),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(118),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(119),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(120),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(121),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(122),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(123),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(124),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(125),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(126),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ NULL,DIJOFS_BUTTON(127),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
	{ &GUID_XAxis,FIELD_OFFSET(DIJOYSTATE2,lVX),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_YAxis,FIELD_OFFSET(DIJOYSTATE2,lVY),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_ZAxis,FIELD_OFFSET(DIJOYSTATE2,lVZ),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_RxAxis,FIELD_OFFSET(DIJOYSTATE2,lVRx),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_RyAxis,FIELD_OFFSET(DIJOYSTATE2,lVRy),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_RzAxis,FIELD_OFFSET(DIJOYSTATE2,lVRz),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTVELOCITY },
	{ &GUID_XAxis,FIELD_OFFSET(DIJOYSTATE2,lAX),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_YAxis,FIELD_OFFSET(DIJOYSTATE2,lAY),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_ZAxis,FIELD_OFFSET(DIJOYSTATE2,lAZ),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_RxAxis,FIELD_OFFSET(DIJOYSTATE2,lARx),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_RyAxis,FIELD_OFFSET(DIJOYSTATE2,lARy),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_RzAxis,FIELD_OFFSET(DIJOYSTATE2,lARz),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTACCEL|DIDOI_ASPECTPOSITION|DIDOI_ASPECTVELOCITY },
	{ &GUID_XAxis,FIELD_OFFSET(DIJOYSTATE2,lFX),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
	{ &GUID_YAxis,FIELD_OFFSET(DIJOYSTATE2,lFY),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
	{ &GUID_ZAxis,FIELD_OFFSET(DIJOYSTATE2,lFZ),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
	{ &GUID_RxAxis,FIELD_OFFSET(DIJOYSTATE2,lFRx),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
	{ &GUID_RyAxis,FIELD_OFFSET(DIJOYSTATE2,lFRy),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
	{ &GUID_RzAxis,FIELD_OFFSET(DIJOYSTATE2,lFRz),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
	{ &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
	{ &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTFORCE },
};

static const DIDATAFORMAT c_dfDIJoystick = {
	sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT),
	DIDFT_ABSAXIS, sizeof(DIJOYSTATE),
	sizeof(c_dfDIJoystick_data_format) / sizeof(DIOBJECTDATAFORMAT),
	c_dfDIJoystick_data_format,
};

static const DIDATAFORMAT c_dfDIJoystick2 = {
	sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT),
	DIDFT_ABSAXIS, sizeof(DIJOYSTATE2),
	sizeof(c_dfDIJoystick2_data_format) / sizeof(DIOBJECTDATAFORMAT),
	c_dfDIJoystick2_data_format,
};
ReenableWarnings();

//~
#if 0
// NOTE(ljre): This function retrieves the current process GPU Memory Usage using win32's Perf API.

#include <perflib.h>
#include <stdio.h>
#pragma comment(lib, "AdvAPI32.lib")

static void
Test(void)
{
	ULONG status;
	HANDLE handle;
	
	status = PerfOpenQueryHandle(NULL, &handle);
	SafeAssert(status == ERROR_SUCCESS);
	
	struct
	{
		PERF_COUNTER_IDENTIFIER identifier;
		wchar_t filter[512]; // for sure should be enough!
		
		int64_t pad; // make struct 8-byte aligned
	} buffer = {
		.identifier = {
			.Size = sizeof(buffer),
			// https://github.com/winsiderss/systeminformer/blob/master/plugins/ExtendedTools/counters.c#L79
			.CounterSetGuid = { 0xF802502B, 0x77B4, 0x4713, 0x81, 0xB3, 0x3B, 0xE0, 0x57, 0x59, 0xDA, 0x5D },
			.CounterId = PERF_WILDCARD_COUNTER,
			.InstanceId = 0xFFFFFFFF,
		},
		.filter = L"pid_",
	};
	
	//- Write buffer.filter field -- it should have the format L"pid_N*", where N is our PID
	DWORD this_pid = GetCurrentProcessId();
	SafeAssert(this_pid != 0);
	
	char number_buffer[32];
	int number_size = snprintf(number_buffer, sizeof(number_buffer), "%lu", this_pid);
	
	wchar_t* head = buffer.filter;
	for (; *head; ++head); // find NUL terminator
	for (int i = 0; i < number_size-1; ++i)
		*head++ = number_buffer[i];
	*head++ = L'*';
	*head++ = 0;
	
	// PerfAddCounters doesn't seem to care if size is way bigger than it needs to be
	status = PerfAddCounters(handle, &buffer.identifier, sizeof(buffer));
	SafeAssert(status == ERROR_SUCCESS);
	SafeAssert(buffer.identifier.Status == ERROR_SUCCESS);
	
	DWORD query_buffer_size = 0;
	status = PerfQueryCounterData(handle, NULL, 0, &query_buffer_size);
	SafeAssert(status == ERROR_NOT_ENOUGH_MEMORY);
	
	PERF_DATA_HEADER* query_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, query_buffer_size);
	status = PerfQueryCounterData(handle, query_buffer, query_buffer_size, &query_buffer_size);
	SafeAssert(status == ERROR_SUCCESS);
	SafeAssert(query_buffer->dwNumCounters == 1); // we only set 1 counter
	
	// here is it
	struct _ET_GPU_COUNTER_DATA
	{
		ULONG DataSize; // Size of the counter data, in bytes.
		ULONG Size; // sizeof(PERF_COUNTER_DATA) + sizeof(Data) + sizeof(Padding)
		ULONGLONG Value;
	}
	typedef ET_GPU_COUNTER_DATA;
	
	PERF_COUNTER_HEADER* counter_header = (PERF_COUNTER_HEADER*)&query_buffer[1];
	if (counter_header->dwStatus != ERROR_NO_DATA)
	{
		SafeAssert(counter_header->dwStatus == ERROR_SUCCESS);
		SafeAssert(counter_header->dwType == PERF_COUNTERSET);
		
		PERF_MULTI_COUNTERS* multi_counters = (PERF_MULTI_COUNTERS*)&counter_header[1];
		PERF_MULTI_INSTANCES* multi_instances = (void*)( (uint8*)multi_counters+multi_counters->dwSize );
		PERF_INSTANCE_HEADER* current_instance = (PERF_INSTANCE_HEADER*)&multi_instances[1];
		ULONG* counter_id_array = (ULONG*)&multi_counters[1];
		
		char buf[32<<10]; // 32KiB
		char* head = buf;
		char* end = buf + sizeof(buf);
		
		for (ULONG i = 0; i < multi_instances->dwInstances; ++i)
		{
			ET_GPU_COUNTER_DATA* current_counter = (void*)( (uint8*)current_instance+current_instance->Size );
			PWSTR instance_name = (void*)( (uint8*)current_instance+sizeof(PERF_INSTANCE_HEADER) );
			
			head += snprintf(head, end-head, "Instance ID: %lu\nInstance Name: ", current_instance->InstanceId);
			head += -1 + WideCharToMultiByte(CP_UTF8, 0, instance_name, -1, head, end-head, NULL, NULL);
			head += snprintf(head, end-head, "\n\n");
			
			for (ULONG j = 0; j < multi_counters->dwCounters; ++j)
			{
				switch (counter_id_array[j])
				{
					// ET_GPU_PROCESSMEMORY_TOTALCOMMITED_INDEX 1
					case 1: head += snprintf(head, end-head, "Commit Usage: %llu\n", (ULONGLONG)current_counter->Value); break;
					// ET_GPU_PROCESSMEMORY_DEDICATEDUSAGE_INDEX 4
					case 4: head += snprintf(head, end-head, "Dedicated Usage: %llu\n", (ULONGLONG)current_counter->Value); break;
					// ET_GPU_PROCESSMEMORY_SHAREDUSAGE_INDEX 5
					case 5: head += snprintf(head, end-head, "Shared Usage: %llu\n", (ULONGLONG)current_counter->Value); break;
				}
				current_counter = (void*)( (uint8*)current_counter+current_counter->Size );
			}
			
			current_instance = (void*)current_counter;
		}
		
		MessageBoxA(NULL, buf, "Title", MB_OK);
	}
	
	HeapFree(GetProcessHeap(), 0, query_buffer);
	PerfCloseQueryHandle(handle);
}
#endif
