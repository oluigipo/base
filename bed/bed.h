#ifndef BED_H
#define BED_H

#include "common_basic.h"
#include "api_os.h"
#include "api_render3.h"

enum TextBufferKind
{
	TextBufferKind_Null = 0,
	TextBufferKind_OwnedGapBuffer,
}
typedef TextBufferKind;

struct TextBuffer
{
	TextBufferKind kind;
	uint8* utf8_text;
	intz size;
	intz gap_start;
	intz gap_end;
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

struct App
{
	Arena* arena;
	OS_Window window;
	R3_Context* r3;

	bool is_closing;
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
	int32 tab_size;

	// glyph entries
	GlyphEntry_* glyph_entries;
	intz glyph_entries_log2cap;
	intz glyph_entries_count;

	// starter view
	TextCursor cursor;
	TextBuffer buffer;
}
typedef App;

// ===========================================================================
// ===========================================================================
// Rect cut API
struct Rect
{
	int32 x1, y1;
	int32 x2, y2;
}
typedef Rect;

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

#endif