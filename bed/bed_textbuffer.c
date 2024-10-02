#include "bed.h"
#include <immintrin.h>

static TextBuffer*
AllocTextBuffer_(App* app, TextBufferHandle* out_handle)
{
	Trace();
	intz index;

	if (app->textbuf_pool_first_free)
	{
		index = app->textbuf_pool_first_free - 1;
		app->textbuf_pool_first_free = app->textbuf_pool[index].next_free;
	}
	else
	{
		if (app->textbuf_pool_count+1 >= app->textbuf_pool_cap)
		{
			intz new_cap = app->textbuf_pool_cap + (app->textbuf_pool_cap >> 1) + 1;
			AllocatorError err;
			if (!AllocatorResizeArrayOk(&app->heap, new_cap, sizeof(TextBuffer), alignof(TextBuffer), &app->textbuf_pool, app->textbuf_pool_cap, &err))
			{
				Log(LOG_ERROR, "could not resize textbuf pool: %u", (uint32)err);
				*out_handle = 0;
				return NULL;
			}
			app->textbuf_pool_cap = new_cap;
		}
		index = app->textbuf_pool_count++;
	}

	TextBuffer* textbuf = &app->textbuf_pool[index];
	*out_handle = (TextBufferHandle)((uint64)index+1 | (uint64)textbuf->generation << 48);
	return &app->textbuf_pool[index];
}

static void
FreeTextBuffer_(App* app, TextBuffer* textbuf, intz index)
{
	Trace();
	if (textbuf->utf8_text)
		AllocatorFree(&app->heap, textbuf->utf8_text, textbuf->size, NULL);
	if (textbuf->cf_kinds)
		AllocatorFreeArray(&app->heap, sizeof(CF_TokenKind), textbuf->cf_kinds, textbuf->cf_cap, NULL);
	if (textbuf->cf_ranges)
		AllocatorFreeArray(&app->heap, sizeof(CF_SourceRange), textbuf->cf_ranges, textbuf->cf_cap, NULL);
	if (textbuf->edits)
		AllocatorFreeArray(&app->heap, sizeof(TextBufferEdit), textbuf->edits, textbuf->edits_cap, NULL);
	if (textbuf->edit_text_buffer)
		AllocatorFree(&app->heap, textbuf->edit_text_buffer, textbuf->edit_text_buffer_cap, NULL);
	if (textbuf->file_absolute_path.size)
		AllocatorFree(&app->heap, (void*)textbuf->file_absolute_path.data, textbuf->file_absolute_path.size, NULL);
	uint32 generation = (uint16)(textbuf->generation + 1);
	MemoryZero(textbuf, sizeof(*textbuf));
	textbuf->next_free = (uint32)app->textbuf_pool_first_free;
	textbuf->generation = generation;
	app->textbuf_pool_first_free = index+1;
}

static intz
PushEditText_(App* app, TextBuffer* textbuf, intz deleted_size)
{
	Trace();
	if (textbuf->edit_text_buffer_size + deleted_size > textbuf->edit_text_buffer_cap)
	{
		AllocatorError err;
		intz desired_cap = textbuf->edit_text_buffer_cap + (textbuf->edit_text_buffer_cap>>1) + 1;
		desired_cap = Max(desired_cap, textbuf->edit_text_buffer_size + deleted_size);
		if (!AllocatorResizeOk(&app->heap, desired_cap, 1, &textbuf->edit_text_buffer, textbuf->edit_text_buffer_cap, &err))
		{
			Log(LOG_ERROR, "could not resize textbuf edit text buffer: %u", (uint32)err);
			return -1;
		}
		textbuf->edit_text_buffer_cap = desired_cap;
	}

	intz offset = textbuf->edit_text_buffer_size;
	textbuf->edit_text_buffer_size += deleted_size;
	textbuf->edit_text_buffer_usable_size = textbuf->edit_text_buffer_size;
	return offset;
}

static void
PushEdit_(App* app, TextBuffer* textbuf, intz offset, intz size, String changed, bool is_insertion, uint64 tick)
{
	Trace();
	SafeAssert(size == changed.size);
	AllocatorError err;
	Range changed_text_range = {};
	if (changed.size)
	{
		intz text_offset = PushEditText_(app, textbuf, changed.size);
		if (text_offset == -1)
			return;
		changed_text_range = (Range) {
			.start = text_offset,
			.end = text_offset + changed.size,
		};
		MemoryCopy(
			textbuf->edit_text_buffer + text_offset,
			changed.data,
			changed.size);
	}

	if (textbuf->edits_count+1 > textbuf->edits_cap)
	{
		intz desired_cap = textbuf->edits_cap + (textbuf->edits_cap>>1) + 1;
		if (!AllocatorResizeArrayOk(&app->heap, desired_cap, sizeof(TextBufferEdit), alignof(TextBufferEdit), &textbuf->edits, textbuf->edits_cap, &err))
		{
			Log(LOG_ERROR, "could not resize textbuf edit buffer: %u", (uint32)err);
			return;
		}
		textbuf->edits_cap = desired_cap;
	}

	textbuf->edits[textbuf->edits_count++] = (TextBufferEdit) {
		.kind = is_insertion ? TextBufferEditKind_Insert : TextBufferEditKind_Delete,
		.tick = tick,
		.insert = {
			.edit_range = {
				offset,
				offset + size,
			},
			.edit_text_buffer = changed_text_range,
		},
	};
	textbuf->edits_usable_count = textbuf->edits_count;
}

