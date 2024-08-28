#ifndef API_OS_H
#define API_OS_H

#include "common_basic.h"

#ifdef SafeAssert_OnFailure
#   undef SafeAssert_OnFailure
#endif
#define SafeAssert_OnFailure(expr, file, line, func) OS_ExitWithErrorMessage("SafeAssert() error\nFile: " __FILE__ "\nFunc: %s\nLine: %i\nExpr: " expr, __func__, line)

enum
{
	OS_Limits_MaxGamepadCount = 16,
	OS_Limits_MaxGestureCount = 5,
	OS_Limits_MaxBufferedInput = 256,
	OS_Limits_MaxCodepointsPerFrame = 64,
	OS_Limits_MaxAudioDeviceCount = 32,
};

struct OS_Error
{
	bool ok;
	uint32 code;
	String what;
}
typedef OS_Error;

//------------------------------------------------------------------------
// Input
struct OS_ButtonState
{
	uint8 changes: 7;
	uint8 is_down: 1;
}
typedef OS_ButtonState;

static_assert(sizeof(OS_ButtonState) == 1, "bitfield didn't work?");

enum OS_KeyboardKey
{
	OS_KeyboardKey_Any = 0,
	
	OS_KeyboardKey_Escape,
	OS_KeyboardKey_Up, OS_KeyboardKey_Down, OS_KeyboardKey_Left, OS_KeyboardKey_Right,
	OS_KeyboardKey_LeftControl, OS_KeyboardKey_RightControl, OS_KeyboardKey_Control,
	OS_KeyboardKey_LeftShift, OS_KeyboardKey_RightShift, OS_KeyboardKey_Shift,
	OS_KeyboardKey_LeftAlt, OS_KeyboardKey_RightAlt, OS_KeyboardKey_Alt,
	OS_KeyboardKey_Tab, OS_KeyboardKey_Enter, OS_KeyboardKey_Backspace,
	OS_KeyboardKey_PageUp, OS_KeyboardKey_PageDown, OS_KeyboardKey_End, OS_KeyboardKey_Home,
	// NOTE(ljre): Try to not mess up. OS_KeyboardKey_Space = 32, and those above may reach it if you blindly add stuff.
	
	OS_KeyboardKey_Space = ' ',
	
	OS_KeyboardKey_0 = '0',
	OS_KeyboardKey_1, OS_KeyboardKey_2, OS_KeyboardKey_3, OS_KeyboardKey_4,
	OS_KeyboardKey_5, OS_KeyboardKey_6, OS_KeyboardKey_7, OS_KeyboardKey_8,
	OS_KeyboardKey_9,
	
	OS_KeyboardKey_A = 'A',
	OS_KeyboardKey_B, OS_KeyboardKey_C, OS_KeyboardKey_D, OS_KeyboardKey_E,
	OS_KeyboardKey_F, OS_KeyboardKey_G, OS_KeyboardKey_H, OS_KeyboardKey_I,
	OS_KeyboardKey_J, OS_KeyboardKey_K, OS_KeyboardKey_L, OS_KeyboardKey_M,
	OS_KeyboardKey_N, OS_KeyboardKey_O, OS_KeyboardKey_P, OS_KeyboardKey_Q,
	OS_KeyboardKey_R, OS_KeyboardKey_S, OS_KeyboardKey_T, OS_KeyboardKey_U,
	OS_KeyboardKey_V, OS_KeyboardKey_W, OS_KeyboardKey_X, OS_KeyboardKey_Y,
	OS_KeyboardKey_Z,
	
	OS_KeyboardKey_Numpad0,
	OS_KeyboardKey_Numpad1, OS_KeyboardKey_Numpad2, OS_KeyboardKey_Numpad3, OS_KeyboardKey_Numpad4,
	OS_KeyboardKey_Numpad5, OS_KeyboardKey_Numpad6, OS_KeyboardKey_Numpad7, OS_KeyboardKey_Numpad8,
	OS_KeyboardKey_Numpad9,
	
