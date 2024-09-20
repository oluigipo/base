#include "common.h"
#include "api_os.h"
#include "api_render3.h"
#include "bed.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#define INITGUID
#include "api_os_win32.h"
#include "third_party/cd2d.h"
#include "third_party/cdwrite.h"
#include <d3d11_1.h>

#include <math.h>

IncludeBinary(g_quads_vs, "shader_quad_vs.dxil");
IncludeBinary(g_quads_ps, "shader_quad_ps.dxil");

static bool
IsStartOfCodepoint_(uint8 ch)
{
	return (ch & 0x80) == 0 || (ch & 0x40) == 0;
}

static void
TextBufferGetString_(TextBuffer const* textbuf, String* out_left, String* out_right)
{
	if (out_left)
		*out_left = StrMake(textbuf->gap_start, textbuf->utf8_text);
	if (out_right)
		*out_right = StrMake(textbuf->size - textbuf->gap_end, textbuf->utf8_text + textbuf->gap_end);
}

static uint8
TextBufferSample_(TextBuffer* textbuf, intz offset)
{
	intz gap_size = textbuf->gap_end - textbuf->gap_start;
	SafeAssert(gap_size >= 0 && gap_size <= textbuf->size);
	SafeAssert(offset >= 0 && offset < textbuf->size - gap_size);

	if (offset < textbuf->gap_start)
		return textbuf->utf8_text[offset];
	return textbuf->utf8_text[offset + gap_size];
}

static intz
TextBufferSize_(TextBuffer* textbuf)
{
	return textbuf->size - (textbuf->gap_end - textbuf->gap_start);
}

static int32
TextBufferLineCount_(TextBuffer* textbuf)
{
	int32 result = 1;
	String left_str = {};
	String right_str = {};
	TextBufferGetString_(textbuf, &left_str, &right_str);

	for (intz i = 0; i < left_str.size; ++i)
		result += (left_str.data[i] == '\n');
	for (intz i = 0; i < right_str.size; ++i)
		result += (right_str.data[i] == '\n');

	return result;
}

static LineCol
TextBufferLineColFromOffset_(TextBuffer* textbuf, intz offset, int32 tab_size)
{
	LineCol result = { 1, 1 };
	String left_str = {};
	String right_str = {};
	TextBufferGetString_(textbuf, &left_str, &right_str);
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

static int32
ColFromTextCursor_(TextCursor* cursor, TextBuffer* textbuf, int32 tab_size)
{
	int32 result = 1;
	intz it = cursor->offset;

	while (it > 0)
	{
		--it;
		uint8 sample = TextBufferSample_(textbuf, it);
		if (sample == '\n')
			return result;
		else if (sample == '\t')
			result += tab_size;
		else if (IsStartOfCodepoint_(sample))
			++result;
	}

	return result;
}

static bool
SimpleBoundarySnakeWordProc_(uint8 ch)
{
	if (ch >= 0x80)
		return false;
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
	intz textbuf_size = TextBufferSize_(textbuf);

	for (; it < textbuf_size; ++it)
	{
		uint8 sample = TextBufferSample_(textbuf, it);
		if (!IsStartOfCodepoint_(sample))
			continue;
		last_valid_it = it;
		if (sample & 0x80)
			continue;

		if (!proc(sample))
			break;
	}

	for (; it < textbuf_size; ++it)
	{
		uint8 sample = TextBufferSample_(textbuf, it);
		if (!IsStartOfCodepoint_(sample))
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
		uint8 sample = TextBufferSample_(textbuf, it-1);
		if (!IsStartOfCodepoint_(sample))
			continue;
		if (sample & 0x80)
			continue;

		if (!proc(sample))
			break;
	}
	
	for (; it > 0; --it)
	{
		uint8 sample = TextBufferSample_(textbuf, it-1);
		if (!IsStartOfCodepoint_(sample))
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

static void
TextBufferMoveGapToOffset_(TextBuffer* textbuf, intz offset)
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

static uint8*
TextBufferInsert_(TextBuffer* textbuf, intz offset, intz size)
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
		TextBufferMoveGapToOffset_(textbuf, offset);

	uint8* result = &textbuf->utf8_text[textbuf->gap_start];
	textbuf->gap_start += size;
	return result;
}

static void
TextBufferRemove_(TextBuffer* textbuf, intz offset, intz size)
{
	if (offset == textbuf->gap_start)
		textbuf->gap_end += size;
	else if (offset + size == textbuf->gap_start)
		textbuf->gap_start -= size;
	else
	{
		TextBufferMoveGapToOffset_(textbuf, offset);
		textbuf->gap_end += size;
	}
}

static String
TextBufferStringFromRange_(TextBuffer* textbuf, intz offset, intz size)
{
	if (offset < textbuf->gap_start && offset + size > textbuf->gap_start)
		TextBufferMoveGapToOffset_(textbuf, offset);

	if (offset < textbuf->gap_start)
		return StrMake(size, textbuf->utf8_text+offset);
	else
		return StrMake(size, textbuf->utf8_text+offset+(textbuf->gap_end-textbuf->gap_start));
}

static void
TextCursorCmdInsert_(TextCursor* cursor, TextBuffer* textbuf, intz amount, uint32 codepoint)
{
	Trace();
	intz encoded_size = StringEncodedCodepointSize(codepoint);
	intz size = amount * encoded_size;

	uint8* insert_buffer = TextBufferInsert_(textbuf, cursor->offset, size);
	if (cursor->offset < cursor->marker_offset)
		cursor->marker_offset += size;
	cursor->offset += size;

	for (intz i = 0; i < amount; ++i)
		StringEncode(insert_buffer + i * encoded_size, size, codepoint);
}

static void
TextCursorCmdDeleteBackward_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();

	intz it = cursor->offset;
	while (amount > 0 && it > 0)
	{
		--it;
		uint8 sample = TextBufferSample_(textbuf, it);
		if (IsStartOfCodepoint_(sample))
			--amount;
	}
	intz size = cursor->offset - it;

	TextBufferRemove_(textbuf, it, size);
	if (cursor->offset < cursor->marker_offset)
		cursor->marker_offset -= size;
	cursor->offset = it;
}

static void
TextCursorCmdLeft_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	intz it = cursor->offset;
	
	while (amount > 0 && it > 0)
	{
		--it;
		uint8 sample = TextBufferSample_(textbuf, it);
		if (IsStartOfCodepoint_(sample))
			--amount;
	}

	cursor->offset = it;
}

