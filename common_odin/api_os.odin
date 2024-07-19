package common_odin

OS_Limits_MaxGamepadCount :: 16
OS_Limits_MaxGestureCount :: 5
OS_Limits_MaxBufferedInput :: 256
OS_Limits_MaxCodepointsPerFrame :: 64
OS_Limits_MaxAudioDeviceCount :: 32
OS_Error :: struct
{
	ok: bool,
	code: u32,
	what: string,
}
OS_ButtonState :: bit_field u8
{
	changes: u8 | 7,
	is_down: u8 | 1,
}
OS_KeyboardKey :: enum i32{
	Any = 0,
	Escape,
	Up,
	Down,
	Left,
	Right,
	LeftControl,
	RightControl,
	Control,
	LeftShift,
	RightShift,
	Shift,
	LeftAlt,
	RightAlt,
	Alt,
	Tab,
	Enter,
	Backspace,
	PageUp,
	PageDown,
	End,
	Home,
	Space = ' ',
	_0 = '0',
	_1,
	_2,
	_3,
	_4,
	_5,
	_6,
	_7,
	_8,
	_9,
	A = 'A',
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
	Numpad0,
	Numpad1,
	Numpad2,
	Numpad3,
	Numpad4,
	Numpad5,
	Numpad6,
	Numpad7,
	Numpad8,
	Numpad9,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	F13,
	F14,
	Count = 256,
}
OS_KeyboardState :: struct
{
	buffered_count: i32,
	buttons: [256]OS_ButtonState,
	buffered: [OS_Limits_MaxBufferedInput]u8,
}
OS_MouseButton :: enum i32{
	Left,
	Right,
	Middle,
	Other0,
	Other1,
	Other2,
	Count = 6,
}
OS_MouseState :: struct
{
	pos: [2]f32,
	old_pos: [2]f32,
	scroll: i32,
	buttons: [6]OS_ButtonState,
}
OS_GamepadButton :: enum i32{
	A,
	B,
	X,
	Y,
	Left,
	Right,
	Up,
	Down,
	LB,
	RB,
	RS,
	LS,
	Start,
	Back,
	Count = 14,
}
OS_GamepadState :: struct
{
	left: [2]f32,
	right: [2]f32,
	lt: f32,
	rt: f32,
	buttons: [14]OS_ButtonState,
}
OS_GamepadStates :: struct
{
	connected_bitset: u32,
	gamepads: [OS_Limits_MaxGamepadCount]OS_GamepadState,
}
OS_GestureState :: struct
{
	active: bool,
	released: bool,
	start_pos: [2]f32,
	current_pos: [2]f32,
	delta: [2]f32,
}
OS_Init_Audio :: 0x0001
OS_Init_WindowAndGraphics :: 0x0002
OS_Init_Sleep :: 0x0004
OS_Init_Gamepad :: 0x0008
OS_Init_MediaFoundation :: 0x0010
OS_Capabilities :: struct
{
	has_mouse: bool,
	has_keyboard: bool,
	has_gamepad: bool,
	has_audio: bool,
	has_gestures: bool,
	is_mobile_like: bool,
	has_multiple_audio_devices: bool,
	monitors: [32]struct
	{
		virtual_x: i32,
		virtual_y: i32,
		virtual_width: i32,
		virtual_height: i32,
		scaling_percentage: i32,
		refresh_rate: struct
		{
			num: i32,
			den: i32,
		},
	},
}
OS_SystemTime :: struct
{
	year: i32,
	month: i32,
	week_day: i32,
	day: i32,
	hours: i32,
	minutes: i32,
	seconds: i32,
	milliseconds: i32,
}
OS_AudioProcArgs :: struct
{
	user_data: rawptr,
	samples: ^i16,
	sample_count: i32,
	sample_rate: i32,
	channel_count: i32,
}
OS_AudioProc :: #type proc "c"(#by_ptr args: OS_AudioProcArgs)
OS_AudioDeviceInfo :: struct
{
	id: u32,
	name: [64]u8,
}
OS_RetrieveAudioDevicesResult :: struct
{
	count: i32,
	default_device_id: u32,
	devices: [OS_Limits_MaxAudioDeviceCount]OS_AudioDeviceInfo,
}
OS_Thread :: struct
{
	ptr: rawptr,
}
OS_ThreadProc :: #type proc "c"(arg: rawptr) -> i32
OS_ThreadDesc :: struct
{
	proc_: ^OS_ThreadProc,
	user_data: rawptr,
	name: string,
	scratch_chunk_size: int,
	scratch_chunk_count: int,
}
OS_Window :: struct
{
	ptr: rawptr,
}
OS_WindowDesc :: struct
{
	fullscreen: bool,
	resizeable: bool,
	x: i32,
	y: i32,
	width: i32,
	height: i32,
	title: string,
}
OS_MouseLockKind :: enum i32{
	None = 0,
	Hide,
	HideAndClip,
	LockInWindowRegion,
}
OS_EventKind :: enum i32{
	Null = 0,
	InternalEvent,
	WindowTyping,
	WindowMove,
	WindowResize,
	WindowClose,
	WindowGotFocus,
	WindowLostFocus,
	WindowMouseWheel,
}
OS_Event :: struct
{
	kind: OS_EventKind,
	window_handle: OS_Window,
	using _: struct #raw_union
	{
		window_typing: struct
		{
			codepoint: u32,
			is_repeat: bool,
		},
		window_move: struct
		{
			x: i32,
			y: i32,
		},
		window_resize: struct
		{
			total_width: i32,
			total_height: i32,
			user_width: i32,
			user_height: i32,
		},
		window_mouse_wheel: struct
		{
			delta: i32,
		},
	},
}
OS_GamepadMap_Button_A :: 1
OS_GamepadMap_Button_B :: 2
OS_GamepadMap_Button_X :: 3
OS_GamepadMap_Button_Y :: 4
OS_GamepadMap_Button_Left :: 5
OS_GamepadMap_Button_Right :: 6
OS_GamepadMap_Button_Up :: 7
OS_GamepadMap_Button_Down :: 8
OS_GamepadMap_Button_LeftShoulder :: 9
OS_GamepadMap_Button_RightShoulder :: 10
OS_GamepadMap_Button_LeftStick :: 11
OS_GamepadMap_Button_RightStick :: 12
OS_GamepadMap_Button_Start :: 13
OS_GamepadMap_Button_Back :: 14
OS_GamepadMap_Pressure_LeftTrigger :: 15
OS_GamepadMap_Pressure_RightTrigger :: 16
OS_GamepadMap_Axis_LeftX :: 17
OS_GamepadMap_Axis_LeftY :: 18
OS_GamepadMap_Axis_RightX :: 19
OS_GamepadMap_Axis_RightY :: 20
OS_GamepadMap__Count :: 21
OS_GamepadMapping :: struct
{
	guid: [32]u8,
	buttons: [32]u16,
	axes: [16]u16,
	povs: [8][4]u16,
}
OS_VirtualFlags_ReserveOnly :: 0x0001
OS_VirtualFlags_ReadOnly :: 0x0002
OS_VirtualFlags_ExecuteOnly :: 0x0004
OS_FileInfo :: struct
{
	exists: bool,
	read_only: bool,
	created_at: u64,
	modified_at: u64,
	size: u64,
}
OS_Library :: struct
{
	ptr: rawptr,
}
OS_OpenFileFlags :: enum i32{
	Read = 0x01,
	Write = 0x02,
	FailIfAlreadyExists = 0x04,
	FailIfDoesntExist = 0x08,
}
OS_File :: struct
{
	ptr: rawptr,
}
OS_FileMapping :: struct
{
	ptr: rawptr,
}
OS_ChildProcessDesc :: struct
{
	stdin_file: OS_File,
	stdout_file: OS_File,
	stderr_file: OS_File,
}
OS_ChildProcess :: struct
{
	ptr: rawptr,
}
OS_RWLock :: struct
{
	ptr: rawptr,
}
OS_Semaphore :: struct
{
	ptr: rawptr,
}
OS_ConditionVariable :: struct
{
	ptr: rawptr,
}
@(default_calling_convention="c")
foreign {
	OS_Init :: proc "c"(systems: i32) ---;
	OS_InitConsoleApp :: proc "c"() ---;
	OS_InitGraphicsApp :: proc "c"() ---;
	OS_Sleep :: proc "c"(ms: u64) ---;
	OS_SleepPrecise :: proc "c"(ns: u64) ---;
	OS_MessageBox :: proc "c"(title: string, message: string) ---;
	OS_ScratchArena :: proc "c"(conflict: [^]^Arena, conflict_count: int) -> ^Arena ---;
	OS_PosixTime :: proc "c"() -> u64 ---;
	OS_Tick :: proc "c"() -> u64 ---;
	OS_TickRate :: proc "c"() -> u64 ---;
	OS_TimeInSeconds :: proc "c"() -> f64 ---;
	OS_LogOut :: proc "c"(fmt: cstring, #c_vararg varargs: ..any) ---;
	OS_LogErr :: proc "c"(fmt: cstring, #c_vararg varargs: ..any) ---;
	OS_ExitWithErrorMessage :: proc "c"(fmt: cstring, #c_vararg varargs: ..any) ---;
	OS_DebugMessageBox :: proc "c"(fmt: cstring, #c_vararg varargs: ..any) ---;
	OS_DebugLog :: proc "c"(fmt: cstring, #c_vararg varargs: ..any) ---;
	OS_DebugLogPrintfFormat :: proc "c"(fmt: cstring, #c_vararg varargs: ..any) -> i32 ---;
	OS_GetSystemCapabilities :: proc "c"() -> OS_Capabilities ---;
	OS_GetSystemTime :: proc "c"() -> OS_SystemTime ---;
	OS_StartAudioProc :: proc "c"(device_id: u32, proc_: ^OS_AudioProc, user_data: rawptr, out_err: ^OS_Error) -> bool ---;
	OS_StopAudioProc :: proc "c"(out_err: ^OS_Error) -> bool ---;
	OS_RetrieveAudioDevices :: proc "c"() -> OS_RetrieveAudioDevicesResult ---;
	OS_CreateThread :: proc "c"(#by_ptr desc: OS_ThreadDesc) -> OS_Thread ---;
	OS_CheckThread :: proc "c"(thread: OS_Thread) -> bool ---;
	OS_JoinThread :: proc "c"(thread: OS_Thread) -> i32 ---;
	OS_DestroyThread :: proc "c"(thread: OS_Thread) ---;
	OS_CreateWindow :: proc "c"(#by_ptr desc: OS_WindowDesc) -> OS_Window ---;
	OS_DestroyWindow :: proc "c"(window: OS_Window) ---;
	OS_GetWindowUserSize :: proc "c"(window: OS_Window, out_width: ^i32, out_height: ^i32) ---;
	OS_SetWindowMouseLock :: proc "c"(window: OS_Window, kind: OS_MouseLockKind) ---;
	OS_GetWindowMouseState :: proc "c"(window: OS_Window) -> OS_MouseState ---;
	OS_PollEvents :: proc "c"(wait: bool, output_arena: ^Arena, out_event_count: ^int) -> ^OS_Event ---;
	OS_PollGamepadStates :: proc "c"(states: ^OS_GamepadStates, accumulate: bool) ---;
	OS_PollKeyboardState :: proc "c"(state: ^OS_KeyboardState, accumulate: bool) ---;
	OS_PollMouseState :: proc "c"(state: ^OS_MouseState, accumulate: bool, optional_window: OS_Window) ---;
	OS_SetGamepadMappings :: proc "c"(#by_ptr mappings: OS_GamepadMapping, mapping_count: int) ---;
	OS_HeapAlloc :: proc "c"(size: uint) -> rawptr ---;
	OS_HeapRealloc :: proc "c"(ptr: rawptr, size: uint) -> rawptr ---;
	OS_HeapFree :: proc "c"(ptr: rawptr) ---;
	OS_VirtualAlloc :: proc "c"(address: rawptr, size: uint, flags: u32) -> rawptr ---;
	OS_VirtualCommit :: proc "c"(address: rawptr, size: uint) -> bool ---;
	OS_VirtualProtect :: proc "c"(address: rawptr, size: uint, flags: u32) -> bool ---;
	OS_VirtualFree :: proc "c"(address: rawptr, size: uint) -> bool ---;
	OS_ArenaCommitMemoryProc :: proc "c"(arena: ^Arena, needed_size: uint) -> bool ---;
	OS_VirtualAllocArena :: proc "c"(total_reserved_size: uint) -> Arena ---;
	OS_ReadEntireFile :: proc "c"(path: string, output_arena: ^Arena, out_data: ^rawptr, out_size: ^uint, out_err: ^OS_Error) -> bool ---;
	OS_WriteEntireFile :: proc "c"(path: string, data: rawptr, size: uint, out_err: ^OS_Error) -> bool ---;
	OS_GetFileInfoFromPath :: proc "c"(path: string, out_err: ^OS_Error) -> OS_FileInfo ---;
	OS_CopyFile :: proc "c"(from: string, to: string, out_err: ^OS_Error) -> bool ---;
	OS_DeleteFile :: proc "c"(path: string, out_err: ^OS_Error) -> bool ---;
	OS_MakeDirectory :: proc "c"(path: string, out_err: ^OS_Error) -> bool ---;
	OS_LoadLibrary :: proc "c"(name: string, out_err: ^OS_Error) -> OS_Library ---;
	OS_LoadSymbol :: proc "c"(library: OS_Library, symbol_name: cstring) -> rawptr ---;
	OS_UnloadLibrary :: proc "c"(library: OS_Library) ---;
	OS_OpenFile :: proc "c"(path: string, flags: OS_OpenFileFlags, out_err: ^OS_Error) -> OS_File ---;
	OS_GetFileInfo :: proc "c"(file: OS_File, out_err: ^OS_Error) -> OS_FileInfo ---;
	OS_WriteFile :: proc "c"(file: OS_File, size: int, buffer: rawptr, out_err: ^OS_Error) -> int ---;
	OS_ReadFile :: proc "c"(file: OS_File, size: int, buffer: rawptr, out_err: ^OS_Error) -> int ---;
	OS_TellFile :: proc "c"(file: OS_File, out_err: ^OS_Error) -> i64 ---;
	OS_SeekFile :: proc "c"(file: OS_File, offset: i64, relative: bool, out_err: ^OS_Error) ---;
	OS_WriteFileAt :: proc "c"(file: OS_File, offset: i64, size: int, buffer: rawptr, out_err: ^OS_Error) -> int ---;
	OS_ReadFileAt :: proc "c"(file: OS_File, offset: i64, size: int, buffer: rawptr, out_err: ^OS_Error) -> int ---;
	OS_CloseFile :: proc "c"(file: OS_File) ---;
	OS_GetStdout :: proc "c"() -> OS_File ---;
	OS_GetStdin :: proc "c"() -> OS_File ---;
	OS_GetStderr :: proc "c"() -> OS_File ---;
	OS_CreatePipe :: proc "c"(inheritable: bool, out_err: ^OS_Error) -> OS_File ---;
	OS_IsFilePipe :: proc "c"(file: OS_File) -> bool ---;
	OS_MapFileForReading :: proc "c"(file: OS_File, out_buffer: ^rawptr, out_size: ^uint, out_err: ^OS_Error) -> OS_FileMapping ---;
	OS_UnmapFile :: proc "c"(mapping: OS_FileMapping) ---;
	OS_Exec :: proc "c"(cmd: string, out_err: ^OS_Error) -> i32 ---;
	OS_ExecAsync :: proc "c"(cmd: string, #by_ptr child_desc: OS_ChildProcessDesc, out_err: ^OS_Error) -> OS_ChildProcess ---;
	OS_CreateChildProcess :: proc "c"(executable: string, args: string, #by_ptr child_desc: OS_ChildProcessDesc, out_err: ^OS_Error) -> OS_ChildProcess ---;
	OS_WaitChildProcess :: proc "c"(child: OS_ChildProcess, out_err: ^OS_Error) ---;
	OS_GetReturnCodeChildProcess :: proc "c"(child: OS_ChildProcess, out_code: ^i32, out_err: ^OS_Error) -> bool ---;
	OS_KillChildProcess :: proc "c"(child: OS_ChildProcess, out_err: ^OS_Error) ---;
	OS_DestroyChildProcess :: proc "c"(child: OS_ChildProcess) ---;
	OS_InitRWLock :: proc "c"(lock: ^OS_RWLock) ---;
	OS_LockShared :: proc "c"(lock: ^OS_RWLock) ---;
	OS_LockExclusive :: proc "c"(lock: ^OS_RWLock) ---;
	OS_TryLockShared :: proc "c"(lock: ^OS_RWLock) -> bool ---;
	OS_TryLockExclusive :: proc "c"(lock: ^OS_RWLock) -> bool ---;
	OS_UnlockShared :: proc "c"(lock: ^OS_RWLock) ---;
	OS_UnlockExclusive :: proc "c"(lock: ^OS_RWLock) ---;
	OS_DeinitRWLock :: proc "c"(lock: ^OS_RWLock) ---;
	OS_InitSemaphore :: proc "c"(sem: ^OS_Semaphore, max_count: i32) ---;
	OS_WaitForSemaphore :: proc "c"(sem: ^OS_Semaphore) -> bool ---;
	OS_SignalSemaphore :: proc "c"(sem: ^OS_Semaphore, count: i32) ---;
	OS_DeinitSemaphore :: proc "c"(sem: ^OS_Semaphore) ---;
	OS_InitConditionVariable :: proc "c"(var: ^OS_ConditionVariable) ---;
	OS_WaitConditionVariable :: proc "c"(var: ^OS_ConditionVariable, lock: ^OS_RWLock, is_lock_shared: bool, ms: i32) -> bool ---;
	OS_SignalConditionVariable :: proc "c"(var: ^OS_ConditionVariable) ---;
	OS_SignalAllConditionVariable :: proc "c"(var: ^OS_ConditionVariable) ---;
	OS_DeinitConditionVariable :: proc "c"(var: ^OS_ConditionVariable) ---;
}
