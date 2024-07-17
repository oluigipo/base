#include "common.h"
#include "api_os.h"
#define _FILE_OFFSET_BITS 64

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <jni.h>
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <android/asset_manager.h>
#include <aaudio/AAudio.h>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <dlfcn.h>
#include <errno.h>

#include "api_os_opengl.h"

struct FileData_
{
	// can be either of those
	int fd;
	AAsset* asset;
}
typedef FileData_;

struct
{
	struct android_app* app_state;
	struct timespec started_time;
	
	bool can_terminate : 1;
	bool has_focus : 1;
	
	Arena* event_arena;
	OS_MouseState mouse;
	OS_KeyboardState keyboard;
	
	EGLDisplay gl_display;
	EGLContext gl_context;
	EGLSurface gl_surface;
	
	alignas(64) struct
	{
		OS_AudioProc* audio_user_thread_proc;
		void* audio_user_thread_data;
		AAudioStream* audio_stream;
		int32 audio_sample_rate;
		int32 audio_channel_count;
		int32 audio_samples_per_frame;
	};
}
static g_android;

struct
{
	uint8* memory;
	intsize chunk_size;
	intsize chunk_count;
}
static thread_local g_scratch;

static void
SetupThreadScratchMemory_(intsize chunk_size, intsize chunk_count)
{
	Trace();
	Assert(IsPowerOf2(chunk_size));
	
	uint8* memory = mmap(NULL, chunk_size*chunk_count, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	Assert(memory);
	g_scratch.memory = memory;
	g_scratch.chunk_size = chunk_size;
	g_scratch.chunk_count = chunk_count;
	
	for (intsize i = 0; i < chunk_count; ++i)
	{
		Arena* arena = (Arena*)(memory + chunk_size*i);
		*arena = ArenaFromMemory(memory + chunk_size*i + AlignUp(sizeof(Arena), 15), chunk_size - AlignUp(sizeof(Arena), 15));
	}
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
	return err.ok;
}

static uint64
TimespecToPosix_(struct timespec ts)
{
	uint64 result = 0;
	result += ts.tv_sec * 1000000000;
	result += ts.tv_nsec;
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
		desc.scratch_chunk_count = 2;
	
	SetupThreadScratchMemory_(desc.scratch_chunk_size, desc.scratch_chunk_count);
	int32 result = desc.proc(desc.user_data);
	
	munmap(g_scratch.memory, g_scratch.chunk_size*g_scratch.chunk_count);
	return (void*)(intptr)result;
}

static aaudio_data_callback_result_t
AudioCallback_(AAudioStream* stream, void* user_data, void* audio_data, int32 num_frames)
{
	const int32 sample_rate       = g_android.audio_sample_rate;
	const int32 channel_count     = g_android.audio_channel_count;
	const int32 samples_per_frame = g_android.audio_samples_per_frame;
	const int32 sample_count      = num_frames * samples_per_frame;
	
	OS_AudioProc* user_proc      = g_android.audio_user_thread_proc;
	void*         user_proc_data = g_android.audio_user_thread_data;
	
	MemoryZero(audio_data, sizeof(int16) * sample_count);
	user_proc(&(OS_AudioProcArgs) {
		.user_data = user_proc_data,
		.samples = audio_data,
		.sample_count = sample_count,
		.sample_rate = sample_rate,
		.channel_count = samples_per_frame,
	});
	
	return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static int32
OnInputEventProc_(struct android_app* app, AInputEvent* event)
{
	int32 type = AInputEvent_getType(event);
	
	switch (type)
	{
		case AINPUT_EVENT_TYPE_MOTION:
		{
			int32 flags = AMotionEvent_getFlags(event);
			int32 action = AMotionEvent_getAction(event);
			uintsize pointer_index = 0;
			
			float32 x_axis = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, pointer_index);
			float32 y_axis = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, pointer_index);
			
			g_android.mouse.pos[0] = x_axis;
			g_android.mouse.pos[1] = y_axis;
			
			if (action == 0 || action == 2)
			{
				g_android.mouse.buttons[OS_MouseButton_Left].is_down = (action == 0);
				g_android.mouse.buttons[OS_MouseButton_Left].changes += 1;
			}
		} break;
		
		//case AINPUT_EVENT_TYPE_KEY: OS_DebugLog("[OnInputEvent] AINPUT_EVENT_TYPE_KEY"); break;
		//case AINPUT_EVENT_TYPE_FOCUS: OS_DebugLog("[OnInputEvent] AINPUT_EVENT_TYPE_FOCUS"); break;
		//case AINPUT_EVENT_TYPE_CAPTURE: OS_DebugLog("[OnInputEvent] AINPUT_EVENT_TYPE_CAPTURE"); break;
		//case AINPUT_EVENT_TYPE_DRAG: OS_DebugLog("[OnInputEvent] AINPUT_EVENT_TYPE_DRAG"); break;
	}
	
	return 0;
}

