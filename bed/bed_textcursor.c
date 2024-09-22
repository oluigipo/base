#include "bed.h"

static bool
SimpleBoundarySnakeWordProc_(uint8 ch)
{
	if (ch >= 0x80)
		return false;
	// NOTE(ljre): Proudly taken from 4coder because i like it
	//             https://github.com/4coder-archive/4coder/blob/c38c7384b91ccb89a88a2d209649b62950d33f09/custom/4coder_helper.h#L112-L117
	static uint8 pred_table[16] = {
		0,   0,   0,   0,   0,   0, 255,   3,
        254, 255, 255, 7, 254, 255, 255,   7,
	};
	return !(pred_table[ch >> 3] & 1 << (ch & 7));
}

typedef bool SimpleBoundaryProc_(uint8 ch);

static intz
FindSimpleBoundaryForward_(TextBuffer* textbuf, intz start_offset, SimpleBoundaryProc_* proc)
{
	Trace();
	intz it = start_offset;
	intz last_valid_it = it;
	intz textbuf_size = TextBufferSize(textbuf);

	for (; it < textbuf_size; ++it)
	{
		uint8 sample = TextBufferSample(textbuf, it);
		if (!IsStartOfCodepoint(sample))
			continue;
		last_valid_it = it;
		if (sample & 0x80)
			continue;

		if (!proc(sample))
			break;
	}

	for (; it < textbuf_size; ++it)
	{
		uint8 sample = TextBufferSample(textbuf, it);
		if (!IsStartOfCodepoint(sample))
			continue;
		last_valid_it = it;
		if (sample & 0x80)
			continue;

		if (proc(sample))
			break;
	}

	if (it == textbuf_size)
		last_valid_it = it;
	return last_valid_it;
}

static intz
FindSimpleBoundaryBackward_(TextBuffer* textbuf, intz start_offset, SimpleBoundaryProc_* proc)
{
	Trace();
	intz it = start_offset;

	for (; it > 0; --it)
	{
		uint8 sample = TextBufferSample(textbuf, it-1);
		if (!IsStartOfCodepoint(sample))
			continue;
		if (sample & 0x80)
			continue;

		if (!proc(sample))
			break;
	}
	
	for (; it > 0; --it)
	{
		uint8 sample = TextBufferSample(textbuf, it-1);
		if (!IsStartOfCodepoint(sample))
			continue;
		if (sample & 0x80)
			continue;

		if (proc(sample))
			break;
	}

	// for (; it > 0; --it)
	// {
	// 	uint8 sample = TextBufferSample_(textbuf, it);
	// 	if (!IsStartOfCodepoint_(sample))
	// 		continue;
	// 	if (sample & 0x80)
	// 		continue;

	// 	if (proc(sample))
	// 		break;
	// }

	return it;
}

BED_API void
TextCursorCmdInsert(TextCursor* cursor, TextBuffer* textbuf, intz amount, uint32 codepoint)
{
	Trace();
	intz encoded_size = StringEncodedCodepointSize(codepoint);
	intz size = amount * encoded_size;

	uint8* insert_buffer = TextBufferInsert(textbuf, cursor->offset, size);
	if (cursor->offset < cursor->marker_offset)
		cursor->marker_offset += size;
	cursor->offset += size;

	for (intz i = 0; i < amount; ++i)
		StringEncode(insert_buffer + i * encoded_size, size, codepoint);
}

BED_API void
TextCursorCmdDeleteBackward(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();

	intz it = cursor->offset;
	while (amount > 0 && it > 0)
	{
		--it;
		uint8 sample = TextBufferSample(textbuf, it);
		if (IsStartOfCodepoint(sample))
			--amount;
	}
	intz size = cursor->offset - it;

	TextBufferDelete(textbuf, it, size);
	if (cursor->offset < cursor->marker_offset)
		cursor->marker_offset -= size;
	cursor->offset = it;
}

BED_API void
TextCursorCmdLeft(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	intz it = cursor->offset;
	
	while (amount > 0 && it > 0)
	{
		--it;
		uint8 sample = TextBufferSample(textbuf, it);
		if (IsStartOfCodepoint(sample))
			--amount;
	}

	cursor->offset = it;
}

