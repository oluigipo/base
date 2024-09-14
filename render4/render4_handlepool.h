#ifndef RENDER4_HANDLEPOOL_H
#define RENDER4_HANDLEPOOL_H

#include "common.h"
#include "common_atomic.h"

struct HandlePoolEntry_
{
	uint32 generation;
	int32 next_free;
	uint64 impl[];
}
typedef HandlePoolEntry_;

struct HandlePool_
{
	void* entries;
	intz entry_count;
	intz entry_size;

	int64 first_free;
	int64 allocated_count;
}
typedef HandlePool_;

static inline FORCE_INLINE void*
EncodeHandle_(intz index, uint32 generation)
{
	SafeAssert(index >= 0 && index <= (1ULL << 48) - 1);
	uint64 result = 0;
	result |= (uint64)generation << 48;
	result |= (uint64)(index + 1 & (1LL << 48) - 1);
	return (void*)result;
}

static inline FORCE_INLINE void
DecodeHandle_(void* handle, intz* out_index, uint32* out_generation)
{
	uint64 value = (uint64)handle;
	*out_index = (value & (1ULL << 48) - 1) - 1;
	*out_generation = value >> 48;
}

static inline HandlePool_
InitHandlePool_(intz item_count, intz item_size, Allocator allocator, AllocatorError* out_err)
{
	Trace();
	item_size += SignedSizeof(HandlePoolEntry_);
	return (HandlePool_) {
		.entries = AllocatorAllocArray(&allocator, item_count, item_size, 8, out_err),
		.entry_count = item_count,
		.entry_size = item_size,
	};
}

static inline void*
AllocateFromHandlePool_(HandlePool_* pool, void* out_handle_, intz* out_index)
{
	Trace();
	void** out_handle = (void**)out_handle_;
	HandlePoolEntry_* entry = NULL;
	intz index = 0;

	for (;;)
	{
		int64 first_free = AtomicLoad64Acq(&pool->first_free);
		if (!first_free)
			break;

		index = first_free - 1;
		entry = (HandlePoolEntry_*)((uint8*)pool->entries + index * pool->entry_size);
		if (AtomicCompareExchange64Rel(&pool->first_free, &first_free, entry->next_free))
			break;
		entry = NULL;
	}

	if (!entry)
	{
		for (;;)
		{
			int64 allocated_count = AtomicLoad64Acq(&pool->allocated_count);
			if (allocated_count >= pool->entry_count)
				break;

			index = allocated_count;
			if (AtomicCompareExchange64Rel(&pool->allocated_count, &allocated_count, allocated_count + 1))
			{
				entry = (HandlePoolEntry_*)((uint8*)pool->entries + index * pool->entry_size);
				break;
			}
		}
	}

	if (!entry)
	{
		*out_handle = NULL;
		return NULL;
	}

	uint32 generation = entry->generation + 1 & UINT16_MAX;
	MemoryZero(entry, pool->entry_size);
	entry->generation = generation | 0x80000000;
	entry->next_free = 0;

	if (out_index)
		*out_index = index;
	*out_handle = EncodeHandle_(index, generation);
	return entry->impl;
}

static inline void*
FetchFromHandlePool_(HandlePool_* pool, void* handle, intz* out_index)
{
	Trace();
	intz index;
	uint32 generation;
	DecodeHandle_(handle, &index, &generation);
	if (index < 0 || index >= pool->entry_count)
		return NULL;
	
	HandlePoolEntry_* entry = (HandlePoolEntry_*)((uint8*)pool->entries + index * pool->entry_size);
	if (!(entry->generation >> 31) || (entry->generation & UINT16_MAX) != generation)
		entry = NULL;
	if (out_index)
		*out_index = index;
	return entry->impl;
}

static inline void
FreeFromHandlePool_(HandlePool_* pool, void* handle)
{
	Trace();
	intz index;
	uint32 generation;
	DecodeHandle_(handle, &index, &generation);
	if (index < 0 || index >= pool->entry_count)
		return;
	
	HandlePoolEntry_* entry = (HandlePoolEntry_*)((uint8*)pool->entries + index * pool->entry_size);
	if (!(entry->generation >> 31) || (entry->generation & UINT16_MAX) != generation)
		return;

	entry->generation &= UINT16_MAX;
	entry->next_free = AtomicExchange64AcqRel(&pool->first_free, index + 1);
}

static inline void*
FetchByIndexFromHandlePool_(HandlePool_* pool, intz index)
{
	Trace();
	if (index < 0 || index >= pool->entry_count)
		return NULL;
	HandlePoolEntry_* entry = (HandlePoolEntry_*)((uint8*)pool->entries + index * pool->entry_size);
	if (!(entry->generation >> 31))
		return NULL;
	return entry->impl;
}

#ifdef __cplusplus

template <typename T>
struct CppHandlePool_ : HandlePool_ {};

template <typename T>
static inline CppHandlePool_<T>
InitCppHandlePool_(intz item_count, Allocator allocator, AllocatorError* out_err)
{
	CppHandlePool_<T> result = {};
	*(HandlePool_*)&result = InitHandlePool_(item_count, sizeof(T), allocator, out_err);
	return result;
}

template <typename T>
static inline T*
AllocateFromCppHandlePool_(CppHandlePool_<T>* pool, void* out_handle_, intz* out_index)
{ return (T*)AllocateFromHandlePool_(pool, out_handle_, out_index); }

template <typename T>
static inline T*
FetchFromCppHandlePool_(CppHandlePool_<T>* pool, void* handle, intz* out_index)
{ return (T*)FetchFromHandlePool_(pool, handle, out_index); }

template <typename T>
static inline T*
FetchByIndexFromCppHandlePool_(CppHandlePool_<T>* pool, intz index)
{ return (T*)FetchByIndexFromHandlePool_(pool, index); }

#endif

#endif //RENDER4_HANDLEPOOL_H