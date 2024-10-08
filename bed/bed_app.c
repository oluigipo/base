#include "bed.h"

struct DecodedHandle_
{
	uint32 generation;
	intz index;
}
typedef DecodedHandle_;

static DecodedHandle_
DecodeTextBufferHandle_(TextBufferHandle handle)
{
	uint64 as_u64 = (uint64)handle;
	return (DecodedHandle_) {
		.generation = as_u64 >> 48,
		.index = (as_u64 << 16 >> 16) - 1,
	};
}

static TextBufferHandle
EncodeTextBufferHandle_(uint32 generation, intz index)
{
	uint64 as_u64 = 0;
	as_u64 |= (uint64)generation << 48;
	as_u64 |= (uint64)(index + 1) << 16 >> 16;
	return (TextBufferHandle)as_u64;
}

static TextBuffer*
AllocTextBuffer_(App* app, TextBufferHandle* out_handle)
{
	Trace();
	intz index;

	if (app->textbuf_pool_first_free)
	{
		index = app->textbuf_pool_first_free - 1;
		app->textbuf_pool_first_free = app->textbuf_pool[index].next_free;
	}
	else
	{
		if (app->textbuf_pool_count+1 >= app->textbuf_pool_cap)
		{
			intz new_cap = app->textbuf_pool_cap + (app->textbuf_pool_cap >> 1) + 1;
			AllocatorError err;
			if (!AllocatorResizeArrayOk(&app->heap, new_cap, sizeof(TextBuffer), alignof(TextBuffer), &app->textbuf_pool, app->textbuf_pool_cap, &err))
			{
				Log(LOG_ERROR, "could not resize textbuf pool: %u", (uint32)err);
				*out_handle = 0;
				return NULL;
			}
			app->textbuf_pool_cap = new_cap;
		}
		index = app->textbuf_pool_count++;
	}

	TextBuffer* textbuf = &app->textbuf_pool[index];
	*out_handle = (TextBufferHandle)((uint64)index+1 | (uint64)textbuf->generation << 48);
	return &app->textbuf_pool[index];
}

static void
FreeTextBuffer_(App* app, TextBuffer* textbuf, intz index)
{
	Trace();
	if (textbuf->utf8_text)
		AllocatorFree(&app->heap, textbuf->utf8_text, textbuf->size, NULL);
	if (textbuf->cf_kinds)
		AllocatorFreeArray(&app->heap, sizeof(CF_TokenKind), textbuf->cf_kinds, textbuf->cf_cap, NULL);
	if (textbuf->cf_ranges)
		AllocatorFreeArray(&app->heap, sizeof(CF_SourceRange), textbuf->cf_ranges, textbuf->cf_cap, NULL);
	if (textbuf->edits)
		AllocatorFreeArray(&app->heap, sizeof(TextBufferEdit), textbuf->edits, textbuf->edits_cap, NULL);
	if (textbuf->edit_text_buffer)
		AllocatorFree(&app->heap, textbuf->edit_text_buffer, textbuf->edit_text_buffer_cap, NULL);
	if (textbuf->file_absolute_path.data)
		AllocatorFreeString(&app->heap, textbuf->file_absolute_path, NULL);
	if (textbuf->name.data)
		AllocatorFreeString(&app->heap, textbuf->name, NULL);
	uint32 generation = (uint16)(textbuf->generation + 1);
	MemoryZero(textbuf, sizeof(*textbuf));
	textbuf->next_free = (uint32)app->textbuf_pool_first_free;
	textbuf->generation = generation;
	app->textbuf_pool_first_free = index+1;
}

