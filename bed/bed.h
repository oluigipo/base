#ifndef BED_H
#define BED_H

#include "common_basic.h"
#include "api_os.h"
#include "api_render3.h"
#include "cfront/api.h"

#ifndef BED_API
#	define BED_API
#endif

struct Rect
{
	int32 x1, y1;
	int32 x2, y2;
}
typedef Rect;

enum TextBufferKind
{
	TextBufferKind_Null = 0,
	TextBufferKind_NormalText,
	TextBufferKind_C,
}
typedef TextBufferKind;

struct TextBuffer
{
	TextBufferKind kind;
	uint32 next_free;

	uint8* utf8_text;
	intz size;
	intz gap_start;
	intz gap_end;

	intz change_start;
	intz change_end;

	intz cf_count;
	intz cf_cap;
	CF_TokenKind* cf_kinds;
	CF_SourceRange* cf_ranges;
}
typedef TextBuffer;

struct LineCol
{
	int32 line;
	int32 col;
}
typedef LineCol;

struct TextCursor
{
	intz offset;
	intz marker_offset;
}
typedef TextCursor;

struct GlyphEntry_
{
	uint32 codepoint;
	int16 atlas_x, atlas_y;
	int16 draw_offset_x, draw_offset_y;
}
typedef GlyphEntry_;

enum TextViewKind
{
	TextViewKind_Null = 0,
	TextViewKind_File,
	TextViewKind_Scratch,
}
typedef TextViewKind;

struct TextViewLayout
{
	Rect all;
	Rect status_bar;
	Rect status_bar_text;
	Rect line_bar;
	Rect line_bar_text;
	Rect text_screen;
}
typedef TextViewLayout;

struct TextView
{
	TextViewKind kind;
	TextCursor cursor;
	intz textbuf_index;
	String file_path;
	TextViewLayout layout;

	int32 line;
}
typedef TextView;

struct App
{
	Arena* arena;
	OS_Window window;
	R3_Context* r3;
	Allocator heap;

	bool is_closing;
	bool is_mouse_dragging;
	int32 window_width, window_height;

	// r3 resources
	R3_Pipeline quads_pipeline;
	R3_Buffer quads_vbuf;
	R3_Buffer quads_ibuf;
	R3_Buffer quads_ubuf;
	R3_Sampler sampler;
	R3_Texture white_texture;
	intz max_quad_count;

	// dwrite & d2d
	struct ID2D1Factory1* d2d_factory;
	struct ID2D1RenderTarget* d2d_rt;
	R3_Texture d2d_rt_texture;
	struct ID2D1SolidColorBrush* d2d_brush;
	struct IDWriteFactory* dw_factory;
	struct IDWriteFontFile* dw_font_file;
	struct IDWriteFontFace* dw_font_face;
	struct IDWriteFontFace1* dw_font_face1;
	int32 glyph_width;
	int32 glyph_height;
	int32 glyph_texture_size;
	int32 glyph_advance;
	int32 glyph_line_height;
	int32 tab_size;

	// glyph entries
	GlyphEntry_* glyph_entries;
	intz glyph_entries_log2cap;
	intz glyph_entries_count;
	intz invalid_glyph_index;

	// starter view
	bool is_right_view_selected;
	TextView left_view;
	TextView right_view;

	// textbuf pool
	TextBuffer* textbuf_pool;
	intz textbuf_pool_cap;
	intz textbuf_pool_count;
	intz textbuf_pool_first_free;
}
typedef App;

// ===========================================================================
// ===========================================================================
// Rect cut API

static inline Rect
RectCutLeft(Rect* restrict from, int32 amount)
{
	Rect result = *from;
	from->x1 = ClampMax(from->x1 + amount, from->x2);
	result.x2 = from->x1;
	return result;
}

static inline Rect
RectCutRight(Rect* restrict from, int32 amount)
{
	Rect result = *from;
	from->x2 = ClampMin(from->x2 - amount, from->x1);
	result.x1 = from->x2;
	return result;
}

