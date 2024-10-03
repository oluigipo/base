#include "common.h"
#include "api.h"
#include "api_os.h"

__declspec(dllexport) __declspec(noinline) int32
FuzzLexerProc(String input, Arena arenas[4])
{
	OS_Error err;
	int32 r = 0;

	String input_data;
	if (!OS_ReadEntireFile(input, &arenas[0], (void**)&input_data.data, &input_data.size, &err))
	{
		OS_LogErr("could not open input file '%S' (0x%x): %S\n", input, err.code, err.what);
		r |= 1;
	}
	else
	{
		CF_TokensFromStringArgs args = {
			.str = input_data,
			.lexing_flags =
				CF_LexingFlag_AllowGnu |
				CF_LexingFlag_AllowMsvc |
				CF_LexingFlag_IncludeLinebreaks |
				CF_LexingFlag_IncludeEscapedLinebreaks |
				CF_LexingFlag_IncludeComments |
				CF_LexingFlag_AddNullTerminatingToken,
			.standard = CF_Standard_C23,
			.kind_allocator = AllocatorFromArena(&arenas[0]),
			.source_range_allocator = AllocatorFromArena(&arenas[1]),
			.loc_allocator = AllocatorFromArena(&arenas[2]),
		};
		CF_TokensFromStringResult result = {};
		if (CF_TokensFromString(&result, &args))
		{
			OS_LogErr("allocation failed on CF_TokensFromString\n");
			r |= 1;
		}
		else
		{
			r |= result.lex_error_count;
			uint8* str_begin = ArenaEnd(&arenas[0]);
			for (intsize i = 0; i < result.token_count; ++i)
			{
				CF_TokenKind kind = result.token_kinds[i];
				if (!kind || kind == CF_TokenKind_Linebreak || kind == CF_TokenKind_EscapedLinebreak)
					continue;
				uint8 const* begin = input_data.data + result.source_ranges[i].begin;
				uint8 const* end   = input_data.data + result.source_ranges[i].end;
				ArenaPrintf(&arenas[0], "%S\n", StrRange(begin, end));
			}
			uint8* str_end = ArenaEnd(&arenas[0]);
			OS_LogOut("%S", StrRange(str_begin, str_end));
		}
	}

	ArenaClear(&arenas[0]);
	ArenaClear(&arenas[1]);
	ArenaClear(&arenas[2]);
	ArenaClear(&arenas[3]);

	return r;
}

static AllocatorError
TokensFromFileProc_(
	CF_TokensFromFileProcResult* out_result,
	CF_PreprocessFileArgs const* pp_args,
	String requested_path,
	String current_relative_path)
{
	Trace();

	void* data = NULL;
	uintsize size = 0;
	OS_Error err = {};
	bool ok = OS_ReadEntireFile(requested_path, (Arena*)pp_args->trees_allocator.instance, &data, &size, &err);
	AllocatorError alloc_err = 0;

	if (ok)
	{
		String source_code = StrMake(size, data);
		CF_TokensFromStringResult tokens_result = {};
		CF_TokensFromStringArgs tokens_args = {
			.str = source_code,
			.kind_allocator = pp_args->dynamic_allocator,
			.loc_allocator = pp_args->dynamic_allocator,
			.source_range_allocator = pp_args->dynamic_allocator,
			.standard = pp_args->standard,
			.lexing_flags =
				pp_args->lexing_flags |
				CF_LexingFlag_IncludeLinebreaks |
				CF_LexingFlag_IncludeEscapedLinebreaks |
				CF_LexingFlag_KeepKeywordsAsIdentifiers,
		};

		alloc_err = CF_TokensFromString(&tokens_result, &tokens_args);
		if (!alloc_err)
		{
			*out_result = (CF_TokensFromFileProcResult) {
				.file_data = source_code,
				.tokens = tokens_result,
				// TODO(ljre): fill in absolute path for this file
				.new_relative_path = StrInit(""),
				.path = requested_path,
			};
		}
		else
		{
			*out_result = (CF_TokensFromFileProcResult) {
				.error = StrInit("allocation failure"),
			};
		}
	}
	else
	{
		*out_result = (CF_TokensFromFileProcResult) {
			.error = err.what,
		};
	}

	return alloc_err;
}

__declspec(dllexport) __declspec(noinline) int32
FuzzPreprocessorProc(String input, Arena arenas[4])
{
	Trace();
	int32 r = 0;

	CF_PreprocessFileResult result = {};
	CF_PreprocessFileArgs args = {
		.input_file_path = input,
		.tokens_from_file_proc = TokensFromFileProc_,
		.user_data = NULL,
		.standard = CF_Standard_C23,
		.lexing_flags =
			CF_LexingFlag_AllowMsvc |
			CF_LexingFlag_AllowGnu,
		.preprocessing_flags =
			CF_PreprocessingFlag_PredefineStandardMacros,
		.predefined_macro_count = 0,
		.predefined_macros = (String[0]) {
		},
		.user_include_dir_count = 0,
		.user_include_dirs = (String[0]) {
		},
		.system_include_dir_count = 0,
		.system_include_dirs = (String[0]) {
		},
		.kind_allocator = AllocatorFromArena(&arenas[0]),
		.source_trace_allocator = AllocatorFromArena(&arenas[1]),
		.trees_allocator = AllocatorFromArena(&arenas[2]),
		.error_allocator = AllocatorFromArena(&arenas[2]),
		.dynamic_allocator = OS_HeapAllocator(),
		.scratch_allocator = AllocatorFromArena(&arenas[3]),
	};
	AllocatorError err = CF_PreprocessFile(&result, &args);

	if (err)
		r = -err;
	else
	{
		if (result.pp_error_count)
			r = 1;
		for (CF_PreprocessError* pp_err = result.pp_errors; pp_err; pp_err = pp_err->next)
			OS_LogErr("%S:%i:%i: error: %S\n", pp_err->trace.file->path, pp_err->trace.loc.line, pp_err->trace.loc.col, pp_err->what);

		uint8* str_begin = ArenaEnd(&arenas[0]);
		for (intsize i = 0; i < result.token_count; ++i)
		{
			CF_TokenKind kind = result.token_kinds[i];
			if (!kind || kind == CF_TokenKind_Linebreak || kind == CF_TokenKind_EscapedLinebreak)
				continue;
			CF_SourceTrace* trace = &result.source_traces[i];
			// while (trace->macro_expansion)
				// trace = trace->macro_expansion;
			CF_SourceTraceFile* file = trace->file;
			String input_data = file->data;
			uint8 const* begin = input_data.data + trace->range.begin;
			uint8 const* end   = input_data.data + trace->range.end;
			ArenaPrintf(&arenas[0], "%S\n", StrRange(begin, end));
		}
		uint8* str_end = ArenaEnd(&arenas[0]);
		OS_WriteEntireFile(StringPrintfLocal(256, "%S_preproc.txt", input), str_begin, str_end - str_begin, NULL);
	}

	ArenaClear(&arenas[0]);
	ArenaClear(&arenas[1]);
	ArenaClear(&arenas[2]);
	ArenaClear(&arenas[3]);
	return r;
}

API int32
EntryPoint(int32 argc, char const* const* argv)
{
	if (argc < 2)
		return 1;
	Arena arenas[4] = {
		OS_VirtualAllocArena(1<<30),
		OS_VirtualAllocArena(1<<30),
		OS_VirtualAllocArena(1<<30),
		OS_VirtualAllocArena(1<<30),
	};

	return FuzzPreprocessorProc(StringFromCString(argv[1]), arenas);
}