#ifndef COMMON_HPP
#define COMMON_HPP

#include "common.h"

template <typename T>
static inline T*
AllocatorNew(Allocator* allocator, AllocatorError* out_err)
{
	return (T*)allocator->proc(allocator->instance, AllocatorMode_Alloc, sizeof(T), alignof(T), NULL, 0, out_err);
}

template <typename T>
static inline T*
AllocatorNewArray(Allocator* allocator, intsize count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTSIZE_MAX / sizeof(T));
	return (T*)allocator->proc(allocator->instance, AllocatorMode_Alloc, count*sizeof(T), alignof(T), NULL, 0, out_err);
}

template <typename T>
static inline void
AllocatorDelete(Allocator* allocator, T* ptr, AllocatorError* out_err)
{
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, ptr, sizeof(T), out_err);
}

template <typename T>
static inline void
AllocatorDeleteArray(Allocator* allocator, T* ptr, intsize count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTSIZE_MAX / sizeof(T));
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, ptr, count*sizeof(T), out_err);
}

template <typename T>
static inline T*
AllocatorResizeArray(Allocator* allocator, intsize count, T* ptr, intsize old_count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTSIZE_MAX / sizeof(T));
	SafeAssert(old_count >= 0 && old_count <= INTSIZE_MAX / sizeof(T));
	return (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * sizeof(T), alignof(T), ptr, old_count * sizeof(T), out_err);
}

template <typename T>
static inline bool
AllocatorResizeArrayOk(Allocator* allocator, intsize count, T** ptr, intsize old_count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTSIZE_MAX / sizeof(T));
	T* result = (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * sizeof(T), alignof(T), ptr, old_count * sizeof(T), out_err);
	if (result || !count)
	{
		*ptr = result;
		return true;
	}
	return false;
}

template <typename T>
struct Slice
{
	T* data;
	intz count;

	inline T& operator[](intz index) const { return data[index]; }
	inline T* begin() const { return data; }
	inline T* end() const { return data + count; }
	inline operator bool() const { return count > 0; }
	inline bool operator==(Slice<T> other) const { return data == other.data && count == other.count; }
	inline operator Slice<T const>() const { return { data, count }; }
	inline explicit operator Buffer() const
	{
		SafeAssert(count <= UINTSIZE_MAX / sizeof(T));
		return { (uint8 const*)data, count * sizeof(T) };
	}
};

template <typename T, size_t N>
static inline Slice<T>
SliceFromArray(T (&array)[N])
{
	return { array, N };
}

template <typename T>
static inline Slice<T>
SliceFromRange(T* begin, T* end)
{
	return { begin, end - begin };
}

template <typename T>
static inline Slice<T>
AllocatorNewSlice(Allocator* allocator, intsize count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTSIZE_MAX / sizeof(T));
	T* ptr = (T*)allocator->proc(allocator->instance, AllocatorMode_Alloc, count*sizeof(T), alignof(T), NULL, 0, out_err);
	if (ptr)
		return { ptr, count };
	return {};
}

template <typename T>
static inline void
AllocatorDeleteSlice(Allocator* allocator, Slice<T> slice, AllocatorError* out_err)
{
	SafeAssert(slice.count >= 0 && slice.count <= INTSIZE_MAX / sizeof(T));
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, slice.data, slice.count*sizeof(T), out_err);
}

template <typename T>
static inline Slice<T>
AllocatorResizeSlice(Allocator* allocator, intsize count, Slice<T> slice, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTSIZE_MAX / sizeof(T));
	SafeAssert(slice.count >= 0 && slice.count <= INTSIZE_MAX / sizeof(T));
	T* ptr = (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * sizeof(T), alignof(T), slice.data, slice.count * sizeof(T), out_err);
	if (ptr)
		return { ptr, count };
	return {};
}

template <typename T>
static inline bool
AllocatorResizeSliceOk(Allocator* allocator, intsize count, Slice<T>* slice_ptr, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTSIZE_MAX / sizeof(T));
	SafeAssert(slice_ptr->count >= 0 && slice_ptr->count <= INTSIZE_MAX / sizeof(T));
	T* result = (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * sizeof(T), alignof(T), slice_ptr->data, slice_ptr->count * sizeof(T), out_err);
	if (result || !count)
	{
		*slice_ptr = { result, count };
		return true;
	}
	return false;
}

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

#define _defer__cat_(a, b) a ## b
#define _defer__cat(a, b) _defer__cat_(a, b)
#define defer DeferredLambda _defer__cat(_deferred_lambda_, __LINE__) = [&]

template <typename T>
struct DeferredLambda
{
	T lambda;
	inline DeferredLambda(T lambda)
		: lambda((T&&)lambda)
	{}
	inline ~DeferredLambda()
	{
		lambda();
	}
};

#endif