static void
TextCursorCmdRight_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	intz it = cursor->offset;
	intz last_valid_it = it;
	
	while (amount > 0 && it+1 < TextBufferSize_(textbuf))
	{
		++it;
		uint8 sample = TextBufferSample_(textbuf, it);
		if (IsStartOfCodepoint_(sample))
		{
			last_valid_it = it;
			--amount;
		}
	}

	cursor->offset = last_valid_it;
}

static void
TextCursorCmdUp_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	intz it = cursor->offset;
	intz desired_column = ColFromTextCursor_(cursor, textbuf, 1);

	// NOTE(ljre): find the start of the line N times, then advance to desired column
	++amount;
	while (amount > 0 && it > 0)
	{
		--it;
		uint8 sample = TextBufferSample_(textbuf, it);
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
	while (desired_column > 0 && it+1 < TextBufferSize_(textbuf))
	{
		++it;
		uint8 sample = TextBufferSample_(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			break;
		}
		if (IsStartOfCodepoint_(sample))
		{
			last_valid_it = it;
			--desired_column;
		}
	}

	cursor->offset = last_valid_it;
}

static void
TextCursorCmdDown_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	intz it = cursor->offset;
	intz last_valid_it = it;
	intz desired_column = ColFromTextCursor_(cursor, textbuf, 1);

	if (it >= TextBufferSize_(textbuf))
		return;

	// NOTE(ljre): find the start of the line N times, then advance to desired column
	while (amount > 0 && it < TextBufferSize_(textbuf))
	{
		uint8 sample = TextBufferSample_(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			--amount;
		}
		else if (IsStartOfCodepoint_(sample))
			last_valid_it = it;
		++it;
	}

	// NOTE(ljre): advance to desired column
	while (desired_column > 0 && it < TextBufferSize_(textbuf))
	{
		uint8 sample = TextBufferSample_(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			break;
		}
		else if (IsStartOfCodepoint_(sample))
		{
			last_valid_it = it;
			--desired_column;
		}
		++it;
	}

	if (it == TextBufferSize_(textbuf))
		last_valid_it = it;

	cursor->offset = last_valid_it;
}

static void
TextCursorCmdEndOfLine_(TextCursor* cursor, TextBuffer* textbuf)
{
	intz it = cursor->offset;
	intz last_valid_it = it;
	bool found_eol = false;

	if (it >= TextBufferSize_(textbuf))
		return;
	if (TextBufferSample_(textbuf, it) == '\n')
		return;

	while (!found_eol && it < TextBufferSize_(textbuf))
	{
		uint8 sample = TextBufferSample_(textbuf, it);
		if (sample == '\n')
		{
			last_valid_it = it;
			found_eol = true;
		}
		else if (IsStartOfCodepoint_(sample))
			last_valid_it = it;
		++it;
	}

	if (it == TextBufferSize_(textbuf))
		last_valid_it = it;

	cursor->offset = last_valid_it;
}

static void
TextCursorCmdStartOfLine_(TextCursor* cursor, TextBuffer* textbuf)
{
	intz it = cursor->offset;
	bool found_sol = false;

	while (!found_sol && it > 0)
	{
		uint8 sample = TextBufferSample_(textbuf, it-1);
		if (sample == '\n')
		{
			found_sol = true;
			break;
		}
		--it;
	}

	cursor->offset = it;
}

static void
TextCursorCmdPlaceMarker_(TextCursor* cursor)
{
	cursor->marker_offset = cursor->offset;
}

static void
TextCursorCmdDeleteToMarker_(TextCursor* cursor, TextBuffer* textbuf)
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

	TextBufferRemove_(textbuf, offset, size);
	cursor->offset = offset;
	cursor->marker_offset = offset;
}

static void
TextCursorCmdRightSnakeWord_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	for (intz i = 0; i < amount; ++i)
	{
		cursor->offset = FindSimpleBoundaryForward_(textbuf, cursor->offset, SimpleBoundarySnakeWordProc_);
		if (cursor->offset == TextBufferSize_(textbuf))
			break;
	}
}

static void
TextCursorCmdLeftSnakeWord_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	for (intz i = 0; i < amount; ++i)
	{
		cursor->offset = FindSimpleBoundaryBackward_(textbuf, cursor->offset, SimpleBoundarySnakeWordProc_);
		if (cursor->offset == 0)
			break;
	}
}