static inline Rect
RectCutTop(Rect* restrict from, int32 amount)
{
	Rect result = *from;
	from->y1 = ClampMax(from->y1 + amount, from->y2);
	result.y2 = from->y1;
	return result;
}

static inline Rect
RectCutBottom(Rect* restrict from, int32 amount)
{
	Rect result = *from;
	from->y2 = ClampMin(from->y2 - amount, from->y1);
	result.y1 = from->y2;
	return result;
}

static inline Rect
RectCutMargin(Rect from, int32 amount)
{
	from.x1 += amount;
	from.x2 = ClampMin(from.x1, from.x2 - amount);
	from.y1 += amount;
	from.y2 = ClampMin(from.y1, from.y2 - amount);
	return from;
}

static inline Rect
RectCutMarginTopLeft(Rect from, int32 amount)
{
	from.x1 = ClampMax(from.x1 + amount, from.x2);
	from.y1 = ClampMax(from.y1 + amount, from.y2);
	return from;
}

static inline void
RectCutSplitH(Rect* from, Rect* out_left, Rect* out_right)
{
	Rect f = *from;
	int32 width = f.x2 - f.x1;
	*out_left = (Rect) { f.x1, f.y1, f.x1 + width/2, f.y2 };
	*out_right = (Rect) { f.x1 + width/2, f.y1, f.x2, f.y2 };
}

// ===========================================================================
// ===========================================================================
// UTF-8 Utils
static inline bool
IsStartOfCodepoint(uint8 ch)
{ return (ch & 0x80) == 0 || (ch & 0x40) != 0; }

// ===========================================================================
// ===========================================================================
// TextBuffer API
BED_API TextBuffer* TextBufferFromIndex    (App* app, intz index);
BED_API TextBuffer* TextBufferFromFile     (App* app, String path, TextBufferKind kind, intz* out_index);
BED_API TextBuffer* TextBufferFromString   (App* app, String str, TextBufferKind kind, intz* out_index);
BED_API void        TextBufferRefreshTokens(App* app, TextBuffer* textbuf);

BED_API void    TextBufferGetStrings       (TextBuffer* textbuf, String* out_left, String* out_right);
BED_API uint8   TextBufferSample           (TextBuffer* textbuf, intz offset);
BED_API intz    TextBufferSize             (TextBuffer* textbuf);
BED_API int32   TextBufferLineCount        (TextBuffer* textbuf);
BED_API LineCol TextBufferLineColFromOffset(TextBuffer* textbuf, intz offset, int32 tab_size);
BED_API int32   TextBufferColFromOffset    (TextBuffer* textbuf, intz offset, int32 tab_size);
BED_API void    TextBufferMoveGapToOffset  (TextBuffer* textbuf, intz offset);
BED_API uint8*  TextBufferInsert           (App* app, TextBuffer* textbuf, intz offset, intz size);
BED_API void    TextBufferDelete           (TextBuffer* textbuf, intz offset, intz size);
BED_API String  TextBufferStringFromRange  (TextBuffer* textbuf, intz offset, intz size);
BED_API bool    TextBufferLineIterator     (TextBuffer* textbuf, intz* it, String* left_str, String* right_str);
BED_API intz    TextBufferOffsetFromLineCol(TextBuffer* textbuf, LineCol pos, int32 tab_size);

// ===========================================================================
// ===========================================================================
// TextCursor API
BED_API void TextCursorCmdInsert                 (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount, uint32 codepoint);
BED_API void TextCursorCmdDeleteBackward         (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdLeft                   (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdRight                  (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdUp                     (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDown                   (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdEndOfLine              (TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdStartOfLine            (TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdPlaceMarker            (TextCursor* cursor);
BED_API void TextCursorCmdDeleteToMarker         (TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdRightSnakeWord         (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdLeftSnakeWord          (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDeleteBackwardSnakeWord(TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdCopy                   (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdPaste                  (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdUpParagraph            (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDownParagraph          (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdSet                    (TextCursor* cursor, TextBuffer* textbuf, LineCol pos, int32 tab_size);
BED_API void TextCursorCmdSetMarker              (TextCursor* cursor, TextBuffer* textbuf, LineCol pos, int32 tab_size);

#endif