BED_API TextBuffer*
AppCreateFileTextBuffer(App* app, String path, TextBufferHandle* out_handle)
{
	Trace();
	*out_handle = NULL;

	static String const cfile_exts[] = {
		StrInit(".c"),
		StrInit(".C"),
		StrInit(".cpp"),
		StrInit(".cxx"),
		StrInit(".c++"),
		StrInit(".cc"),
		StrInit(".h"),
		StrInit(".hpp"),
		StrInit(".hxx"),
		StrInit(".h++"),
		StrInit(".H"),
	};

	TextBufferKind kind = TextBufferKind_TextFile;
	for (intz i = 0; i < ArrayLength(cfile_exts); ++i)
	{
		if (StringEndsWith(path, cfile_exts[i]))
		{
			kind = TextBufferKind_CFile;
			break;
		}
	}

	OS_Error err = {};
	TextBufferHandle handle = NULL;

	String absolute_path = OS_ResolveToAbsolutePath(path, app->heap, &err);
	if (!err.ok)
	{
		Log(LOG_ERROR, "could resolve path to full string when opening path: %S", path);
		return NULL;
	}

	for (intz i = 0; i < app->textbuf_pool_count; ++i)
	{
		TextBuffer* i_textbuf = &app->textbuf_pool[i];
		if (i_textbuf->ref_count && StringEquals(absolute_path, i_textbuf->file_absolute_path))
		{
			// NOTE(ljre): Text buffer is already open, so just return the existing one.
			Log(LOG_INFO, "reusing file text buffer: %S", absolute_path);
			AllocatorFreeString(&app->heap, absolute_path, NULL);
			handle = EncodeTextBufferHandle_(i_textbuf->generation, i);
			*out_handle = handle;
			return AppIncTextBufferRef(app, handle);
		}
	}

	OS_File file = OS_OpenFile(absolute_path, OS_OpenFileFlags_Read | OS_OpenFileFlags_FailIfDoesntExist, &err);
	if (!err.ok)
	{
		Log(LOG_ERROR, "could not open file from path!\n\tPath: %S\nError code: %u\nError string: %S", absolute_path, err.code, err.what);
		AllocatorFreeString(&app->heap, absolute_path, NULL);
		return NULL;
	}

	OS_FileInfo fileinfo = OS_GetFileInfo(file, &err);
	if (!err.ok)
	{
		Log(LOG_ERROR, "failed to get file info when opening path: %S", absolute_path);
		AllocatorFreeString(&app->heap, absolute_path, NULL);
		return NULL;
	}

	if (fileinfo.size > INTZ_MAX/2)
	{
		Log(LOG_ERROR, "file is too big to fit on RAM (size is 0x%X): %S", fileinfo.size, absolute_path);
		OS_CloseFile(file);
		AllocatorFreeString(&app->heap, absolute_path, NULL);
		return NULL;
	}

	TextBuffer* textbuf = AllocTextBuffer_(app, &handle);
	if (!textbuf)
	{
		Log(LOG_ERROR, "could not make text buffer when opening path: %S", absolute_path);
		OS_CloseFile(file);
		AllocatorFreeString(&app->heap, absolute_path, NULL);
		return NULL;
	}

	AllocatorError alloc_err = 0;
	intz file_size = (intz)fileinfo.size;
	intz total_size = AlignUp(file_size, 31) + 4096;
	uint8* utf8_text = AllocatorAlloc(&app->heap, total_size, 1, &alloc_err);
	if (alloc_err)
	{
		Log(LOG_ERROR, "could not allocate buffer for text buffer when opening path: %S", absolute_path);
		FreeTextBuffer_(app, textbuf, DecodeTextBufferHandle_(handle).index);
		OS_CloseFile(file);
		AllocatorFreeString(&app->heap, absolute_path, NULL);
		return NULL;
	}
	
	intz amount_read = OS_ReadFile(file, file_size+1, utf8_text, &err);
	if (!err.ok)
	{
		Log(LOG_ERROR, "could read file for text buffer when opening path: %S", absolute_path);
		AllocatorFree(&app->heap, utf8_text, total_size, NULL);
		FreeTextBuffer_(app, textbuf, DecodeTextBufferHandle_(handle).index);
		OS_CloseFile(file);
		AllocatorFreeString(&app->heap, absolute_path, NULL);
		return NULL;
	}

	OS_CloseFile(file);
	*textbuf = (TextBuffer) {
		.generation = textbuf->generation,
		.kind = kind,
		.ref_count = 1,
		.utf8_text = utf8_text,
		.size = total_size,
		.gap_start = file_size,
		.gap_end = total_size,
		.file_absolute_path = absolute_path,
	};

	if (app->openbuffers_count+1 > app->openbuffers_cap)
	{
		intz desired_cap = app->openbuffers_cap + (app->openbuffers_cap>>1) + 1;
		desired_cap = Max(desired_cap, app->openbuffers_count+1);
		AllocatorError err = 0;
		if (!AllocatorResizeArrayOk(&app->heap, desired_cap, sizeof(TextBufferHandle), alignof(TextBufferHandle), &app->openbuffers, app->openbuffers_cap, &err))
		{
			Log(LOG_ERROR, "failed to resize open files array when opening file \"%S\": %u", path, (uint32)err);
			AppReleaseTextBuffer(app, handle);
			return NULL;
		}
		app->openbuffers_cap = desired_cap;
	}

	AppIncTextBufferRef(app, handle);
	app->openbuffers[app->openbuffers_count++] = handle;

	*out_handle = handle;
	return textbuf;
}

