#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include "api_os.h"

#define __USE_MISC
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>

struct
{
	uint8* memory;
	intsize chunk_size;
	intsize chunk_count;
}
static thread_local g_threadlocal;

static void
SetupThreadScratchMemory_(intsize chunk_size, intsize chunk_count)
{
	Trace();
	SafeAssert(chunk_size && chunk_count && IsPowerOf2(chunk_size));
	
	uint8* memory = mmap(NULL, chunk_size*chunk_count, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	SafeAssert(memory);
	for (intsize i = 0; i < chunk_size; ++i)
		ArenaFromMemory(memory + chunk_size*i, chunk_size);
	
	g_threadlocal.memory = memory;
	g_threadlocal.chunk_size = chunk_size;
	g_threadlocal.chunk_count = chunk_count;
}

static bool
FillOsErr_(OS_Error* out_err, int no)
{
	OS_Error err = {
		.ok = (no == 0),
		.code = (uint32)no,
		.what = StrInit("TODO: error strings on android"),
	};
	
	*out_err = err;
}

static uint64
TimespecToPosix_(struct timespec ts)
{
	uint64 result = 0;
	result += t.tv_sec * 1000000000;
	result += t.tv_nsec;
	return result;
}

static void*
ThreadProc_(void* arg)
{
	OS_ThreadDesc* desc2 = arg;
	OS_ThreadDesc desc = *desc2;
	OS_HeapFree(desc2);
	
	if (!desc.scratch_chunk_size)
		desc.scratch_chunk_size = 16<<20;
	if (!desc.scratch_chunk_count)
		desc.scratch_chunk_count = 3;
	
	SetupThreadScratchMemory_(desc.scratch_chunk_size, desc.scratch_chunk_count);
	int32 result = desc.proc(desc.user_data);
	
	munmap(g_threadlocal.memory, g_threadlocal.chunk_size*g_threadlocal.chunk_count);
	return (void*)result;
}

int
main(int argc, char* argv[])
{
	Trace();
	SetupThreadScratchMemory_(16<<20, 3);
	
	int32 result = EntryPoint(argc, (char const* const*)argv);
	
	exit(result);
	return INT32_MAX;
}

//~ Implement OS API
API void
OS_Init(int32 systems)
{
	Trace();
	
	if (systems & OS_Init_WindowAndGraphics)
	{
		
	}
	
	if (systems & OS_Init_Sleep)
	{
		
	}
	
	if (systems & OS_Init_Audio)
	{
		
	}
	
	if (systems & OS_Init_Audio)
	{
		
	}
}

API void
OS_Sleep(uint64 ms)
{
	Trace();
	struct timespec t = {
		.tv_sec = ms / 1000,
		.tv_nsec = (ms % 1000) * 1000000,
	};
	struct timespec rem;
	nanosleep(t, &rem);
}

API void
OS_SleepPrecise(uint64 us)
{
	Trace();
	struct timespec t = {
		.tv_sec = us / 1000000000,
		.tv_nsec = us,
	};
	struct timespec rem;
	nanosleep(t, &rem);
}

API void
OS_ExitWithErrorMessage(char const* fmt, ...)
{
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	va_list args;
	va_start(args, fmt);
	
	for ArenaTempScope(scratch_arena)
	{
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		write(2, str.data, str.size);
	}
	
	va_end(args);
	Unreachable();
}

API void
OS_MessageBox(String title, String message)
{
	
}

API Arena*
OS_ScratchArena(Arena* const* conflict, intsize conflict_count)
{
	Trace();
	if (!conflict_count)
		return (Arena*)g_threadlocal.memory;
	if (conflict_count >= g_threadlocal.chunk_count)
		SafeAssert(false);
	
	for (intsize arena_index = 0; arena_index < g_threadlocal.chunk_count; ++arena_index)
	{
		Arena* arena = (Arena*)(g_threadlocal.memory + g_threadlocal.chunk_size*arena_index);
		bool ok = true;
		
		for (intsize i = 0; i < conflict_count; ++i)
		{
			if (arena == conflict[i])
			{
				ok = false;
				break;
			}
		}
		
		if (ok)
			return arena;
	}
	
	SafeAssert(false);
	return NULL;
}

API uint64
OS_PosixTime(void)
{
	time_t result = time(NULL);
	if (result == -1)
		result = 0;
	
	return (uint64)result;
}

API uint64
OS_Tick(void)
{
	uint64 result = 0;
	
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) == 0)
		result = TimespecToPosix_(t);
	
	return result;
}

API uint64
OS_TickRate(void)
{
	return 1000000000;
}

