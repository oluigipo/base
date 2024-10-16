#include "common.h"
#include "api_os.h"
#include "api_render3.h"
#include "cfront/api.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#define INITGUID
#include "third_party/cd2d.h"
#include "third_party/cdwrite.h"
#include <d3d11_1.h>
#include <math.h>

#define BED_API static
#include "bed.h"
#include "bed_textcursor.c"
#include "bed_panel.c"
#include "bed_app.c"
#include "bed_textbuffer.c"

IncludeBinary(g_quads_vs, "shader_quad_vs.dxil");
IncludeBinary(g_quads_ps, "shader_quad_ps.dxil");

static uint64
UnsetMsb(uint64 value)
{
	uint64 mask = value;
	mask |= mask >> 1;
	mask |= mask >> 2;
	mask |= mask >> 4;
	mask |= mask >> 8;
	mask |= mask >> 16;
	mask |= mask >> 32;
	return value & mask >> 1;
}

static int32
MsbIndex(uint64 value)
{
	return 64 - BitClz64(value);
}

BED_API int32
CIndentPushRange(CIndentCtx* cindent, Range line_range, intz cf_count, CF_TokenKind const cf_kinds[], CF_SourceRange const cf_ranges[], intz start_cf_index)
{
	// TODO(ljre): make this function faster
	Trace();
	int32 braces_nesting = MsbIndex(cindent->braces_nesting_bitset);
	int32 parens_nesting = MsbIndex(cindent->parens_nesting_bitset);
	int32 unfinished_stmt_nesting = MsbIndex(cindent->unfinished_stmt_nesting_bitset);
	int32 scope_nesting = Max(braces_nesting, parens_nesting);
	scope_nesting = Max(scope_nesting, unfinished_stmt_nesting);
	int32 this_scope_nesting = scope_nesting;
	int32 parens_nesting_at_start = parens_nesting;

	bool prev_is_preproc_line = cindent->preproc_nesting;
	bool is_preproc_line = cindent->preproc_nesting;
	bool is_first_iter = true;
	CF_TokenKind last_token = 0;
	for (intz i = start_cf_index; i < cf_count; ++i)
	{
		CF_TokenKind kind = cf_kinds[i];
		CF_SourceRange const* range = &cf_ranges[i];
		if (range->end <= line_range.start)
			continue;
		if (range->begin >= line_range.end)
			break;

		if (is_first_iter && kind == CF_TokenKind_SymHash)
			is_preproc_line = true;

		if (kind != CF_TokenKind_Comment &&
			kind != CF_TokenKind_MultilineComment &&
			kind != CF_TokenKind_Linebreak)
			last_token = kind;

		bool just_closed = false;
		bool just_opened = false;
		if (kind == CF_TokenKind_SymLeftCurl)
		{
			// NOTE(ljre): If we're opening a brace in the same line a paren was open, the "ownership" of
			//             this indentation level will go to the brace.
			//             This makes it indent the following code:
			//                 Test(a, {
			//                         .f = "abc"
			//                     }, c)
			//             as:
			//                 Test(a, {
			//                    .f = "abc"
			//                 }, c);
			if (braces_nesting + 1 == parens_nesting && parens_nesting > parens_nesting_at_start)
			{
				cindent->parens_nesting_bitset = UnsetMsb(cindent->parens_nesting_bitset);
				--scope_nesting; // it will be incremented this iteration again
			}
			else if (scope_nesting > 0 && scope_nesting == unfinished_stmt_nesting)
			{
				cindent->unfinished_stmt_nesting_bitset = UnsetMsb(cindent->unfinished_stmt_nesting_bitset);
				--scope_nesting;
			}

			SafeAssert(scope_nesting <= 63);
			cindent->braces_nesting_bitset |= 1ULL << scope_nesting;
			braces_nesting = MsbIndex(cindent->braces_nesting_bitset);
			just_opened = true;
		}
		else if (kind == CF_TokenKind_SymLeftParen)
		{
			SafeAssert(scope_nesting <= 63);
			cindent->parens_nesting_bitset |= 1ULL << scope_nesting;
			parens_nesting = MsbIndex(cindent->parens_nesting_bitset);
			just_opened = true;
		}
		else if (kind == CF_TokenKind_SymRightCurl)
		{
			cindent->braces_nesting_bitset = UnsetMsb(cindent->braces_nesting_bitset);
			braces_nesting = MsbIndex(cindent->braces_nesting_bitset);
			just_closed = true;
		}
		else if (kind == CF_TokenKind_SymRightParen)
		{
			if (scope_nesting > 0 && scope_nesting == unfinished_stmt_nesting)
			{
				cindent->unfinished_stmt_nesting_bitset = UnsetMsb(cindent->unfinished_stmt_nesting_bitset);
				--scope_nesting;
			}
			cindent->parens_nesting_bitset = UnsetMsb(cindent->parens_nesting_bitset);
			parens_nesting = MsbIndex(cindent->parens_nesting_bitset);
			just_closed = true;
		}
		else
		{
			is_first_iter = false;
			continue;
		}

		scope_nesting = Max(braces_nesting, parens_nesting);
		scope_nesting = Max(scope_nesting, unfinished_stmt_nesting);
		if (is_first_iter)
		{
			if (just_closed)
				this_scope_nesting = scope_nesting;
			else if (just_opened)
			{
				SafeAssert(scope_nesting > 0);
				this_scope_nesting = scope_nesting - 1;
			}
		}
		is_first_iter = false;
	}

	// NOTE(ljre): indent multi-line preproc statements
	if (is_preproc_line && prev_is_preproc_line)
		++this_scope_nesting;

	// NOTE(ljre): indent next line if this stmt/expr didn't end in this line
	cindent->preproc_nesting = false;
	if (last_token != 0)
	{
		if (is_preproc_line)
			cindent->preproc_nesting = (last_token == CF_TokenKind_EscapedLinebreak);
		else
		{
			bool could_be_unfinished_stmt = (
				last_token != CF_TokenKind_SymRightCurl &&
				last_token != CF_TokenKind_SymLeftCurl &&
				last_token != CF_TokenKind_SymLeftParen &&
				last_token != CF_TokenKind_SymComma &&
				last_token != CF_TokenKind_SymColon &&
				last_token != CF_TokenKind_SymSemicolon);

			if (scope_nesting > 0 && scope_nesting == unfinished_stmt_nesting && !could_be_unfinished_stmt)
				cindent->unfinished_stmt_nesting_bitset = UnsetMsb(cindent->unfinished_stmt_nesting_bitset);
			else if (could_be_unfinished_stmt)
			{
				if (scope_nesting > 0 && scope_nesting == braces_nesting)
					cindent->unfinished_stmt_nesting_bitset |= 1ULL << scope_nesting;
				// else if (scope_nesting > 0 && scope_nesting == parens_nesting && parens_nesting_at_start == parens_nesting)
				// 	cindent->unfinished_stmt_nesting_bitset |= 1ULL << scope_nesting;
			}
		}
	}

	return this_scope_nesting;
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
	int32 max_x, max_y;
}
typedef TextPositioning_;

