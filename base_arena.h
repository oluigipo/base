#ifndef LJRE_BASE_ARENA_H
#define LJRE_BASE_ARENA_H

#include "base.h"
#include "base_intrinsics.h"
#include "base_string.h"

#define ArenaPushStruct(arena, Type) \
	((Type*)ArenaPushAligned(arena, SignedSizeof(Type), alignof(Type)))
#define ArenaPushStructData(arena, Type, ...) \
	((Type*)MemoryCopy(ArenaPushDirtyAligned(arena, SignedSizeof(Type), alignof(Type)), __VA_ARGS__, SignedSizeof(Type)))
#define ArenaPushArray(arena, Type, count) \
	((Type*)ArenaPushAligned(arena, SignedSizeof(Type)*(count), alignof(Type)))
#define ArenaPushArrayData(arena, Type, data, count) \
	((Type*)MemoryCopy(ArenaPushDirtyAligned(arena, SignedSizeof(Type)*(count), alignof(Type)), data, SignedSizeof(Type)*(count)))
#define ArenaPushData(arena, data) \
	MemoryCopy(ArenaPushDirtyAligned(arena, SignedSizeof(*(data)), 1), data, SignedSizeof(*(data)))
#define ArenaPushDataArray(arena, data, count) \
	MemoryCopy(ArenaPushDirtyAligned(arena, SignedSizeof(*(data))*(count), 1), data, SignedSizeof(*(data))*(count))
#define ArenaTempScope(arena_) \
	(ArenaSavepoint _temp__ = { arena_, (arena_)->offset }; _temp__.arena; _temp__.arena->offset = _temp__.offset, _temp__.arena = NULL)
#ifndef __cplusplus
#   define ArenaPushStructInit(arena, Type, ...) \
		((void*)ArenaPushMemoryAligned(arena, &(Type) __VA_ARGS__, SignedSizeof(Type), alignof(Type)))
#else //__cplusplus
#   define ArenaPushStructInit(arena, Type, ...) \
		((Type*)ArenaPushMemoryAligned(arena, &(Type const&) Type __VA_ARGS__, SignedSizeof(Type), alignof(Type)))
#endif //__cplusplus

static inline Arena ArenaFromMemory(void* memory, intz size);
static inline Arena* ArenaBootstrap(Arena arena);
static inline void* ArenaPush(Arena* arena, intz size);
static inline void* ArenaPushDirty(Arena* arena, intz size);
static inline void* ArenaPushAligned(Arena* arena, intz size, intz alignment);
static inline void* ArenaPushDirtyAligned(Arena* arena, intz size, intz alignment);
static inline void* ArenaPushMemory(Arena* arena, void const* buf, intz size);
static inline void* ArenaPushMemoryAligned(Arena* arena, void const* buf, intz size, intz alignment);
static inline String ArenaPushString(Arena* arena, String str);
static inline String ArenaPushStringAligned(Arena* arena, String str, intz alignment);
static inline char* ArenaPushCString(Arena* arena, String str);
static inline String ArenaVPrintf(Arena* arena, const char* fmt, va_list args);
static inline String ArenaPrintf(Arena* arena, const char* fmt, ...);
static inline void  ArenaPop(Arena* arena, void* ptr);
static inline void* ArenaEndAligned(Arena* arena, intz alignment);
static inline void  ArenaClear(Arena* arena);
static inline void* ArenaEnd(Arena* arena);
static inline ArenaSavepoint ArenaSave(Arena* arena);
static inline void           ArenaRestore(ArenaSavepoint savepoint);

static inline Allocator AllocatorFromArena(Arena* arena);

static inline Arena
ArenaFromMemory(void* memory, intz size)
{
	Assert(((uintptr)memory & (uintptr)~(CONFIG_ARENA_DEFAULT_ALIGNMENT-1)) == (uintptr)memory);
	
	Arena result = {
		.size = size,
		.offset = 0,
		.memory = (uint8*)memory,
	};
	
	return result;
}

static inline Arena*
ArenaBootstrap(Arena arena)
{
	Arena* ptr = (Arena*)ArenaPushAligned(&arena, sizeof(Arena), alignof(Arena));
	*ptr = arena;
	return ptr;
}

static inline void*
ArenaEndAligned(Arena* arena, intz alignment)
{
	Assert(alignment != 0 && IsPowerOf2(alignment));
	
	intptr target_offset = AlignUp((intptr)arena->memory + arena->offset, alignment-1) - (intptr)arena->memory;
	arena->offset = target_offset;
	return arena->memory + arena->offset;
}

static inline void*
ArenaPushDirtyAligned(Arena* arena, intz size, intz alignment)
{
	Trace();
	Assert(alignment != 0 && IsPowerOf2(alignment));
	SafeAssert((intz)size >= 0);
	
	intptr target_offset = AlignUp((intptr)arena->memory + arena->offset, alignment-1) - (intptr)arena->memory;
	intz needed = target_offset + size;
	
	if (Unlikely(needed > arena->size))
	{
		if (!arena->commit_memory_proc || !arena->commit_memory_proc(arena, needed))
			return NULL;
		Assert(needed <= arena->size);
	}
	
	void* result = arena->memory + target_offset;
	arena->offset = needed;
	
	return result;
}