API float64
OS_GetTimeInSeconds(void)
{
	struct timespec t;
	if (0 != clock_gettime(CLOCK_MONOTONIC, &t))
		return 0.0;
	
	t.tv_sec -= g_android.started_time.tv_sec;
	t.tv_nsec -= g_android.started_time.tv_nsec;
	
	if (t.tv_nsec < 0)
	{
		t.tv_nsec += 1000000000L;
		t.tv_sec -= 1;
	}
	
	return (float64)t.tv_nsec / 1000000000.0 + (float64)t.tv_sec;
}

API void
OS_LogOut(char const* fmt, ...)
{
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	va_list args;
	va_start(args, fmt);
	
	for ArenaTempScope(scratch_arena)
	{
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		write(1, str.data, str.size);
	}
	
	va_end(args);
}

API void
OS_LogErr(char const* fmt, ...)
{
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	va_list args;
	va_start(args, fmt);
	
	for ArenaTempScope(scratch_arena)
	{
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		write(2, str.data, str.size);
	}
	
	va_end(args);
}

API bool
OS_IsValidImpl(void* handle_ptr)
{
	return handle_ptr != NULL;
}

//~ Audio
API void
OS_StartAudioProc(uint32 device_id, OS_AudioProc* proc, void* user_data)
{
	Trace();
	
}

API void
OS_StopAudioProc(void)
{
	Trace();
	
}

API OS_RetrieveAudioDevicesResult
OS_RetrieveAudioDevices(void)
{
	Trace();
	
	return (OS_RetrieveAudioDevicesResult) { 0 };
}

//~ Threads
API OS_Thread
OS_CreateThread(OS_ThreadDesc const* desc)
{
	Trace();
	OS_Thread result = { 0 };
	OS_ThreadDesc* desc2 = OS_HeapAlloc(sizeof(OS_ThreadDesc));
	*desc2 = *desc;
	
	pthread_t impl;
	int errcode = pthread_create(&impl, NULL, ThreadProc_, desc2);
	if (errcode == 0)
		result.ptr = (void*)impl;
	else
		OS_HeapFree(desc2);
	
	return result;
}

API bool
OS_CheckThread(OS_Thread thread)
{
	Trace();
	bool result = false;
	pthread_t impl = (pthread_t)thread.ptr;
	pthread_attr_t attr;
	
	int errcode = pthread_getattr_np(impl, &attr);
	if (errcode == 0)
	{
		int detachstate;
		errcode = pthread_attr_getdetachstate(&attr, &detachstate);
		if (errcode == 0)
			result = (detachstate == PTHREAD_CREATE_DETACHED);
	}
	
	return result;
}

API int32
OS_JoinThread(OS_Thread thread)
{
	Trace();
	int32 result = INT32_MIN;
	pthread_t impl = (pthread_t)thread.ptr;
	void* retcode;
	
	int errcode = pthread_join(impl);
	if (errcode == 0)
		result = (int32)(intptr)retcode;
	
	return result;
}

API void
OS_DestroyThread(OS_Thread thread)
{
	Trace();
	pthread_t impl = (pthread_t)thread.ptr;
	pthread_detach(impl);
}

//~ Window
API OS_Window
OS_CreateWindow(OS_WindowDesc const* desc)
{
	
	return (OS_Window) { 0 };
}

API void
OS_UpdateWindow(OS_Window window, OS_WindowDesc const* desc)
{
	
}

API void
OS_DestroyWindow(OS_Window window)
{
	
}

API void
OS_PollWindowEvents(OS_Window window, bool wait)
{
	Trace();
	
	g_android.state.mouse.old_pos[0] = g_android.state.mouse.pos[0];
	g_android.state.mouse.old_pos[1] = g_android.state.mouse.pos[1];
	for (intsize i = 0; i < ArrayLength(g_android.state.mouse.buttons); ++i)
		g_android.state.mouse.buttons[i].changes = 0;
	
	int32 ident;
	int32 events;
	struct android_poll_source* source;
	struct android_app* state = g_android.app_state;
	
	// NOTE(ljre): If app is not in focus, timeout is -1 (block until next event)
	while ((ident = ALooper_pollAll(!g_android.has_focus ? -1 : 0, NULL, &events, (void**)&source)) >= 0)
	{
		if (source != NULL)
			source->process(state, source);
		
		if (state->destroyRequested != 0)
		{
			g_android.state.is_terminating = true;
			return;
		}
	}
}

API OS_WindowState const*
OS_GetWindowState(OS_Window window)
{
	return NULL;
}