BED_API void
TextCursorCmdRight(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	intz it = cursor->offset;
	intz last_valid_it = it;
	
	while (amount > 0)
	{
		if (it >= TextBufferSize(textbuf))
			break;
		if (it+1 >= TextBufferSize(textbuf))
		{
			last_valid_it = ++it;
			break;
		}

		++it;
		uint8 sample = TextBufferSample(textbuf, it);
		if (IsStartOfCodepoint(sample))
		{
			last_valid_it = it;
			--amount;
		}
	}

	cursor->offset = last_valid_it;
}

BED_API void
TextCursorCmdUp(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	intz it = cursor->offset;
	intz desired_column = TextBufferColFromOffset(textbuf, cursor->offset, 1);

	// NOTE(ljre): find the start of the line N times, then advance to desired column
	++amount;
	while (amount > 0 && it > 0)
	{
		--it;
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
			--amount;
	}

	if (amount > 0)
	{
		// NOTE(ljre): reached start of buffer before finding another '\n', so it's one less column to advance
		if (amount == 1)
			--desired_column;
		else
			desired_column = 0;
	}

	// NOTE(ljre): advance to desired column
	intz last_valid_it = it;
	while (desired_column > 0 && it+1 < TextBufferSize(textbuf))
	{
		++it;
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			break;
		}
		if (IsStartOfCodepoint(sample))
		{
			last_valid_it = it;
			--desired_column;
		}
	}

	cursor->offset = last_valid_it;
}

BED_API void
TextCursorCmdDown(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	intz it = cursor->offset;
	intz last_valid_it = it;
	intz desired_column = TextBufferColFromOffset(textbuf, cursor->offset, 1);

	if (it >= TextBufferSize(textbuf))
		return;

	// NOTE(ljre): find the start of the line N times, then advance to desired column
	while (amount > 0 && it < TextBufferSize(textbuf))
	{
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			--amount;
		}
		else if (IsStartOfCodepoint(sample))
			last_valid_it = it;
		++it;
	}

	// NOTE(ljre): advance to desired column
	while (desired_column > 0 && it < TextBufferSize(textbuf))
	{
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			break;
		}
		else if (IsStartOfCodepoint(sample))
		{
			last_valid_it = it;
			--desired_column;
		}
		++it;
	}

	if (it == TextBufferSize(textbuf))
		last_valid_it = it;

	cursor->offset = last_valid_it;
}

BED_API void
TextCursorCmdEndOfLine(TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	intz it = cursor->offset;
	intz last_valid_it = it;
	bool found_eol = false;

	if (it >= TextBufferSize(textbuf))
		return;
	if (TextBufferSample(textbuf, it) == '\n')
		return;

	while (!found_eol && it < TextBufferSize(textbuf))
	{
		uint8 sample = TextBufferSample(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			found_eol = true;
		}
		else if (IsStartOfCodepoint(sample))
			last_valid_it = it;
		++it;
	}

	if (it == TextBufferSize(textbuf))
		last_valid_it = it;

	cursor->offset = last_valid_it;
}

BED_API void
TextCursorCmdStartOfLine(TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	intz it = cursor->offset;
	bool found_sol = false;

	while (!found_sol && it > 0)
	{
		uint8 sample = TextBufferSample(textbuf, it-1);
		if (sample == '\n')
		{
			found_sol = true;
			break;
		}
		--it;
	}

	cursor->offset = it;
}

BED_API void
TextCursorCmdPlaceMarker(TextCursor* cursor)
{
	Trace();
	cursor->marker_offset = cursor->offset;
}

BED_API void
TextCursorCmdDeleteToMarker(TextCursor* cursor, TextBuffer* textbuf)
{
	intz offset, size;
	if (cursor->offset < cursor->marker_offset)
	{
		offset = cursor->offset;
		size = cursor->marker_offset - cursor->offset;
	}
	else if (cursor->offset > cursor->marker_offset)
	{
		offset = cursor->marker_offset;
		size = cursor->offset - cursor->marker_offset;
	}
	else
		return;

	TextBufferDelete(textbuf, offset, size);
	cursor->offset = offset;
	cursor->marker_offset = offset;
}