	OS_KeyboardKey_F1,
	OS_KeyboardKey_F2,  OS_KeyboardKey_F3,  OS_KeyboardKey_F4,  OS_KeyboardKey_F5,
	OS_KeyboardKey_F6,  OS_KeyboardKey_F7,  OS_KeyboardKey_F8,  OS_KeyboardKey_F9,
	OS_KeyboardKey_F10, OS_KeyboardKey_F11, OS_KeyboardKey_F12, OS_KeyboardKey_F13,
	OS_KeyboardKey_F14,
	
	OS_KeyboardKey_Count = 256,
}
typedef OS_KeyboardKey;

struct OS_KeyboardState
{
	int32 buffered_count;
	
	OS_ButtonState buttons[256];
	uint8 buffered[OS_Limits_MaxBufferedInput];
}
typedef OS_KeyboardState;

enum OS_MouseButton
{
	OS_MouseButton_Left,
	OS_MouseButton_Right,
	OS_MouseButton_Middle,
	OS_MouseButton_Other0,
	OS_MouseButton_Other1,
	OS_MouseButton_Other2,
	
	OS_MouseButton_Count = 6,
}
typedef OS_MouseButton;

struct OS_MouseState
{
	// NOTE(ljre): Position relative to window.
	float32 pos[2];
	float32 old_pos[2];
	int32 scroll;
	
	OS_ButtonState buttons[6];
}
typedef OS_MouseState;

enum OS_GamepadButton
{
	OS_GamepadButton_A,
	OS_GamepadButton_B,
	OS_GamepadButton_X,
	OS_GamepadButton_Y,
	OS_GamepadButton_Left,
	OS_GamepadButton_Right,
	OS_GamepadButton_Up,
	OS_GamepadButton_Down,
	OS_GamepadButton_LB,
	OS_GamepadButton_RB,
	OS_GamepadButton_RS,
	OS_GamepadButton_LS,
	OS_GamepadButton_Start,
	OS_GamepadButton_Back,
	
	OS_GamepadButton_Count = 14,
}
typedef OS_GamepadButton;

struct OS_GamepadState
{
	// [0] = -1 <= left < 0 < right <= 1
	// [1] = -1 <= up < 0 < down <= 1
	float32 left[2];
	float32 right[2];
	
	// 0..1
	float32 lt, rt;
	
	OS_ButtonState buttons[14];
}
typedef OS_GamepadState;

struct OS_GamepadStates
{
	uint32 connected_bitset;
	OS_GamepadState gamepads[OS_Limits_MaxGamepadCount];
}
typedef OS_GamepadStates;

// NOTE(ljre): Helper macros.
//             Returns a bool.
//
//             state: OS_GamepadState, OS_MouseState, or OS_KeyboardState.
//             btn: A button enum.
#define OS_IsPressed(state, btn) ((state).buttons[btn].is_down && (state).buttons[btn].changes & 1)
#define OS_IsDown(state, btn) ((state).buttons[btn].is_down)
#define OS_IsReleased(state, btn) (!(state).buttons[btn].is_down && (state).buttons[btn].changes & 1)
#define OS_IsUp(state, btn) (!(state).buttons[btn].is_down)

struct OS_GestureState
{
	bool active;
	bool released;
	
	float32 start_pos[2];
	float32 current_pos[2];
	float32 delta[2];
}
typedef OS_GestureState;

//------------------------------------------------------------------------
// Functions
enum
{
	OS_Init_Audio = 0x0001,
	OS_Init_WindowAndGraphics = 0x0002,
	OS_Init_Sleep = 0x0004,
	OS_Init_Gamepad = 0x0008,
	OS_Init_MediaFoundation = 0x0010,
};

#define OS_IsValid(handle) ((handle).ptr != (void*)0)

