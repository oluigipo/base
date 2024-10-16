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

static bool
SimpleBoundaryPascalWordProc_(uint8 ch)
{
	if (ch >= 0x80)
		return false;
	static uint8 pred_table[16] = {
		0,   0,   0,   0,   0,   0, 255,   3,
        0, 0, 0, 0, 254, 255, 255,   7,
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
TextCursorCmdInsert(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount, uint32 codepoint)
{
	Trace();
	intz encoded_size = StringEncodedCodepointSize(codepoint);
	intz size = amount * encoded_size;

	ArenaSavepoint scratch = ArenaSave(app->arena);
	uint8* insert_buffer = ArenaPushDirtyAligned(scratch.arena, size, 1);
	for (intz i = 0; i < amount; ++i)
		StringEncode(insert_buffer + i * encoded_size, size, codepoint);

	TextBufferInsert(app, textbuf, cursor->offset, StrMake(size, insert_buffer));
	if (cursor->marker_offset >= cursor->offset)
		cursor->marker_offset += size;
	cursor->offset += size;
}

BED_API void
TextCursorCmdInsertString(App* app, TextCursor* cursor, TextBuffer* textbuf, String str)
{
	TextBufferInsert(app, textbuf, cursor->offset, str);
	if (cursor->marker_offset >= cursor->offset)
		cursor->marker_offset += str.size;
	cursor->offset += str.size;
}

BED_API void
TextCursorCmdDeleteBackward(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount)
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

	TextBufferDelete(app, textbuf, it, size);
	if (cursor->marker_offset >= cursor->offset)
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
	Trace();
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
TextCursorCmdDeleteToMarker(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
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

	TextBufferDelete(app, textbuf, offset, size);
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
TextCursorCmdRightPascalWord(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		cursor->offset = FindSimpleBoundaryForward_(textbuf, cursor->offset, SimpleBoundaryPascalWordProc_);
		if (cursor->offset == TextBufferSize(textbuf))
			break;
	}
}

BED_API void
TextCursorCmdLeftPascalWord(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		cursor->offset = FindSimpleBoundaryBackward_(textbuf, cursor->offset, SimpleBoundaryPascalWordProc_);
		if (cursor->offset == 0)
			break;
	}
}

BED_API void
TextCursorCmdDeleteBackwardSnakeWord(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		if (cursor->offset == 0)
			break;
		intz start = FindSimpleBoundaryBackward_(textbuf, cursor->offset, SimpleBoundarySnakeWordProc_);
		TextBufferDelete(app, textbuf, start, cursor->offset - start);
		if (cursor->marker_offset >= cursor->offset)
			cursor->marker_offset -= (cursor->offset - start);
		cursor->offset -= (cursor->offset - start);
	}
}

