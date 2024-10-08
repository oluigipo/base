#include "common.h"
#include "common_atomic.h"
#include "bed.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct Console
{
	HPCON hpcon;
	HANDLE conside_input, conside_output;
	HANDLE con_input, con_output;
	PROCESS_INFORMATION cmd_process_info;
	OS_Thread thread;

	alignas(64) int32 should_return;
}
typedef Console;

static int32
ConsoleThreadProc_(void* user_data)
{
	Trace();
	Console* console = user_data;

	while (!AtomicLoad32Acq(&console->should_return))
	{
		DWORD wait_result = WaitForSingleObject(console->con_output, 1);
		if (wait_result != WAIT_OBJECT_0)
			continue;

		char string_read[4096] = {};
		DWORD chars_read = 0;
		BOOL ok = ReadFile(console->con_output, string_read, sizeof(string_read)-2, &chars_read, NULL);
		if (!ok)
		{
			if (AtomicLoad32Relaxed(&console->should_return))
				break;
			HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
			SafeAssert(SUCCEEDED(hr)); // just so we can see it in the debugger
		}
		string_read[chars_read] = '\n';
		string_read[chars_read+1] = 0;

		OutputDebugStringA(string_read);
	}

	return 0;
}

BED_API void
ConsoleDeinit(App* app, Console* console)
{
	AtomicStore32Rel(&console->should_return, 1);
	if (console->hpcon)
		ClosePseudoConsole(console->hpcon);
	if (console->thread.ptr)
	{
		OS_JoinThread(console->thread);
		OS_DestroyThread(console->thread);
	}
	if (console->conside_input)
		CloseHandle(console->conside_input);
	if (console->conside_output)
		CloseHandle(console->conside_output);
	if (console->con_input)
		CloseHandle(console->con_input);
	if (console->con_output)
		CloseHandle(console->con_output);
	MemoryZero(console, sizeof(*console));
}

BED_API bool
ConsoleInit(App* app, Console* console)
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	MemoryZero(console, sizeof(*console));
	if (!CreatePipe(&console->conside_input, &console->con_input, NULL, 0))
		return ConsoleDeinit(app, console), false;
	if (!CreatePipe(&console->con_output, &console->conside_output, NULL, 0))
		return ConsoleDeinit(app, console), false;

	HRESULT hr = CreatePseudoConsole(
		(COORD) { 80, 8001 },
		console->conside_input,
		console->conside_output,
		0,
		&console->hpcon);
	if (!SUCCEEDED(hr))
		return ConsoleDeinit(app, console), false;

	STARTUPINFOEXW startup_info = {
		.StartupInfo.cb = sizeof(STARTUPINFOEXW),
	};

	SIZE_T bytes_required = 0;
	InitializeProcThreadAttributeList(NULL, 1, 0, &bytes_required);

	startup_info.lpAttributeList = AllocatorAlloc(&app->heap, (intz)bytes_required, 16, NULL);
	if (!InitializeProcThreadAttributeList(startup_info.lpAttributeList, 1, 0, &(SIZE_T) { bytes_required }))
	{
		AllocatorFree(&app->heap, startup_info.lpAttributeList, (intz)bytes_required, NULL);
		return ConsoleDeinit(app, console), false;
	}

	if (!UpdateProcThreadAttribute(
			startup_info.lpAttributeList,
			0,
			PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
			console->hpcon,
			sizeof(HPCON),
			NULL,
			NULL))
	{
		AllocatorFree(&app->heap, startup_info.lpAttributeList, (intz)bytes_required, NULL);
		return ConsoleDeinit(app, console), false;
	}

	wchar_t cmd_path[] = L"C:\\Windows\\System32\\cmd.exe";
	BOOL process_ok = CreateProcessW(
			NULL,
			cmd_path,
			NULL,
			NULL,
			FALSE,
			EXTENDED_STARTUPINFO_PRESENT,
			NULL,
			NULL,
			&startup_info.StartupInfo,
			&console->cmd_process_info);
	AllocatorFree(&app->heap, startup_info.lpAttributeList, (intz)bytes_required, NULL);
	if (!process_ok)
		return ConsoleDeinit(app, console), false;

	console->thread = OS_CreateThread(&(OS_ThreadDesc) {
		.proc = ConsoleThreadProc_,
		.user_data = console,
		.name = StrInit("Console Thread"),
	});

	String command = StrInit("start.\r\n");
	WriteFile(console->con_input, command.data, command.size, NULL, NULL);

	return true;
}
