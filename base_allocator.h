#ifndef LJRE_BASE_ALLOCATOR_H
#define LJRE_BASE_ALLOCATOR_H

#include "base.h"
#include "base_intrinsics.h"
#include "base_string.h"

static inline void*  AllocatorAlloc            (Allocator* allocator, intz size, intz alignment, AllocatorError* out_err);
static inline String AllocatorCloneString      (Allocator* allocator, String str, AllocatorError* out_err);
static inline String AllocatorCloneBuffer      (Allocator* allocator, intz alignment, Buffer buf, AllocatorError* out_err);
static inline void*  AllocatorResize           (Allocator* allocator, intz size, intz alignment, void* old_ptr, intz old_size, AllocatorError* out_err);
static inline void*  AllocatorAllocNonZeroed   (Allocator* allocator, intz size, intz alignment, AllocatorError* out_err);
static inline void*  AllocatorResizeNonZeroed  (Allocator* allocator, intz size, intz alignment, void* old_ptr, intz old_size, AllocatorError* out_err);
static inline void   AllocatorFree             (Allocator* allocator, void* old_ptr, intz old_size, AllocatorError* out_err);
static inline void   AllocatorFreeString       (Allocator* allocator, String str, AllocatorError* out_err);
static inline void   AllocatorFreeBuffer       (Allocator* allocator, Buffer buf, AllocatorError* out_err);
static inline void   AllocatorFreeAll          (Allocator* allocator, AllocatorError* out_err);
static inline void   AllocatorPop              (Allocator* allocator, void* old_ptr, AllocatorError* out_err);
static inline bool   AllocatorResizeOk         (Allocator* allocator, intz size, intz alignment, void* inout_ptr, intz old_size, AllocatorError* out_err);
static inline bool   AllocatorResizeNonZeroedOk(Allocator* allocator, intz size, intz alignment, void* inout_ptr, intz old_size, AllocatorError* out_err);
static inline void*  AllocatorAllocArray       (Allocator* allocator, intz count, intz size, intz alignment, AllocatorError* out_err);
static inline void*  AllocatorResizeArray      (Allocator* allocator, intz count, intz size, intz alignment, void* old_ptr, intz old_count, AllocatorError* out_err);
static inline bool   AllocatorResizeArrayOk    (Allocator* allocator, intz count, intz size, intz alignment, void* inout_ptr, intz old_count, AllocatorError* out_err);
static inline void   AllocatorFreeArray        (Allocator* allocator, intz size, void* old_ptr, intz old_count, AllocatorError* out_err);

static inline Allocator NullAllocator(void);
static inline bool      IsNullAllocator(Allocator allocator);

static inline void*
AllocatorAlloc(Allocator* allocator, intz size, intz alignment, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_Alloc, size, alignment, NULL, 0, out_err);
}

static inline String
AllocatorCloneString(Allocator* allocator, String str, AllocatorError* out_err)
{
	void* ptr = allocator->proc(allocator->instance, AllocatorMode_Alloc, str.size, 1, NULL, 0, out_err);
	if (ptr)
	{
		MemoryCopy(ptr, str.data, str.size);
		return StringMake(str.size, ptr);
	}
	return STRNULL;
}

static inline String
AllocatorCloneBuffer(Allocator* allocator, intz alignment, Buffer buf, AllocatorError* out_err)
{
	void* ptr = allocator->proc(allocator->instance, AllocatorMode_Alloc, buf.size, alignment, NULL, 0, out_err);
	if (ptr)
	{
		MemoryCopy(ptr, buf.data, buf.size);
		return BufferMake(buf.size, ptr);
	}
	return STRNULL;
}

static inline void*
AllocatorResize(Allocator* allocator, intz size, intz alignment, void* old_ptr, intz old_size, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_Resize, size, alignment, old_ptr, old_size, out_err);
}

