#include "common.h"
#include "api.h"

struct LoadedFile_
{
	String data;
	String path;
	String relative_path;
	intsize token_count;
	CF_TokenKind*   token_kinds;
	CF_SourceRange* source_ranges;
	CF_Loc*         locs;
}
typedef LoadedFile_;

enum MacroCommandKind_
{
	MacroCommandKind_Paste,              // raw tokens to enqueue
	MacroCommandKind_ExpandParameter,    // param
	MacroCommandKind_StringifyParameter, // #param
	MacroCommandKind_ConcatLeft,         // param ## more tokens
	MacroCommandKind_ConcatRight,        // more tokens ## param
	MacroCommandKind_VaOpt,              // __VA_OPT__(more tokens)
	MacroCommandKind_ConcatCommaVaArgs,  // , ## __VA_ARGS__
}
typedef MacroCommandKind_;

struct MacroCommand_
{
	// NOTE(ljre): tokens indices are relative to base_token
	MacroCommandKind_ kind;
	// NOTE(ljre): we're assuming no macro definition will have more than 2^32 tokens...
	uint32 tok_begin;
	uint32 tok_end;
	// NOTE(ljre): the parameter index, 0-based.
	uint32 parameter;
}
typedef MacroCommand_;

struct Macro_
{
	uint64 name_hash;
	// NOTE(ljre): there are some specific predefined macros that need to be handled separately. they won't
	//             have any fields (other than `name_hash`, `name`, and `param_count = -1`) set.
	//             they are: __LINE__, __FILE__, __COUNTER__, __DATE__, __TIME__
	String name;
	LoadedFile_* file; // NULL if predefined
	// NOTE(ljre): the base index for the tokens to be expanded, e.g.
	//             #define FOO bar + baz
	//                         ^ it'll point to this token
	//             #define MIN(x, y) ((x) < (y) ? (x) : (y))
	//                               ^ it'll point to this token
	//            all offsets to this base (`commands[i].parameter`, `.paste.tok_begin`, etc.) should
	//            be valid. there should be no Linebreak tokens in any of them also.
	intsize base_token;
	// NOTE(ljre): will be -1 for non-function-like macros. does NOT include __VA_ARGS__ argument. if
	//             `command.parameter == param_count`, then it refers to __VA_ARGS__.
	intsize param_count;
	intsize command_count;
	MacroCommand_ commands[];
}
typedef Macro_;

struct TokenQueue_ typedef TokenQueue_;
struct TokenQueue_
{
	TokenQueue_* next;
	
	CF_TokenKind kind;
	uint32 interned_string;
	CF_SourceTrace trace;
};

struct PreprocessCtx_
{
	AllocatorError err;
	bool fatal_error_occured; // e.g. failed to include file
	CF_PreprocessFileArgs args;

	intsize token_count;
	intsize token_allocated_count;
	CF_TokenKind* token_kinds;
	CF_SourceTrace* source_traces;

	intsize error_count;
	CF_PreprocessError* errors;
	CF_PreprocessError** errors_tail;

	intsize loaded_file_count;
	intsize loaded_file_allocated_count;
	LoadedFile_* loaded_files;

	// NOTE(ljre): Token reading
	intsize token_index;
	LoadedFile_* current_file;
	CF_SourceTraceFile* current_file_trace;
	TokenQueue_* queue;
	CF_TokenKind tok_kind;
	CF_SourceTrace tok_trace;

	// NOTE(ljre): Macros
	intsize macromap_count;
	intsize macromap_allocated_log2count;
	Macro_** macromap;
}
typedef PreprocessCtx_;

struct TokenToAppend_
{
	CF_TokenKind kind;
	CF_SourceTrace trace;
}
typedef TokenToAppend_;