struct ColorRange_
{
	uint32 color;
	intz start;
	intz end;
}
typedef ColorRange_;

static TextPositioning_
PushText_(App* app, Arena* arena, TextPositioning_ pos, String str, uint32 color, int16 texindex, int16 texkind, intz color_ranges_count, ColorRange_ const color_ranges[])
{
	Trace();
	int32 x = pos.start_x;
	int32 y = pos.start_y;
	ColorRange_ color_range = {
		.color = color,
		.start = 0,
		.end = str.size,
	};
	intz color_ranges_index = 0;
	if (color_ranges_count)
		color_range = color_ranges[color_ranges_index++];

	intz it = 0;
	intz prev_it = 0;
	uint32 codepoint = 0;
	while (prev_it = it, StringDecode(str, &it, &codepoint))
	{
		if (codepoint == ' ')
		{
			x += app->glyph_advance;
			continue;
		}
		else if (codepoint == '\t')
		{
			int32 col = (x - pos.line_start_x);
			x = RoundToNextTab(col / app->glyph_advance, app->tab_size) * app->glyph_advance + pos.line_start_x;
			// x += app->tab_size * app->glyph_advance;
			continue;
		}
		else if (codepoint == '\n')
		{
			x = pos.line_start_x;
			y += app->glyph_line_height;
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
			if (!this_glyph->codepoint)
			{
				// not found
				glyph = &app->glyph_entries[app->invalid_glyph_index];
				break;
			}
		}

		SafeAssert(glyph);
		int32 xx = x + glyph->draw_offset_x;
		int32 yy = y + glyph->draw_offset_y;
		if (xx >= pos.max_x || yy >= pos.max_y)
			continue;

		int32 w = ClampMax(app->glyph_width, pos.max_x - xx);
		int32 h = ClampMax(app->glyph_line_height, pos.max_y - yy);

		int16 texcoords[4] = {
			glyph->atlas_x * INT16_MAX / app->glyph_texture_size,
			glyph->atlas_y * INT16_MAX / app->glyph_texture_size,
			w * INT16_MAX / app->glyph_texture_size,
			h * INT16_MAX / app->glyph_texture_size,
		};

		uint32 this_color = color;
		if (prev_it >= color_range.end && color_ranges_index < color_ranges_count)
			color_range = color_ranges[color_ranges_index++];
		if (prev_it >= color_range.start && prev_it < color_range.end)
			this_color = color_range.color;

		PushQuad_(arena, (float32[4]) { xx, yy, w, h }, texcoords, texindex, texkind, this_color);

		x += app->glyph_advance;
	}

	return (TextPositioning_) { x, y, pos.line_start_x, pos.max_x, pos.max_y };
}