static void
TextCursorCmdDeleteBackwardSnakeWord_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	for (intz i = 0; i < amount; ++i)
	{
		if (cursor->offset == 0)
			break;
		intz start = FindSimpleBoundaryBackward_(textbuf, cursor->offset, SimpleBoundarySnakeWordProc_);
		TextBufferRemove_(textbuf, start, cursor->offset - start);
		if (cursor->marker_offset > cursor->offset)
			cursor->marker_offset -= (cursor->offset - start);
		cursor->offset = start;
	}
}

static void
TextCursorCmdPaste_(App* app, TextCursor* cursor, TextBuffer* textbuf)
{
	Trace();
	ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
	OS_ClipboardContents clip = OS_GetClipboard(AllocatorFromArena(scratch.arena), OS_ClipboardContentType_Text, app->window, NULL);

	uint8* mem = TextBufferInsert_(textbuf, cursor->offset, clip.contents.size);
	if (cursor->marker_offset > cursor->offset)
		cursor->marker_offset += clip.contents.size;
	cursor->offset += clip.contents.size;

	MemoryCopy(mem, clip.contents.data, clip.contents.size);
	ArenaRestore(scratch);
}

static void
TextCursorCmdCopy_(App* app, TextCursor* cursor, TextBuffer* textbuf)
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

	String str = TextBufferStringFromRange_(textbuf, offset, size);
	OS_SetClipboard((OS_ClipboardContents) {
		.type = OS_ClipboardContentType_Text,
		.contents = str,
	}, app->window, NULL);
}

static void
TextCursorCmdUpParagraph_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		bool encontered_non_whitespace_before = false;
		while (cursor->offset > 0)
		{
			TextCursorCmdLeft_(cursor, textbuf, 1);
			TextCursorCmdStartOfLine_(cursor, textbuf);
			intz it = cursor->offset;
			bool has_non_whitespace = false;
			for (; it < TextBufferSize_(textbuf); ++it)
			{
				uint8 sample = TextBufferSample_(textbuf, it);
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
				TextCursorCmdEndOfLine_(cursor, textbuf);
				break;
			}
		}
	}
}

static void
TextCursorCmdDownParagraph_(TextCursor* cursor, TextBuffer* textbuf, intz amount)
{
	Trace();
	for (intz i = 0; i < amount; ++i)
	{
		bool encontered_non_whitespace_before = false;
		TextCursorCmdDown_(cursor, textbuf, 1);
		while (cursor->offset < TextBufferSize_(textbuf))
		{
			TextCursorCmdStartOfLine_(cursor, textbuf);
			intz it = cursor->offset;
			bool has_non_whitespace = false;
			for (; it < TextBufferSize_(textbuf); ++it)
			{
				uint8 sample = TextBufferSample_(textbuf, it);
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
				TextCursorCmdEndOfLine_(cursor, textbuf);
				break;
			}
			TextCursorCmdDown_(cursor, textbuf, 1);
		}
	}
}

struct QuadVertex_
{
	float32 pos[2];
	int16 texcoords[2];
	int16 texindex[2];
	uint32 color;
}
typedef QuadVertex_;

struct QuadUniform_
{
	float32 view[4][4];
}
typedef QuadUniform_;

static void
PushQuad_(Arena* arena, float32 pos[4], int16 texcoords[4], int16 texindex, int16 texkind, uint32 color)
{
	if (!texcoords)
		texcoords = (int16[4]) { 0, 0, INT16_MAX, INT16_MAX };
	ArenaPushStructInit(arena, QuadVertex_, {
		.pos = { pos[0], pos[1] },
		.texcoords = { texcoords[0], texcoords[1] },
		.texindex = { texindex, texkind },
		.color = color,
	});
	ArenaPushStructInit(arena, QuadVertex_, {
		.pos = { pos[0]+pos[2], pos[1] },
		.texcoords = { texcoords[0]+texcoords[2], texcoords[1] },
		.texindex = { texindex, texkind },
		.color = color,
	});
	ArenaPushStructInit(arena, QuadVertex_, {
		.pos = { pos[0], pos[1]+pos[3] },
		.texcoords = { texcoords[0], texcoords[1]+texcoords[3] },
		.texindex = { texindex, texkind },
		.color = color,
	});
	ArenaPushStructInit(arena, QuadVertex_, {
		.pos = { pos[0]+pos[2], pos[1]+pos[3] },
		.texcoords = { texcoords[0]+texcoords[2], texcoords[1]+texcoords[3] },
		.texindex = { texindex, texkind },
		.color = color,
	});
}

struct TextPositioning_
{
	int32 start_x, start_y;
	int32 line_start_x;
}
typedef TextPositioning_;