static void
PushTranposeEdit_(App* app, TextBuffer* textbuf, Range first, Range second, uint64 tick)
{
	Trace();
	AllocatorError err;
	if (textbuf->edits_count+1 > textbuf->edits_cap)
	{
		intz desired_cap = textbuf->edits_cap + (textbuf->edits_cap>>1) + 1;
		if (!AllocatorResizeArrayOk(&app->heap, desired_cap, sizeof(TextBufferEdit), alignof(TextBufferEdit), &textbuf->edits, textbuf->edits_cap, &err))
		{
			Log(LOG_ERROR, "could not resize textbuf edit buffer: %u", (uint32)err);
			return;
		}
		textbuf->edits_cap = desired_cap;
	}

	textbuf->edits[textbuf->edits_count++] = (TextBufferEdit) {
		.kind = TextBufferEditKind_Transpose,
		.tick = tick,
		.transpose = {
			.from = first,
			.to = second,
		},
	};
	textbuf->edits_usable_count = textbuf->edits_count;
}

static void
InsertWithoutMakingEdit_(App* app, TextBuffer* textbuf, intz offset, String str)
{
	Trace();
	intz gap_size = textbuf->gap_end - textbuf->gap_start;
	SafeAssert(gap_size >= 0);
	SafeAssert(textbuf->size >= gap_size);
	SafeAssert(offset >= 0 && offset <= textbuf->size - gap_size);

	if (str.size > gap_size)
	{
		intz new_size = textbuf->size + (textbuf->size >> 1) + 1;
		new_size = Max(new_size, textbuf->size + str.size);
		new_size = AlignUp(new_size, 31);
		intz new_gap_size = new_size - (textbuf->size - gap_size);
		AllocatorError err;
		uint8* new_buffer = AllocatorAlloc(&app->heap, new_size, 1, &err);
		SafeAssert(!err);
		if (offset < textbuf->gap_start)
		{
			intz size_to_gap = textbuf->gap_start - offset;
			MemoryCopy(new_buffer, textbuf->utf8_text, offset);
			MemoryCopy(new_buffer+offset+new_gap_size, textbuf->utf8_text+offset, size_to_gap);
			MemoryCopy(new_buffer+offset+new_gap_size+size_to_gap, textbuf->utf8_text+textbuf->gap_end, textbuf->size - textbuf->gap_end);
		}
		else if (offset > textbuf->gap_start)
		{
			intz size_to_gap = textbuf->gap_end - (offset + gap_size);
			MemoryCopy(new_buffer, textbuf->utf8_text, offset);
			MemoryCopy(new_buffer+offset+new_gap_size, textbuf->utf8_text+offset, size_to_gap);
			MemoryCopy(new_buffer+offset+new_gap_size+size_to_gap, textbuf->utf8_text+textbuf->gap_end, textbuf->size - textbuf->gap_end);
		}
		else
		{
			MemoryCopy(new_buffer, textbuf->utf8_text, offset);
			MemoryCopy(new_buffer+offset+new_gap_size, textbuf->utf8_text+textbuf->gap_end, textbuf->size - textbuf->gap_end);
		}
		AllocatorFree(&app->heap, textbuf->utf8_text, textbuf->size, &err);
		SafeAssert(!err);
		textbuf->utf8_text = new_buffer;
		textbuf->gap_start = offset;
		textbuf->gap_end = offset + new_gap_size;
		textbuf->size = new_size;
	}
	else if (offset != textbuf->gap_start)
		TextBufferMoveGapToOffset(textbuf, offset);

	uint8* result = &textbuf->utf8_text[textbuf->gap_start];
	MemoryCopy(result, str.data, str.size);
	textbuf->gap_start += str.size;

	// Dirty changes
	if (textbuf->dirty_changes_start == -1)
	{
		textbuf->dirty_changes_start = offset;
		textbuf->dirty_changes_end = offset + str.size;
	}
	else
	{
		textbuf->dirty_changes_start = Min(textbuf->dirty_changes_start, offset);
		textbuf->dirty_changes_end = Max(textbuf->dirty_changes_end, offset + str.size);
	}
}