static TextPositioning_
PushText2_(App* app, Arena* arena, Rect rect, String str, uint32 color, int16 texindex, int16 texkind, intz color_ranges_count, ColorRange_ const color_ranges[])
{
	return PushText_(app, arena, (TextPositioning_) { rect.x1, rect.y1, rect.x1, rect.x2, rect.y2 }, str, color, texindex, texkind, color_ranges_count, color_ranges);
}

// static TextPositioning_
// PushTextBuffer_(App* app, Arena* arena, TextPositioning_ pos, TextBuffer* buf, uint32 color, int16 texindex, int16 texkind)
// {
// 	String left_str = {};
// 	String right_str = {};
// 	TextBufferGetStrings(buf, &left_str, &right_str);

// 	pos = PushText_(app, arena, pos, left_str, color, texindex, texkind, 0, NULL);
// 	pos = PushText_(app, arena, pos, right_str, color, texindex, texkind, 0, NULL);
// 	return pos;
// }

// static TextPositioning_
// PushTextBuffer2_(App* app, Arena* arena, int32 x, int32 y, TextBuffer* buf, uint32 color, int16 texindex, int16 texkind)
// {
// 	return PushTextBuffer_(app, arena, (TextPositioning_) { x, y, x }, buf, color, texindex, texkind);
// }

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

static void
UpdateTextViewLayout_(App* app, TextView* view, Rect rect, int32 line_count)
{
	Trace();
	if (line_count == 0)
	{
		TextBuffer* textbuf = AppGetTextBuffer(app, view->textbuf);
		line_count = TextBufferLineCount(textbuf);
	}

	int32 line_log10count = 0;
	{
		int32 it = line_count;
		while (it > 0)
		{
			it /= 10;
			++line_log10count;
		}
	}

	Rect status_bar = RectCutTop(&rect, app->glyph_line_height + 8);
	Rect status_bar_text = RectCutMargin(status_bar, 4);
	Rect line_bar = RectCutLeft(&rect, line_log10count * app->glyph_advance + 4);
	Rect line_bar_text = RectCutMargin(line_bar, 2);
	RectCutTop(&line_bar_text, 2);
	Rect text_screen = RectCutMarginTopLeft(rect, 4);

	view->layout = (TextViewLayout) {
		.all = rect,
		.status_bar = status_bar,
		.status_bar_text = status_bar_text,
		.line_bar = line_bar,
		.line_bar_text = line_bar_text,
		.text_screen = text_screen,
	};
}