static inline void
ArenaPop(Arena* arena, void* ptr)
{
	uint8* p = (uint8*)ptr;
	SafeAssert(p >= arena->memory && p <= arena->memory + arena->offset);
	
	intz new_offset = p - arena->memory;
	arena->offset = new_offset;
}

static inline String
ArenaVPrintf(Arena* arena, char const* fmt, va_list args)
{
	Trace();
	va_list args2;
	va_copy(args2, args);
	
	intz size = StringVPrintfSize(fmt, args2);
	uint8* data = (uint8*)ArenaPushDirtyAligned(arena, size, 1);
	String result = { 0 };
	
	if (data)
	{
		size = StringVPrintfBuffer((char*)data, size, fmt, args);
		result = StrMake(size, data);
	}
	
	va_end(args2);
	return result;
}

static inline String
ArenaPrintf(Arena* arena, char const* fmt, ...)
{
	Trace();
	va_list args;
	va_start(args, fmt);
	String result = ArenaVPrintf(arena, fmt, args);
	va_end(args);
	
	return result;
}

static inline ArenaSavepoint
ArenaSave(Arena* arena)
{
	ArenaSavepoint ret = {
		arena,
		arena->offset,
	};
	
	return ret;
}

static inline char*
ArenaPushCString(Arena* arena, String str)
{
	Trace();
	char* buffer = (char*)ArenaPushDirtyAligned(arena, str.size + 1, 1);
	if (buffer)
	{
		MemoryCopy(buffer, str.data, str.size);
		buffer[str.size] = 0;
	}
	return buffer;
}

static inline void
ArenaRestore(ArenaSavepoint savepoint)
{ savepoint.arena->offset = savepoint.offset; }

static inline void*
ArenaPushDirty(Arena* arena, intz size)
{ return ArenaPushDirtyAligned(arena, size, CONFIG_ARENA_DEFAULT_ALIGNMENT); }

static inline void*
ArenaPushAligned(Arena* arena, intz size, intz alignment)
{
	Trace();
	void* data = ArenaPushDirtyAligned(arena, size, alignment);
	if (data)
		MemoryZero(data, size);
	return data;
}

static inline void*
ArenaPush(Arena* arena, intz size)
{
	Trace();
	void* data = ArenaPushDirtyAligned(arena, size, CONFIG_ARENA_DEFAULT_ALIGNMENT);
	if (data)
		MemoryZero(data, size);
	return data;
}

static inline void*
ArenaPushMemory(Arena* arena, const void* buf, intz size)
{
	Trace();
	void* data = ArenaPushDirtyAligned(arena, size, 1);
	if (data)
		MemoryCopy(data, buf, size);
	return data;
}

static inline void*
ArenaPushMemoryAligned(Arena* arena, const void* buf, intz size, intz alignment)
{
	Trace();
	void* data = ArenaPushDirtyAligned(arena, size, alignment);
	if (data)
		MemoryCopy(data, buf, size);
	return data;
}

static inline String
ArenaPushString(Arena* arena, String str)
{
	Trace();
	void* data = ArenaPushDirtyAligned(arena, str.size, 1);
	String result = { 0 };
	if (data)
		result = StrMake(str.size, MemoryCopy(data, str.data, str.size));
	return result;
}

static inline String
ArenaPushStringAligned(Arena* arena, String str, intz alignment)
{
	Trace();
	void* data = ArenaPushDirtyAligned(arena, str.size, alignment);
	String result = { 0 };
	if (data)
		result = StrMake(str.size, MemoryCopy(data, str.data, str.size));
	return result;
}

static inline void
ArenaClear(Arena* arena)
{ arena->offset = 0; }

static inline void*
ArenaEnd(Arena* arena)
{ return arena->memory + arena->offset; }

