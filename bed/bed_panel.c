#include "bed.h"

static String const g_commands_names[] = {
	[CommandKind_Null] = StrInit("(null command)"),

	// Panel commands
	[CommandKind_OpenFile] = StrInit("open file"),
	[CommandKind_NewFile] = StrInit("new file"),

	// Cursor commands
	[CommandKind_CursorDeleteBackward] = StrInit("cursor delete backward"),
	[CommandKind_CursorLeft] = StrInit("cursor left"),
	[CommandKind_CursorRight] = StrInit("cursor right"),
	[CommandKind_CursorUp] = StrInit("cursor up"),
	[CommandKind_CursorDown] = StrInit("cursor down"),
	[CommandKind_CursorEndOfLine] = StrInit("cursor end of line"),
	[CommandKind_CursorStartOfLine] = StrInit("cursor start of line"),
	[CommandKind_CursorPlaceMarker] = StrInit("cursor place marker"),
	[CommandKind_CursorDeleteToMarker] = StrInit("cursor delete to marker"),
	[CommandKind_CursorRightSnakeWord] = StrInit("cursor right snake word"),
	[CommandKind_CursorLeftSnakeWord] = StrInit("cursor left snake word"),
	[CommandKind_CursorDeleteBackwardSnakeWord] = StrInit("cursor delete backward snake word"),
	[CommandKind_CursorRightPascalWord] = StrInit("cursor right pascal word"),
	[CommandKind_CursorLeftPascalWord] = StrInit("cursor left pascal word"),
	[CommandKind_CursorDeleteBackwardPascalWord] = StrInit("cursor delete backward pascal word"),
	[CommandKind_CursorCopy] = StrInit("cursor copy"),
	[CommandKind_CursorCut] = StrInit("cursor cut"),
	[CommandKind_CursorPaste] = StrInit("cursor paste"),
	[CommandKind_CursorUpParagraph] = StrInit("cursor up paragraph"),
	[CommandKind_CursorDownParagraph] = StrInit("cursor down paragraph"),
	[CommandKind_CursorSet] = StrInit("cursor set"),
	[CommandKind_CursorSetMarker] = StrInit("cursor set marker"),
	[CommandKind_CursorUndo] = StrInit("cursor undo"),
	[CommandKind_CursorRedo] = StrInit("cursor redo"),
	[CommandKind_CursorDeleteLine] = StrInit("cursor delete line"),
	[CommandKind_CursorMoveLineUp] = StrInit("cursor move line up"),
	[CommandKind_CursorMoveLineDown] = StrInit("cursor move line down"),
	[CommandKind_CursorDuplicateLine] = StrInit("cursor duplicate line"),
};

static_assert(ArrayLength(g_commands_names) == CommandKind__Count);

static int32
SizeInLinesOfTextView_(App* app, TextView* view)
{
	int32 visible_height = view->layout.text_screen.y2 - view->layout.text_screen.y1;
	int32 visible_line_count = visible_height / app->glyph_line_height;
	float32 visible_line_count_rem = (visible_height % app->glyph_line_height) / (float32)app->glyph_line_height;
	int32 line_count = visible_line_count - (visible_line_count_rem < 0.75f);
	return line_count;
}

static void
ScrollTextViewToCursor_(App* app, TextView* view)
{
	Trace();
	TextBuffer* textbuf = TextBufferFromHandle(app, view->textbuf);
	LineCol cursor_pos = TextBufferLineColFromOffset(textbuf, view->cursor.offset, app->tab_size);
	int32 line_count = SizeInLinesOfTextView_(app, view);

	if (view->line > cursor_pos.line)
		view->line = cursor_pos.line;
	else if (view->line + line_count < cursor_pos.line)
		view->line = cursor_pos.line - line_count;
}