API void    OS_Init                (int32 systems);
API void    OS_InitConsoleApp      (void); // maybe?
API void    OS_InitGraphicsApp     (void); // maybe?
API void    OS_Sleep               (uint64 ms);
API void    OS_SleepPrecise        (uint64 ns);
API void    OS_MessageBox          (String title, String message);
API Arena*  OS_ScratchArena        (Arena* const conflict[], intsize conflict_count);
API uint64  OS_PosixTime    (void);
API uint64  OS_Tick         (void);
API uint64  OS_TickRate     (void);
API float64 OS_TimeInSeconds(void);
API void    OS_LogOut       (char const* fmt, ...);
API void    OS_LogErr       (char const* fmt, ...);
API NO_RETURN void OS_ExitWithErrorMessage(char const* fmt, ...);

API int32 EntryPoint(int32 argc, char const* const* argv);

#ifdef CONFIG_DEBUG
API void OS_DebugMessageBox(const char* fmt, ...);
API void OS_DebugLog(const char* fmt, ...);
API int OS_DebugLogPrintfFormat(const char* fmt, ...);
#else
#	define OS_DebugMessageBox(...) ((void)0)
#	define OS_DebugLog(...) ((void)0)
#	define OS_DebugLogPrintfFormat(...) ((void)0)
#endif

struct OS_Capabilities
{
	bool has_mouse;
	bool has_keyboard;
	bool has_gamepad;
	bool has_audio;
	bool has_gestures;
	bool is_mobile_like;
	bool has_multiple_audio_devices;
	
	struct
	{
		int32 virtual_x;
		int32 virtual_y;
		int32 virtual_width;
		int32 virtual_height;
		int32 scaling_percentage; // default: 100 (which means 100%)
		struct
		{
			int32 num;
			int32 den;
		} refresh_rate;
	} monitors[32];
}
typedef OS_Capabilities;

API OS_Capabilities OS_GetSystemCapabilities(void);

struct OS_SystemTime
{
	int32 year;
	int32 month; // 1..12
	int32 week_day; // 0..6, where 0 = Sunday
	int32 day; // 1..31
	int32 hours; // 0..23
	int32 minutes; // 0..59
	int32 seconds; // 0..59
	int32 milliseconds; // 0..999
}
typedef OS_SystemTime;

API OS_SystemTime OS_GetSystemTime(void);

//------------------------------------------------------------------------
// Audio
struct OS_AudioProcArgs
{
	void* user_data;
	int16* samples;
	int32 sample_count;
	int32 sample_rate;
	int32 channel_count;
}
typedef OS_AudioProcArgs;

typedef void OS_AudioProc(OS_AudioProcArgs const* args);

struct OS_AudioDeviceInfo
{
	uint32 id;
	uint8 name[64];
}
typedef OS_AudioDeviceInfo;

struct OS_RetrieveAudioDevicesResult
{
	int32 count;
	uint32 default_device_id;
	OS_AudioDeviceInfo devices[OS_Limits_MaxAudioDeviceCount];
}
typedef OS_RetrieveAudioDevicesResult;

// NOTE(ljre): Pass 0 to device_id to use the default one.
API bool OS_StartAudioProc(uint32 device_id, OS_AudioProc* proc, void* user_data, OS_Error* out_err);
API bool OS_StopAudioProc (OS_Error* out_err);
API OS_RetrieveAudioDevicesResult OS_RetrieveAudioDevices(void);

//------------------------------------------------------------------------
// Threads
struct OS_Thread
{ void* ptr; }
typedef OS_Thread;

typedef int32 OS_ThreadProc(void* arg);

struct OS_ThreadDesc
{
	OS_ThreadProc* proc;
	void* user_data;
	
	String name;
	
	intsize scratch_chunk_size;
	intsize scratch_chunk_count;
}
typedef OS_ThreadDesc;