static void
PushTextView_(App* app, TextView* view, Arena* arena, Rect rect, int16 texindex, uint32 status_bar_color)
{
	Trace();
	TextBuffer* textbuf = AppGetTextBuffer(app, view->textbuf);
	TextBufferRefreshTokens(app, textbuf);
	int32 line_count = TextBufferLineCount(textbuf);
	
	UpdateTextViewLayout_(app, view, rect, line_count);
	TextViewLayout const* layout = &view->layout;

	int32 first_line = view->line;
	int32 visible_line_count = (layout->text_screen.y2 - layout->text_screen.y1 + app->glyph_line_height-1) / app->glyph_line_height;
	int32 last_line = ClampMax(first_line + visible_line_count, line_count);

	int32 scope_nesting_at_cursor = 0;
	int32 scope_nesting_at_marker = 0;
	int32 consumed_nesting_at_cursor = 0;
	int32 consumed_nesting_at_marker = 0;
	bool cursor_is_inside_view = false;
	bool marker_is_inside_view = false;
	{
		CIndentCtx cindent = {};
		intz last_cf_index = 0;

		String left_str = {};
		String right_str = {};
		int32 line = 1;
		intz it = 0;
		intz prev_it = 0;
		intz prev_pushed_range = 0;
		for (; prev_it = it, TextBufferLineIterator(textbuf, &it, &left_str, &right_str); ++line)
		{
			if (line < first_line)
				continue;
			if (line > last_line)
				break;

			ArenaSavepoint scratch = ArenaSave(ScratchArena(1, &arena));
			String str = {};
			if (right_str.size)
			{
				uint8* mem = ArenaPushDirtyAligned(scratch.arena, left_str.size+right_str.size, 1);
				MemoryCopy(mem, left_str.data, left_str.size);
				MemoryCopy(mem+left_str.size, right_str.data, right_str.size);
				str = StrMake(left_str.size + right_str.size, mem);
			}
			else
				str = left_str;

			if (prev_pushed_range != prev_it)
			{
				// NOTE(ljre): On the first line we'll actually render, push entire range before it
				Range range_to_push = RangeMake(prev_pushed_range, prev_it);
				prev_pushed_range = prev_it;
				CIndentPushRange(&cindent, range_to_push, textbuf->cf_count, textbuf->cf_kinds, textbuf->cf_ranges, 0);
			}

			Range range_to_push = RangeMake(prev_pushed_range, it);
			prev_pushed_range = it;
			intz this_scope_nesting = CIndentPushRange(&cindent, range_to_push, textbuf->cf_count, textbuf->cf_kinds, textbuf->cf_ranges, last_cf_index);

			intz consumed_tabs = 0;
			while (consumed_tabs < str.size && consumed_tabs < this_scope_nesting)
			{
				if (str.data[consumed_tabs] == '\t')
					++consumed_tabs;
				else
					break;
			}

			bool is_last_iteration = (it == TextBufferSize(textbuf));
			if (view->cursor.offset >= prev_it && view->cursor.offset < it ||
				is_last_iteration && view->cursor.offset == it)
			{
				scope_nesting_at_cursor = this_scope_nesting;
				consumed_nesting_at_cursor = consumed_tabs;
				cursor_is_inside_view = true;
			}
			if (view->cursor.marker_offset >= prev_it && view->cursor.marker_offset < it ||
				is_last_iteration && view->cursor.marker_offset == it)
			{
				scope_nesting_at_marker = this_scope_nesting;
				consumed_nesting_at_marker = consumed_tabs;
				marker_is_inside_view = true;
			}

			str = StringSlice(str, consumed_tabs, -1);

			ColorRange_* color_ranges = NULL;
			intz color_ranges_count = 0;
			if (textbuf->kind == TextBufferKind_CFile)
			{
				color_ranges = ArenaEndAligned(scratch.arena, alignof(ColorRange_));
				bool is_preproc_line = false;
				bool is_first_iter = true;
				for (intz i = last_cf_index; i < textbuf->cf_count; last_cf_index = i++)
				{
					CF_SourceRange const* range = &textbuf->cf_ranges[i];
					if (range->end < prev_it)
						continue;
					if (range->begin >= it)
						break;
					CF_TokenKind tok_kind = textbuf->cf_kinds[i];
					uint32 color = 0;

					if (tok_kind == CF_TokenKind_Linebreak ||
						tok_kind == CF_TokenKind_EscapedLinebreak)
						continue;

					if (is_first_iter && tok_kind == CF_TokenKind_SymHash)
						is_preproc_line = true;
					is_first_iter = false;

					if (is_preproc_line)
						color = app->c_preproc;
					else if (tok_kind > CF_TokenKind__OneBeforeFirstKw && tok_kind < CF_TokenKind__OnePastLastKw)
						color = app->c_keyword;
					else if (tok_kind == CF_TokenKind_Comment || tok_kind == CF_TokenKind_MultilineComment)
						color = app->c_comment;
					else if (tok_kind == CF_TokenKind_LitNumber)
						color = app->c_number;
					else if (
						tok_kind == CF_TokenKind_LitString || tok_kind == CF_TokenKind_LitUtf8String ||
						tok_kind == CF_TokenKind_LitWideString || tok_kind == CF_TokenKind_LitChar ||
						tok_kind == CF_TokenKind_LitUtf8Char || tok_kind == CF_TokenKind_LitWideChar)
						color = app->c_string;
					else if (
						tok_kind == CF_TokenKind_SymLeftParen || tok_kind == CF_TokenKind_SymRightParen ||
						tok_kind == CF_TokenKind_SymLeftBrkt || tok_kind == CF_TokenKind_SymRightBrkt ||
						tok_kind == CF_TokenKind_SymLeftCurl || tok_kind == CF_TokenKind_SymRightCurl ||
						tok_kind == CF_TokenKind_SymComma || tok_kind == CF_TokenKind_SymSemicolon ||
						tok_kind == CF_TokenKind_SymColon)
						color = 0xFFFFFFFF;
					else if (tok_kind > CF_TokenKind__OneBeforeFirstSym && tok_kind < CF_TokenKind__OnePastLastSym)
						color = app->c_operators;

					if (color != 0)
						ArenaPushStructInit(scratch.arena, ColorRange_, {
							.color = color,
							.start = range->begin - prev_it - consumed_tabs,
							.end = range->end - prev_it - consumed_tabs,
						});
				}
				color_ranges_count = (ColorRange_*)ArenaEnd(scratch.arena) - color_ranges;
			}

			Rect pos = layout->text_screen;
			pos.x1 += this_scope_nesting * app->glyph_advance * app->tab_size;
			pos.y1 += (line-first_line) * app->glyph_line_height;
			PushText2_(app, arena, pos, str, app->c_foreground, 0, 1, color_ranges_count, color_ranges);

			ArenaRestore(scratch);
		}

		if (line == 1)
		{
			// NOTE(ljre): If the textbuffer is empty, just draw the cursor and marker
			cursor_is_inside_view = true;
			marker_is_inside_view = true;
		}
	}

	if (cursor_is_inside_view)
	{
		LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.offset, app->tab_size);
		Rect rect = RectMakeSized(
			layout->text_screen.x1 + (pos.col - 1 + (scope_nesting_at_cursor - consumed_nesting_at_cursor) * app->tab_size) * app->glyph_advance,
			layout->text_screen.y1 + (pos.line - first_line) * app->glyph_line_height,
			app->glyph_advance,
			app->glyph_height
		);
		PushRect_(arena, RectClip(rect, layout->text_screen), NULL, 1, 0, app->c_cursor);
	}
	if (marker_is_inside_view)
	{
		LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.marker_offset, app->tab_size);
		Rect rect = RectMakeSized(
			layout->text_screen.x1 + (pos.col - 1 + (scope_nesting_at_marker - consumed_nesting_at_marker) * app->tab_size) * app->glyph_advance,
			layout->text_screen.y1 + (pos.line - first_line) * app->glyph_line_height,
			app->glyph_advance,
			app->glyph_height
		);
		PushRect_(arena, RectClip(rect, layout->text_screen), NULL, 1, 0, app->c_cursor_marker);
	}

	PushRect_(arena, layout->line_bar, NULL, 1, 0, 0xFF2F2F2F);
	for (int32 i = 0; i <= last_line-first_line; ++i)
	{
		String str = StringPrintfLocal(32, "%i", i+first_line);
		Rect pos = layout->line_bar_text;
		pos.y1 += i * app->glyph_line_height;
		PushText2_(app, arena, pos, str, 0xFFFFFFFF, 0, 1, 0, NULL);
	}

	PushRect_(arena, layout->status_bar, NULL, 1, 0, status_bar_color);
	Rect status_bar_text = layout->status_bar_text;
	status_bar_text.y1 += 2;
	String view_name = {};
	if (textbuf->file_absolute_path.size)
	{
		view_name = textbuf->file_absolute_path;
		if (StringStartsWith(view_name, app->project_path))
		{
			view_name = StringSlice(view_name, app->project_path.size, -1);
			if (view_name.size > 0 && view_name.data[0] == '/')
				view_name = StringSlice(view_name, 1, -1);
		}
	}
	else if (textbuf->name.size)
		view_name = textbuf->name;
	PushText2_(app, arena, status_bar_text, view_name, 0xFFFFFFFF, 0, 1, 0, NULL);
}