BED_API void
TextCursorCmdRightSnakeWord(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		cursor->offset = FindSimpleBoundaryForward_(textbuf, cursor->offset, SimpleBoundarySnakeWordProc_);
		if (cursor->offset == TextBufferSize(textbuf))
			break;
	}
}

BED_API void
TextCursorCmdLeftSnakeWord(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		cursor->offset = FindSimpleBoundaryBackward_(textbuf, cursor->offset, SimpleBoundarySnakeWordProc_);
		if (cursor->offset == 0)
			break;
	}
}

BED_API void
TextCursorCmdDeleteBackwardSnakeWord(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		if (cursor->offset == 0)
			break;
		intz start = FindSimpleBoundaryBackward_(textbuf, cursor->offset, SimpleBoundarySnakeWordProc_);
		TextBufferDelete(textbuf, start, cursor->offset - start);
		if (cursor->marker_offset > cursor->offset)
			cursor->marker_offset -= (cursor->offset - start);
		cursor->offset -= (cursor->offset - start);
	}
}

BED_API void
TextCursorCmdCopy(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	intz offset;
	intz size;
	if (cursor->offset < cursor->marker_offset)
	{
		offset = cursor->offset;
		size = cursor->marker_offset - cursor->offset;
	}
	else
	{
		offset = cursor->marker_offset;
		size = cursor->offset - cursor->marker_offset;
	}

	String str = TextBufferStringFromRange(textbuf, offset, size);
	OS_SetClipboard((OS_ClipboardContents) {
		.type = OS_ClipboardContentType_Text,
		.contents = str,
	}, app->window, NULL);
}

BED_API void
TextCursorCmdPaste(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
	OS_ClipboardContents clip = OS_GetClipboard(AllocatorFromArena(scratch.arena), OS_ClipboardContentType_Text, app->window, NULL);

	uint8* mem = TextBufferInsert(textbuf, cursor->offset, clip.contents.size);
	if (cursor->marker_offset > cursor->offset)
		cursor->marker_offset += clip.contents.size;
	cursor->offset += clip.contents.size;

	MemoryCopy(mem, clip.contents.data, clip.contents.size);
	ArenaRestore(scratch);
}

BED_API void
TextCursorCmdUpParagraph(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		bool encontered_non_whitespace_before = false;
		while (cursor->offset > 0)
		{
			TextCursorCmdLeft(cursor, textbuf, 1);
			TextCursorCmdStartOfLine(cursor, textbuf);
			intz it = cursor->offset;
			bool has_non_whitespace = false;
			for (; it < TextBufferSize(textbuf); ++it)
			{
				uint8 sample = TextBufferSample(textbuf, it);
				if (sample == '\n')
					break;
				if (sample != ' ' && sample != '\t')
				{
					has_non_whitespace = true;
					break;
				}
			}
			if (has_non_whitespace)
				encontered_non_whitespace_before = true;
			else if (encontered_non_whitespace_before)
			{
				TextCursorCmdEndOfLine(cursor, textbuf);
				break;
			}
		}
	}
}

BED_API void
TextCursorCmdDownParagraph(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		bool encontered_non_whitespace_before = false;
		TextCursorCmdDown(cursor, textbuf, 1);
		while (cursor->offset < TextBufferSize(textbuf))
		{
			TextCursorCmdStartOfLine(cursor, textbuf);
			intz it = cursor->offset;
			bool has_non_whitespace = false;
			for (; it < TextBufferSize(textbuf); ++it)
			{
				uint8 sample = TextBufferSample(textbuf, it);
				if (sample == '\n')
					break;
				if (sample != ' ' && sample != '\t')
				{
					has_non_whitespace = true;
					break;
				}
			}

			if (has_non_whitespace)
				encontered_non_whitespace_before = true;
			else if (encontered_non_whitespace_before)
			{
				TextCursorCmdEndOfLine(cursor, textbuf);
				break;
			}
			TextCursorCmdDown(cursor, textbuf, 1);
		}
	}
}