API OS_Thread OS_CreateThread   (OS_ThreadDesc const* desc);
API bool      OS_CheckThread    (OS_Thread thread);
API int32     OS_JoinThread     (OS_Thread thread);
API void      OS_DestroyThread  (OS_Thread thread);

//------------------------------------------------------------------------
// Window
struct OS_Window
{ void* ptr; }
typedef OS_Window;

struct OS_WindowDesc
{
	bool fullscreen;
	bool resizeable;
	
	int32 x, y;
	int32 width, height;
	String title;
}
typedef OS_WindowDesc;

enum OS_MouseLockKind
{
	OS_MouseLockKind_None = 0,

	OS_MouseLockKind_Hide,
	OS_MouseLockKind_HideAndClip,
	OS_MouseLockKind_LockInWindowRegion,
}
typedef OS_MouseLockKind;

API OS_Window OS_CreateWindow(OS_WindowDesc const* desc);
API void OS_DestroyWindow    (OS_Window window);
API void OS_GetWindowUserSize(OS_Window window, int32* out_width, int32* out_height);
API void OS_SetWindowMouseLock(OS_Window window, OS_MouseLockKind kind);
API OS_MouseState OS_GetWindowMouseState(OS_Window window);

enum OS_EventKind
{
	OS_EventKind_Null = 0,
	OS_EventKind_InternalEvent,
	
	OS_EventKind_WindowTyping,
	OS_EventKind_WindowMove,
	OS_EventKind_WindowResize,
	OS_EventKind_WindowClose,
	OS_EventKind_WindowGotFocus,
	OS_EventKind_WindowLostFocus,
	OS_EventKind_WindowMouseWheel,
}
typedef OS_EventKind;

struct OS_Event
{
	OS_EventKind kind;
	OS_Window window_handle; // NULL when not OS_EventKind_Window*
	
	union
	{
		struct { uint32 codepoint; bool is_repeat; } window_typing;
		struct { int32 x, y; } window_move;
		struct { int32 total_width, total_height; int32 user_width, user_height; } window_resize;
		struct { int32 delta; } window_mouse_wheel;
	};
}
typedef OS_Event;

API OS_Event* OS_PollEvents(bool wait, Arena* output_arena, intsize* out_event_count);

//------------------------------------------------------------------------
// Polling Input
API void OS_PollGamepadStates(OS_GamepadStates* states, bool accumulate);
API void OS_PollKeyboardState(OS_KeyboardState* state, bool accumulate);
API void OS_PollMouseState(OS_MouseState* state, bool accumulate, OS_Window optional_window);

enum
{
	OS_GamepadMap_Button_A = 1,
	OS_GamepadMap_Button_B = 2,
	OS_GamepadMap_Button_X = 3,
	OS_GamepadMap_Button_Y = 4,
	OS_GamepadMap_Button_Left = 5,
	OS_GamepadMap_Button_Right = 6,
	OS_GamepadMap_Button_Up = 7,
	OS_GamepadMap_Button_Down = 8,
	OS_GamepadMap_Button_LeftShoulder = 9, // LB
	OS_GamepadMap_Button_RightShoulder = 10, // RB
	OS_GamepadMap_Button_LeftStick = 11, // LS
	OS_GamepadMap_Button_RightStick = 12, // RS
	OS_GamepadMap_Button_Start = 13,
	OS_GamepadMap_Button_Back = 14,
	
	OS_GamepadMap_Pressure_LeftTrigger = 15, // LT
	OS_GamepadMap_Pressure_RightTrigger = 16, // RT
	
	OS_GamepadMap_Axis_LeftX = 17,
	OS_GamepadMap_Axis_LeftY = 18,
	OS_GamepadMap_Axis_RightX = 19,
	OS_GamepadMap_Axis_RightY = 20,
	