static SimpleViewLayout
UpdateSimpleViewLayout_(App* app, Rect rect)
{
	Rect all = rect;
	Rect filter_bar = RectCutTop(&rect, app->glyph_line_height + 8);
	Rect filter_bar_text = RectCutMargin(filter_bar, 4);
	Rect entries = rect;

	return (SimpleViewLayout) {
		.all = all,
		.filter_bar = filter_bar,
		.filter_bar_text = filter_bar_text,
		.entries = entries,
	};
}

static uint32
ColorFromFileName_(String filename, uint32 default_color)
{
	uint32 color = default_color;
	if (StringEndsWith(filename, Str(".exe")))
		color = 0xFFCCCC00;
	else if (
		StringEndsWith(filename, Str(".c")) ||
		StringEndsWith(filename, Str(".h")) ||
		StringEndsWith(filename, Str(".cpp")) ||
		StringEndsWith(filename, Str(".hpp")) ||
		StringEndsWith(filename, Str(".c++")) ||
		StringEndsWith(filename, Str(".h++")) ||
		StringEndsWith(filename, Str(".odin")))
		color = 0xFFCC00CC;
	else if (
		StringEndsWith(filename, Str(".py")) ||
		StringEndsWith(filename, Str(".bat")))
		color = 0xFF00CCCC;
	else if (
		StringEndsWith(filename, Str(".rdbg")) ||
		StringEndsWith(filename, Str(".ttf")) ||
		StringEndsWith(filename, Str(".obj")) ||
		StringEndsWith(filename, Str(".exp")) ||
		StringEndsWith(filename, Str(".lib")) ||
		StringEndsWith(filename, Str(".pdb")))
		color = 0xFF444444;
	return color;
}

