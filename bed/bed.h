#ifndef BED_H
#define BED_H

#include "common_basic.h"
#include "api_os.h"
#include "api_render3.h"
#include "cfront/api.h"

#ifndef BED_API
#	define BED_API
#endif

typedef struct TextBufferHandle_* TextBufferHandle;

struct Range
{
	intz start;
	intz end;
}
typedef Range;

struct Rect
{
	int32 x1, y1;
	int32 x2, y2;
}
typedef Rect;

String typedef HeapAllocatedString;
Buffer typedef HeapAllocatedBuffer;

enum TextBufferEditKind
{
	TextBufferEditKind_Delete = 0,
	TextBufferEditKind_Insert,
	TextBufferEditKind_Transpose,
}
typedef TextBufferEditKind;

struct TextBufferEdit
{
	TextBufferEditKind kind : 8;
	uint64 tick : 56;
	union
	{
		struct
		{
			Range edit_range;
			Range edit_text_buffer;
		} insert, delete;
		struct
		{
			Range from;
			Range to;
		} transpose;
	};
}
typedef TextBufferEdit;

enum TextBufferKind
{
	TextBufferKind_Null = 0,
	TextBufferKind_TextFile,
	TextBufferKind_CFile,
	TextBufferKind_Scratch,
	TextBufferKind_SingleLine,
}
typedef TextBufferKind;

struct TextBuffer
{
	// pool stuff
	uint32 generation;
	uint32 next_free;

	// buffer
	TextBufferKind kind;
	bool is_crlf; // default is LF
	intz ref_count; // multiple textviews can access the same textbuffer

	uint8* utf8_text;
	intz size;
	intz gap_start;
	intz gap_end;

	intz dirty_changes_start;
	intz dirty_changes_end;
	intz cf_count;
	intz cf_cap;
	CF_TokenKind* cf_kinds;
	CF_SourceRange* cf_ranges;

	// for undo & redo
	intz edits_count;
	intz edits_usable_count; // if count < usable_count, then we can redo
	intz edits_cap;
	TextBufferEdit* edits;
	intz edit_text_buffer_size;
	intz edit_text_buffer_usable_size;
	intz edit_text_buffer_cap;
	uint8* edit_text_buffer;

	// for file operations
	HeapAllocatedString file_absolute_path;
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
	TextCursor cursor;
	TextBufferHandle textbuf;
	TextViewLayout layout;
	int32 line;
}
typedef TextView;

struct OpenFileViewLayout
{
	Rect all;
	Rect filter_bar;
	Rect filter_bar_text;
	Rect entries;
}
typedef OpenFileViewLayout;

struct OpenFileViewEntry
{
	OS_FileInfo info;
	intz name_size;
	uint8 name[];
}
typedef OpenFileViewEntry;

struct OpenFileView
{
	TextBufferHandle filter;
	TextCursor cursor;
	intz selected_option;

	intz slash_count;
	intz entry_count;
	int32 visible_entries_count;
	HeapAllocatedBuffer all_entries;

	int32 first_line;
	OpenFileViewLayout layout;
}
typedef OpenFileView;

struct CommandView
{
	TextBufferHandle filter;
	TextCursor cursor;
	intz selected_option;
}
typedef CommandView;

enum PanelState
{
	PanelState_TextView = 0,
	PanelState_OpenFileView,
	PanelState_CommandView,
}
typedef PanelState;

struct Panel
{
	PanelState state;

	TextView text_view;
	OpenFileView open_file_view;
}
typedef Panel;

struct App
{
	Arena* arena;
	OS_Window window;
	R3_Context* r3;
	Allocator heap;
	uint64 tick_rate;

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
	bool is_right_panel_selected;
	Panel left_panel;
	Panel right_panel;

	// default buffers
	TextBufferHandle textbuf_scratch;

	// textbuf pool
	TextBuffer* textbuf_pool;
	intz textbuf_pool_cap;
	intz textbuf_pool_count;
	intz textbuf_pool_first_free;

	// colors
	uint32 c_background;
	uint32 c_foreground;
	uint32 c_operators;
	uint32 c_preproc;
	uint32 c_comment;
	uint32 c_number;
	uint32 c_string;
	uint32 c_keyword;
	uint32 c_cursor_marker;
	uint32 c_cursor;