	// The lower bits of this enum is one of these entries specified above.
	// The higher bits are used to store extra information about the object.
	//
	// Higher Bits: 0b7654_3210
	// 7 = If the axis should be inverted
	// 6 = If the input axis should be limited to 0..MAX
	// 5 = If the input axis should be limited to MIN..0
	// 4 = If the output axis should be limited to 0..MAX
	// 3 = If the output axis should be limited to MIN..0
	//
	// Bits 6 & 5 should never be set at the same time
	// Bits 4 & 3 should never be set at the same time
	
	OS_GamepadMap__Count = 21,
};

struct OS_GamepadMapping
{
	uint8 guid[32];
	uint16 buttons[32];
	uint16 axes[16];
	uint16 povs[8][4];
}
typedef OS_GamepadMapping;

API void OS_SetGamepadMappings(OS_GamepadMapping const* mappings, intsize mapping_count);

//------------------------------------------------------------------------
// Memory & file stuff
API void* OS_HeapAlloc  (intz size);
API void* OS_HeapRealloc(void* ptr, intz size);
API void  OS_HeapFree   (void* ptr);

enum
{
	// Only valid for OS_VirtualAlloc
	OS_VirtualFlags_ReserveOnly = 0x0001,
	
	// Only valid for OS_VirtualProtect
	OS_VirtualFlags_Read    = 0x0002,
	OS_VirtualFlags_Write   = 0x0004,
	OS_VirtualFlags_Execute = 0x0008,
};

API void* OS_VirtualAlloc  (void* address, intz size, uint32 flags);
API bool  OS_VirtualCommit (void* address, intz size);
API bool  OS_VirtualProtect(void* address, intz size, uint32 flags);
API bool  OS_VirtualFree   (void* address, intz size);
API bool  OS_ArenaCommitMemoryProc(Arena* arena, intz needed_size);
API Arena OS_VirtualAllocArena(intz total_reserved_size);

API Allocator OS_HeapAllocator(void);

struct OS_FileInfo
{
	bool exists;
	bool read_only;
	uint64 created_at;
	uint64 modified_at;
	uint64 size;
}
typedef OS_FileInfo;

API bool        OS_ReadEntireFile (String path, Arena* output_arena, void** out_data, intz* out_size, OS_Error* out_err);
API bool        OS_WriteEntireFile(String path, void const* data, intz size, OS_Error* out_err);
API OS_FileInfo OS_GetFileInfoFromPath(String path, OS_Error* out_err);
API bool        OS_CopyFile       (String from, String to, OS_Error* out_err);
API bool        OS_DeleteFile     (String path, OS_Error* out_err);
API bool        OS_MakeDirectory  (String path, OS_Error* out_err);

struct OS_Library
{ void* ptr; }
typedef OS_Library;
API OS_Library OS_LoadLibrary(String name, OS_Error* out_err);
API void*      OS_LoadSymbol(OS_Library library, char const* symbol_name);
API void       OS_UnloadLibrary(OS_Library library);

enum OS_OpenFileFlags
{
	OS_OpenFileFlags_Read = 0x01,
	OS_OpenFileFlags_Write = 0x02,
	OS_OpenFileFlags_FailIfAlreadyExists = 0x04,
	OS_OpenFileFlags_FailIfDoesntExist = 0x08,
}
typedef OS_OpenFileFlags;

struct OS_File
{ void* ptr; }
typedef OS_File;
API OS_File     OS_OpenFile   (String path, OS_OpenFileFlags flags, OS_Error* out_err);
API OS_FileInfo OS_GetFileInfo(OS_File file, OS_Error* out_err);
API intsize     OS_WriteFile  (OS_File file, intsize size, void const* buffer, OS_Error* out_err);
API intsize     OS_ReadFile   (OS_File file, intsize size, void* buffer, OS_Error* out_err);
API int64       OS_TellFile   (OS_File file, OS_Error* out_err);
API void        OS_SeekFile   (OS_File file, int64 offset, bool relative, OS_Error* out_err);
// NOTE(ljre): A negative offset is relative to the end of the file. So -1 would be *after* the very last
//             byte currently written in the file.
API intsize     OS_WriteFileAt(OS_File file, int64 offset, intsize size, void const* buffer, OS_Error* out_err);
API intsize     OS_ReadFileAt (OS_File file, int64 offset, intsize size, void* buffer, OS_Error* out_err);
API void        OS_CloseFile  (OS_File file);