static String
DeleteWithoutMakingEdit_(TextBuffer* textbuf, intz offset, intz size)
{
	Trace();
	String result = {};
	if (offset == textbuf->gap_start)
	{
		result = StrMake(size, textbuf->utf8_text + textbuf->gap_end);
		textbuf->gap_end += size;
	}
	else if (offset + size == textbuf->gap_start)
	{
		result = StrMake(size, textbuf->utf8_text + textbuf->gap_start - size);
		textbuf->gap_start -= size;
	}
	else
	{
		TextBufferMoveGapToOffset(textbuf, offset);
		result = StrMake(size, textbuf->utf8_text + textbuf->gap_end);
		textbuf->gap_end += size;
	}

	// Dirty changes
	if (textbuf->dirty_changes_start == -1)
	{
		textbuf->dirty_changes_start = offset;
		textbuf->dirty_changes_end = offset + size;
	}
	else
	{
		Range range = RangeMakeSized(offset, size);
		textbuf->dirty_changes_start = RangeRemoveFromOffset(textbuf->dirty_changes_start, range);
		textbuf->dirty_changes_end = RangeRemoveFromOffset(textbuf->dirty_changes_end, range);
	}

	return result;
}

static void
TransposeWithoutMakingEdit_(App* app, TextBuffer* textbuf, Range first, Range second)
{
	Trace();
	SafeAssert(!RangesIntersects(first, second));
	SafeAssert(RangeInside(first, RangeMakeSized(0, TextBufferSize(textbuf))));
	SafeAssert(RangeInside(second, RangeMakeSized(0, TextBufferSize(textbuf))));

	if (first.start > second.start)
	{
		Range tmp = second;
		second = first;
		first = tmp;
	}

	ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
	intz first_size = RangeSize(first);
	uint8* first_mem = ArenaPushAligned(scratch.arena, first_size, 1);
	Buffer first_buffer = TextBufferWriteRangeToBuffer(textbuf, first, first_size, first_mem);
	intz second_size = RangeSize(second);
	uint8* second_mem = ArenaPushAligned(scratch.arena, second_size, 1);
	Buffer second_buffer = TextBufferWriteRangeToBuffer(textbuf, second, second_size, second_mem);

	if (second_size)
		DeleteWithoutMakingEdit_(textbuf, second.start, second_size);
	if (first_size)
		InsertWithoutMakingEdit_(app, textbuf, second.start, first_buffer);
	if (first_size)
		DeleteWithoutMakingEdit_(textbuf, first.start, first_size);
	if (second_size)
		InsertWithoutMakingEdit_(app, textbuf, first.start, second_buffer);
}

static int32
CountLinesInTwoStrings_(String left, String right)
{
	Trace();
	int32 result = 1;

#if CONFIG_ARCH_SSELEVEL < 51
	for (intz i = 0; i < left_str.size; ++i)
		result += (left_str.data[i] == '\n');
	for (intz i = 0; i < right_str.size; ++i)
		result += (right_str.data[i] == '\n');
#else
	__m256i linebreak = _mm256_set1_epi8('\n');
	__m256i lsbmask = _mm256_set1_epi32(0xFF);
	__m256i acc = _mm256_setzero_si256();
	__m256i zero = _mm256_setzero_si256();

	intz i;
	for (i = 0; i+31 < left.size; i += 32)
	{
		__m256i chars = _mm256_loadu_si256((void*)&left.data[i]);
		chars = _mm256_cmpeq_epi8(chars, linebreak);
		chars = _mm256_sub_epi8(zero, chars);
		
		__m256i v0 = chars;
		__m256i v1 = _mm256_srli_epi32(chars, 8);
		__m256i v2 = _mm256_srli_epi32(chars, 16);
		__m256i v3 = _mm256_srli_epi32(chars, 24);

		v0 = _mm256_and_si256(v0, lsbmask);
		v1 = _mm256_and_si256(v1, lsbmask);
		v2 = _mm256_and_si256(v2, lsbmask);

		__m256i total = _mm256_add_epi32(_mm256_add_epi32(v0, v1), _mm256_add_epi32(v2, v3));
		acc = _mm256_add_epi32(acc, total);
	}
	for (; i < left.size; ++i)
		result += (left.data[i] == '\n');
	for (i = 0; i+31 < right.size; i += 32)
	{
		__m256i chars = _mm256_loadu_si256((void*)&right.data[i]);
		chars = _mm256_cmpeq_epi8(chars, linebreak);
		chars = _mm256_sub_epi8(zero, chars);
		
		__m256i v0 = chars;
		__m256i v1 = _mm256_srli_epi32(chars, 8);
		__m256i v2 = _mm256_srli_epi32(chars, 16);
		__m256i v3 = _mm256_srli_epi32(chars, 24);

		v0 = _mm256_and_si256(v0, lsbmask);
		v1 = _mm256_and_si256(v1, lsbmask);
		v2 = _mm256_and_si256(v2, lsbmask);

		__m256i total = _mm256_add_epi32(_mm256_add_epi32(v0, v1), _mm256_add_epi32(v2, v3));
		acc = _mm256_add_epi32(acc, total);
	}
	for (; i < right.size; ++i)
		result += (right.data[i] == '\n');

	acc = _mm256_add_epi32(acc, _mm256_permute2f128_ps(acc, acc, 1));
	result += _mm256_extract_epi32(acc, 0);
	result += _mm256_extract_epi32(acc, 1);
	result += _mm256_extract_epi32(acc, 2);
	result += _mm256_extract_epi32(acc, 3);
#endif

	return result;
}