	// path operations
	String project_path;
	alignas(16) uint8 project_path_buffer[32767+2];
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
// Utils
static inline bool
IsStartOfCodepoint(uint8 ch)
{ return (ch & 0x80) == 0 || (ch & 0x40) != 0; }

static inline int32
RoundToNextTab(int32 col, int32 tab_size)
{
	int32 rem = col % tab_size;
	col += tab_size - rem;
	return col;
}

static inline bool
RangesIntersects(Range a, Range b)
{
	if (a.end >= b.end && a.start < b.end)
		return true;
	if (b.end >= a.end && b.start < a.end)
		return true;
	return false;
}

static inline Range
RangeContaining(Range a, Range b)
{
	return (Range) {
		.start = Min(a.start, b.start),
		.end = Min(a.end, b.end),
	};
}

static inline void
RangesTranpose(Range* restrict a, Range* restrict b)
{
	intz asize = a->end - a->start;
	intz bsize = b->end - b->start;
	if (a->start < b->start)
	{
		b->start += (bsize - asize);
		a->end = a->start + bsize;
		b->end = b->start + asize;
	}
	else
	{
		a->start += (asize - bsize);
		a->end = a->start + bsize;
		b->end = b->start + asize;
	}
}

static inline bool
RangeInside(Range subset, Range set)
{
	intz start = Min(subset.start, set.start);
	intz end = Min(subset.end, set.end);
	return (start == set.start || end == set.end);
}

static inline intz
RangeSize(Range range)
{
	return range.end - range.start;
}

static inline Range
RangeMakeSized(intz start, intz size)
{
	return (Range) {
		.start = start,
		.end = start + size,
	};
}

static inline Range
RangeMake(intz start, intz end)
{
	return (Range) {
		.start = start,
		.end = end,
	};
}

static inline intz
RangeRemoveFromOffset(intz offset, Range range)
{
	if (offset > range.end)
		return offset - (range.end - range.start);
	if (offset > range.start)
		return range.start;
	return offset;
}

static inline bool
RangeIsValid(Range range)
{
	return range.start <= range.end;
}

static inline bool
FuzzyMatch(String filter, String str)
{
	Trace();
	intz filter_i = 0;

	for (intz i = 0; filter_i < filter.size && i < str.size; ++i)
		filter_i += (filter.data[filter_i] == str.data[i]);

	return (filter_i == filter.size);
}

// ===========================================================================
// ===========================================================================
// TextBuffer API
BED_API TextBuffer* TextBufferFromHandle   (App* app, TextBufferHandle handle);
BED_API TextBuffer* TextBufferFromFile     (App* app, String path, TextBufferKind kind, TextBufferHandle* out_handle);
BED_API TextBuffer* TextBufferFromString   (App* app, String str, TextBufferKind kind, TextBufferHandle* out_handle);
BED_API TextBuffer* TextBufferIncRefCount  (App* app, TextBufferHandle handle);
BED_API void        TextBufferDecRefCount  (App* app, TextBufferHandle handle);
BED_API void        TextBufferRefreshTokens(App* app, TextBuffer* textbuf);
BED_API bool        TextBufferIterate      (App* app, intz* it, TextBuffer** out_textbuf, TextBufferHandle* out_handle);

BED_API void    TextBufferGetStrings       (TextBuffer* textbuf, String* out_left, String* out_right);
BED_API uint8   TextBufferSample           (TextBuffer* textbuf, intz offset);
BED_API intz    TextBufferSize             (TextBuffer* textbuf);
BED_API int32   TextBufferLineCount        (TextBuffer* textbuf);
BED_API LineCol TextBufferLineColFromOffset(TextBuffer* textbuf, intz offset, int32 tab_size);
BED_API int32   TextBufferColFromOffset    (TextBuffer* textbuf, intz offset, int32 tab_size);
BED_API void    TextBufferMoveGapToOffset  (TextBuffer* textbuf, intz offset);
BED_API String  TextBufferStringFromRange  (TextBuffer* textbuf, intz offset, intz size);
BED_API bool    TextBufferLineIterator     (TextBuffer* textbuf, intz* it, String* left_str, String* right_str);
BED_API intz    TextBufferOffsetFromLineCol(TextBuffer* textbuf, LineCol pos, int32 tab_size);
BED_API Buffer  TextBufferWriteRangeToBuffer(TextBuffer* textbuf, Range range, intz size, uint8 buf[]);

BED_API void    TextBufferInsert           (App* app, TextBuffer* textbuf, intz offset, String str);
BED_API void    TextBufferDelete           (App* app, TextBuffer* textbuf, intz offset, intz size);
BED_API void    TextBufferTranspose        (App* app, TextBuffer* textbuf, Range first, Range second);
BED_API bool    TextBufferUndo             (App* app, TextBuffer* textbuf, TextBufferEdit* out_edit);
BED_API bool    TextBufferRedo             (App* app, TextBuffer* textbuf, TextBufferEdit* out_edit);
BED_API void    TextBufferReplace          (App* app, TextBuffer* textbuf, Range range, String str);

// ===========================================================================
// ===========================================================================
// TextCursor API
BED_API void TextCursorCmdInsert                  (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount, uint32 codepoint);
BED_API void TextCursorCmdInsertString            (App* app, TextCursor* cursor, TextBuffer* textbuf, String str);
BED_API void TextCursorCmdDeleteBackward          (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdLeft                    (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdRight                   (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdUp                      (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDown                    (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdEndOfLine               (TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdStartOfLine             (TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdPlaceMarker             (TextCursor* cursor);
BED_API void TextCursorCmdDeleteToMarker          (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdRightSnakeWord          (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdLeftSnakeWord           (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDeleteBackwardSnakeWord (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdCopy                    (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdCut                     (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdPaste                   (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdUpParagraph             (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDownParagraph           (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdSet                     (TextCursor* cursor, TextBuffer* textbuf, LineCol pos, int32 tab_size);
BED_API void TextCursorCmdSetMarker               (TextCursor* cursor, TextBuffer* textbuf, LineCol pos, int32 tab_size);
BED_API void TextCursorCmdRightPascalWord         (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdLeftPascalWord          (TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDeleteBackwardPascalWord(App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdUndo                    (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdRedo                    (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdDeleteLine              (App* app, TextCursor* cursor, TextBuffer* textbuf);
BED_API void TextCursorCmdMoveLineUp              (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdMoveLineDown            (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount);
BED_API void TextCursorCmdDuplicateLine           (App* app, TextCursor* cursor, TextBuffer* textbuf, intz amount);

// ===========================================================================
// ===========================================================================
// Automatic C/C++ Indentation API
struct CIndentCtx
{
	bool preproc_nesting;
	uint64 braces_nesting_bitset;
	uint64 parens_nesting_bitset;
	uint64 unfinished_stmt_nesting_bitset;
}
typedef CIndentCtx;

BED_API int32 CIndentPushRange(CIndentCtx* cindent, Range line_range, intz cf_count, CF_TokenKind const cf_kinds[], CF_SourceRange const cf_ranges[], intz start_cf_index);

// ===========================================================================
// ===========================================================================
// Commands
enum CommandKind
{
	CommandKind_Null = 0,

	// Panel commands
	CommandKind_OpenFile,                 //
	CommandKind_NewFile,                  //

	// Cursor commands
	CommandKind_Insert,                   // amount, codepoint
	CommandKind_InsertString,             // string
	CommandKind_DeleteBackward,           // amount
	CommandKind_Left,                     // amount
	CommandKind_Right,                    // amount
	CommandKind_Up,                       // amount
	CommandKind_Down,                     // amount
	CommandKind_EndOfLine,                //
	CommandKind_StartOfLine,              //
	CommandKind_PlaceMarker,              //
	CommandKind_DeleteToMarker,           //
	CommandKind_RightSnakeWord,           // amount
	CommandKind_LeftSnakeWord,            // amount
	CommandKind_DeleteBackwardSnakeWord,  // amount
	CommandKind_RightPascalWord,          // amount
	CommandKind_LeftPascalWord,           // amount
	CommandKind_DeleteBackwardPascalWord, // amount
	CommandKind_Copy,                     //
	CommandKind_Cut,                      //
	CommandKind_Paste,                    // amount
	CommandKind_UpParagraph,              // amount
	CommandKind_DownParagraph,            // amount
	CommandKind_Set,                      // linecol
	CommandKind_SetMarker,                // linecol
	CommandKind_Undo,                     // 
	CommandKind_Redo,                     // 
	CommandKind_DeleteLine,               //
	CommandKind_MoveLineUp,               // amount
	CommandKind_MoveLineDown,             // amount
	CommandKind_DuplicateLine,            // amount
}
typedef CommandKind;

struct Command
{
	CommandKind kind;

	LineCol linecol;
	intz amount;
	String string;
}
typedef Command;

#endif