API OS_GfxCtx const*
OS_GetWindowGfxCtx(OS_Window window)
{
	return NULL;
}

//~ Gamepad
API void
OS_PollGamepadStates(bool accumulate)
{
	
}

API uint32
OS_GetGamepadStates(OS_GamepadState out_gamepads[OS_Limits_MaxGamepadCount])
{
	return 0;
}

API void
OS_PollKeyboardState(OS_KeyboardState* state, bool accumulate)
{
	MemoryZero(state, sizeof(*state));
}

API void
OS_PollMouseState(OS_MouseState* state, bool accumulate, OS_Window optional_window)
{
	MemoryZero(state, sizeof(*state));
}

//~ Memory & File
API void*
OS_HeapAlloc(uintsize size)
{
	Trace();
	return malloc(size);
}

API void*
OS_HeapRealloc(void* ptr, uintsize size)
{
	Trace();
	return realloc(ptr, size);
}

API void
OS_HeapFree(void* ptr)
{
	Trace();
	free(ptr);
}

API void*
OS_VirtualReserve(void* address, uintsize size)
{
	Trace();
	return mmap(address, size, PROT_NONE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
}

API bool
OS_VirtualCommit(void* ptr, uintsize size)
{
	Trace();
	return mprotect(ptr, size, PROT_READ|PROT_WRITE) == 0;
}

API void
OS_VirtualDecommit(void* ptr, uintsize size)
{
	Trace();
	mprotect(ptr, size, PROT_NONE);
}

API void
OS_VirtualRelease(void* ptr, uintsize size)
{
	Trace();
	munmap(ptr, size);
}

API bool
OS_ReadEntireFile(String path, Arena* output_arena, void** out_data, uintsize* out_size, OS_Error* out_err)
{
	return false;
}

API bool
OS_ReadEntireFile(String path, void const* out_data, uintsize size, OS_Error* out_err)
{
	return false;
}

API OS_FileInfo
OS_GetFileInfoFromPath(String path, OS_Error* out_err)
{
	return (OS_FileInfo) { 0 };
}

API bool
OS_CopyFile(String from, String to, OS_Error* out_err)
{
	return false;
}

API bool
OS_DeleteFile(String path, OS_Error* out_err)
{
	return false;
}

API bool
OS_MakeDirectory(String path, OS_Error* out_err)
{
	return false;
}

API void
OS_UnmapFile(OS_MappedFile mapped_file, OS_Error* out_err)
{
	
}

API OS_Library
OS_LoadLibrary(String name, OS_Error* out_err)
{
	Trace();
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
	OS_Library result = { 0 };
	
	for ArenaTempScope(scratch_arena)
	{
		char const* cname = ArenaPushCString(scratch_arena, name);
		void* lib = dlopen(cname, RTLD_LAZY);
		if (lib)
			result.ptr = lib;
	}
	
	FillOsErr_(out_err, 0);
	return result;
}

API void*
OS_LoadSymbol(OS_Library library, char const* symbol_name)
{
	Trace();
	void* lib = library.ptr;
	void* symbol = dlsym(lib, symbol_name);
	
	return symbol;
}

API void
OS_UnloadLibrary(OS_Library library)
{
	Trace();
	void* lib = library.ptr;
	dlclose(lib);
}

API OS_File
OS_OpenFile(String path, OS_OpenFileFlags flags, OS_Error* out_err)
{
	Trace();
	OS_File result = { 0 };
	errno = 0;
	
	int rwflags = flags & (OS_OpenFileFlags_Read | OS_OpenFileFlags_Write);
	int failflags = flags & (OS_OpenFileFlags_FailIfAlreadyExists | OS_OpenFileFlags_FailIfDoesntExist);
	
	char const* mode = "";
	switch (rwflags)
	{
		case 0: Assert(false); break;
		case OS_OpenFileFlags_Read: mode = "rb"; break;
		case OS_OpenFileFlags_Write: mode = "wb"; break;
		case OS_OpenFileFlags_Read | OS_OpenFileFlags_Write: mode = "ab"; break;
	}
	
	for ArenaTempScope(scratch_arena)
	{
		char const* cpath = ArenaPushCString(scratch_arena, path);
		bool ok = true;
		
		if (failflags)
		{
			int exist = (stat(cpath, &(struct stat) { 0 }) == 0);
			errno = 0;
			switch (failflags)
			{
				default: Assert(false); break;
				case OS_OpenFileFlags_FailIfAlreadyExists: ok = !exist; break;
				case OS_OpenFileFlags_FailIfDoesntExist: ok = exist; break;
			}
		}
		
		if (ok)
		{
			FILE* stdfile = fopen(cpath, mode);
			result.ptr = stdfile;
		}
	}
	
	FillOsErr_(out_err, errno);
	return result;
}

API OS_FileInfo
OS_GetFileInfo(OS_File file, OS_Error* out_err)
{
	Trace();
	OS_FileInfo result = { 0 };
	FILE* stdfile = file.ptr;
	int fd = fileno(stdfile);
	errno = 0;
	
	struct stat buf;
	if (fstat(fd, &buf) == 0)
	{
		result.exists = true;
		result.read_only = false;
		result.created_at = TimespecToPosix_(buf.st_mtim);
		result.modified_at = TimespecToPosix_(buf.st_mtim);
		result.size = buf.st_size;
	}
	
	FillOsErr_(out_err, errno);
	return result;
}

API intsize
OS_WriteFile(OS_File file, intsize size, void const* buffer, OS_Error* out_err)
{
	Trace();
	FILE* stdfile = file.ptr;
	errno = 0;
	intsize result = fwrite(buffer, 1, size, stdfile);
	FillOsErr_(out_err, errno);
	return result;
}

API intsize
OS_ReadFile(OS_File file, intsize size, void* buffer, OS_Error* out_err)
{
	Trace();
	FILE* stdfile = file.ptr;
	errno = 0;
	intsize result = fread(buffer, 1, size, stdfile);
	FillOsErr_(out_err, errno);
	return result;
}

API int64
OS_TellFile(OS_File file, OS_Error* out_err)
{
	Trace();
	FILE* stdfile = file.ptr;
	errno = 0;
	int64 result = ftello(stdfile);
	FillOsErr_(out_err, errno);
	return result;
}

API void
OS_SeekFile(OS_File file, int64 offset, bool relative, OS_Error* out_err)
{
	Trace();
	FILE* stdfile = file.ptr;
	errno = 0;
	
	off_t seek_pos = offset;
	int move_method = SEEK_SET;
	if (relative)
		move_method = SEEK_CUR;
	else if (offset < 0)
	{
		seek_pos = offset + 1;
		move_method = SEEK_END;
	}
	
	fseeko(stdfile, seek_pos, move_method);
}

API intsize
OS_WriteFileAt(OS_File file, int64 offset, intsize size, void const* buffer, OS_Error* out_err)
{
	Trace();
	FILE* stdfile = file.ptr;
	errno = 0;
	
	off_t seek_pos = offset;
	int move_method = SEEK_SET;
	if (offset < 0)
	{
		seek_pos = offset + 1;
		move_method = SEEK_END;
	}
	
	fseeko(stdfile, seek_pos, move_method);
	intsize result = fwrite(buffer, 1, size, stdfile);
	FillOsErr_(out_err, errno);
	return result;
}

API intsize
OS_ReadFileAt(OS_File file, int64 offset, intsize size, void* buffer, OS_Error* out_err)
{
	Trace();
	FILE* stdfile = file.ptr;
	errno = 0;
	
	off_t seek_pos = offset;
	int move_method = SEEK_SET;
	if (offset < 0)
	{
		seek_pos = offset + 1;
		move_method = SEEK_END;
	}
	
	fseeko(stdfile, seek_pos, move_method);
	intsize result = fread(buffer, 1, size, stdfile);
	FillOsErr_(out_err, errno);
	return result;
}

API void
OS_CloseFile(OS_File file)
{
	Trace();
	FILE* stdfile = file.ptr;
	fclose(stdfile);
}

API OS_File
OS_GetStdout(void)
{
	return (OS_File) { stdout };
}

API OS_File
OS_GetStdin(void)
{
	return (OS_File) { stdin };
}

API OS_File
OS_GetStderr(void)
{
	return (OS_File) { stderr };
}

API OS_File
OS_CreatePipe(bool inheritable, OS_Error* out_err)
{
	return (OS_File) { 0 };
}

API bool
OS_IsFilePipe(OS_File file)
{
	return false;
}

API int32
OS_Exec(String cmd, OS_Error* out_err)
{
	return INT32_MIN;
}

API OS_ChildProcess
OS_ExecAsync(String cmd, OS_ChildProcessDesc const* child_desc, OS_Error* out_err)
{
	return (OS_ChildProcess) { 0 };
}

API OS_ChildProcess
OS_CreateChildProcess(String executable, String args, OS_ChildProcessDesc const* child_desc, OS_Error* out_err)
{
	return (OS_ChildProcess) { 0 };
}

API void
OS_WaitChildProcess(OS_ChildProcess child, OS_Error* out_err)
{
	
}

API bool
OS_GetReturnCodeChildProcess(OS_ChildProcess child, int32* out_code, OS_Error* out_err)
{
	return false;
}

API void
OS_KillChildProcess(OS_ChildProcess child, OS_Error* out_err)
{
	
}

API void
OS_DestroyChildProcess(OS_ChildProcess child)
{
	
}

//~ Threading Stuff
API void
OS_InitRWLock(OS_RWLock* lock)
{
	Trace();
	pthread_mutex_t* impl = OS_HeapAlloc(sizeof(pthread_mutex_t));
	SafeAssert(pthread_mutex_init(mem, NULL) == 0);
	lock->ptr = impl;
}

API void
OS_LockShared(OS_RWLock* lock)
{
	Trace();
	SafeAssert(pthread_mutex_lock(lock->ptr) == 0);
}

API void
OS_LockExclusive(OS_RWLock* lock)
{
	Trace();
	SafeAssert(pthread_mutex_lock(lock->ptr) == 0);
}

API bool
OS_TryLockShared(OS_RWLock* lock)
{
	Trace();
	return pthread_mutex_trylock(lock->ptr) == 0;
}

API bool
OS_TryLockExclusive(OS_RWLock* lock)
{
	Trace();
	return pthread_mutex_trylock(lock->ptr) == 0;
}

API void
OS_UnlockShared(OS_RWLock* lock)
{
	Trace();
	SafeAssert(pthread_mutex_unlock(lock->ptr) == 0);
}

API void
OS_UnlockExclusive(OS_RWLock* lock)
{
	Trace();
	SafeAssert(pthread_mutex_unlock(lock->ptr) == 0);
}

API void
OS_DeinitRWLock(OS_RWLock* lock)
{
	Trace();
	if (lock->ptr)
	{
		SafeAssert(pthread_mutex_destroy(lock->ptr) == 0);
		OS_HeapFree(lock->ptr);
		lock->ptr = NULL;
	}
}

API void
OS_InitSemaphore(OS_Semaphore* sem, int32 max_count)
{
	Trace();
	sem->ptr = NULL;
	if (max_count > 0)
	{
		sem_t* mem = OS_HeapAlloc(sizeof(sem_t));
		SafeAssert(sem_init(mem, 0, max_count) == 0);
		sem->ptr = mem;
	}
}

API bool
OS_WaitForSemaphore(OS_Semaphore* sem)
{
	Trace();
	if (sem->ptr)
		return sem_wait(sem->ptr) == 0;
	return false;
}

API void
OS_SignalSemaphore(OS_Semaphore* sem, int32 count)
{
	Trace();
	if (sem->ptr)
		while (count-- > 0 && sem_post(sem->ptr) == 0);
}

API void
OS_DeinitSemaphore(OS_Semaphore* sem)
{
	Trace();
	if (sem->ptr)
	{
		sem_destroy(sem->ptr);
		OS_HeapFree(sem->ptr);
		sem->ptr = NULL;
	}
}

API void
OS_InitConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	pthread_cond_t* impl = OS_HeapAlloc(sizeof(pthread_cond_t));
	pthread_condattr_t attr;
	SafeAssert(pthread_condattr_init(&attr) == 0);
	SafeAssert(pthread_cond_init(impl, &attr) == 0);
	SafeAssert(pthread_condattr_destroy(&attr) == 0);
	
	var->ptr = impl;
}

API bool
OS_SleepOnConditionVariable(OS_ConditionVariable* var, OS_RWLock* lock, bool is_lock_shared, int32 ms)
{
	Trace();
	bool result = false;
	pthread_cond_t* impl = var->ptr;
	pthread_mutex_t* mutex = lock->ptr;
	struct timespec time = {
		.tv_sec = (time_t)ms / 1000,
		.tv_nsec = ((long)ms % 1000) * 1000 * 1000,
	};
	
	int errcode = pthread_cond_timedwait(impl, mutex, &time);
	if (errcode == 0)
		result = true;
	
	return result;
}

API void
OS_WakeOnConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	pthread_cond_t* impl = var->ptr;
	SafeAssert(pthread_cond_signal(impl) == 0);
}

API void
OS_WakeAllOnConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	pthread_cond_t* impl = var->ptr;
	SafeAssert(pthread_cond_broadcast(impl) == 0);
}

API void
OS_DeinitConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	if (var->ptr)
	{
		pthread_cond_destroy(var->ptr);
		OS_HeapFree(var->ptr);
		var->ptr = NULL;
	}
}