static void
SetCursorPosFromMouse_(App* app, TextView* view, TextBuffer* textbuf, int32 mouse_x, int32 mouse_y)
{
	Trace();
	int32 left_padding = view->layout.text_screen.x1;
	int32 top_padding = view->layout.text_screen.y1;
	int32 col = (mouse_x - left_padding) / app->glyph_advance + 1;
	int32 line = (mouse_y - top_padding) / app->glyph_line_height + view->line;
	TextCursorCmdSet(&view->cursor, textbuf, (LineCol) { line, col }, 4);
}

static void
RefreshOpenFileViewEntries_(App* app, OpenFileView* view)
{
	OS_Error err = { .ok = true, };
	ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
	ArenaSavepoint scratch2 = ArenaSave(ScratchArena(1, &scratch.arena));
	TextBuffer* textbuf = TextBufferFromHandle(app, view->filter);

	String path = {};
	intz slash_count = 0;
	{
		String left, right;
		TextBufferGetStrings(textbuf, &left, &right);
		path = ArenaPrintf(scratch.arena, "%S%S", left, right);

		intz last_slash_index = -1;
		for (intz i = 0; i < path.size; ++i)
		{
			if (path.data[i] == '/')
			{
				last_slash_index = i;
				++slash_count;
			}
		}
		if (last_slash_index)
			path = StringSlice(path, 0, last_slash_index);
	}

	OS_FileIterator it = OS_OpenFileIterator(ArenaPrintf(scratch.arena, "%S/*", path), AllocatorFromArena(scratch.arena), &err);
	HeapAllocatedBuffer all_entries = {};
	intz entry_count = 0;

	if (err.ok)
	{
		uint8* entries_start = ArenaEndAligned(scratch2.arena, alignof(OpenFileViewEntry));
		while (OS_IterateFiles(&it, &err))
		{
			for ArenaTempScope(scratch.arena)
			{
				String str = OS_FilePathFromFileIterator(&it, AllocatorFromArena(scratch.arena), &err);
				if (!err.ok)
					continue;
				if (StringEquals(str, Str(".")) ||
					StringEquals(str, Str("..")) ||
					StringEquals(str, Str(".git")))
					continue;
				OS_FileInfo info = OS_FileInfoFromFileIterator(&it, &err);
				OpenFileViewEntry* new_entry = ArenaPushAligned(scratch2.arena, SignedSizeof(OpenFileViewEntry) + str.size, alignof(OpenFileViewEntry));
				SafeAssert(new_entry);
				new_entry->info = info;
				new_entry->name_size = str.size;
				MemoryCopy(new_entry->name, str.data, str.size);
				++entry_count;
			}
			if (!err.ok)
				break;
		}
		uint8* entries_end = ArenaEnd(scratch2.arena);
		all_entries = AllocatorCloneBuffer(&app->heap, alignof(OpenFileViewEntry), BufRange(entries_start, entries_end), NULL);
	}

	if (!err.ok)
		Log(LOG_ERROR, "failed to update OpenFileView entries: (%x) %S", (uint32)err.code, err.what);
	else
	{
		if (view->all_entries.data)
			AllocatorFreeBuffer(&app->heap, view->all_entries, NULL);
		view->all_entries = all_entries;
		view->entry_count = entry_count;
		view->slash_count = slash_count;
	}

	OS_CloseFileIterator(&it);
	ArenaRestore(scratch2);
	ArenaRestore(scratch);
}

static intz
CountOfAvailableOptionsInGenericListView_(App* app, GenericListView* view, PanelState kind)
{
	Trace();
	TextBuffer* textbuf = TextBufferFromHandle(app, view->filter);
	SafeAssert(textbuf);

	ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
	String filter_text = {};
	{
		String left, right;
		TextBufferGetStrings(textbuf, &left, &right);
		filter_text = ArenaPrintf(scratch.arena, "%S%S", left, right);
	}

	intz result = 0;
	if (kind == PanelState_CommandView)
	{
		for (intz i = 1; i < CommandKind__Count; ++i)
			result += (FuzzyMatch(filter_text, g_commands_names[i]));
	}
	else
	{
		for (intz i = app->openbuffers_count-1; i >= 0; --i)
		{
			TextBufferHandle handle = app->openbuffers[i];
			SafeAssert(handle);
			TextBuffer* file_textbuf = TextBufferFromHandle(app, handle);
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
			result += (FuzzyMatch(filter_text, path));
		}
	}

	ArenaRestore(scratch);
	return result;
}