BED_API TextBuffer*
TextBufferFromHandle(App* app, TextBufferHandle handle)
{
	intz index = (uint64)handle << 16 >> 16;
	uint32 generation = (uint64)handle >> 48;
	--index;
	if (index < 0 || index >= app->textbuf_pool_count)
	{
		Log(LOG_ERROR, "index into text buffer pool is invalid: %Z", index);
		return NULL;
	}
	TextBuffer* textbuf = &app->textbuf_pool[index];
	if (textbuf->generation != generation)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z has different generation than expected: %u != %u", index, generation, textbuf->generation);
		return NULL;
	}
	if (textbuf->next_free || !textbuf->kind)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z is not active", index);
		return NULL;
	}
	return textbuf;
}

BED_API TextBuffer*
TextBufferFromFile(App* app, String path, TextBufferKind kind, TextBufferHandle* out_handle)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(app->arena);
	void* buffer;
	intz size;
	OS_Error err = {};

	if (!OS_ReadEntireFile(path, scratch.arena, &buffer, &size, &err))
	{
		Log(LOG_ERROR, "could not open file from path!\n\tPath: %S\nError code: %u\nError string: %S", path, err.code, err.what);
		return NULL;
	}

	TextBuffer* textbuf = AllocTextBuffer_(app, out_handle);
	if (!textbuf)
	{
		Log(LOG_ERROR, "could not make text buffer when opening path: %S", path);
		ArenaRestore(scratch);
		return NULL;
	}

	AllocatorError alloc_err = 0;
	intz total_size = AlignUp(size, 31) + 4096;
	uint8* utf8_text = AllocatorAlloc(&app->heap, total_size, 1, &alloc_err);
	if (alloc_err)
	{
		Log(LOG_ERROR, "could not allocate buffer for text buffer when opening path: %S", path);
		FreeTextBuffer_(app, textbuf, ((uint64)*out_handle << 16 >> 16) - 1);
		ArenaRestore(scratch);
		return NULL;
	}
	MemoryCopy(utf8_text, buffer, size);

	String absolute_path = OS_ResolveToAbsolutePath(path, app->heap, &err);
	if (!err.ok)
	{
		Log(LOG_ERROR, "could resolve path to full string when opening path: %S", path);
		FreeTextBuffer_(app, textbuf, ((uint64)*out_handle << 16 >> 16) - 1);
		ArenaRestore(scratch);
		return NULL;
	}

	*textbuf = (TextBuffer) {
		.generation = textbuf->generation,
		.kind = kind,
		.ref_count = 1,
		.utf8_text = utf8_text,
		.size = total_size,
		.gap_start = size,
		.gap_end = total_size,
		.file_absolute_path = absolute_path,
	};
	return textbuf;
}

BED_API TextBuffer*
TextBufferFromString(App* app, String str, TextBufferKind kind, TextBufferHandle* out_handle)
{
	Trace();
	TextBuffer* textbuf = AllocTextBuffer_(app, out_handle);
	if (!textbuf)
	{
		Log(LOG_ERROR, "could not make text buffer from string: %S", str);
		return NULL;
	}

	AllocatorError alloc_err = 0;
	intz total_size = AlignUp(str.size, 31);
	total_size = ClampMin(total_size, 4096);
	uint8* utf8_text = AllocatorAlloc(&app->heap, total_size, 1, &alloc_err);
	if (alloc_err)
	{
		Log(LOG_ERROR, "could not allocate buffer for text buffer from string: %S", str);
		return NULL;
	}
	MemoryCopy(utf8_text, str.data, str.size);

	*textbuf = (TextBuffer) {
		.generation = textbuf->generation,
		.kind = kind,
		.ref_count = 1,
		.utf8_text = utf8_text,
		.size = total_size,
		.gap_start = str.size,
		.gap_end = total_size,
	};
	return textbuf;
}