static void
OnAppCommandProc_(struct android_app* app, int32_t cmd)
{
	switch (cmd)
	{
		case APP_CMD_INIT_WINDOW:
		{
			// NOTE(ljre): When this event is received, g_android.app_state->window is non-NULL.
			g_android.has_focus = true;
		} break;
		
		case APP_CMD_TERM_WINDOW:
		{
			if (g_android.event_arena)
			{
				ArenaPushStructInit(g_android.event_arena, OS_Event, {
					.kind = OS_EventKind_WindowClose,
				});
			}
		} break;
		
		case APP_CMD_GAINED_FOCUS:
		{
			g_android.has_focus = true;
			if (g_android.event_arena)
			{
				ArenaPushStructInit(g_android.event_arena, OS_Event, {
					.kind = OS_EventKind_WindowGotFocus,
				});
			}
		} break;
		
		case APP_CMD_LOST_FOCUS:
		{
			g_android.has_focus = false;
			if (g_android.event_arena)
			{
				ArenaPushStructInit(g_android.event_arena, OS_Event, {
					.kind = OS_EventKind_WindowLostFocus,
				});
			}
		} break;
		
		case APP_CMD_STOP:
		{
			//g_android.can_terminate = true;
		} break;
	}
}

void
android_main(struct android_app* state)
{
	Trace();
	DisableWarnings();
	app_dummy();
	ReenableWarnings();
	
	// NOTE(ljre): It's possible for 'android_main' to run multiple times...
	//             This also means that leaking stuff to let the OS clean it is unreliable on Android between
	//             multiple "runs".
	MemoryZero(&g_android, sizeof(g_android));
	
	SetupThreadScratchMemory_(16<<20, 2);
	clock_gettime(CLOCK_MONOTONIC, &g_android.started_time);
	
	g_android.app_state = state;
	state->onAppCmd = OnAppCommandProc_;
	state->onInputEvent = OnInputEventProc_;
	
	int32 result = EntryPoint(1, (char const*[]) { "app", NULL });
	(void)result;
	
	if (g_android.audio_stream)
	{
		AAudioStream_requestStop(g_android.audio_stream);
		AAudioStream_close(g_android.audio_stream);
		
		g_android.audio_stream = NULL;
	}
	
	if (g_android.gl_context)
	{
		eglMakeCurrent(g_android.gl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(g_android.gl_display, g_android.gl_context);
		eglDestroySurface(g_android.gl_display, g_android.gl_surface);
		eglTerminate(g_android.gl_display);
		
		g_android.gl_display = NULL;
		g_android.gl_context = NULL;
		g_android.gl_surface = NULL;
	}
	
	ANativeActivity_finish(g_android.app_state->activity);
	while (!g_android.can_terminate)
	{
		struct android_poll_source* source;
		int32 events;
		int32 ident = ALooper_pollAll(!g_android.has_focus ? -1 : 0, NULL, &events, (void**)&source);
		
		if (ident >= 0)
		{
			if (source != NULL)
				source->process(g_android.app_state, source);
			
			if (g_android.app_state->destroyRequested != 0)
			{
				g_android.can_terminate = true;
				break;
			}
		}
	}
	
	munmap(g_scratch.memory, g_scratch.chunk_size*g_scratch.chunk_count);
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
	
	if (systems & OS_Init_Gamepad)
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
	nanosleep(&t, &rem);
}

API void
OS_SleepPrecise(uint64 ns)
{
	Trace();
	uint64 end;
	{
		struct timespec then_ts;
		clock_gettime(CLOCK_MONOTONIC, &then_ts);
		end = (uint64)then_ts.tv_sec * 1000000000 + then_ts.tv_nsec + ns;
	}

	if (ns > 1000)
	{
		ns -= 1000;
		struct timespec t = {
			.tv_sec = ns / 1000000000,
			.tv_nsec = ns,
		};
		struct timespec rem;
		nanosleep(&t, &rem);
	}

	for (;;)
	{
		struct timespec now_ts;
		clock_gettime(CLOCK_MONOTONIC, &now_ts);
		uint64 now = (uint64)now_ts.tv_sec * 1000000000 + now_ts.tv_nsec;
		if (now >= end)
			break;
	}
}

API void
OS_ExitWithErrorMessage(char const* fmt, ...)
{
	uint8 tmpbuf[4096];
	Arena scratch_arena = ArenaFromMemory(tmpbuf, sizeof(tmpbuf));
	va_list args;
	va_start(args, fmt);
	
	for ArenaTempScope(&scratch_arena)
	{
		String str = ArenaVPrintf(&scratch_arena, fmt, args);
		write(2, str.data, str.size);
		__android_log_print(ANDROID_LOG_INFO, "NativeExample", "%.*s", (int)str.size, str.data);
	}
	
	va_end(args);
	exit(1);
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
		return (Arena*)g_scratch.memory;
	if (conflict_count >= g_scratch.chunk_count)
		SafeAssert(false);
	
	for (intsize arena_index = 0; arena_index < g_scratch.chunk_count; ++arena_index)
	{
		Arena* arena = (Arena*)(g_scratch.memory + g_scratch.chunk_size*arena_index);
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
	Trace();
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
OS_TimeInSeconds(void)
{
	Trace();
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

API OS_SystemTime
OS_GetSystemTime(void)
{
	Trace();
	OS_SystemTime result = { 0 };
	
	struct timespec timespec;
	if (clock_gettime(CLOCK_REALTIME, &timespec) == 0)
	{
		time_t time = timespec.tv_sec;
		struct tm tm;
		if (localtime_r(&time, &tm))
		{
			result.year = tm.tm_year + 1900;
			result.month = tm.tm_mon + 1;
			result.week_day = tm.tm_wday;
			result.day = tm.tm_mday;
			result.hours = tm.tm_hour;
			result.minutes = tm.tm_min;
			result.seconds = tm.tm_sec;
			result.milliseconds = timespec.tv_nsec / 1000000;
		}
	}
	
	return result;
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
		__android_log_print(ANDROID_LOG_INFO, "NativeExample", "%.*s", (int)str.size, str.data);
	}
	
	va_end(args);
}

//~ Audio
API bool
OS_StartAudioProc(uint32 device_id, OS_AudioProc* proc, void* user_data, OS_Error* out_err)
{
	Trace();
	aaudio_result_t r;
	
	AAudioStreamBuilder* stream_builder;
	r = AAudio_createStreamBuilder(&stream_builder);
	if (r != AAUDIO_OK)
	{
		*out_err = (OS_Error) { false, (uint32)r, StrInit("could not create AAudioStreamBuilder") };
		return false;
	}
	
	AAudioStreamBuilder_setDirection(stream_builder, AAUDIO_DIRECTION_OUTPUT);
	AAudioStreamBuilder_setSharingMode(stream_builder, AAUDIO_SHARING_MODE_SHARED);
	AAudioStreamBuilder_setSampleRate(stream_builder, 48000);
	AAudioStreamBuilder_setChannelCount(stream_builder, 2);
	AAudioStreamBuilder_setFormat(stream_builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setDataCallback(stream_builder, AudioCallback_, NULL);
	
	AAudioStream* audio_stream;
	r = AAudioStreamBuilder_openStream(stream_builder, &audio_stream);
	
	AAudioStreamBuilder_delete(stream_builder);
	if (r != AAUDIO_OK)
	{
		*out_err = (OS_Error) { false, (uint32)r, StrInit("AAudioStreamBuilder_openStream failed") };
		return false;
	}
	
	g_android.audio_stream = audio_stream;
	g_android.audio_sample_rate = AAudioStream_getSampleRate(audio_stream);
	g_android.audio_channel_count = AAudioStream_getChannelCount(audio_stream);
	g_android.audio_samples_per_frame = AAudioStream_getSamplesPerFrame(audio_stream);
	g_android.audio_user_thread_proc = proc;
	g_android.audio_user_thread_data = user_data;
	
	r = AAudioStream_requestStart(audio_stream);
	if (r != AAUDIO_OK)
	{
		AAudioStream_close(g_android.audio_stream);
		g_android.audio_stream = NULL;
		
		*out_err = (OS_Error) { false, (uint32)r, StrInit("AAudioStream_requestStart failed") };
		return false;
	}
	
	return true;
}

API bool
OS_StopAudioProc(OS_Error* out_err)
{
	Trace();
	
	if (g_android.audio_stream)
	{
		AAudioStream_requestStop(g_android.audio_stream);
		AAudioStream_close(g_android.audio_stream);
		g_android.audio_stream = NULL;
	}
	
	return true;
}

API OS_RetrieveAudioDevicesResult
OS_RetrieveAudioDevices(void)
{
	Trace();
	OS_RetrieveAudioDevicesResult result = {
		.count = 1,
		.default_device_id = 1,
		.devices[0] = {
			.id = 1,
			.name = u8"Default Audio Device",
		},
	};
	
	return result;
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
	
	int errcode = pthread_join(impl, &retcode);
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
	Trace();
	
	while (!g_android.app_state->window)
	{
		struct android_poll_source* source;
		int32 events;
		int32 ident = ALooper_pollAll(-1, NULL, &events, (void**)&source);
		if (ident < 0)
			break;
		
		if (source != NULL)
			source->process(g_android.app_state, source);
		if (g_android.app_state->destroyRequested != 0)
			g_android.can_terminate = true;
	}
	
	if (!g_android.app_state->window)
		return (OS_Window) { 0 };
	
	EGLint const attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE,
	};
	
	EGLDisplay display = NULL;
	EGLSurface surface = NULL;
	EGLContext context = NULL;
	
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY)
		goto failure;
	
	if (!eglInitialize(display, 0, 0))
		goto failure;
	
	EGLConfig config;
	EGLint num_configs;
	if (!eglChooseConfig(display, attribs, &config, 1, &num_configs))
		goto failure;
	
	EGLint format;
	if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format))
		goto failure;
	
	ANativeWindow_setBuffersGeometry(g_android.app_state->window, 0, 0, format);
	surface = eglCreateWindowSurface(display, config, g_android.app_state->window, NULL);
	if (!surface)
		goto failure;
	
	context = eglCreateContext(display, config, NULL, (EGLint[]) { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE });
	if (!context)
		goto failure;
	
	g_android.gl_context = context;
	g_android.gl_surface = surface;
	g_android.gl_display = display;
	
	return (OS_Window) { (void*)1 };
	failure:
	{
		if (context)
			eglDestroyContext(display, context);
		if (surface)
			eglDestroySurface(display, surface);
		if (display && display != EGL_NO_DISPLAY)
			eglTerminate(display);
		return (OS_Window) { 0 };
	}
}