API OS_File     OS_GetStdout  (void);
API OS_File     OS_GetStdin   (void);
API OS_File     OS_GetStderr  (void);

API OS_File     OS_CreatePipe (bool inheritable, OS_Error* out_err);
API bool        OS_IsFilePipe (OS_File file);

struct OS_FileMapping
{ void* ptr; }
typedef OS_FileMapping;
API OS_FileMapping OS_MapFileForReading(OS_File file, void const** out_buffer, intz* out_size, OS_Error* out_err);
API OS_FileMapping OS_MapFile          (OS_File file, int64 offset, int64 size, uint32 virtual_flags, void** out_buffer, intz* out_size, OS_Error* out_err);
API void           OS_UnmapFile        (OS_FileMapping mapping);

//------------------------------------------------------------------------
// Process stuff
struct OS_ChildProcessDesc
{
	OS_File stdin_file;
	OS_File stdout_file;
	OS_File stderr_file;
}
typedef OS_ChildProcessDesc;

struct OS_ChildProcess
{ void* ptr; }
typedef OS_ChildProcess;
API int32           OS_Exec(String cmd, OS_Error* out_err);
API OS_ChildProcess OS_ExecAsync(String cmd, OS_ChildProcessDesc const* child_desc, OS_Error* out_err);
API OS_ChildProcess OS_CreateChildProcess(String executable, String args, OS_ChildProcessDesc const* child_desc, OS_Error* out_err);
API void OS_WaitChildProcess         (OS_ChildProcess child, OS_Error* out_err);
API bool OS_GetReturnCodeChildProcess(OS_ChildProcess child, int32* out_code, OS_Error* out_err);
API void OS_KillChildProcess         (OS_ChildProcess child, OS_Error* out_err);
API void OS_DestroyChildProcess      (OS_ChildProcess child);

//------------------------------------------------------------------------
// Threading stuff
struct OS_RWLock
{ void* ptr; }
typedef OS_RWLock;
API void OS_InitRWLock      (OS_RWLock* lock);
API void OS_LockShared      (OS_RWLock* lock);
API void OS_LockExclusive   (OS_RWLock* lock);
API bool OS_TryLockShared   (OS_RWLock* lock);
API bool OS_TryLockExclusive(OS_RWLock* lock);
API void OS_UnlockShared    (OS_RWLock* lock);
API void OS_UnlockExclusive (OS_RWLock* lock);
API void OS_DeinitRWLock    (OS_RWLock* lock);

struct OS_Semaphore
{ void* ptr; }
typedef OS_Semaphore;
API void OS_InitSemaphore   (OS_Semaphore* sem, int32 max_count);
API bool OS_WaitForSemaphore(OS_Semaphore* sem);
API void OS_SignalSemaphore (OS_Semaphore* sem, int32 count);
API void OS_DeinitSemaphore (OS_Semaphore* sem);

struct OS_ConditionVariable
{ void* ptr; }
typedef OS_ConditionVariable;
API void OS_InitConditionVariable     (OS_ConditionVariable* var);
API bool OS_WaitConditionVariable     (OS_ConditionVariable* var, OS_RWLock* lock, bool is_lock_shared, int32 ms);
API void OS_SignalConditionVariable   (OS_ConditionVariable* var);
API void OS_SignalAllConditionVariable(OS_ConditionVariable* var);
API void OS_DeinitConditionVariable   (OS_ConditionVariable* var);

#endif //API_OS_H