BED_API TextBuffer*
TextBufferIncRefCount(App* app, TextBufferHandle handle)
{
	Trace();
	intz index = (uint64)handle << 16 >> 16;
	uint32 generation = (uint64)handle >> 48;
	--index;
	if (index < 0 || index >= app->textbuf_pool_count)
	{
		Log(LOG_ERROR, "trying to acquire invalid textbuffer index: %Z", index);
		return NULL;
	}
	TextBuffer* textbuf = &app->textbuf_pool[index];
	if (textbuf->generation != generation)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z has different generation than expected: %u != %u", index, generation, textbuf->generation);
		return NULL;
	}
	if (textbuf->next_free || !textbuf->kind)
	{
		Log(LOG_ERROR, "trying to acquire textbuffer that is inactive", index);
		return NULL;
	}

	++textbuf->ref_count;
	return textbuf;
}

BED_API void
TextBufferDecRefCount(App* app, TextBufferHandle handle)
{
	Trace();
	intz index = (uint64)handle << 16 >> 16;
	uint32 generation = (uint64)handle >> 48;
	--index;
	if (index < 0 || index >= app->textbuf_pool_count)
	{
		Log(LOG_ERROR, "trying to release invalid textbuffer index: %Z", index);
		return;
	}
	TextBuffer* textbuf = &app->textbuf_pool[index];
	if (textbuf->generation != generation)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z has different generation than expected: %u != %u", index, generation, textbuf->generation);
		return;
	}
	if (textbuf->next_free || !textbuf->kind)
	{
		Log(LOG_ERROR, "trying to release textbuffer that is inactive", index);
		return;
	}
	
	if (--textbuf->ref_count == 0)
		FreeTextBuffer_(app, textbuf, index);
}

BED_API void
TextBufferRefreshTokens(App* app, TextBuffer* textbuf)
{
	Trace();
	if (textbuf->dirty_changes_start == -1 && textbuf->cf_cap)
	{
		// NOTE: no change has been made, nothing to do
		return;
	}

	AllocatorError err;
	if (textbuf->cf_cap)
	{
		AllocatorFreeArray(&app->heap, sizeof(CF_TokenKind), textbuf->cf_kinds, textbuf->cf_cap, &err);
		SafeAssert(!err);
		AllocatorFreeArray(&app->heap, sizeof(CF_SourceRange), textbuf->cf_ranges, textbuf->cf_cap, &err);
		SafeAssert(!err);
	}

	ArenaSavepoint scratch = ArenaSave(app->arena);
	String source = {};
	{
		String left, right;
		TextBufferGetStrings(textbuf, &left, &right);
		uint8* mem = ArenaPushDirtyAligned(app->arena, left.size+right.size, 1);
		MemoryCopy(mem, left.data, left.size);
		MemoryCopy(mem+left.size, right.data, right.size);
		source = StrMake(left.size+right.size, mem);
	}

	CF_TokensFromStringResult lex = {};
	err = CF_TokensFromString(&lex, &(CF_TokensFromStringArgs) {
		.str = source,
		.standard = CF_Standard_C23,
		.lexing_flags = 
			CF_LexingFlag_AllowMsvc |
			CF_LexingFlag_AllowGnu |
			CF_LexingFlag_AllowContextSensetiveKeywords |
			CF_LexingFlag_AllowCxx |
			CF_LexingFlag_IncludeComments |
			CF_LexingFlag_IncludeEscapedLinebreaks |
			CF_LexingFlag_IncludeLinebreaks,
		.kind_allocator = app->heap,
		.source_range_allocator = app->heap,
	});
	SafeAssert(!err);
	ArenaRestore(scratch);

	textbuf->cf_cap = lex.allocated_token_count;
	textbuf->cf_count = lex.token_count;
	textbuf->cf_kinds = lex.token_kinds;
	textbuf->cf_ranges = lex.source_ranges;
	textbuf->dirty_changes_start = -1;
	textbuf->dirty_changes_end = -1;
}

BED_API void
TextBufferGetStrings(TextBuffer* textbuf, String* out_left, String* out_right)
{
	if (out_left)
		*out_left = StrMake(textbuf->gap_start, textbuf->utf8_text);
	if (out_right)
		*out_right = StrMake(textbuf->size - textbuf->gap_end, textbuf->utf8_text + textbuf->gap_end);
}

BED_API uint8
TextBufferSample(TextBuffer* textbuf, intz offset)
{
	intz gap_size = textbuf->gap_end - textbuf->gap_start;
	SafeAssert(gap_size >= 0 && gap_size <= textbuf->size);
	SafeAssert(offset >= 0 && offset < textbuf->size - gap_size);

	if (offset < textbuf->gap_start)
		return textbuf->utf8_text[offset];
	return textbuf->utf8_text[offset + gap_size];
}

