#ifndef OS_WIN32_H
#define OS_WIN32_H

#pragma comment(lib, "ntdll.lib")

EXTERN_C __declspec(dllimport) LONG WINAPI RtlGetVersion(RTL_OSVERSIONINFOW*);

struct WindowData_ typedef WindowData_;
struct WindowData_
{
	WindowData_* next;
	HWND handle;
	HDC acquired_hdc; // only useful if opengl

	OS_MouseLockKind mouse_lock;
	OS_RWLock mouse_state_lock;
	OS_MouseState mouse_state;
};

struct FileMappingData_
{
	HANDLE handle;
	void* address;
}
typedef FileMappingData_;

#endif //OS_WIN32_H