static void
PushToken_(PreprocessCtx_* ctx, TokenToAppend_ const* tok)
{
	Trace();
	if (ctx->token_count+1 > ctx->token_allocated_count)
	{
		intsize current_count = ctx->token_allocated_count;
		intsize amount_to_alloc = (ctx->token_allocated_count >> 1) + 1;
		intsize desired_new_count = amount_to_alloc + ctx->token_allocated_count;
		if (!ctx->err && ctx->args.kind_allocator.proc)
			AllocatorResizeArrayOk(&ctx->args.kind_allocator, desired_new_count, sizeof(CF_TokenKind), alignof(CF_TokenKind), &ctx->token_kinds, current_count, &ctx->err);
		if (!ctx->err && ctx->args.source_trace_allocator.proc)
			AllocatorResizeArrayOk(&ctx->args.source_trace_allocator, desired_new_count, sizeof(CF_SourceTrace), alignof(CF_SourceTrace), &ctx->source_traces, current_count, &ctx->err);
		if (ctx->err)
			return;
		ctx->token_allocated_count = desired_new_count;
	}

	CF_TokenKind kind = tok->kind;
	if (kind == CF_TokenKind_Identifier && tok->trace.file)
	{
		String file_data = tok->trace.file->data;
		CF_SourceRange range = tok->trace.range;
		String ident = StringSlice(file_data, range.begin, range.end);
		CF_TokenKind new_kind = CF_KeywordFromString(ident, ctx->args.standard, ctx->args.lexing_flags);
		if (new_kind)
			kind = new_kind;
	}

	intsize index = ctx->token_count++;
	if (ctx->token_kinds)
		ctx->token_kinds[index] = tok->kind;
	if (ctx->source_traces)
		ctx->source_traces[index] = tok->trace;
}

static void
PushError_(PreprocessCtx_* ctx, CF_PreprocessError const* err)
{
	Trace();
	++ctx->error_count;
	if (!ctx->err && ctx->args.error_allocator.proc)
	{
		CF_PreprocessError* new_err = AllocatorAlloc(&ctx->args.error_allocator, sizeof(CF_PreprocessError), alignof(CF_PreprocessError), &ctx->err);
		if (new_err)
		{
			*new_err = *err;
			*ctx->errors_tail = new_err;
			ctx->errors_tail = &new_err->next;
		}
	}
}

static void
NextToken_(PreprocessCtx_* ctx)
{
	if (ctx->queue)
	{
		ctx->tok_kind = ctx->queue->kind;
		ctx->tok_trace = ctx->queue->trace;
		ctx->queue = ctx->queue->next;
	}
	else if (ctx->token_index < ctx->current_file->token_count)
	{
		ctx->tok_kind = ctx->current_file->token_kinds[ctx->token_index];
		ctx->tok_trace = (CF_SourceTrace) {
			.file = ctx->current_file_trace,
			.loc = ctx->current_file->locs[ctx->token_index],
			.range = ctx->current_file->source_ranges[ctx->token_index],
		};
		++ctx->token_index;
	}
	else
		ctx->tok_kind = 0;
}

static bool
AssertToken_(PreprocessCtx_* ctx, CF_TokenKind kind)
{
	if (Unlikely(ctx->tok_kind != kind))
	{
		PushError_(ctx, &(CF_PreprocessError) {
			.trace = ctx->tok_trace,
			.what = StrInit("expected something but got something else instead"),
		});
		return false;
	}
	return true;
}

static bool
EatToken_(PreprocessCtx_* ctx, CF_TokenKind kind)
{
	if (AssertToken_(ctx, kind))
	{
		NextToken_(ctx);
		return true;
	}
	return false;
}

static bool
TryEatToken_(PreprocessCtx_* ctx, CF_TokenKind kind)
{
	if (ctx->tok_kind == kind)
	{
		NextToken_(ctx);
		return true;
	}
	return false;
}

static String
TokenAsString_(PreprocessCtx_* ctx)
{
	if (!ctx->tok_kind)
		return StrNull;
	if (!ctx->tok_trace.file)
		return StrNull;

	String file_data = ctx->tok_trace.file->data;
	CF_SourceRange range = ctx->tok_trace.range;
	return StringSlice(file_data, range.begin, range.end);
}