BED_API intz
TextBufferSize(TextBuffer* textbuf)
{
	return textbuf->size - (textbuf->gap_end - textbuf->gap_start);
}

BED_API int32
TextBufferLineCount(TextBuffer* textbuf)
{
	Trace();
	String left_str = {};
	String right_str = {};
	TextBufferGetStrings(textbuf, &left_str, &right_str);

	return CountLinesInTwoStrings_(left_str, right_str);
}

BED_API LineCol
TextBufferLineColFromOffset(TextBuffer* textbuf, intz offset, int32 tab_size)
{
	Trace();
	LineCol result = { 1, 1 };
	String left_str = {};
	String right_str = {};
	TextBufferGetStrings(textbuf, &left_str, &right_str);

	if (offset > textbuf->gap_start)
		right_str = StringSlice(right_str, 0, offset - textbuf->gap_start);
	else
	{
		left_str = StringSlice(left_str, 0, offset);
		right_str = StrNull;
	}

	result.line = CountLinesInTwoStrings_(left_str, right_str);
	result.col = TextBufferColFromOffset(textbuf, offset, tab_size);

	return result;
}

BED_API int32
TextBufferColFromOffset(TextBuffer* textbuf, intz offset, int32 tab_size)
{
	Trace();
	int32 result = 1;

	intz start_of_line = offset-1;
	for (; start_of_line >= 0; --start_of_line)
	{
		uint8 sample = TextBufferSample(textbuf, start_of_line);
		if (sample == '\n')
			break;
	}
	for (intz it = start_of_line+1; it < offset; ++it)
	{
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\t')
			result = RoundToNextTab(result-1, tab_size) + 1;
		else if (IsStartOfCodepoint(sample))
			++result;
	}

	return result;
}

BED_API void
TextBufferMoveGapToOffset(TextBuffer* textbuf, intz offset)
{
	Trace();
	if (offset < textbuf->gap_start)
	{
		intz amount_to_copy = textbuf->gap_start - offset;
		SafeAssert(amount_to_copy > 0);
		intz new_gap_end = textbuf->gap_end - amount_to_copy;
		MemoryMove(textbuf->utf8_text + new_gap_end, textbuf->utf8_text + offset, amount_to_copy);
		textbuf->gap_start -= amount_to_copy;
		textbuf->gap_end -= amount_to_copy;
	}
	else if (offset > textbuf->gap_start)
	{
		intz amount_to_copy = offset - textbuf->gap_start;
		SafeAssert(amount_to_copy > 0);
		MemoryMove(textbuf->utf8_text + textbuf->gap_start, textbuf->utf8_text + textbuf->gap_end, amount_to_copy);
		textbuf->gap_start += amount_to_copy;
		textbuf->gap_end += amount_to_copy;
	}
}

BED_API void
TextBufferInsert(App* app, TextBuffer* textbuf, intz offset, String str)
{
	Trace();
	InsertWithoutMakingEdit_(app, textbuf, offset, str);

	// Edits
	uint64 now = OS_Tick();
	if (textbuf->edits_count > 0 &&
		textbuf->edits[textbuf->edits_count-1].kind == TextBufferEditKind_Insert &&
		textbuf->edits[textbuf->edits_count-1].insert.edit_range.end == offset &&
		textbuf->edits[textbuf->edits_count-1].tick >= now - app->tick_rate / 10 * 3)
	{
		TextBufferEdit* edit = &textbuf->edits[textbuf->edits_count-1];
		PushEditText_(app, textbuf, str.size);

		uint8* mem = textbuf->edit_text_buffer + edit->insert.edit_text_buffer.start;
		intz prev_size = RangeSize(edit->insert.edit_text_buffer);
		MemoryCopy(mem + prev_size, str.data, str.size);
		edit->insert.edit_range.end += str.size;
		edit->insert.edit_text_buffer.end += str.size;
		edit->tick = now;
	}
	else
		PushEdit_(app, textbuf, offset, str.size, str, true, now);
}

BED_API void
TextBufferDelete(App* app, TextBuffer* textbuf, intz offset, intz size)
{
	Trace();
	String deleted_str = DeleteWithoutMakingEdit_(textbuf, offset, size);
	SafeAssert(deleted_str.size == size);

	// Edits
	uint64 now = OS_Tick();
	if (textbuf->edits_count > 0 &&
		textbuf->edits[textbuf->edits_count-1].kind == TextBufferEditKind_Delete &&
		textbuf->edits[textbuf->edits_count-1].delete.edit_range.start == offset + size &&
		textbuf->edits[textbuf->edits_count-1].tick >= now - app->tick_rate / 10 * 3 && false)
	{
		TextBufferEdit* edit = &textbuf->edits[textbuf->edits_count-1];
		PushEditText_(app, textbuf, size);

		uint8* mem = textbuf->edit_text_buffer + edit->delete.edit_text_buffer.start;
		intz prev_size = RangeSize(edit->delete.edit_text_buffer);
		MemoryMove(mem + size, mem, prev_size);
		MemoryCopy(mem, deleted_str.data, size);
		edit->delete.edit_range.start -= size;
		edit->delete.edit_text_buffer.end += size;
		edit->tick = now;
	}
	else
		PushEdit_(app, textbuf, offset, size, deleted_str, false, now);
}