BED_API TextBuffer*
AppCreateNamedTextBuffer(App* app, String initial_contents, String name, TextBufferKind kind, TextBufferHandle* out_handle)
{
	Trace();
	TextBufferHandle handle = NULL;
	*out_handle = NULL;

	if (name.size)
	{
		for (intz i = 0; i < app->textbuf_pool_count; ++i)
		{
			TextBuffer* i_textbuf = &app->textbuf_pool[i];
			if (i_textbuf->ref_count && StringEquals(name, i_textbuf->name))
			{
				// NOTE(ljre): Text buffer is already open, so just return the existing one.
				Log(LOG_INFO, "reusing named text buffer: %S", name);
				handle = EncodeTextBufferHandle_(i_textbuf->generation, i);
				*out_handle = handle;
				return AppIncTextBufferRef(app, handle);
			}
		}
	}

	TextBuffer* textbuf = AllocTextBuffer_(app, &handle);
	if (!textbuf)
	{
		Log(LOG_ERROR, "could not make text buffer from string: %S", name);
		return NULL;
	}

	AllocatorError alloc_err = 0;
	intz total_size = AlignUp(initial_contents.size, 31);
	total_size = ClampMin(total_size, 4096);
	uint8* utf8_text = AllocatorAlloc(&app->heap, total_size, 1, &alloc_err);
	if (alloc_err)
	{
		Log(LOG_ERROR, "could not allocate buffer for text buffer from string: %S", name);
		FreeTextBuffer_(app, textbuf, textbuf - app->textbuf_pool);
		return NULL;
	}
	MemoryCopy(utf8_text, initial_contents.data, initial_contents.size);

	if (name.size)
		name = AllocatorCloneString(&app->heap, name, &alloc_err);
	if (alloc_err)
	{
		Log(LOG_ERROR, "could not allocate buffer for text buffer name from string: %S", name);
		AllocatorFree(&app->heap, utf8_text, total_size, NULL);
		FreeTextBuffer_(app, textbuf, textbuf - app->textbuf_pool);
		return NULL;
	}

	*textbuf = (TextBuffer) {
		.generation = textbuf->generation,
		.kind = kind,
		.ref_count = 1,
		.utf8_text = utf8_text,
		.size = total_size,
		.gap_start = initial_contents.size,
		.gap_end = total_size,
		.name = name,
	};

	if (app->openbuffers_count+1 > app->openbuffers_cap)
	{
		intz desired_cap = app->openbuffers_cap + (app->openbuffers_cap>>1) + 1;
		desired_cap = Max(desired_cap, app->openbuffers_count+1);
		AllocatorError err = 0;
		if (!AllocatorResizeArrayOk(&app->heap, desired_cap, sizeof(TextBufferHandle), alignof(TextBufferHandle), &app->openbuffers, app->openbuffers_cap, &err))
		{
			Log(LOG_ERROR, "failed to resize open files array when creating named buffer \"%S\": %u", name, (uint32)err);
			AppDecTextBufferRef(app, handle);
			return NULL;
		}
		app->openbuffers_cap = desired_cap;
	}

	AppIncTextBufferRef(app, handle);
	app->openbuffers[app->openbuffers_count++] = handle;

	*out_handle = handle;
	return textbuf;
}

