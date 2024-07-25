#ifndef COMMON_RPMALLOC_H
#define COMMON_RPMALLOC_H

#include "common.h"
#if !defined(RPMALLOC_FIRST_CLASS_HEAPS) || RPMALLOC_FIRST_CLASS_HEAPS == 0
#	error "RPMALLOC_FIRST_CLASS_HEAPS must be defined!"
#endif
#include "third_party/rpmalloc/rpmalloc.h"

static void*
RpmallocHeapAllocatorProc_(Allocator* allocator, AllocatorMode mode, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	Trace();
	void* result = NULL;
	AllocatorError error = AllocatorError_Ok;
	rpmalloc_heap_t* heap = (rpmalloc_heap_t*)allocator->instance;

	bool is_invalid_alignment = (!alignment || (alignment & alignment-1) != 0);
	if (alignment < sizeof(void*))
		alignment = sizeof(void*);

	switch (mode)
	{
		case AllocatorMode_Alloc:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			result = rpmalloc_heap_aligned_alloc(heap, alignment, size);
			if (!result)
				error = AllocatorError_OutOfMemory;
			else
				MemoryZero(result, size);
		} break;
		case AllocatorMode_Free:
		{
			if (!old_ptr)
				break; // free(NULL) has to be noop?
			rpmalloc_heap_free(heap, old_ptr);
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
				result = rpmalloc_heap_aligned_alloc(heap, alignment, size);
				if (!result)
					error = AllocatorError_OutOfMemory;
				else
					MemoryZero(result, size);
				break;
			}

			result = rpmalloc_heap_aligned_realloc(heap, old_ptr, alignment, size, 0);
			if (!result)
				error = AllocatorError_OutOfMemory;
			else if (size > old_size)
				MemoryZero((uint8*)result + old_size, size - old_size);
		} break;
		case AllocatorMode_FreeAll:
		{
			rpmalloc_heap_free_all(heap);
		} break;
		case AllocatorMode_AllocNonZeroed:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			result = rpmalloc_heap_aligned_alloc(heap, alignment, size);
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
				result = rpmalloc_heap_aligned_alloc(heap, alignment, size);
				if (!result)
					error = AllocatorError_OutOfMemory;
				else
					MemoryZero(result, size);
				break;
			}

			result = rpmalloc_heap_aligned_realloc(heap, old_ptr, alignment, size, 0);
			if (!result)
				error = AllocatorError_OutOfMemory;
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
				AllocatorMode_QueryFeatures;
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
AllocatorFromRpmallocHeap(rpmalloc_heap_t* heap)
{
	return (Allocator) {
		.proc = RpmallocHeapAllocatorProc_,
		.instance = heap,
	};
}

static void*
RpmallocAllocatorProc_(Allocator* allocator, AllocatorMode mode, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	Trace();
	void* result = NULL;
	AllocatorError error = AllocatorError_Ok;

	bool is_invalid_alignment = (!alignment || (alignment & alignment-1) != 0);
	if (alignment < sizeof(void*))
		alignment = sizeof(void*);

	switch (mode)
	{
		case AllocatorMode_Alloc:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			result = rpaligned_alloc(alignment, size);
			if (!result)
				error = AllocatorError_OutOfMemory;
			else
				MemoryZero(result, size);
		} break;
		case AllocatorMode_Free:
		{
			if (!old_ptr)
				break; // free(NULL) has to be noop?
			rpfree(old_ptr);
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
				result = rpaligned_alloc(alignment, size);
				if (!result)
					error = AllocatorError_OutOfMemory;
				else
					MemoryZero(result, size);
				break;
			}

			result = rpaligned_realloc(old_ptr, alignment, size, old_size, 0);
			if (!result)
				error = AllocatorError_OutOfMemory;
			else if (size > old_size)
				MemoryZero((uint8*)result + old_size, size - old_size);
		} break;
		case AllocatorMode_AllocNonZeroed:
		{
			if (is_invalid_alignment)
			{
				error = AllocatorError_InvalidArgument;
				break;
			}
			result = rpaligned_alloc(alignment, size);
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
				result = rpaligned_alloc(alignment, size);
				if (!result)
					error = AllocatorError_OutOfMemory;
				else
					MemoryZero(result, size);
				break;
			}

			result = rpaligned_realloc(old_ptr, alignment, size, old_size, 0);
			if (!result)
				error = AllocatorError_OutOfMemory;
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
				AllocatorMode_QueryFeatures;
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
AllocatorFromRpmalloc(void)
{
	return (Allocator) {
		.proc = RpmallocAllocatorProc_,
		.instance = NULL,
	};
}

#endif