BED_API void
TextBufferTranspose(App* app, TextBuffer* textbuf, Range first, Range second)
{
	Trace();
	TransposeWithoutMakingEdit_(app, textbuf, first, second);

	// Edits
	uint64 now = OS_Tick();
	if (false)
	{
		// TODO(ljre): Accumulate transpose edits
	}
	else
		PushTranposeEdit_(app, textbuf, first, second, now);
}

BED_API String
TextBufferStringFromRange(TextBuffer* textbuf, intz offset, intz size)
{
	Trace();
	if (offset < textbuf->gap_start && offset + size > textbuf->gap_start)
		TextBufferMoveGapToOffset(textbuf, offset);

	if (offset < textbuf->gap_start)
		return StrMake(size, textbuf->utf8_text+offset);
	else
		return StrMake(size, textbuf->utf8_text+offset+(textbuf->gap_end-textbuf->gap_start));
}

BED_API bool
TextBufferLineIterator(TextBuffer* textbuf, intz* it_, String* left_str, String* right_str)
{
	Trace();
	intz it = *it_;
	if (it >= TextBufferSize(textbuf))
		return false;
	
	intz start = it;
	intz end = it;
	
#if 0
	for (;; ++it)
	{
		if (it >= TextBufferSize(textbuf))
			break;

		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
			break;
	}
	end = it;
	++it;
#else
	String left, right;
	TextBufferGetStrings(textbuf, &left, &right);
	bool found = false;

	if (it < left.size)
	{
		String rem = StringSlice(left, it, -1);
		uint8 const* ptr_end = MemoryFindByte(rem.data, '\n', rem.size);
		if (ptr_end)
		{
			end = ptr_end - left.data;
			it = end + 1;
			found = true;
		}
		else
			it = left.size;
	}

	if (!found)
	{
		String rem = StringSlice(right, it - left.size, -1);
		uint8 const* ptr_end = MemoryFindByte(rem.data, '\n', rem.size);
		if (!ptr_end)
			ptr_end = rem.data + rem.size;

		end = left.size + (ptr_end - right.data);
		it = end + 1;
	}
#endif

	if (start < textbuf->gap_start && end > textbuf->gap_start)
	{
		*left_str = StrMake(textbuf->gap_start - start, textbuf->utf8_text + start);
		*right_str = StrMake(end - textbuf->gap_start, textbuf->utf8_text + textbuf->gap_end);
	}
	else if (start >= textbuf->gap_start)
	{
		*left_str = StrMake(end - start, textbuf->utf8_text + start + (textbuf->gap_end-textbuf->gap_start));
		*right_str = StrNull;
	}
	else
	{
		*left_str = StrMake(end - start, textbuf->utf8_text + start);
		*right_str = StrNull;
	}

	*it_ = it;
	return true;
}

BED_API intz
TextBufferOffsetFromLineCol(TextBuffer* textbuf, LineCol pos, int32 tab_size)
{
	Trace();
	intz it = 0;

	int32 line = 1;
	for (; line < pos.line && it < TextBufferSize(textbuf); ++it)
	{
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
			++line;
	}
	if (line == pos.line)
	{
		int32 col = 1;
		for (; col < pos.col && it < TextBufferSize(textbuf); ++it)
		{
			uint8 sample = TextBufferSample(textbuf, it);
			if (sample == '\n')
				break;
			col += (sample == '\t') ? tab_size : 1;
		}
	}

	return it;
}