static inline void*
AllocatorAllocNonZeroed(Allocator* allocator, intz size, intz alignment, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_AllocNonZeroed, size, alignment, NULL, 0, out_err);
}

static inline void*
AllocatorResizeNonZeroed(Allocator* allocator, intz size, intz alignment, void* old_ptr, intz old_size, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_ResizeNonZeroed, size, alignment, old_ptr, old_size, out_err);
}

static inline void
AllocatorFree(Allocator* allocator, void* old_ptr, intz old_size, AllocatorError* out_err)
{
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, old_ptr, old_size, out_err);
}

static inline void
AllocatorFreeString(Allocator* allocator, String str, AllocatorError* out_err)
{
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, (void*)str.data, str.size, out_err);
}

static inline void
AllocatorFreeBuffer(Allocator* allocator, Buffer buf, AllocatorError* out_err)
{
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, (void*)buf.data, buf.size, out_err);
}

static inline void
AllocatorFreeAll(Allocator* allocator, AllocatorError* out_err)
{
	allocator->proc(allocator->instance, AllocatorMode_FreeAll, 0, 0, NULL, 0, out_err);
}

static inline void
AllocatorPop(Allocator* allocator, void* old_ptr, AllocatorError* out_err)
{
	allocator->proc(allocator->instance, AllocatorMode_Pop, 0, 0, old_ptr, 0, out_err);
}

static inline bool
AllocatorResizeOk(Allocator* allocator, intz size, intz alignment, void* inout_ptr, intz old_size, AllocatorError* out_err)
{
	void* new_ptr = allocator->proc(allocator->instance, AllocatorMode_Resize, size, alignment, *(void**)inout_ptr, old_size, out_err);
	if (new_ptr || size == 0)
	{
		*(void**)inout_ptr = new_ptr;
		return true;
	}
	return false;
}

static inline bool
AllocatorResizeNonZeroedOk(Allocator* allocator, intz size, intz alignment, void* inout_ptr, intz old_size, AllocatorError* out_err)
{
	void* new_ptr = allocator->proc(allocator->instance, AllocatorMode_ResizeNonZeroed, size, alignment, *(void**)inout_ptr, old_size, out_err);
	if (new_ptr || size == 0)
	{
		*(void**)inout_ptr = new_ptr;
		return true;
	}
	return false;
}

static inline void*
AllocatorAllocArray(Allocator* allocator, intz count, intz size, intz alignment, AllocatorError* out_err)
{
	SafeAssert(!size || count >= 0 && count <= INTZ_MAX / size);
	return allocator->proc(allocator->instance, AllocatorMode_Alloc, count*size, alignment, NULL, 0, out_err);
}

static inline void*
AllocatorResizeArray(Allocator* allocator, intz count, intz size, intz alignment, void* old_ptr, intz old_count, AllocatorError* out_err)
{
	SafeAssert(!size || (count >= 0 && count <= INTZ_MAX / size && old_count >= 0 && old_count <= INTZ_MAX / size));
	return allocator->proc(allocator->instance, AllocatorMode_Resize, count*size, alignment, old_ptr, old_count*size, out_err);
}

static inline bool
AllocatorResizeArrayOk(Allocator* allocator, intz count, intz size, intz alignment, void* inout_ptr, intz old_count, AllocatorError* out_err)
{
	SafeAssert(!size || (count >= 0 && count <= INTZ_MAX / size && old_count >= 0 && old_count <= INTZ_MAX / size));
	void* new_ptr = allocator->proc(allocator->instance, AllocatorMode_Resize, count*size, alignment, *(void**)inout_ptr, old_count*size, out_err);
	if (new_ptr || !size)
	{
		*(void**)inout_ptr = new_ptr;
		return true;
	}
	return false;
}

static inline void
AllocatorFreeArray(Allocator* allocator, intz size, void* old_ptr, intz old_count, AllocatorError* out_err)
{
	SafeAssert(!size || old_count >= 0 && old_count <= INTZ_MAX / size);
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, old_ptr, old_count*size, out_err);
}