static TextPositioning_
PushText_(App* app, Arena* arena, TextPositioning_ pos, String str, uint32 color, int16 texindex, int16 texkind)
{
	int32 x = pos.start_x;
	int32 y = pos.start_y;

	intz it = 0;
	uint32 codepoint;
	while (codepoint = StringDecode(str, &it), codepoint)
	{
		if (codepoint == ' ')
		{
			x += app->glyph_advance;
			continue;
		}
		else if (codepoint == '\t')
		{
			x += app->glyph_advance * app->tab_size;
			continue;
		}
		else if (codepoint == '\n')
		{
			x = pos.line_start_x;
			y += app->glyph_height;
		}

		uint64 hash = HashInt64(codepoint);
		intz index = (intz)hash;
		GlyphEntry_* glyph = NULL;
		for (;;)
		{
			index = HashMsi(app->glyph_entries_log2cap, hash, index);
			GlyphEntry_* this_glyph = &app->glyph_entries[index];

			if (this_glyph->codepoint == codepoint)
			{
				glyph = this_glyph;
				break;
			}
			if (this_glyph->codepoint)
				continue;
			// not found
			break;
		}

		if (glyph)
		{
			float32 w = app->glyph_width;
			float32 h = app->glyph_height;
			int16 texcoords[4] = {
				glyph->atlas_x * INT16_MAX / app->glyph_texture_size,
				glyph->atlas_y * INT16_MAX / app->glyph_texture_size,
				app->glyph_width * INT16_MAX / app->glyph_texture_size,
				app->glyph_height * INT16_MAX / app->glyph_texture_size,
			};
			int32 xx = x + glyph->draw_offset_x;
			int32 yy = y + glyph->draw_offset_y;

			PushQuad_(arena, (float32[4]) { xx, yy, w, h }, texcoords, texindex, texkind, color);

			x += app->glyph_advance;
		}
	}

	return (TextPositioning_) { x, y, pos.line_start_x };
}

static TextPositioning_
PushText2_(App* app, Arena* arena, int32 x, int32 y, String str, uint32 color, int16 texindex, int16 texkind)
{
	return PushText_(app, arena, (TextPositioning_) { x, y, x }, str, color, texindex, texkind);
}


static TextPositioning_
PushTextBuffer_(App* app, Arena* arena, TextPositioning_ pos, TextBuffer const* buf, uint32 color, int16 texindex, int16 texkind)
{
	String left_str = {};
	String right_str = {};
	TextBufferGetString_(buf, &left_str, &right_str);

	pos = PushText_(app, arena, pos, left_str, color, texindex, texkind);
	pos = PushText_(app, arena, pos, right_str, color, texindex, texkind);
	return pos;
}

static TextPositioning_
PushTextBuffer2_(App* app, Arena* arena, int32 x, int32 y, TextBuffer const* buf, uint32 color, int16 texindex, int16 texkind)
{
	return PushTextBuffer_(app, arena, (TextPositioning_) { x, y, x }, buf, color, texindex, texkind);
}

static void
PushRect_(Arena* arena, Rect rect, int16 texcoords[4], int16 texindex, int16 texkind, uint32 color)
{
	float32 pos[4] = {
		rect.x1,
		rect.y1,
		rect.x2 - rect.x1,
		rect.y2 - rect.y1,
	};
	PushQuad_(arena, pos, texcoords, texindex, texkind, color);
}