BED_API bool
TextBufferUndo(App* app, TextBuffer* textbuf, TextBufferEdit* out_edit)
{
	Trace();
	if (textbuf->edits_count <= 0)
		return false;

	TextBufferEdit* edit = &textbuf->edits[--textbuf->edits_count];
	intz offset = -1;
	intz size = -1;
	switch (edit->kind)
	{
		case TextBufferEditKind_Delete:
		{
			offset = edit->delete.edit_range.start;
			size = RangeSize(edit->delete.edit_text_buffer);
			String str = StrMake(size, textbuf->edit_text_buffer + edit->delete.edit_text_buffer.start);
			InsertWithoutMakingEdit_(app, textbuf, offset, str);
			textbuf->edit_text_buffer_size = edit->delete.edit_text_buffer.end;
		} break;
		case TextBufferEditKind_Insert:
		{
			offset = edit->insert.edit_range.start;
			size = RangeSize(edit->insert.edit_range);
			DeleteWithoutMakingEdit_(textbuf, offset, size);
			textbuf->edit_text_buffer_size = edit->insert.edit_text_buffer.end;
		} break;
		case TextBufferEditKind_Transpose:
		{
			Range first = edit->transpose.from;
			Range second = edit->transpose.to;
			Range containing = RangeContaining(first, second);
			offset = containing.start;
			size = RangeSize(containing);
			RangesTranpose(&first, &second);
			TransposeWithoutMakingEdit_(app, textbuf, first, second);
		} break;
	}
	SafeAssert(offset != -1 && size != -1);

	if (out_edit)
		*out_edit = *edit;
	return true;
}

BED_API bool
TextBufferRedo(App* app, TextBuffer* textbuf, TextBufferEdit* out_edit)
{
	Trace();
	if (textbuf->edits_count >= textbuf->edits_usable_count)
		return false;

	TextBufferEdit* edit = &textbuf->edits[textbuf->edits_count++];
	intz offset = -1;
	intz size = -1;
	switch (edit->kind)
	{
		case TextBufferEditKind_Delete:
		{
			offset = edit->delete.edit_range.start;
			size = RangeSize(edit->delete.edit_range);
			DeleteWithoutMakingEdit_(textbuf, offset, size);
			textbuf->edit_text_buffer_size = edit->delete.edit_text_buffer.end;
			SafeAssert(textbuf->edit_text_buffer_size <= textbuf->edit_text_buffer_usable_size);
		} break;
		case TextBufferEditKind_Insert:
		{
			offset = edit->insert.edit_range.start;
			size = RangeSize(edit->insert.edit_text_buffer);
			String str = StrMake(size, textbuf->edit_text_buffer + edit->insert.edit_text_buffer.start);
			InsertWithoutMakingEdit_(app, textbuf, offset, str);
			textbuf->edit_text_buffer_size = edit->insert.edit_text_buffer.end;
			SafeAssert(textbuf->edit_text_buffer_size <= textbuf->edit_text_buffer_usable_size);
		} break;
		case TextBufferEditKind_Transpose:
		{
			Range first = edit->transpose.from;
			Range second = edit->transpose.to;
			Range containing = RangeContaining(first, second);
			offset = containing.start;
			size = containing.end - containing.start;
			TransposeWithoutMakingEdit_(app, textbuf, first, second);
		} break;
	}
	SafeAssert(offset != -1 && size != -1);

	if (out_edit)
		*out_edit = *edit;
	return offset;
}

BED_API void
TextBufferReplace(App* app, TextBuffer* textbuf, Range range, String str)
{
	Trace();
	// TODO(ljre): Rewrite this so that Undo does both at once
	TextBufferDelete(app, textbuf, range.start, RangeSize(range));
	TextBufferInsert(app, textbuf, range.start, str);
}

BED_API Buffer
TextBufferWriteRangeToBuffer(TextBuffer* textbuf, Range range, intz size, uint8 buf[])
{
	Trace();
	SafeAssert(RangeIsValid(range));
	intz textbuf_size = TextBufferSize(textbuf);
	intz range_size = RangeSize(range);
	size = Min(size, range_size);
	size = Min(size, textbuf_size - range.start);

	if (range.end <= textbuf->gap_start)
		MemoryCopy(buf, textbuf->utf8_text + range.start, size);
	else if (range.start < textbuf->gap_start && textbuf->gap_start < range.end)
	{
		intz before_gap = textbuf->gap_start - range.start;
		MemoryCopy(buf, textbuf->utf8_text + range.start, before_gap);
		MemoryCopy(buf + before_gap, textbuf->utf8_text + textbuf->gap_end, size - before_gap);
	}
	else
		MemoryCopy(buf, textbuf->utf8_text + range.start + (textbuf->gap_end - textbuf->gap_start), size);

	return BufMake(size, buf);
}

BED_API bool
TextBufferIterate(App* app, intz* it_, TextBuffer** out_textbuf, TextBufferHandle* out_handle)
{
	Trace();
	intz it = *it_;

	for (; it < app->textbuf_pool_count; ++it)
	{
		TextBuffer* textbuf = &app->textbuf_pool[it];
		if (textbuf->kind)
		{
			*it_ = it + 1;
			*out_textbuf = textbuf;
			*out_handle = (void*)((uint64)textbuf->generation << 48 | (uint64)(it+1));
			return true;
		}
	}

	*it_ = it;
	*out_textbuf = NULL;
	*out_handle = NULL;
	return false;
}