static void
PushPanel_(App* app, Panel* panel, Arena* output_arena, Rect rect, bool is_selected)
{
	Trace();
	switch (panel->state)
	{
		default: Log(LOG_ERROR, "panel is in invalid state: %u", (uint32)panel->state); break;
		case PanelState_TextView:
		{
			TextView* view = &panel->text_view;
			PushTextView_(app, view, output_arena, rect, 0, is_selected ? 0xFF3F3F3F : 0xFF1F1F1F);
		} break;
		case PanelState_OpenFileView:
		{
			ArenaSavepoint scratch = ArenaSave(ScratchArena(1, &output_arena));
			OpenFileView* view = &panel->open_file_view;
			TextBuffer* textbuf = AppGetTextBuffer(app, view->filter);
			view->layout = UpdateSimpleViewLayout_(app, rect);

			String line = TextBufferWriteToArena(textbuf, scratch.arena);
			PushText2_(app, output_arena, view->layout.filter_bar_text, line, 0xFFFFFFFF, 0, 1, 0, NULL);

			intz last_slash_index = -1;
			for (intz i = 0; i < line.size; ++i)
			{
				if (line.data[i] == '/')
					last_slash_index = i;
			}
			if (last_slash_index != -1)
				line = StringSlice(line, last_slash_index+1, -1);

			view->visible_entries_count = view->entry_count;
			Rect entries_rect = view->layout.entries;
			intz entry_index = 0;
			intz filtered_entry_count = 0;
			for (intz i = 0; i < 2; ++i)
			{
				Buffer entries = view->all_entries;
				for (; entries.size > 0;)
				{
					if (entries_rect.y1 >= entries_rect.y2)
					{
						view->visible_entries_count = entry_index - view->first_line - 1;
						break;
					}

					OpenFileViewEntry* entry = (OpenFileViewEntry*)entries.data;
					entries = StringSlice(entries, SignedSizeof(OpenFileViewEntry) + AlignUp(entry->name_size, (intz)alignof(OpenFileViewEntry)-1), -1);
					String entry_name = StringMake(entry->name_size, entry->name);

					if (entry->info.directory && i != 0)
						continue;
					else if (!entry->info.directory && i != 1)
						continue;

					if (!FuzzyMatch(line, entry_name))
						continue;

					++filtered_entry_count;
					if (entry_index < view->first_line)
					{
						++entry_index;
						continue;
					}

					Rect line_rect = RectCutTop(&entries_rect, app->glyph_line_height);
					if (view->selected_option == entry_index)
						PushRect_(output_arena, line_rect, NULL, 1, 0, 0xFF3F3F3F);
					Rect icon = RectCutLeft(&line_rect, app->glyph_line_height);
					RectCutLeft(&line_rect, 4);

					uint32 color = 0xFFCCCCCC;
					if (entry->info.directory)
						color = 0xFF0080FF;
					else
						color = ColorFromFileName_(entry_name, color);

					PushRect_(output_arena, icon, NULL, 1, 0, color);
					PushText2_(app, output_arena, line_rect, entry_name, 0xFFFFFFFF, 0, 1, 0, NULL);
					++entry_index;
				}
			}
			view->selected_option = Clamp(view->selected_option, 0, filtered_entry_count-1);

			{
				LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.offset, app->tab_size);
				Rect rect = RectMakeSized(
					view->layout.filter_bar_text.x1 + (pos.col - 1) * app->glyph_advance,
					view->layout.filter_bar_text.y1,
					app->glyph_advance,
					app->glyph_height
				);
				PushRect_(output_arena, RectClip(rect, view->layout.filter_bar_text), NULL, 1, 0, app->c_cursor);
			}
			{
				LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.marker_offset, app->tab_size);
				Rect rect = RectMakeSized(
					view->layout.filter_bar_text.x1 + (pos.col - 1) * app->glyph_advance,
					view->layout.filter_bar_text.y1,
					app->glyph_advance,
					app->glyph_height
				);
				PushRect_(output_arena, RectClip(rect, view->layout.filter_bar_text), NULL, 1, 0, app->c_cursor_marker);
			}

			ArenaRestore(scratch);
		} break;
		case PanelState_CommandView:
		{
			ArenaSavepoint scratch = ArenaSave(ScratchArena(1, &output_arena));
			CommandView* view = &panel->command_view;
			TextBuffer* textbuf = AppGetTextBuffer(app, view->filter);
			view->layout = UpdateSimpleViewLayout_(app, rect);

			String filter_str = TextBufferWriteToArena(textbuf, scratch.arena);
			PushText2_(app, output_arena, view->layout.filter_bar_text, filter_str, 0xFFFFFFFF, 0, 1, 0, NULL);

			Rect entries_rect = view->layout.entries;
			intz running_index = 0;
			for (intz i = 1; i < CommandKind__Count; ++i)
			{
				String command_name = g_commands_names[i];
				if (!FuzzyMatch(filter_str, command_name))
					continue;
				intz this_index = running_index++;

				Rect line_rect = RectCutTop(&entries_rect, app->glyph_line_height);
				if (this_index == view->selected_option)
					PushRect_(output_arena, line_rect, NULL, 1, 0, 0xFF3F3F3F);
				PushText2_(app, output_arena, line_rect, command_name, 0xFFFFFFFF, 0, 1, 0, NULL);
			}
			
			{
				LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.offset, app->tab_size);
				Rect rect = RectMakeSized(
					view->layout.filter_bar_text.x1 + (pos.col - 1) * app->glyph_advance,
					view->layout.filter_bar_text.y1,
					app->glyph_advance,
					app->glyph_height
				);
				PushRect_(output_arena, RectClip(rect, view->layout.filter_bar_text), NULL, 1, 0, app->c_cursor);
			}
			{
				LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.marker_offset, app->tab_size);
				Rect rect = RectMakeSized(
					view->layout.filter_bar_text.x1 + (pos.col - 1) * app->glyph_advance,
					view->layout.filter_bar_text.y1,
					app->glyph_advance,
					app->glyph_height
				);
				PushRect_(output_arena, RectClip(rect, view->layout.filter_bar_text), NULL, 1, 0, app->c_cursor_marker);
			}


			ArenaRestore(scratch);
		} break;
		case PanelState_AlreadyOpenFileView:
		{
			ArenaSavepoint scratch = ArenaSave(ScratchArena(1, &output_arena));
			AlreadyOpenFileView* view = &panel->already_open_file_view;
			TextBuffer* textbuf = AppGetTextBuffer(app, view->filter);
			view->layout = UpdateSimpleViewLayout_(app, rect);
			String filter_str = TextBufferWriteToArena(textbuf, scratch.arena);
			PushText2_(app, output_arena, view->layout.filter_bar_text, filter_str, 0xFFFFFFFF, 0, 1, 0, NULL);

			Rect entries_rect = view->layout.entries;
			intz running_index = 0;
			for (intz i = app->openbuffers_count-1; i >= 0; --i)
			{
				TextBufferHandle file_textbuf_handle = app->openbuffers[i];
				SafeAssert(file_textbuf_handle);
				TextBuffer* file_textbuf = AppGetTextBuffer(app, file_textbuf_handle);
				SafeAssert(file_textbuf);
				String path = file_textbuf->file_absolute_path;
				if (path.size)
				{
					if (StringStartsWith(path, app->project_path))
					{
						path = StringSlice(path, app->project_path.size, -1);
						if (path.size > 0 && path.data[0] == '/')
							path = StringSlice(path, 1, -1);
					}
				}
				else
					path = file_textbuf->name;
				if (!FuzzyMatch(filter_str, path))
					continue;
				intz this_index = running_index++;
				if (view->first_option < this_index)
					continue;

				Rect line_rect = RectCutTop(&entries_rect, app->glyph_line_height);
				if (this_index == view->selected_option)
					PushRect_(output_arena, line_rect, NULL, 1, 0, 0xFF3F3F3F);
				Rect icon = RectCutLeft(&line_rect, app->glyph_line_height);
				RectCutLeft(&line_rect, 4);

				PushRect_(output_arena, icon, NULL, 1, 0, ColorFromFileName_(path, 0xFFCCCCCC));
				PushText2_(app, output_arena, line_rect, path, 0xFFFFFFFF, 0, 1, 0, NULL);
			}

			{
				LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.offset, app->tab_size);
				Rect rect = RectMakeSized(
					view->layout.filter_bar_text.x1 + (pos.col - 1) * app->glyph_advance,
					view->layout.filter_bar_text.y1,
					app->glyph_advance,
					app->glyph_height
				);
				PushRect_(output_arena, RectClip(rect, view->layout.filter_bar_text), NULL, 1, 0, app->c_cursor);
			}
			{
				LineCol pos = TextBufferLineColFromOffset(textbuf, view->cursor.marker_offset, app->tab_size);
				Rect rect = RectMakeSized(
					view->layout.filter_bar_text.x1 + (pos.col - 1) * app->glyph_advance,
					view->layout.filter_bar_text.y1,
					app->glyph_advance,
					app->glyph_height
				);
				PushRect_(output_arena, RectClip(rect, view->layout.filter_bar_text), NULL, 1, 0, app->c_cursor_marker);
			}

			ArenaRestore(scratch);
		} break;
	}
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
		.heap = OS_HeapAllocator(),
		.tick_rate = OS_TickRate(),
		.c_background = 0xFF0F0F0F,
		.c_foreground = 0xFFCCCCCC,
		.c_operators = 0xFF7575DC,
		.c_preproc = 0xFFAD519B,
		.c_comment = 0xFF7D7D7D,
		.c_number = 0xFF65B1FF,
		.c_string = 0xFF76E2E2,
		.c_keyword = 0xFFF34CFF,
		.c_cursor_marker = 0x7F3F3F3F,
		.c_cursor = 0xCF7F7F7F,
	});

	{
		Arena local = ArenaFromMemory(app->project_path_buffer, sizeof(app->project_path_buffer));
		app->project_path = OS_ResolveToAbsolutePath(Str("."), AllocatorFromArena(&local), NULL);
		Log(LOG_INFO, "working dir: %S", app->project_path);
	}

	app->window = OS_CreateWindow(&(OS_WindowDesc) {
		.title = StrInit("bed"),
		.width = 800,
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
				0x21, 0x7E, // ASCII
				0xA1, 0xFF, // Latin-1
				0xFFFD, 0xFFFD, // invalid char glyph
				0x3C0, 0x3C0, // π
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
			app->glyph_line_height = ceilf(units * (font_metrics.Base.ascent + font_metrics.Base.descent + font_metrics.Base.lineGap));
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
				if (codepoint == 0xFFFD)
					app->invalid_glyph_index = index;

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

	// make default views
	{
		app->left_panel = (Panel) {
			.state = PanelState_TextView,
			.text_view = {
				.cursor = {},
				.line = 1,
			},
		};
		String file = StrInit("os/os_win32.c");
		AppCreateFileTextBuffer(app, file, &app->left_panel.text_view.textbuf);

		app->right_panel = (Panel) {
			.state = PanelState_TextView,
			.text_view = {
				.cursor = {},
				.line = 1,
			},
		};
		AppCreateNamedTextBuffer(app, Str("Hello, World!\nπ"), Str("*scratch*"), TextBufferKind_Scratch, &app->right_panel.text_view.textbuf);
	}

	// console test
	// Console console = {};
	// SafeAssert(InitConsole(app, &console));

	bool is_first_frame = true;
	for (; !app->is_closing; is_first_frame = false)
	{
		TraceFrameBegin();
		ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
		intz event_count;
		OS_Event* events = OS_PollEvents(is_first_frame ? 0 : UINT64_MAX, scratch.arena, &event_count);

		bool set_cursor_pos_from_mouse_already = false;
		bool should_resize_buffers = false;
		for (intz i = 0; i < event_count; ++i)
		{
			OS_Event* event = &events[i];
			if (event->window_handle.ptr != app->window.ptr)
				continue;
			Panel* panel_to_handle_event = (app->is_right_panel_selected) ? &app->right_panel : &app->left_panel;

			if (event->kind == OS_EventKind_WindowClose)
			{
				app->is_closing = true;
				panel_to_handle_event = NULL;
			}
			else if (event->kind == OS_EventKind_WindowResize)
			{
				should_resize_buffers = true;
				panel_to_handle_event = NULL;
			}
			else if (event->kind == OS_EventKind_WindowKeyPressed)
			{
				switch (event->window_key.key)
				{
					default: break;
					case OS_KeyboardKey_Comma:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							app->is_right_panel_selected ^= 1;
							panel_to_handle_event = NULL;
						}
					} break;
				}
			}
			else if (event->kind == OS_EventKind_WindowTyping)
			{
				uint32 codepoint = event->window_typing.codepoint;
				if (event->window_typing.ctrl && codepoint == ' ')
					panel_to_handle_event = NULL;
			}
			else if (event->kind == OS_EventKind_WindowMouseWheel)
			{
				if (event->window_mouse_wheel.mouse_x < app->window_width / 2)
					panel_to_handle_event = &app->left_panel;
				else
					panel_to_handle_event = &app->right_panel;
			}
			else if (event->kind == OS_EventKind_WindowMouseClick)
			{
				if (event->window_mouse_click.button == OS_MouseButton_Left)
				{
					if (event->window_mouse_click.mouse_x < app->window_width / 2)
						app->is_right_panel_selected = false;
					else
						app->is_right_panel_selected = true;

					panel_to_handle_event = (app->is_right_panel_selected) ? &app->right_panel : &app->left_panel;
					app->is_mouse_dragging = true;
					set_cursor_pos_from_mouse_already = true;
				}
			}
			else if (event->kind == OS_EventKind_WindowMouseRelease)
			{
				if (event->window_mouse_click.button == OS_MouseButton_Left)
					app->is_mouse_dragging = false;
			}

			if (panel_to_handle_event)
				PanelProcessEvent(app, panel_to_handle_event, event);
		}

		if (app->is_mouse_dragging && !set_cursor_pos_from_mouse_already)
		{
			Panel* panel_to_handle_event = (app->is_right_panel_selected) ? &app->right_panel : &app->left_panel;
			OS_MouseState mouse = OS_GetWindowMouseState(app->window);
			PanelProcessCursorDrag(app, panel_to_handle_event, &mouse);
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

			Rect left_view_rect, right_view_rect;
			RectCutSplitH(&screen, &left_view_rect, &right_view_rect);
			PushPanel_(app, &app->left_panel, scratch.arena, left_view_rect, !app->is_right_panel_selected);
			PushPanel_(app, &app->right_panel, scratch.arena, right_view_rect, app->is_right_panel_selected);

			QuadVertex_* last_vertex = ArenaEnd(scratch.arena);
			intz vertex_count = last_vertex - first_vertex;
			quad_count = vertex_count / 4;
			R3_UpdateBuffer(app->r3, &app->quads_vbuf, first_vertex, sizeof(QuadVertex_) * (uint32)vertex_count);
		}

		R3_SetRenderTarget(app->r3, NULL);
		R3_SetViewports(app->r3, 1, &(R3_Viewport) { 0, 0, width, height, 0, 1 });
		R3_Clear(app->r3, &(R3_ClearDesc) {
			.flag_color = true,
			.color = {
				(app->c_background       & 0xFF) / 255.0f,
				(app->c_background >> 8  & 0xFF) / 255.0f,
				(app->c_background >> 16 & 0xFF) / 255.0f,
				(app->c_background >> 24       ) / 255.0f,
			},
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
		TraceFrameEnd();
	}

	// DeinitConsole(&console);

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