BED_API void
PanelProcessEvent(App* app, Panel* panel, OS_Event const* event)
{
	Trace();

	switch (panel->state)
	{
		default: Log(LOG_WARN, "panel is in invalid state: %u", (uint32)panel->state); break;
		case PanelState_TextView:
		{
			TextView* view = &panel->text_view;
			TextBuffer* textbuf = TextBufferFromHandle(app, view->textbuf);
			if (!textbuf)
			{
				Log(LOG_ERROR, "failed to get textbuffer from view to handle panel event");
				break;
			}

			if (event->kind == OS_EventKind_WindowKeyPressed)
			{
				bool should_scroll_to_cursor = true;
				switch (event->window_key.key)
				{
					default: should_scroll_to_cursor = false; break;
					case OS_KeyboardKey_Left:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdLeftSnakeWord(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdLeftPascalWord(&view->cursor, textbuf, 1);
						else
							TextCursorCmdLeft(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Right:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdRightSnakeWord(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdRightPascalWord(&view->cursor, textbuf, 1);
						else
							TextCursorCmdRight(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Up:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdUpParagraph(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdMoveLineUp(app, &view->cursor, textbuf, 1);
						else
							TextCursorCmdUp(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Down:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdDownParagraph(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdMoveLineDown(app, &view->cursor, textbuf, 1);
						else
							TextCursorCmdDown(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Backspace:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdDeleteBackwardSnakeWord(app, &view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdDeleteBackwardPascalWord(app, &view->cursor, textbuf, 1);
						else
							TextCursorCmdDeleteBackward(app, &view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_End: TextCursorCmdEndOfLine(&view->cursor, textbuf); break;
					case OS_KeyboardKey_Home: TextCursorCmdStartOfLine(&view->cursor, textbuf); break;
					case OS_KeyboardKey_Enter:
					{
						TextCursorCmdInsert(app, &view->cursor, textbuf, 1, '\n');
					} break;
					case OS_KeyboardKey_Tab:
					{
						if (event->window_key.shift)
							TextCursorCmdInsert(app, &view->cursor, textbuf, 1, '\t');
					} break;
					case 'D':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdDeleteToMarker(app, &view->cursor, textbuf);
					} break;
					case 'C':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdCopy(app, &view->cursor, textbuf);
					} break;
					case 'X':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdCut(app, &view->cursor, textbuf);
						else if (event->window_key.alt)
						{
							TextBufferHandle filter = NULL;
							TextBufferFromString(app, Str(""), StrNull, TextBufferKind_SingleLine, &filter);
							SafeAssert(filter);
							panel->command_view = (CommandView) {
								.filter = filter,
								.cursor = {},
								.prev_panel_state = panel->state,
								.selected_option = 0,
							};
							panel->state = PanelState_CommandView;
						}
					} break;
					case 'V':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdPaste(app, &view->cursor, textbuf, 1);
					} break;
					case ' ':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdPlaceMarker(&view->cursor);
					} break;
					case 'Z':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdUndo(app, &view->cursor, textbuf);
					} break;
					case 'Y':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdRedo(app, &view->cursor, textbuf);
					} break;
					case 'R':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdDeleteLine(app, &view->cursor, textbuf);
					} break;
					case 'Q':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdDuplicateLine(app, &view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_PageUp:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							view->cursor.offset = 0;
						else
							TextCursorCmdUp(&view->cursor, textbuf, SizeInLinesOfTextView_(app, view));
					} break;
					case OS_KeyboardKey_PageDown:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							view->cursor.offset = TextBufferSize(textbuf);
						else
							TextCursorCmdDown(&view->cursor, textbuf, SizeInLinesOfTextView_(app, view));
					} break;
					case 'O':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
							TextBufferHandle filter = NULL;
							TextBuffer* textbuf = TextBufferFromString(app, ArenaPrintf(scratch.arena, "%S/", app->project_path), StrNull, TextBufferKind_SingleLine, &filter);
							ArenaRestore(scratch);
							panel->state = PanelState_OpenFileView;
							panel->open_file_view = (OpenFileView) {
								.filter = filter,
								.selected_option = 0,
								.cursor = {
									.offset = TextBufferSize(textbuf),
									.marker_offset = TextBufferSize(textbuf),
								},
							};
							RefreshOpenFileViewEntries_(app, &panel->open_file_view);
							should_scroll_to_cursor = false;
						}
					} break;
					case 'I':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextBufferHandle filter = NULL;
							TextBufferFromString(app, Str(""), StrNull, TextBufferKind_SingleLine, &filter);
							SafeAssert(filter);
							panel->already_open_file_view = (AlreadyOpenFileView) {
								.filter = filter,
								.cursor = {},
								.prev_panel_state = panel->state,
								.selected_option = 0,
							};
							panel->state = PanelState_AlreadyOpenFileView;
						}
					} break;
					case 'S':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextBufferSaveToDisk(app, textbuf);
					} break;
				}

				if (should_scroll_to_cursor)
					ScrollTextViewToCursor_(app, view);
			}
			else if (event->kind == OS_EventKind_WindowTyping)
			{
				uint32 codepoint = event->window_typing.codepoint;
				if (!(event->window_typing.ctrl && codepoint == ' '))
				{
					TextCursorCmdInsert(app, &view->cursor, textbuf, 1, codepoint);
					ScrollTextViewToCursor_(app, view);
				}
			}
			else if (event->kind == OS_EventKind_WindowMouseWheel)
			{
				int32 max_line = TextBufferLineCount(textbuf);
				view->line -= event->window_mouse_wheel.delta*5;
				view->line = Clamp(view->line, 1, max_line);
			}
			else if (event->kind == OS_EventKind_WindowMouseClick)
			{
				if (event->window_mouse_click.button == OS_MouseButton_Left)
				{
					SetCursorPosFromMouse_(app, view, textbuf, event->window_mouse_click.mouse_x, event->window_mouse_click.mouse_y);
					TextCursorCmdPlaceMarker(&view->cursor);
				}
			}
		} break;
		case PanelState_OpenFileView:
		{
			OpenFileView* view = &panel->open_file_view;
			TextBuffer* textbuf = TextBufferFromHandle(app, view->filter);

			bool is_going_back_to_text_view = false;
			if (event->kind == OS_EventKind_WindowKeyPressed)
			{
				switch (event->window_key.key)
				{
					default: break;
					case OS_KeyboardKey_Escape: is_going_back_to_text_view = true; break;
					case OS_KeyboardKey_Left:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdLeftSnakeWord(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdLeftPascalWord(&view->cursor, textbuf, 1);
						else
							TextCursorCmdLeft(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Right:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdRightSnakeWord(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdRightPascalWord(&view->cursor, textbuf, 1);
						else
							TextCursorCmdRight(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Up:
					{
						view->selected_option = ClampMin(view->selected_option-1, 0);
					} break;
					case OS_KeyboardKey_Down:
					{
						view->selected_option += 1;
					} break;
					case OS_KeyboardKey_Backspace:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdDeleteBackwardSnakeWord(app, &view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdDeleteBackwardPascalWord(app, &view->cursor, textbuf, 1);
						else
							TextCursorCmdDeleteBackward(app, &view->cursor, textbuf, 1);
						view->selected_option = 0;
					} break;
					case OS_KeyboardKey_End: TextCursorCmdEndOfLine(&view->cursor, textbuf); break;
					case OS_KeyboardKey_Home: TextCursorCmdStartOfLine(&view->cursor, textbuf); break;
					case OS_KeyboardKey_Tab:
					case OS_KeyboardKey_Enter:
					{
						ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
						String base_path = {};
						String fuzzy_filter = {};
						{
							String left, right;
							TextBufferGetStrings(textbuf, &left, &right);
							String fullstr = ArenaPrintf(scratch.arena, "%S%S", left, right);
							intz last_slash_index = -1;
							for (intz i = 0; i < fullstr.size; ++i)
							{
								if (fullstr.data[i] == '/')
									last_slash_index = i;
							}
							if (last_slash_index != -1)
							{
								base_path = StringSlice(fullstr, 0, last_slash_index+1);
								fuzzy_filter = StringSlice(fullstr, last_slash_index+1, -1);
							}
							else
								fuzzy_filter = fullstr;
						}

						intz entry_index = 0;
						for (intz i = 0; i < 2; ++i)
						{
							Buffer entries = view->all_entries;
							for (; entries.size > 0;)
							{
								OpenFileViewEntry* entry = (OpenFileViewEntry*)entries.data;
								entries = StringSlice(entries, SignedSizeof(OpenFileViewEntry) + AlignUp(entry->name_size, (intz)alignof(OpenFileViewEntry)-1), -1);
								String entry_name = StringMake(entry->name_size, entry->name);
								if (entry->info.directory && i != 0)
									continue;
								else if (!entry->info.directory && i != 1)
									continue;
								if (!FuzzyMatch(fuzzy_filter, entry_name))
									continue;

								if (view->selected_option > entry_index && entries.size <= 0 && i == 1)
									view->selected_option = entry_index;
								if (view->selected_option == entry_index)
								{
									if (event->window_key.key == OS_KeyboardKey_Tab)
									{
										String fullpath = ArenaPrintf(scratch.arena, "%S%S", base_path, entry_name);
										TextBufferReplace(app, textbuf, RangeMake(0, TextBufferSize(textbuf)), fullpath);
										view->cursor.marker_offset = view->cursor.offset = TextBufferSize(textbuf);
									}
									else
									{
										if (entry->info.directory)
										{
											String fullpath = ArenaPrintf(scratch.arena, "%S%S/", base_path, entry_name);
											TextBufferReplace(app, textbuf, RangeMake(0, TextBufferSize(textbuf)), fullpath);
											view->cursor.marker_offset = view->cursor.offset = TextBufferSize(textbuf);
											RefreshOpenFileViewEntries_(app, view);
										}
										else
										{
											String fullpath = ArenaPrintf(scratch.arena, "%S%S", base_path, entry_name);
											TextBufferHandle new_textbuf = 0;
											AppCreateFileTextBuffer(app, fullpath, &new_textbuf);
											if (new_textbuf)
											{
												if (panel->text_view.textbuf)
													TextBufferDecRefCount(app, panel->text_view.textbuf);
												panel->text_view.textbuf = new_textbuf;
												panel->text_view.line = 1;
												panel->text_view.cursor = (TextCursor) {};
											}
											is_going_back_to_text_view = true;
										}
									}
									goto lbl_out_of_the_loop;
								}
								++entry_index;
							}
						}
						lbl_out_of_the_loop:;
						view->selected_option = 0;

						ArenaRestore(scratch);
					} break;
					case 'D':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdDeleteToMarker(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
					} break;
					case 'C':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdCopy(app, &view->cursor, textbuf);
					} break;
					case 'X':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdCut(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
						else if (event->window_key.alt)
						{
							TextBufferHandle filter = NULL;
							TextBufferFromString(app, Str(""), StrNull, TextBufferKind_SingleLine, &filter);
							SafeAssert(filter);
							panel->command_view = (CommandView) {
								.filter = filter,
								.cursor = {},
								.prev_panel_state = panel->state,
								.selected_option = 0,
							};
							panel->state = PanelState_CommandView;
						}
					} break;
					case 'V':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdPaste(app, &view->cursor, textbuf, 1);
							view->selected_option = 0;
						}
					} break;
					case ' ':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdPlaceMarker(&view->cursor);
					} break;
					case 'Z':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdUndo(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
					} break;
					case 'Y':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdRedo(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
					} break;
					case 'R':
					{

					} break;
					case 'Q':
					{
						
					} break;
					case OS_KeyboardKey_PageUp:
					{

					} break;
					case OS_KeyboardKey_PageDown:
					{

					} break;
				}
			}
			else if (event->kind == OS_EventKind_WindowTyping)
			{
				uint32 codepoint = event->window_typing.codepoint;
				if (!(event->window_typing.ctrl && codepoint == ' '))
				{
					TextCursorCmdInsert(app, &view->cursor, textbuf, 1, codepoint);
					view->selected_option = 0;
				}
			}

			if (is_going_back_to_text_view)
			{
				TextBufferDecRefCount(app, view->filter);
				AllocatorFreeBuffer(&app->heap, view->all_entries, NULL);
				MemoryZero(view, sizeof(OpenFileView));
				panel->state = PanelState_TextView;
				break;
			}

			if (view->selected_option < view->first_line)
				view->first_line = view->selected_option;
			else if (view->selected_option >= view->first_line + view->visible_entries_count)
				view->first_line = view->selected_option - view->visible_entries_count;

			Arena* scratch_arena = ScratchArena(0, NULL);
			for ArenaTempScope(scratch_arena)
			{
				String left, right;
				TextBufferGetStrings(textbuf, &left, &right);
				String filter = ArenaPrintf(scratch_arena, "%S%S", left, right);
				intz slash_count = 0;
				for (intz i = 0; i < filter.size; ++i)
					slash_count += (filter.data[i] == '/');

				if (slash_count != view->slash_count)
					RefreshOpenFileViewEntries_(app, view);
			}
		} break;
		// case PanelState_CommandView:
		// case PanelState_AlreadyOpenFileView:
		{
			GenericListView* view;
			if (0) case PanelState_CommandView: view = &panel->command_view;
			if (0) case PanelState_AlreadyOpenFileView: view = &panel->already_open_file_view;

			TextBuffer* textbuf = TextBufferFromHandle(app, view->filter);
			SafeAssert(textbuf);
			bool is_going_back_to_prev_state = false;

			if (event->kind == OS_EventKind_WindowKeyPressed)
			{
				switch (event->window_key.key)
				{
					default: break;
					case OS_KeyboardKey_Escape: is_going_back_to_prev_state = true; break;
					case OS_KeyboardKey_Left:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdLeftSnakeWord(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdLeftPascalWord(&view->cursor, textbuf, 1);
						else
							TextCursorCmdLeft(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Right:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdRightSnakeWord(&view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdRightPascalWord(&view->cursor, textbuf, 1);
						else
							TextCursorCmdRight(&view->cursor, textbuf, 1);
					} break;
					case OS_KeyboardKey_Up:
					{
						view->selected_option = ClampMin(view->selected_option-1, 0);
					} break;
					case OS_KeyboardKey_Down:
					{
						intz available_count = CountOfAvailableOptionsInGenericListView_(app, view, panel->state);
						view->selected_option = ClampMax(view->selected_option+1, available_count - 1);
					} break;
					case OS_KeyboardKey_Backspace:
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdDeleteBackwardSnakeWord(app, &view->cursor, textbuf, 1);
						else if (event->window_key.alt)
							TextCursorCmdDeleteBackwardPascalWord(app, &view->cursor, textbuf, 1);
						else
							TextCursorCmdDeleteBackward(app, &view->cursor, textbuf, 1);
						view->selected_option = 0;
					} break;
					case OS_KeyboardKey_End: TextCursorCmdEndOfLine(&view->cursor, textbuf); break;
					case OS_KeyboardKey_Home: TextCursorCmdStartOfLine(&view->cursor, textbuf); break;
					case OS_KeyboardKey_Tab:
					case OS_KeyboardKey_Enter:
					{
						ArenaSavepoint scratch = ArenaSave(ScratchArena(0, NULL));
						String filter_str = {};
						{
							String left, right;
							TextBufferGetStrings(textbuf, &left, &right);
							filter_str = ArenaPrintf(scratch.arena, "%S%S", left, right);
						}

						if (panel->state == PanelState_CommandView)
						{
							CommandKind command = 0;
							intz running_index = 0;
							for (intz i = 1; i < CommandKind__Count; ++i)
							{
								if (FuzzyMatch(filter_str, g_commands_names[i]))
								{
									if (running_index == view->selected_option)
									{
										command = i;
										break;
									}
									++running_index;
								}
							}

							if (command)
							{
								Log(LOG_INFO, "pretend i'm executing this command: %S", g_commands_names[command]);
								is_going_back_to_prev_state = true;
							}
						}
						else if (panel->state == PanelState_AlreadyOpenFileView)
						{
							intz running_index = 0;
							for (intz i = app->openbuffers_count-1; i >= 0; --i)
							{
								TextBufferHandle handle = app->openbuffers[i];
								SafeAssert(handle);
								TextBuffer* file_textbuf = TextBufferFromHandle(app, handle);
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

								if (this_index == view->selected_option)
								{
									// put new opened file at the top of the list
									app->openbuffers[i] = app->openbuffers[app->openbuffers_count-1];
									app->openbuffers[app->openbuffers_count-1] = handle;

									TextView* text_view = &panel->text_view;
									if (text_view->textbuf)
										TextBufferDecRefCount(app, text_view->textbuf);
									TextBufferIncRefCount(app, handle);
									text_view->textbuf = handle;
									text_view->cursor = (TextCursor) {};
									text_view->line = 1;
									is_going_back_to_prev_state = true;
									break;
								}
							}
						}

						ArenaRestore(scratch);
					} break;
					case 'D':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdDeleteToMarker(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
					} break;
					case 'C':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdCopy(app, &view->cursor, textbuf);
					} break;
					case 'X':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdCut(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
						else if (event->window_key.alt)
						{
							TextBufferHandle filter = NULL;
							TextBufferFromString(app, Str(""), StrNull, TextBufferKind_SingleLine, &filter);
							SafeAssert(filter);
							panel->command_view = (CommandView) {
								.filter = filter,
								.cursor = {},
								.prev_panel_state = panel->state,
								.selected_option = 0,
							};
							panel->state = PanelState_CommandView;
						}
					} break;
					case 'V':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdPaste(app, &view->cursor, textbuf, 1);
							view->selected_option = 0;
						}
					} break;
					case ' ':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
							TextCursorCmdPlaceMarker(&view->cursor);
					} break;
					case 'Z':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdUndo(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
					} break;
					case 'Y':
					{
						if (event->window_key.ctrl && event->window_key.alt)
						{}
						else if (event->window_key.ctrl)
						{
							TextCursorCmdRedo(app, &view->cursor, textbuf);
							view->selected_option = 0;
						}
					} break;
					case 'R':
					{

					} break;
					case 'Q':
					{
						
					} break;
					case OS_KeyboardKey_PageUp:
					{

					} break;
					case OS_KeyboardKey_PageDown:
					{

					} break;
				}
			}
			else if (event->kind == OS_EventKind_WindowTyping)
			{
				uint32 codepoint = event->window_typing.codepoint;
				if (!(event->window_typing.ctrl && codepoint == ' '))
				{
					TextCursorCmdInsert(app, &view->cursor, textbuf, 1, codepoint);
					view->selected_option = 0;
				}
			}

			if (is_going_back_to_prev_state)
			{
				TextBufferDecRefCount(app, view->filter);
				panel->state = view->prev_panel_state;
				MemoryZero(view, sizeof(*view));
				break;
			}
		} break;
	}
}

BED_API void
PanelProcessCursorDrag(App* app, Panel* panel, OS_MouseState const* mouse)
{
	Trace();

	switch (panel->state)
	{
		default: break;
		case PanelState_TextView:
		{
			TextBuffer* textbuf = TextBufferFromHandle(app, panel->text_view.textbuf);
			SetCursorPosFromMouse_(app, &panel->text_view, textbuf, mouse->pos[0], mouse->pos[1]);
		} break;
		case PanelState_OpenFileView:
		{

		} break;
	}
}