static inline Allocator
NullAllocator(void)
{
	return (Allocator) {};
}

static inline bool
IsNullAllocator(Allocator allocator)
{
	return !allocator.proc;
}

#ifdef __cplusplus
template <typename T>
static inline T*
AllocatorNew(Allocator* allocator, AllocatorError* out_err)
{
	return (T*)allocator->proc(allocator->instance, AllocatorMode_Alloc, SignedSizeof(T), alignof(T), NULL, 0, out_err);
}

template <typename T>
static inline T*
AllocatorNewArray(Allocator* allocator, intz count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTZ_MAX / sizeof(T));
	return (T*)allocator->proc(allocator->instance, AllocatorMode_Alloc, count*SignedSizeof(T), alignof(T), NULL, 0, out_err);
}

template <typename T>
static inline void
AllocatorDelete(Allocator* allocator, T* ptr, AllocatorError* out_err)
{
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, ptr, SignedSizeof(T), out_err);
}

template <typename T>
static inline void
AllocatorDeleteArray(Allocator* allocator, T* ptr, intz count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTZ_MAX / sizeof(T));
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, ptr, count*SignedSizeof(T), out_err);
}

template <typename T>
static inline T*
AllocatorResizeArray(Allocator* allocator, intz count, T* ptr, intz old_count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTZ_MAX / sizeof(T));
	SafeAssert(old_count >= 0 && old_count <= INTZ_MAX / sizeof(T));
	return (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * SignedSizeof(T), alignof(T), ptr, old_count * SignedSizeof(T), out_err);
}

template <typename T>
static inline bool
AllocatorResizeArrayOk(Allocator* allocator, intz count, T** ptr, intz old_count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTZ_MAX / sizeof(T));
	T* result = (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * SignedSizeof(T), alignof(T), ptr, old_count * SignedSizeof(T), out_err);
	if (result || !count)
	{
		*ptr = result;
		return true;
	}
	return false;
}

template <typename T>
static inline Slice<T>
AllocatorNewSlice(Allocator* allocator, intz count, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTZ_MAX / sizeof(T));
	T* ptr = (T*)allocator->proc(allocator->instance, AllocatorMode_Alloc, count*SignedSizeof(T), alignof(T), NULL, 0, out_err);
	if (ptr)
		return { ptr, count };
	return {};
}

template <typename T>
static inline void
AllocatorDeleteSlice(Allocator* allocator, Slice<T> slice, AllocatorError* out_err)
{
	SafeAssert(slice.count >= 0 && slice.count <= INTZ_MAX / sizeof(T));
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, slice.data, slice.count*SignedSizeof(T), out_err);
}

template <typename T>
static inline Slice<T>
AllocatorResizeSlice(Allocator* allocator, intz count, Slice<T> slice, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTZ_MAX / sizeof(T));
	SafeAssert(slice.count >= 0 && slice.count <= INTZ_MAX / sizeof(T));
	T* ptr = (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * SignedSizeof(T), alignof(T), slice.data, slice.count * SignedSizeof(T), out_err);
	if (ptr)
		return { ptr, count };
	return {};
}

template <typename T>
static inline bool
AllocatorResizeSliceOk(Allocator* allocator, intz count, Slice<T>* slice_ptr, AllocatorError* out_err)
{
	SafeAssert(count >= 0 && count <= INTZ_MAX / sizeof(T));
	SafeAssert(slice_ptr->count >= 0 && slice_ptr->count <= INTSIZE_MAX / sizeof(T));
	T* result = (T*)allocator->proc(allocator->instance, AllocatorMode_Resize, count * SignedSizeof(T), alignof(T), slice_ptr->data, slice_ptr->count * SignedSizeof(T), out_err);
	if (result || !count)
	{
		*slice_ptr = { result, count };
		return true;
	}
	return false;
}
#endif

#endif //LJRE_BASE_ALLOCATOR_H