BED_API void
TextCursorCmdDeleteBackwardPascalWord(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		if (cursor->offset == 0)
			break;
		intz start = FindSimpleBoundaryBackward_(textbuf, cursor->offset, SimpleBoundaryPascalWordProc_);
		TextBufferDelete(app, textbuf, start, cursor->offset - start);
		if (cursor->marker_offset >= cursor->offset)
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
TextCursorCmdCut(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	TextCursorCmdCopy(app, cursor, textbuf);
	TextCursorCmdDeleteToMarker(app, cursor, textbuf);
}

BED_API void
TextCursorCmdPaste(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
	OS_ClipboardContents clip = OS_GetClipboard(AllocatorFromArena(scratch.arena), OS_ClipboardContentType_Text, app->window, NULL);

	for (intz i = 0; i < amount; ++i)
	{
		TextBufferInsert(app, textbuf, cursor->offset, clip.contents);
		if (cursor->marker_offset >= cursor->offset)
			cursor->marker_offset += clip.contents.size;
		cursor->offset += clip.contents.size;
	}

	SafeAssert(cursor->offset >= 0 && cursor->offset <= TextBufferSize(textbuf));
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

BED_API void
TextCursorCmdSet(TextCursor* cursor, TextBuffer* textbuf, LineCol pos, int32 tab_size)
{
	Trace();
	cursor->offset = TextBufferOffsetFromLineCol(textbuf, pos, tab_size);
}

BED_API void
TextCursorCmdSetMarker(TextCursor* cursor, TextBuffer* textbuf, LineCol pos, int32 tab_size)
{
	Trace();
	cursor->marker_offset = TextBufferOffsetFromLineCol(textbuf, pos, tab_size);
}

BED_API void
TextCursorCmdUndo(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	TextBufferEdit edit = {};
	if (TextBufferUndo(app, textbuf, &edit))
	{
		switch (edit.kind)
		{
			case TextBufferEditKind_Delete:
			{
				Range range = edit.delete.edit_range;
				intz size = RangeSize(range);
				if (cursor->marker_offset > range.start)
					cursor->marker_offset += size;
				cursor->offset = range.end;
			} break;
			case TextBufferEditKind_Insert:
			{
				Range range = edit.insert.edit_range;
				intz size = RangeSize(range);
				if (cursor->marker_offset > range.end)
					cursor->marker_offset -= size;
				else if (cursor->marker_offset >= range.start)
					cursor->marker_offset = range.start;
				cursor->offset = range.start;
			} break;
			case TextBufferEditKind_Transpose:
			{
				Range from = edit.transpose.from;
				// Range to = edit.transpose.to;
				// RangesTranpose(&from, &to);
				cursor->offset = from.start;
				cursor->marker_offset = from.start;
			} break;
		}
	}
}

BED_API void
TextCursorCmdRedo(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	TextBufferEdit edit = {};
	if (TextBufferRedo(app, textbuf, &edit))
	{
		switch (edit.kind)
		{
			case TextBufferEditKind_Delete:
			{
				Range range = edit.delete.edit_range;
				intz size = RangeSize(range);
				if (cursor->marker_offset > range.end)
					cursor->marker_offset -= size;
				else if (cursor->marker_offset >= range.start)
					cursor->marker_offset = range.start;
				cursor->offset = range.start;
			} break;
			case TextBufferEditKind_Insert:
			{
				Range range = edit.insert.edit_range;
				intz size = RangeSize(range);
				if (cursor->marker_offset > range.start)
					cursor->marker_offset += size;
				cursor->offset = range.end;
			} break;
			case TextBufferEditKind_Transpose:
			{
				Range from = edit.transpose.from;
				Range to = edit.transpose.to;
				RangesTranpose(&from, &to);
				cursor->offset = to.start;
				cursor->marker_offset = to.start;
			} break;
		}
	}
}

BED_API void
TextCursorCmdDeleteLine(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	TextCursorCmdStartOfLine(cursor, textbuf);
	intz start = cursor->offset;
	TextCursorCmdEndOfLine(cursor, textbuf);
	intz end = cursor->offset;
	if (end < TextBufferSize(textbuf))
		++end; // include linebreak

	intz size = end - start;
	if (size)
	{
		TextBufferDelete(app, textbuf, start, size);
		if (cursor->marker_offset >= start + size)
			cursor->marker_offset -= size;
		else if (cursor->marker_offset >= start)
			cursor->marker_offset = start;
		cursor->offset = start;
	}
}

BED_API void
TextCursorCmdMoveLineUp(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	for (intz i = 0; i < amount; ++i)
	{
		TextCursorCmdEndOfLine(cursor, textbuf);
		intz first_end = cursor->offset;
		TextCursorCmdStartOfLine(cursor, textbuf);
		intz first_start = cursor->offset;
		if (first_start == 0)
			break;
		TextCursorCmdUp(cursor, textbuf, 1);
		TextCursorCmdEndOfLine(cursor, textbuf);
		intz second_end = cursor->offset;
		TextCursorCmdStartOfLine(cursor, textbuf);
		intz second_start = cursor->offset;

		Range from = RangeMake(first_start, first_end);
		Range to = RangeMake(second_start, second_end);
		TextBufferTranspose(app, textbuf, from, to);
		RangesTranpose(&from, &to);
		cursor->offset = to.start;
		cursor->marker_offset = to.start;
	}
}

BED_API void
TextCursorCmdMoveLineDown(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	for (intz i = 0; i < amount; ++i)
	{
		TextCursorCmdEndOfLine(cursor, textbuf);
		intz first_end = cursor->offset;
		if (first_end == TextBufferSize(textbuf))
			break;
		TextCursorCmdStartOfLine(cursor, textbuf);
		intz first_start = cursor->offset;
		TextCursorCmdDown(cursor, textbuf, 1);
		TextCursorCmdEndOfLine(cursor, textbuf);
		intz second_end = cursor->offset;
		TextCursorCmdStartOfLine(cursor, textbuf);
		intz second_start = cursor->offset;

		Range from = RangeMake(first_start, first_end);
		Range to = RangeMake(second_start, second_end);
		TextBufferTranspose(app, textbuf, from, to);
		RangesTranpose(&from, &to);
		cursor->offset = to.start;
		cursor->marker_offset = to.start;
	}
}

BED_API void
TextCursorCmdDuplicateLine(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	intz original_offset = cursor->offset;
	TextCursorCmdStartOfLine(cursor, textbuf);
	intz start = cursor->offset;
	intz offset_from_start = original_offset - start;
	TextCursorCmdEndOfLine(cursor, textbuf);
	intz end = cursor->offset;
	Range range = RangeMake(start, end);
	intz size = RangeSize(range);

	ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
	uint8* buf = ArenaPushAligned(scratch.arena, size + 1, 1);
	buf[0] = '\n';
	TextBufferWriteRangeToBuffer(textbuf, range, size, buf + 1);
	String str = StrMake(size + 1, buf);

	for (intz i = 0; i < amount; ++i)
		TextCursorCmdInsertString(app, cursor, textbuf, str);

	TextCursorCmdStartOfLine(cursor, textbuf);
	cursor->offset += offset_from_start;
}