static LoadedFile_*
LoadFileAndCacheIt_(PreprocessCtx_* ctx, String file, String folder, String* out_err)
{
	Trace(); TraceText(file);
	CF_TokensFromFileProcResult result = {};
	ctx->err = ctx->args.tokens_from_file_proc(&result, &ctx->args, file, folder);
	if (result.error.size)
	{
		PushError_(ctx, &(CF_PreprocessError) {
			.trace = ctx->tok_trace,
			.what = result.error,
		});
		ctx->fatal_error_occured = true;
		return NULL;
	}

	if (ctx->loaded_file_count+1 > ctx->loaded_file_allocated_count)
	{
		intsize current_count = ctx->token_allocated_count;
		intsize amount_to_alloc = (ctx->token_allocated_count >> 1) + 1;
		intsize desired_new_count = amount_to_alloc + ctx->token_allocated_count;
		SafeAssert(ctx->args.trees_allocator.proc);
		AllocatorResizeArrayOk(&ctx->args.trees_allocator, desired_new_count, sizeof(LoadedFile_), alignof(LoadedFile_), &ctx->loaded_files, current_count, &ctx->err);
		if (ctx->err)
		{
			*out_err = Str("allocation failed");
			return NULL;
		}
		ctx->loaded_file_allocated_count = desired_new_count;
	}

	intsize index = ctx->loaded_file_count++;
	ctx->loaded_files[index] = (LoadedFile_) {
		.token_count = result.tokens.token_count,
		.token_kinds = result.tokens.token_kinds,
		.source_ranges = result.tokens.source_ranges,
		.locs = result.tokens.locs,
		.data = result.file_data,
		.path = result.path,
		.relative_path = result.new_relative_path,
	};

	return &ctx->loaded_files[index];
}

static Macro_*
FindMacroByName_(PreprocessCtx_* ctx, String name)
{
	Trace();
	uint64 hash = HashString(name);
	int32 index = (int32)hash;
	for (;;)
	{
		index = HashMsi(ctx->macromap_allocated_log2count, hash, index);
		Macro_* macro = ctx->macromap[index];
		if (!macro)
			return NULL;
		if (~(uintptr)macro == 0) // tombstone
			continue;
		if (macro->name_hash != hash)
			continue;
		if (!StringEquals(name, macro->name))
			continue;
		return macro;
	}
	return NULL;
}

static void
ExpandObjectMacro_(PreprocessCtx_* ctx, Macro_* macro)
{

}

static bool
TryExpandIdentifier_(PreprocessCtx_* ctx)
{
	Trace();
	String ident = TokenAsString_(ctx);
	Macro_* macro = FindMacroByName_(ctx, ident);
	if (macro)
	{
		if (macro->param_count == -1)
			ExpandObjectMacro_(ctx, macro);
		else
		{
			// TODO(ljre)
		}
	}

	return false;
}

