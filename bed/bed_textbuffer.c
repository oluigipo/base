#include "bed.h"

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
TextBufferInsert(TextBuffer* textbuf, intz offset, intz size)
{
	Trace();
	if (size > textbuf->gap_end - textbuf->gap_start)
	{
		intz new_size = textbuf->size + (textbuf->size >> 1) + 1;
		intz gap_size = textbuf->gap_end - textbuf->gap_start;
		intz new_gap_size = new_size - (textbuf->size - gap_size);
		uint8* new_buffer = OS_HeapAlloc(new_size);
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
		OS_HeapFree(textbuf->utf8_text);
		textbuf->utf8_text = new_buffer;
		textbuf->gap_start = offset;
		textbuf->gap_end = offset + new_gap_size;
		textbuf->size = new_size;
	}
	else if (offset != textbuf->gap_start)
		TextBufferMoveGapToOffset(textbuf, offset);

	uint8* result = &textbuf->utf8_text[textbuf->gap_start];
	textbuf->gap_start += size;
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