API void
OS_UpdateWindow(OS_Window window, OS_WindowDesc const* desc)
{
	Trace();
	SafeAssert(!"Not Implemented");
}

API void
OS_DestroyWindow(OS_Window window)
{
	Trace();
	
	if (g_android.gl_context)
		eglDestroyContext(g_android.gl_display, g_android.gl_context);
	if (g_android.gl_surface)
		eglDestroySurface(g_android.gl_display, g_android.gl_surface);
	if (g_android.gl_display)
		eglTerminate(g_android.gl_display);
	
	g_android.gl_context = NULL;
	g_android.gl_surface = NULL;
	g_android.gl_display = NULL;
}

API void
OS_GetWindowUserSize(OS_Window window, int32* out_width, int32* out_height)
{
	Trace();
	int32 width = 0;
	int32 height = 0;
	
	if (window.ptr == (void*)1 && g_android.gl_display)
	{
		eglQuerySurface(g_android.gl_display, g_android.gl_surface, EGL_WIDTH, &width);
		eglQuerySurface(g_android.gl_display, g_android.gl_surface, EGL_HEIGHT, &height);
	}
	
	if (out_width)
		*out_width = width;
	if (out_height)
		*out_height = height;
}

API OS_Event*
OS_PollEvents(bool wait, Arena* output_arena, intsize* out_event_count)
{
	Trace();
	wait = wait || !g_android.has_focus;
	
	g_android.mouse.old_pos[0] = g_android.mouse.pos[0];
	g_android.mouse.old_pos[1] = g_android.mouse.pos[1];
	for (intsize i = 0; i < ArrayLength(g_android.mouse.buttons); ++i)
		g_android.mouse.buttons[i].changes = 0;
	
	g_android.event_arena = output_arena;
	OS_Event* first_event = output_arena ? ArenaEndAligned(output_arena, alignof(OS_Event)) : NULL;
	do
	{
		struct android_poll_source* source;
		int32 events;
		int32 ident = ALooper_pollAll(wait ? -1 : 0, NULL, &events, (void**)&source);
		if (ident < 0)
			break;
		
		if (source != NULL)
			source->process(g_android.app_state, source);
		
		if (g_android.app_state->destroyRequested != 0)
			g_android.can_terminate = true;
	}
	while (!g_android.can_terminate);
	OS_Event* last_event = output_arena ? ArenaEnd(output_arena) : NULL;
	
	if (out_event_count)
		*out_event_count = last_event - first_event;
	return first_event;
}