static void
PreprocessFile_(PreprocessCtx_* ctx, LoadedFile_* file)
{
	Trace(); TraceText(file->path);
	// NOTE(ljre): `ctx->queue` is only used for macro expansions enqueueing more tokens. a file cannot
	//             be included inside a macro expansion.
	SafeAssert(!ctx->queue);

	// Save previous state
	intsize prev_token_index = ctx->token_index;
	LoadedFile_* prev_file = ctx->current_file;
	CF_SourceTraceFile* prev_file_trace = ctx->current_file_trace;
	void* scratch_save = AllocatorAlloc(&ctx->args.scratch_allocator, 0, 0, &ctx->err);

	ctx->token_index = 0;
	ctx->current_file = file;
	ctx->current_file_trace = AllocatorAlloc(&ctx->args.trees_allocator, sizeof(CF_SourceTraceFile), alignof(CF_SourceTraceFile), &ctx->err);
	if (ctx->current_file_trace)
	{
		*ctx->current_file_trace = (CF_SourceTraceFile) {
			.path = file->path,
			.data = file->data,
			.includer = prev_file_trace,
			.inclusion_loc = ctx->tok_trace.loc,
		};
		NextToken_(ctx);
	}

	for (; !ctx->err && !ctx->fatal_error_occured; AllocatorPop(&ctx->args.scratch_allocator, scratch_save, &ctx->err))
	{
		while (TryEatToken_(ctx, CF_TokenKind_Linebreak))
		{}

		if (!ctx->tok_kind)
			break;

		if (ctx->tok_kind != CF_TokenKind_SymHash)
		{
			do
			{
				if (ctx->tok_kind != CF_TokenKind_Identifier || !TryExpandIdentifier_(ctx))
				{
					PushToken_(ctx, &(TokenToAppend_) {
						.kind = ctx->tok_kind,
						.trace = ctx->tok_trace,
					});
					NextToken_(ctx);
				}
			}
			while (ctx->tok_kind && ctx->tok_kind != CF_TokenKind_Linebreak);
		}
		else
		{
			EatToken_(ctx, CF_TokenKind_SymHash);
			bool warn_about_excessive_tokens = true;

			if (ctx->tok_kind == CF_TokenKind_Identifier)
			{
				String ident = TokenAsString_(ctx);
				if (StringEquals(ident, Str("include")))
				{
					// handle #include
				}
				else if (StringEquals(ident, Str("warning")))
				{
					// handle #warning
				}
				else if (StringEquals(ident, Str("error")))
				{
				}
				else if (StringEquals(ident, Str("pragma")))
				{
				}
				else if (StringEquals(ident, Str("define")))
				{
				}
				else if (StringEquals(ident, Str("undef")))
				{
				}
				else if (StringEquals(ident, Str("if")))
				{
				}
				else if (StringEquals(ident, Str("ifdef")))
				{
				}
				else if (StringEquals(ident, Str("ifndef")))
				{
				}
				else if (StringEquals(ident, Str("else")))
				{
				}
				else if (StringEquals(ident, Str("elif")))
				{
				}
				else if (StringEquals(ident, Str("endif")))
				{
				}
				else
				{
					PushError_(ctx, &(CF_PreprocessError) {
						.trace = ctx->tok_trace,
						.what = StrInit("unknown preprocessor directive"),
					});
				}
			}
			else if (ctx->tok_kind == CF_TokenKind_SymBang)
				warn_about_excessive_tokens = false;

			if (ctx->tok_kind && ctx->tok_kind != CF_TokenKind_Linebreak)
			{
				if (warn_about_excessive_tokens)
				{
					// TODO(ljre): warn
				}
				while (ctx->tok_kind && ctx->tok_kind != CF_TokenKind_Linebreak)
					NextToken_(ctx);
			}
		}
	}

	// Restore previous state
	AllocatorPop(&ctx->args.scratch_allocator, scratch_save, &ctx->err);
	ctx->token_index = prev_token_index;
	ctx->current_file = prev_file;
	ctx->current_file_trace = prev_file_trace;
	// NOTE(ljre): caller is expected to call `NextToken_` to overwrite the current null token
}

API AllocatorError
CF_PreprocessFile(CF_PreprocessFileResult* out_result, CF_PreprocessFileArgs const* args)
{
	Trace();
	PreprocessCtx_ ctx = {
		.args = *args,
		.errors_tail = &ctx.errors,
	};

	ctx.macromap_allocated_log2count = 10;
	ctx.macromap_count = 0;
	ctx.macromap = AllocatorAllocArray(&ctx.args.dynamic_allocator, 1<<10, sizeof(Macro_*), alignof(Macro_*), &ctx.err);
	if (ctx.err)
		return ctx.err;

	String load_err;
	LoadedFile_* file = LoadFileAndCacheIt_(&ctx, ctx.args.input_file_path, Str(""), &load_err);
	if (file)
		PreprocessFile_(&ctx, file);
	else
		PushError_(&ctx, &(CF_PreprocessError) {
			.trace = {},
			.what = load_err,
		});

	*out_result = (CF_PreprocessFileResult) {
		.pp_errors = ctx.errors,
		.pp_error_count = ctx.error_count,
		.token_count = ctx.token_count,
		.token_kinds = ctx.token_kinds,
		.source_traces = ctx.source_traces,
	};
	return ctx.err;
}