BED_API intz
AppReleaseTextBuffer(App* app, TextBufferHandle handle)
{
	Trace();
	TextBuffer* textbuf = AppGetTextBuffer(app, handle);
	if (!textbuf)
	{
		Log(LOG_ERROR, "cannot release textbuf because handle is invalid: 0x%X", (uint64)handle);
		return -1;
	}
	intz ref_count = textbuf->ref_count - 1;
	textbuf->ref_count = 1;
	AppDecTextBufferRef(app, handle);

	for (intz i = 0; i < app->openbuffers_count; ++i)
	{
		if (app->openbuffers[i] == handle)
		{
			app->openbuffers[i] = app->openbuffers[--app->openbuffers_count];
			return ref_count;
		}
	}

	Log(LOG_ERROR, "failed to close file because handle was not found in open files list: %S", textbuf->file_absolute_path);
	return ref_count;
}

BED_API TextBuffer*
AppGetTextBuffer(App* app, TextBufferHandle handle)
{
	DecodedHandle_ decoded = DecodeTextBufferHandle_(handle);
	if (decoded.index < 0 || decoded.index >= app->textbuf_pool_count)
	{
		Log(LOG_ERROR, "index into text buffer pool is invalid: %Z", decoded.index);
		return NULL;
	}
	TextBuffer* textbuf = &app->textbuf_pool[decoded.index];
	if (textbuf->generation != decoded.generation)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z has different generation than expected: %u != %u", decoded.index, decoded.generation, textbuf->generation);
		return NULL;
	}
	if (textbuf->next_free || !textbuf->kind)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z is not active", decoded.index);
		return NULL;
	}
	return textbuf;
}

BED_API TextBuffer*
AppIncTextBufferRef(App* app, TextBufferHandle handle)
{
	Trace();
	DecodedHandle_ decoded = DecodeTextBufferHandle_(handle);
	if (decoded.index < 0 || decoded.index >= app->textbuf_pool_count)
	{
		Log(LOG_ERROR, "trying to acquire invalid textbuffer index: %Z", decoded.index);
		return NULL;
	}
	TextBuffer* textbuf = &app->textbuf_pool[decoded.index];
	if (textbuf->generation != decoded.generation)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z has different generation than expected: %u != %u", decoded.index, decoded.generation, textbuf->generation);
		return NULL;
	}
	if (textbuf->next_free || !textbuf->kind)
	{
		Log(LOG_ERROR, "trying to acquire textbuffer that is inactive", decoded.index);
		return NULL;
	}

	++textbuf->ref_count;
	return textbuf;
}

BED_API intz
AppDecTextBufferRef(App* app, TextBufferHandle handle)
{
	Trace();
	DecodedHandle_ decoded = DecodeTextBufferHandle_(handle);
	if (decoded.index < 0 || decoded.index >= app->textbuf_pool_count)
	{
		Log(LOG_ERROR, "trying to release invalid textbuffer index: %Z", decoded.index);
		return -1;
	}
	TextBuffer* textbuf = &app->textbuf_pool[decoded.index];
	if (textbuf->generation != decoded.generation)
	{
		Log(LOG_ERROR, "text buffer reffered by index %Z has different generation than expected: %u != %u", decoded.index, decoded.generation, textbuf->generation);
		return -1;
	}
	if (textbuf->next_free || !textbuf->kind)
	{
		Log(LOG_ERROR, "trying to release textbuffer that is inactive", decoded.index);
		return -1;
	}
	
	intz result = --textbuf->ref_count;
	if (result == 0)
		FreeTextBuffer_(app, textbuf, decoded.index);
	return result;
}