//~ Gamepad
API void
OS_PollGamepadStates(OS_GamepadStates* states, bool accumulate)
{
	Trace();
	MemoryZero(states, sizeof(*states));
}

API void
OS_PollKeyboardState(OS_KeyboardState* state, bool accumulate)
{
	Trace();
	*state = g_android.keyboard;
}

API void
OS_PollMouseState(OS_MouseState* state, bool accumulate, OS_Window optional_window)
{
	Trace();
	*state = g_android.mouse;
}

API void
OS_SetGamepadMappings(OS_GamepadMapping const* mappings, intsize mapping_count)
{
	Trace();
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
OS_VirtualAlloc(void* address, uintsize size, uint32 flags)
{
	Trace();
	
	uint32 prot = PROT_READ|PROT_WRITE;
	if (flags & OS_VirtualFlags_ReserveOnly)
		prot = PROT_NONE;
	
	void* result = mmap(address, size, prot, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if (result == MAP_FAILED)
		result = NULL;
	
	return result;
}

API bool
OS_VirtualCommit(void* address, uintsize size)
{
	Trace();
	return mprotect(address, size, PROT_READ|PROT_WRITE) == 0;
}

API bool
OS_VirtualProtect(void* address, uintsize size, uint32 flags)
{
	Trace();
	
	uint32 prot = PROT_READ|PROT_WRITE;
	if (flags & OS_VirtualFlags_ReadOnly)
		prot = PROT_READ;
	else if (flags & OS_VirtualFlags_ExecuteOnly)
		prot = PROT_EXEC;
	
	return mprotect(address, size, prot) == 0;
}

API bool
OS_VirtualFree(void* address, uintsize size)
{
	Trace();
	return munmap(address, size) == 0;
}

API bool
OS_ArenaCommitMemoryProc(Arena* arena, uintsize needed_size)
{
	Trace();
	uintsize commited = arena->size;
	uintsize reserved = arena->reserved;
	uint8*   memory   = arena->memory;
	Assert(commited < needed_size);
	
	uintsize rounded_needed_size = AlignUp(needed_size, (8<<20) - 1);
	if (rounded_needed_size > reserved)
		return false;
	
	int32 result = mprotect(memory + commited, rounded_needed_size - commited, PROT_READ|PROT_WRITE);
	return result == 0;
}

API Arena
OS_VirtualAllocArena(uintsize total_reserved_size)
{
	Trace();
	Arena result = { 0 };
	
	total_reserved_size = AlignUp(total_reserved_size, (8<<20) - 1);
	void* memory = mmap(NULL, total_reserved_size, PROT_NONE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if (memory != MAP_FAILED)
	{
		if (0 == mprotect(memory, 8<<20, PROT_READ|PROT_WRITE))
		{
			result.memory = memory;
			result.size = 32<<20;
			result.offset = 0;
			result.reserved = total_reserved_size;
			result.commit_memory_proc = OS_ArenaCommitMemoryProc;
		}
		else
			munmap(memory, total_reserved_size);
	}
	else
		memory = NULL;
	
	return result;
}

API bool
OS_ReadEntireFile(String path, Arena* output_arena, void** out_data, uintsize* out_size, OS_Error* out_err)
{
	Trace();
	String asset_prefix = StrInit("assets/");
	Arena* scratch_arena = OS_ScratchArena(&output_arena, 1);
	
	if (StringStartsWith(path, asset_prefix))
	{
		String substr = StringSubstr(path, asset_prefix.size, -1);
		AAsset* asset = NULL;
		
		for ArenaTempScope(scratch_arena)
		{
			const char* cstr = ArenaPushCString(scratch_arena, substr);
			asset = AAssetManager_open(g_android.app_state->activity->assetManager, cstr, AASSET_MODE_BUFFER);
		}
		
		if (asset)
		{
			const void* data = AAsset_getBuffer(asset);
			uintsize size = AAsset_getLength(asset);
			
			*out_data = ArenaPushMemory(output_arena, data, size);
			*out_size = size;
			
			AAsset_close(asset);
			return true;
		}
		
		return false;
	}
	
	return out_err->ok;
}

API bool
OS_WriteEntireFile(String path, void const* out_data, uintsize size, OS_Error* out_err)
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
	
	Arena* scratch_arena = OS_ScratchArena(NULL, 0);
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

API OS_FileMapping
OS_MapFileForReading(OS_File file, void const** out_buffer, uintsize* out_size, OS_Error* out_err)
{
	return (OS_FileMapping) { 0 };
}

API void
OS_UnmapFile(OS_FileMapping mapping)
{
	
}

//~ Process Stuff
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
	SafeAssert(pthread_mutex_init(impl, NULL) == 0);
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
OS_WaitConditionVariable(OS_ConditionVariable* var, OS_RWLock* lock, bool is_lock_shared, int32 ms)
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
OS_SignalConditionVariable(OS_ConditionVariable* var)
{
	Trace();
	pthread_cond_t* impl = var->ptr;
	SafeAssert(pthread_cond_signal(impl) == 0);
}

API void
OS_SignalAllConditionVariable(OS_ConditionVariable* var)
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

//~
static void
OglPresentProc_(OS_OpenGLApi* api)
{
	Trace();
	eglSwapBuffers(g_android.gl_display, g_android.gl_surface);
}

static void
OglResizeBuffersProc_(OS_OpenGLApi* api)
{
	Trace();
	// nothing to do here
}

API bool
OS_MakeOpenGLApi(OS_OpenGLApi* out_api, OS_OpenGLApiDesc const* args)
{
	Trace();
	if (!args->window.ptr)
		return false;
	
	if (eglMakeCurrent(g_android.gl_display, g_android.gl_surface, g_android.gl_surface, g_android.gl_context) == EGL_FALSE)
		return false;
	
	out_api->is_es = true;
	out_api->version = 30;
	out_api->window = args->window;
	out_api->present = OglPresentProc_;
	out_api->resize_buffers = OglResizeBuffersProc_;
	
#define X(Type, Name) out_api->Name = Name;
	_GL_FUNCTIONS_X_MACRO_ES20(X);
	_GL_FUNCTIONS_X_MACRO_ES30(X);
	_GL_FUNCTIONS_X_MACRO_ES31(X);
	_GL_FUNCTIONS_X_MACRO_ES32(X);
#undef X
	
	return true;
}

API bool
OS_FreeOpenGLApi(OS_OpenGLApi* api)
{
	return true;
}