static void*
ArenaAllocatorProc(void* instance, AllocatorMode mode, intz size, intz alignment, void* old_ptr, intz old_size, AllocatorError* out_err)
{
	Trace();
	void* result = NULL;
	AllocatorError error = AllocatorError_Ok;
	Arena* arena = (Arena*)instance;

	bool is_invalid_alignment = (!alignment || (alignment & alignment-1) != 0);

	switch (mode)
	{
		case AllocatorMode_Alloc:
		{
			if (size == 0)
				result = alignment ? ArenaEndAligned(arena, alignment) : ArenaEnd(arena);
			else
			{
				if (is_invalid_alignment)
				{
					error = AllocatorError_InvalidArgument;
					break;
				}
				result = ArenaPushAligned(arena, size, alignment);
			}
			if (!result)
				error = AllocatorError_OutOfMemory;
		} break;
		case AllocatorMode_Free:
		{
			if (!old_ptr)
				break; // free(NULL) has to be noop?
			SafeAssert((uint8*)old_ptr >= arena->memory && (uint8*)old_ptr <= arena->memory + arena->offset);
			intz old_offset = (uint8*)old_ptr - arena->memory;
			if (old_offset + old_size == arena->offset)
				ArenaPop(arena, old_ptr);
		} break;
		case AllocatorMode_Resize:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			if (!old_ptr)
			{
				result = ArenaPushAligned(arena, size, alignment);
				if (!result)
					error = AllocatorError_OutOfMemory;
				break;
			}
			SafeAssert((uint8*)old_ptr >= arena->memory && (uint8*)old_ptr <= arena->memory + arena->offset);

			intz old_offset = (uint8*)old_ptr - arena->memory;
			if (old_offset + old_size == arena->offset)
			{
				if (old_size < size)
					ArenaPushAligned(arena, size - old_size, 1);
				else if (old_size > size)
					ArenaPop(arena, arena->memory + old_offset + size);
				result = old_ptr;
			}
			else
			{
				// welp, this is really sad
				result = ArenaPushDirtyAligned(arena, size, alignment);
				if (!result)
				{
					error = AllocatorError_OutOfMemory;
					break;
				}
				intz amount_to_copy = Min(size, old_size);
				MemoryCopy(result, old_ptr, amount_to_copy);
				if (size > old_size)
					MemoryZero((uint8*)result + old_size, size - old_size);
			}
		} break;
		case AllocatorMode_FreeAll:
		{
			ArenaClear(arena);
		} break;
		case AllocatorMode_AllocNonZeroed:
		{
			if (size == 0)
				result = alignment ? ArenaEndAligned(arena, alignment) : ArenaEnd(arena);
			else
			{
				if (is_invalid_alignment)
				{
					error = AllocatorError_InvalidArgument;
					break;
				}
				result = ArenaPushDirtyAligned(arena, size, alignment);
			}
			if (!result)
				error = AllocatorError_OutOfMemory;
		} break;
		case AllocatorMode_ResizeNonZeroed:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			if (!old_ptr)
			{
				result = ArenaPushDirtyAligned(arena, size, alignment);
				if (!result)
					error = AllocatorError_OutOfMemory;
				break;
			}
			SafeAssert((uint8*)old_ptr >= arena->memory && (uint8*)old_ptr <= arena->memory + arena->offset);
			intz old_offset = (uint8*)old_ptr - arena->memory;
			if (old_offset + old_size == arena->offset)
			{
				if (old_size < size)
					ArenaPushDirtyAligned(arena, size - old_size, 1);
				else if (old_size > size)
					ArenaPop(arena, arena->memory + old_offset + size);
				result = old_ptr;
			}
			else
			{
				// also sad
				result = ArenaPushDirtyAligned(arena, size, alignment);
				if (!result)
				{
					error = AllocatorError_OutOfMemory;
					break;
				}
				intz amount_to_copy = Min(size, old_size);
				MemoryCopy(result, old_ptr, amount_to_copy);
			}
		} break;
		case AllocatorMode_QueryFeatures:
		{
			SafeAssert(old_ptr && ((uintptr)old_ptr & sizeof(uint32)-1) == 0);
			uint32* features_ptr = (uint32*)old_ptr;
			*features_ptr =
				AllocatorMode_Alloc |
				AllocatorMode_AllocNonZeroed |
				AllocatorMode_Resize |
				AllocatorMode_ResizeNonZeroed |
				AllocatorMode_Free |
				AllocatorMode_FreeAll |
				AllocatorMode_Pop |
				AllocatorMode_QueryFeatures;
		} break;
		case AllocatorMode_Pop:
		{
			SafeAssert((uint8*)old_ptr >= arena->memory && (uint8*)old_ptr <= arena->memory + arena->offset);
			ArenaPop(arena, old_ptr);
		} break;
		default:
		{
			error = AllocatorError_ModeNotImplemented;
		} break;
	}

	if (out_err)
		*out_err = error;
	return result;
}

static inline Allocator
AllocatorFromArena(Arena* arena)
{
	return (Allocator) {
		.proc = ArenaAllocatorProc,
		.instance = arena,
	};
}

#ifdef __cplusplus
struct ScratchScope
{
	Arena* arena;
	Allocator allocator;
	void* end;

	inline explicit ScratchScope(Arena* arena)
		: arena(arena), allocator(), end(ArenaEnd(arena))
	{}
	inline explicit ScratchScope(Allocator allocator)
		: arena(), allocator(allocator), end()
	{
		AllocatorError err;
		end = AllocatorAllocNonZeroed(&allocator, 0, 0, &err);
		if (err)
			end = NULL;
	}
	inline ScratchScope(ScratchScope const& other) = delete;
	inline ScratchScope(ScratchScope&& other) = delete;
	inline ScratchScope& operator=(ScratchScope const& other) = delete;
	inline ScratchScope& operator=(ScratchScope&& other) = delete;
	inline ~ScratchScope()
	{
		if (arena)
			ArenaPop(arena, end);
		if (allocator.proc)
			AllocatorPop(&allocator, end, NULL);
	}
};
#endif

#endif //LJRE_BASE_ARENA_H