API int32
EntryPoint(int32 argc, const char* const argv[])
{
	Trace();
	OS_Init(OS_Init_WindowAndGraphics);
	Arena* global_arena = ArenaBootstrap(OS_VirtualAllocArena(1<<30));
	App* app = ArenaPushStructInit(global_arena, App, {
		.arena = global_arena,
		.tab_size = 4,
	});

	app->window = OS_CreateWindow(&(OS_WindowDesc) {
		.title = StrInit("bed"),
		.width = 600,
		.height = 600,
		.resizeable = true,
	});
	app->r3 = R3_D3D11_MakeContext(app->arena, &(R3_ContextDesc) {
		.window = &app->window,
	});

	app->max_quad_count = UINT16_MAX / 4;
	app->white_texture = R3_MakeTexture(app->r3, &(R3_TextureDesc) {
		.width = 2,
		.height = 2,
		.binding_flags = R3_BindingFlag_ShaderResource,
		.usage = R3_Usage_Immutable,
		.format = R3_Format_U8x4Norm,
		.initial_data = (uint32[4]) { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
	});
	app->sampler = R3_MakeSampler(app->r3, &(R3_SamplerDesc) {
		.filtering = R3_TextureFiltering_Nearest,
	});
	app->quads_vbuf = R3_MakeBuffer(app->r3, &(R3_BufferDesc) {
		.size = SignedSizeof(QuadVertex_) * app->max_quad_count * 4,
		.binding_flags = R3_BindingFlag_VertexBuffer,
		.usage = R3_Usage_Dynamic,
	});
	{
		ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
		intz index_count = app->max_quad_count * 6;
		uint16* indices = ArenaPushArray(scratch.arena, uint16, index_count);
		for (intz i = 0; i < app->max_quad_count; ++i)
		{
			indices[i*6 + 0] = (uint16)(i*4 + 0);
			indices[i*6 + 1] = (uint16)(i*4 + 1);
			indices[i*6 + 2] = (uint16)(i*4 + 2);
			indices[i*6 + 3] = (uint16)(i*4 + 2);
			indices[i*6 + 4] = (uint16)(i*4 + 1);
			indices[i*6 + 5] = (uint16)(i*4 + 3);
		}
		app->quads_ibuf = R3_MakeBuffer(app->r3, &(R3_BufferDesc) {
			.size = SignedSizeof(uint16) * index_count,
			.binding_flags = R3_BindingFlag_IndexBuffer,
			.usage = R3_Usage_Immutable,
			.initial_data = indices,
		});
		ArenaRestore(scratch);
	}
	app->quads_ubuf = R3_MakeBuffer(app->r3, &(R3_BufferDesc) {
		.size = sizeof(QuadUniform_),
		.usage = R3_Usage_Dynamic,
		.binding_flags = R3_BindingFlag_UniformBuffer,
	});
	app->quads_pipeline = R3_MakePipeline(app->r3, &(R3_PipelineDesc) {
		.dx40 = {
			.vs = BufRange(g_quads_vs_begin, g_quads_vs_end),
			.ps = BufRange(g_quads_ps_begin, g_quads_ps_end),
		},
		.input_layout = {
			[0] = { offsetof(QuadVertex_, pos), R3_Format_F32x2, 0, 0 },
			[1] = { offsetof(QuadVertex_, texcoords), R3_Format_I16x2Norm, 0, 0 },
			[2] = { offsetof(QuadVertex_, texindex), R3_Format_I16x2, 0, 0 },
			[3] = { offsetof(QuadVertex_, color), R3_Format_U8x4Norm, 0, 0 },
		},
		.rendertargets[0] = {
			.enable_blend = true,
			.src = R3_BlendFunc_One,
			.dst = R3_BlendFunc_InvSrcAlpha,
			.op = R3_BlendOp_Add,
			.src_alpha = R3_BlendFunc_One,
			.dst_alpha = R3_BlendFunc_InvSrcAlpha,
			.op_alpha = R3_BlendOp_Add,
		},
	});

	// Init D2D1 & DWrite
	{
		int32 const texture_size = 512;
		float32 const emsize = 13.0f;
		app->d2d_rt_texture = R3_MakeTexture(app->r3, &(R3_TextureDesc) {
			.width = texture_size,
			.height = texture_size,
			.format = R3_Format_U8x4Norm_Bgra,
			.binding_flags = R3_BindingFlag_ShaderResource | R3_BindingFlag_RenderTarget,
			.usage = R3_Usage_GpuReadWrite,
		});
		app->glyph_texture_size = texture_size;

		HRESULT hr;
		D2D1_FACTORY_OPTIONS factory_desc = {
			.debugLevel = D2D1_DEBUG_LEVEL_ERROR,
		};
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, &factory_desc, (void**)&app->d2d_factory);
		SafeAssert(SUCCEEDED(hr));

		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (void**)&app->dw_factory);
		SafeAssert(SUCCEEDED(hr));

		IDXGISurface* rt_texture_surface = NULL;
		hr = ID3D11Texture2D_QueryInterface(app->d2d_rt_texture.d3d11_tex2d, &IID_IDXGISurface, (void**)&rt_texture_surface);
		SafeAssert(SUCCEEDED(hr));

		D2D1_RENDER_TARGET_PROPERTIES rtprops = {
			.type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
			.pixelFormat = {
				.format = DXGI_FORMAT_B8G8R8A8_UNORM,
				.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED,
			},
			.usage = D2D1_RENDER_TARGET_USAGE_NONE,
			.minLevel = D2D1_FEATURE_LEVEL_DEFAULT,
			.dpiX = 0.0f,
			.dpiY = 0.0f,
		};
		hr = ID2D1Factory1_CreateDxgiSurfaceRenderTarget(app->d2d_factory, rt_texture_surface, &rtprops, &app->d2d_rt);
		SafeAssert(SUCCEEDED(hr));
		IDXGISurface_Release(rt_texture_surface);

		hr = ID2D1RenderTarget_CreateSolidColorBrush(app->d2d_rt, &(D2D1_COLOR_F) { 1, 1, 1, 1 }, NULL, &app->d2d_brush);
		SafeAssert(SUCCEEDED(hr));

		// wchar_t const* font_path = L"C:\\Windows\\Fonts\\consola.ttf";
		wchar_t const* font_path = L"liberation-mono.ttf";

		hr = IDWriteFactory_CreateFontFileReference(app->dw_factory, font_path, NULL, &app->dw_font_file);
		SafeAssert(SUCCEEDED(hr));
		hr = IDWriteFactory_CreateFontFace(app->dw_factory, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &app->dw_font_file, 0, DWRITE_FONT_SIMULATIONS_NONE, &app->dw_font_face);
		SafeAssert(SUCCEEDED(hr));
		hr = IDWriteFontFace_QueryInterface(app->dw_font_face, &IID_IDWriteFontFace1, (void**)&app->dw_font_face1);
		SafeAssert(SUCCEEDED(hr));
		DWRITE_FONT_METRICS1 font_metrics = {};
		IDWriteFontFace1_GetMetrics1(app->dw_font_face1, &font_metrics);

		Arena* scratch = ScratchArena(0, NULL);
		for ArenaTempScope(scratch)
		{
			uint32 desired_codepoints[] = {
				0x21, 0x7E,
				0xA1, 0xFF,
			};
			static_assert(ArrayLength(desired_codepoints) % 2 == 0);
			intz codepoint_count = 0;
			for (intz i = 0; i < ArrayLength(desired_codepoints); i += 2)
			{
				uint32 first_codepoint = desired_codepoints[i+0];
				uint32 last_codepoint = desired_codepoints[i+1];
				SafeAssert(first_codepoint <= last_codepoint);
				codepoint_count += last_codepoint - first_codepoint + 1;
			}
			SafeAssert(codepoint_count > 0);

			app->glyph_entries_log2cap = 12;
			app->glyph_entries_count = 0;
			app->glyph_entries = ArenaPushArray(app->arena, GlyphEntry_, 1<<app->glyph_entries_log2cap);

			// generate codepoints array
			UINT32* codepoints = ArenaPushArray(scratch, UINT32, codepoint_count);
			for (intz i = 0, curr_codepoint = 0; i < ArrayLength(desired_codepoints); i += 2)
			{
				uint32 first_codepoint = desired_codepoints[i+0];
				uint32 last_codepoint = desired_codepoints[i+1];

				for (uint32 codepoint = first_codepoint; codepoint <= last_codepoint; ++codepoint)
					codepoints[curr_codepoint++] = codepoint;
			}

			// get glyph indices
			UINT16* glyph_indices = ArenaPushArray(scratch, UINT16, codepoint_count);
			hr = IDWriteFontFace_GetGlyphIndices(app->dw_font_face, codepoints, (UINT32)codepoint_count, glyph_indices);
			SafeAssert(SUCCEEDED(hr));

			// get glyph metrics
			DWRITE_GLYPH_METRICS* glyph_metrics = ArenaPushArray(scratch, DWRITE_GLYPH_METRICS, codepoint_count);
			hr = IDWriteFontFace_GetDesignGlyphMetrics(app->dw_font_face, glyph_indices, codepoint_count, glyph_metrics, false);
			SafeAssert(SUCCEEDED(hr));

			// calculate cell size for all glyphs
			float32 units = emsize / font_metrics.Base.designUnitsPerEm;
			int32 glyph_width = ceilf(units * (font_metrics.glyphBoxRight - font_metrics.glyphBoxLeft));
			int32 glyph_height = ceilf(units * (font_metrics.glyphBoxTop - font_metrics.glyphBoxBottom));
			app->glyph_width = glyph_width;
			app->glyph_height = glyph_height;
			{
				INT32 biggest_advance = 0;
				for (intz i = 0; i < codepoint_count; ++i)
					biggest_advance = Max(biggest_advance, (INT32)glyph_metrics[i].advanceWidth);
				app->glyph_advance = roundf(units * biggest_advance);
			}

			// generate atlas
			FLOAT* glyph_advances = ArenaPushArray(scratch, FLOAT, codepoint_count);
			DWRITE_GLYPH_OFFSET* glyph_offsets = ArenaPushArray(scratch, DWRITE_GLYPH_OFFSET, codepoint_count);
			int32 curr_x_offset = 0;
			int32 curr_y_offset = 0;
			for (intz i = 0; i < codepoint_count; ++i)
			{
				uint32 codepoint = codepoints[i];
				FLOAT* glyph_advance = &glyph_advances[i];
				DWRITE_GLYPH_OFFSET* glyph_offset = &glyph_offsets[i];
				DWRITE_GLYPH_METRICS const* metrics = &glyph_metrics[i];

				SafeAssert(app->glyph_entries_count+1 < 1<<app->glyph_entries_log2cap);
				uint64 hash = HashInt64(codepoint);
				intz index = (intz)hash;
				GlyphEntry_* glyph = NULL;
				for (;;)
				{
					index = HashMsi(app->glyph_entries_log2cap, hash, index);
					GlyphEntry_* this_glyph = &app->glyph_entries[index];

					if (!this_glyph->codepoint)
					{
						this_glyph->codepoint = codepoint;
						glyph = this_glyph;
						++app->glyph_entries_count;
						break;
					}
				}
				SafeAssert(glyph);

				int32 x, y;
				if (curr_x_offset + glyph_width <= texture_size)
				{
					x = curr_x_offset;
					y = curr_y_offset;
					curr_x_offset += glyph_width+1;
				}
				else if (curr_y_offset + glyph_height <= texture_size)
				{
					curr_x_offset = glyph_width+1;
					curr_y_offset += glyph_height+1;
					x = 0;
					y = curr_y_offset;
				}
				else
					SafeAssert(!"ran out of space!");

				glyph->atlas_x = (int16)x;
				glyph->atlas_y = (int16)y;
				glyph->draw_offset_x = floorf(metrics->leftSideBearing * units);
				glyph->draw_offset_y = 0*floorf(metrics->topSideBearing * units);
				glyph_offset->advanceOffset = (float32)(x - glyph->draw_offset_x);
				glyph_offset->ascenderOffset = -(float32)(y) - ceilf((font_metrics.Base.ascent) * units);
				*glyph_advance = 0.0f;
			}

			// draw atlas
			DWRITE_GLYPH_RUN glyph_run = {
				.fontFace = app->dw_font_face,
				.fontEmSize = emsize,
				.glyphCount = (UINT32)codepoint_count,
				.glyphIndices = glyph_indices,
				.glyphAdvances = glyph_advances,
				.glyphOffsets = glyph_offsets,
				.isSideways = FALSE,
				.bidiLevel = 0,
			};

			ID2D1RenderTarget_BeginDraw(app->d2d_rt);
			ID2D1RenderTarget_Clear(app->d2d_rt, &(D2D1_COLOR_F) { 0, 0, 0, 0 });
			ID2D1RenderTarget_SetTransform(app->d2d_rt, &(D2D1_MATRIX_3X2_F) { ._11 = 1.0f, ._22 = 1.0f });
			ID2D1RenderTarget_SetTextAntialiasMode(app->d2d_rt, D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
			ID2D1RenderTarget_DrawGlyphRun(app->d2d_rt, (D2D1_POINT_2F) {}, &glyph_run, (ID2D1Brush*)app->d2d_brush, DWRITE_MEASURING_MODE_NATURAL);
			hr = ID2D1RenderTarget_EndDraw(app->d2d_rt, NULL, NULL);
			SafeAssert(SUCCEEDED(hr));
		}
	}

	// Free d2d1 and dwrite
	{
		IDWriteFontFace1_Release(app->dw_font_face1);
		IDWriteFontFace_Release(app->dw_font_face);
		IDWriteFontFile_Release(app->dw_font_file);
		ID2D1SolidColorBrush_Release(app->d2d_brush);
		ID2D1RenderTarget_Release(app->d2d_rt);
		ID2D1Factory1_Release(app->d2d_factory);
		IDWriteFactory_Release(app->dw_factory);
	}

	String str = StrInit("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n!@#$%&*()[]{}_-+\nHello, World!\nint main() { puts(\"Hello, World!\"); }");
	app->buffer = (TextBuffer) {
		.kind = TextBufferKind_OwnedGapBuffer,
		.utf8_text = OS_HeapAlloc(str.size * 2),
		.size = str.size * 2,
		.gap_start = str.size,
		.gap_end = str.size * 2,
	};
	MemoryCopy(app->buffer.utf8_text, str.data, str.size);
	app->cursor = (TextCursor) {
		.offset = app->buffer.gap_start,
	};

	while (!app->is_closing)
	{
		ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
		intz event_count;
		OS_Event* events = OS_PollEvents(true, scratch.arena, &event_count);

		bool should_resize_buffers = false;
		for (intz i = 0; i < event_count; ++i)
		{
			OS_Event* event = &events[i];
			if (event->kind == OS_EventKind_WindowClose)
				app->is_closing = true;
			else if (event->kind == OS_EventKind_WindowResize)
				should_resize_buffers = true;
			else if (event->kind == OS_EventKind_WindowKeyPressed)
			{
				switch (event->window_key.key)
				{
					default: break;
					case OS_KeyboardKey_Left:
					{
						if (event->window_key.ctrl)
							TextCursorCmdLeftSnakeWord_(&app->cursor, &app->buffer, 1);
						else
							TextCursorCmdLeft_(&app->cursor, &app->buffer, 1);
					} break;
					case OS_KeyboardKey_Right:
					{
						if (event->window_key.ctrl)
							TextCursorCmdRightSnakeWord_(&app->cursor, &app->buffer, 1);
						else
							TextCursorCmdRight_(&app->cursor, &app->buffer, 1);
					} break;
					case OS_KeyboardKey_Up:
					{
						if (event->window_key.ctrl)
							TextCursorCmdUpParagraph_(&app->cursor, &app->buffer, 1);
						else
							TextCursorCmdUp_(&app->cursor, &app->buffer, 1);
					} break;
					case OS_KeyboardKey_Down:
					{
						if (event->window_key.ctrl)
							TextCursorCmdDownParagraph_(&app->cursor, &app->buffer, 1);
						else
							TextCursorCmdDown_(&app->cursor, &app->buffer, 1);
					} break;
					case OS_KeyboardKey_Backspace:
					{
						if (event->window_key.ctrl)
							TextCursorCmdDeleteBackwardSnakeWord_(&app->cursor, &app->buffer, 1);
						else
							TextCursorCmdDeleteBackward_(&app->cursor, &app->buffer, 1);
					} break;
					case OS_KeyboardKey_End: TextCursorCmdEndOfLine_(&app->cursor, &app->buffer); break;
					case OS_KeyboardKey_Home: TextCursorCmdStartOfLine_(&app->cursor, &app->buffer); break;
					case OS_KeyboardKey_Enter: TextCursorCmdInsert_(&app->cursor, &app->buffer, 1, '\n'); break;
					case OS_KeyboardKey_Tab: TextCursorCmdInsert_(&app->cursor, &app->buffer, 1, '\t'); break;
					case 'D':
					{
						if (event->window_key.ctrl)
							TextCursorCmdDeleteToMarker_(&app->cursor, &app->buffer);
					} break;
					case 'C':
					{
						if (event->window_key.ctrl)
							TextCursorCmdCopy_(app, &app->cursor, &app->buffer);
					} break;
					case 'V':
					{
						if (event->window_key.ctrl)
							TextCursorCmdPaste_(app, &app->cursor, &app->buffer);
					} break;
				}
			}
			else if (event->kind == OS_EventKind_WindowTyping)
			{
				uint32 codepoint = event->window_typing.codepoint;
				if (event->window_typing.ctrl)
				{
					if (codepoint == ' ')
						TextCursorCmdPlaceMarker_(&app->cursor);
				}
				else
					TextCursorCmdInsert_(&app->cursor, &app->buffer, 1, codepoint);
			}
		}

		if (should_resize_buffers)
			R3_ResizeBuffers(app->r3);

		OS_GetWindowUserSize(app->window, &app->window_width, &app->window_height);
		float32 width = (float32)app->window_width;
		float32 height = (float32)app->window_height;

		QuadUniform_ uniform = {
			.view = {
				[0][0] =  2.0f / width,
				[1][1] = -2.0f / height,
				[2][2] =  1.0f,
				[3][3] =  1.0f,
				
				[3][0] = -1.0f,
				[3][1] =  1.0f,
			},
		};
		R3_UpdateBuffer(app->r3, &app->quads_ubuf, &uniform, sizeof(uniform));

		Rect screen = { 0, 0, app->window_width, app->window_height };

		intz quad_count = 0;
		{
			QuadVertex_* first_vertex = ArenaEndAligned(scratch.arena, alignof(QuadVertex_));
			int32 line_count = TextBufferLineCount_(&app->buffer);
			int32 line_log10count = 0;
			{
				int32 it = line_count;
				while (it > 0)
				{
					it /= 10;
					++line_log10count;
				}
			}

			Rect status_bar = RectCutTop(&screen, app->glyph_height + 8);
			Rect status_bar_text = RectCutMargin(status_bar, 4);
			Rect line_counter = RectCutLeft(&screen, line_log10count * app->glyph_advance + 4);
			Rect line_counter_text = RectCutMargin(line_counter, 2);
			RectCutTop(&line_counter_text, 2);
			Rect text_screen = RectCutMargin(screen, 4);

			PushTextBuffer2_(app, scratch.arena, text_screen.x1, text_screen.y1, &app->buffer, 0xFFFFFFFF, 0, 1);
			LineCol cursor_pos = TextBufferLineColFromOffset_(&app->buffer, app->cursor.offset, app->tab_size);
			LineCol marker_pos = TextBufferLineColFromOffset_(&app->buffer, app->cursor.marker_offset, app->tab_size);
			PushQuad_(scratch.arena, (float32[4]) {
				text_screen.x1 + (cursor_pos.col - 1) * app->glyph_advance,
				text_screen.y1 + (cursor_pos.line- 1) * app->glyph_height,
				app->glyph_advance,
				app->glyph_height,
			}, NULL, 1, 0, 0xCF7F7F7F);
			PushQuad_(scratch.arena, (float32[4]) {
				text_screen.x1 + (marker_pos.col - 1) * app->glyph_advance,
				text_screen.y1 + (marker_pos.line- 1) * app->glyph_height,
				app->glyph_advance,
				app->glyph_height,
			}, NULL, 1, 0, 0x7F3F3F3F);

			PushRect_(scratch.arena, line_counter, NULL, 1, 0, 0xFF2F2F2F);
			for (int32 i = 0; i < line_count; ++i)
			{
				String str = StringPrintfLocal(32, "%i", i+1);
				PushText2_(app, scratch.arena, line_counter_text.x1, line_counter_text.y1 + i * app->glyph_height, str, 0xFFFFFFFF, 0, 1);
			}

			PushRect_(scratch.arena, status_bar, NULL, 1, 0, 0xFF3F3F3F);
			PushText2_(app, scratch.arena, status_bar_text.x1, status_bar_text.y1, Str("Testing"), 0xFFFF00FF, 0, 1);

			QuadVertex_* last_vertex = ArenaEnd(scratch.arena);
			intz vertex_count = last_vertex - first_vertex;
			quad_count = vertex_count / 4;
			R3_UpdateBuffer(app->r3, &app->quads_vbuf, first_vertex, sizeof(QuadVertex_) * (uint32)vertex_count);
		}

		R3_SetRenderTarget(app->r3, NULL);
		R3_SetViewports(app->r3, 1, &(R3_Viewport) { 0, 0, width, height, 0, 1 });
		R3_Clear(app->r3, &(R3_ClearDesc) {
			.flag_color = true,
			// .color = { 15.0f/255, 15.0f/255, 15.0f/255, 1.0f },
			.color = { 0, 0, 0, 1 },
		});

		R3_SetPrimitiveType(app->r3, R3_PrimitiveType_TriangleList);
		R3_SetPipeline(app->r3, &app->quads_pipeline);
		R3_SetVertexInputs(app->r3, &(R3_VertexInputs) {
			.ibuffer = &app->quads_ibuf,
			.index_format = R3_Format_U16x1,
			.vbuffers[0] = {
				.buffer = &app->quads_vbuf,
				.stride = sizeof(QuadVertex_),
			},
		});
		R3_SetResourceViews(app->r3, 4, (R3_ResourceView[4]) {
			[0] = { .texture = &app->d2d_rt_texture },
			[1] = { .texture = &app->white_texture },
			[2] = { .texture = &app->white_texture },
			[3] = { .texture = &app->white_texture },
		});
		R3_SetSamplers(app->r3, 4, (R3_Sampler*[4]) {
			&app->sampler,
			&app->sampler,
			&app->sampler,
			&app->sampler,
		});
		R3_SetUniformBuffers(app->r3, 1, &(R3_UniformBuffer) {
			.buffer = &app->quads_ubuf,
		});
		R3_DrawIndexed(app->r3, 0, (uint32)quad_count*6, 0, 0, 0);

		R3_Present(app->r3);
		ArenaRestore(scratch);
	}

	R3_FreePipeline(app->r3, &app->quads_pipeline);
	R3_FreeBuffer(app->r3, &app->quads_vbuf);
	R3_FreeBuffer(app->r3, &app->quads_ibuf);
	R3_FreeBuffer(app->r3, &app->quads_ubuf);
	R3_FreeSampler(app->r3, &app->sampler);
	R3_FreeTexture(app->r3, &app->white_texture);
	R3_FreeTexture(app->r3, &app->d2d_rt_texture);
	R3_FreeContext(app->r3);
	OS_DestroyWindow(app->window);
	OS_VirtualFree(app->arena->memory, app->arena->reserved);

	return 0;
}
