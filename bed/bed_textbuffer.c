#include "bed.h"

static TextBuffer*
MakeTextBuffer_(App* app, intz* out_index)
{
	if (app->textbuf_pool_count+1 >= app->textbuf_pool_cap)
	{
		intz new_cap = app->textbuf_pool_cap + (app->textbuf_pool_cap >> 1) + 1;
		AllocatorError err;
		if (!AllocatorResizeArrayOk(&app->heap, new_cap, sizeof(TextBuffer), alignof(TextBuffer), &app->textbuf_pool, app->textbuf_pool_cap, &err))
		{
			Log(LOG_ERROR, "could not resize textbuf pool: %u", (uint32)err);
			*out_index = 0;
			return NULL;
		}
		app->textbuf_pool_cap = new_cap;
	}

	intz index = app->textbuf_pool_count++;
	*out_index = index+1;
	return &app->textbuf_pool[index];
}

BED_API TextBuffer*
TextBufferFromIndex(App* app, intz index)
{
	Trace();
	if (index <= 0 || index > app->textbuf_pool_count)
	{
		Log(LOG_WARN, "index into text buffer pool is invalid: %Z", index);
		return NULL;
	}
	TextBuffer* textbuf = &app->textbuf_pool[index - 1];
	if (textbuf->next_free || !textbuf->kind)
	{
		Log(LOG_WARN, "text buffer reffered by index %Z is not active", index);
		return NULL;
	}
	return textbuf;
}

BED_API TextBuffer*
TextBufferFromFile(App* app, String path, TextBufferKind kind, intz* out_index)
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

	TextBuffer* textbuf = MakeTextBuffer_(app, out_index);
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
		ArenaRestore(scratch);
		return NULL;
	}
	MemoryCopy(utf8_text, buffer, size);

	*textbuf = (TextBuffer) {
		.kind = kind,
		.utf8_text =utf8_text,
		.size = total_size,
		.gap_start = size,
		.gap_end = total_size,
	};
	return textbuf;
}

BED_API TextBuffer*
TextBufferFromString(App* app, String str, TextBufferKind kind, intz* out_index)
{
	Trace();
	TextBuffer* textbuf = MakeTextBuffer_(app, out_index);
	if (!textbuf)
	{
		Log(LOG_ERROR, "could not make text buffer from string: %S", str);
		return NULL;
	}

	AllocatorError alloc_err = 0;
	intz total_size = AlignUp(str.size, 31);
	uint8* utf8_text = AllocatorAlloc(&app->heap, total_size, 1, &alloc_err);
	if (alloc_err)
	{
		Log(LOG_ERROR, "could not allocate buffer for text buffer from string: %S", str);
		return NULL;
	}
	MemoryCopy(utf8_text, str.data, str.size);

	*textbuf = (TextBuffer) {
		.kind = kind,
		.utf8_text =utf8_text,
		.size = total_size,
		.gap_start = str.size,
		.gap_end = total_size,
	};
	return textbuf;
}

BED_API void
TextBufferRefreshTokens(App* app, TextBuffer* textbuf)
{
	Trace();
	if (textbuf->change_start == -1 && textbuf->cf_cap)
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
			CF_LexingFlag_IncludeComments,
		.kind_allocator = app->heap,
		.source_range_allocator = app->heap,
	});
	SafeAssert(!err);
	ArenaRestore(scratch);

	textbuf->cf_cap = lex.allocated_token_count;
	textbuf->cf_count = lex.token_count;
	textbuf->cf_kinds = lex.token_kinds;
	textbuf->cf_ranges = lex.source_ranges;
	textbuf->change_start = -1;
	textbuf->change_end = -1;
}

BED_API void
TextBufferGetStrings(TextBuffer* textbuf, String* out_left, String* out_right)
{
	Trace();
	if (out_left)
		*out_left = StrMake(textbuf->gap_start, textbuf->utf8_text);
	if (out_right)
		*out_right = StrMake(textbuf->size - textbuf->gap_end, textbuf->utf8_text + textbuf->gap_end);
}

BED_API uint8
TextBufferSample(TextBuffer* textbuf, intz offset)
{
	Trace();
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
	Trace();
	return textbuf->size - (textbuf->gap_end - textbuf->gap_start);
}

BED_API int32
TextBufferLineCount(TextBuffer* textbuf)
{
	Trace();
	int32 result = 1;
	String left_str = {};
	String right_str = {};
	TextBufferGetStrings(textbuf, &left_str, &right_str);

	for (intz i = 0; i < left_str.size; ++i)
		result += (left_str.data[i] == '\n');
	for (intz i = 0; i < right_str.size; ++i)
		result += (right_str.data[i] == '\n');

	return result;
}

BED_API LineCol
TextBufferLineColFromOffset(TextBuffer* textbuf, intz offset, int32 tab_size)
{
	Trace();
	LineCol result = { 1, 1 };
	String left_str = {};
	String right_str = {};
	TextBufferGetStrings(textbuf, &left_str, &right_str);
	intz it;
	uint32 codepoint;

	it = 0;
	while (codepoint = StringDecode(left_str, &it), codepoint && it <= offset)
	{
		if (codepoint == '\n')
		{
			result.col = 1;
			++result.line;
		}
		else if (codepoint == '\t')
			result.col += tab_size;
		else
			++result.col;
	}

	it = 0;
	while (codepoint = StringDecode(right_str, &it), codepoint && it + textbuf->gap_start <= offset)
	{
		if (codepoint == '\n')
		{
			result.col = 1;
			++result.line;
		}
		else if (codepoint == '\t')
			result.col += tab_size;
		else
			++result.col;
	}

	return result;
}

BED_API int32
TextBufferColFromOffset(TextBuffer* textbuf, intz offset, int32 tab_size)
{
	Trace();
	int32 result = 1;
	intz it = offset;

	while (it > 0)
	{
		--it;
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
			return result;
		else if (sample == '\t')
			result += tab_size;
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

BED_API uint8*
TextBufferInsert(App* app, TextBuffer* textbuf, intz offset, intz size)
{
	Trace();
	if (size > textbuf->gap_end - textbuf->gap_start)
	{
		intz new_size = textbuf->size + (textbuf->size >> 1) + 1;
		intz gap_size = textbuf->gap_end - textbuf->gap_start;
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
	textbuf->gap_start += size;
	if (textbuf->change_start == -1)
	{
		textbuf->change_start = offset;
		textbuf->change_end = offset + size;
	}
	else
	{
		textbuf->change_start = Min(textbuf->change_start, offset);
		textbuf->change_end = Max(textbuf->change_end, offset + size);
	}
	return result;
}

BED_API void
TextBufferDelete(TextBuffer* textbuf, intz offset, intz size)
{
	Trace();
	if (offset == textbuf->gap_start)
		textbuf->gap_end += size;
	else if (offset + size == textbuf->gap_start)
		textbuf->gap_start -= size;
	else
	{
		TextBufferMoveGapToOffset(textbuf, offset);
		textbuf->gap_end += size;
	}

	if (textbuf->change_start == -1)
	{
		textbuf->change_start = offset;
		textbuf->change_end = offset;
	}
	else
	{
		if (textbuf->change_start > offset)
			textbuf->change_start -= size;
		if (textbuf->change_end > offset+size)
			textbuf->change_end -= size;
		else if (textbuf->change_end > offset)
			textbuf->change_end = offset;
	}
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