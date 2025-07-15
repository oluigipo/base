#ifndef LJRE_BASE_HASH_H
#define LJRE_BASE_HASH_H

#include "base.h"

// NOTE(ljre): FNV-1a implementation.
static inline FORCE_INLINE uint64
HashFnv1a(String memory)
{
	Trace();
	uint64 result = 14695981039346656037u;
	
	for (intz i = 0; i < memory.size; ++i)
	{
		uint64 value = (uint64)memory.data[i] & 0xff;
		
		result ^= value;
		result *= 1099511628211u;
	}
	
	return result;
}

static inline FORCE_INLINE uint64
HashString(String memory)
{
	return HashFnv1a(memory);
}

// NOTE(ljre): Perfect hash of 32bit integer permutation.
//             Name: lowbias32
//             https://github.com/skeeto/hash-prospector
static inline FORCE_INLINE uint32
HashInt32(uint32 x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

// NOTE(ljre): Perfect hash of 64bit integer permitation.
//             Name: SplittableRandom / SplitMix64
//             https://xoshiro.di.unimi.it/splitmix64.c
static inline FORCE_INLINE uint64
HashInt64(uint64 x)
{
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9;
    x ^= x >> 27;
    x *= 0x94d049bb133111eb;
    x ^= x >> 31;
    return x;
}

// NOTE(ljre): This is a implementation of "MSI hash table".
//             https://nullprogram.com/blog/2022/08/08/
static inline FORCE_INLINE intz
HashMsi(uint32 log2_of_cap, uint64 hash, intz index)
{
	uint32 exp = log2_of_cap;
	uint32 mask = (1u << exp) - 1;
	uint32 step = (uint32)(hash >> (64 - exp)) | 1;
	return (index + step) & mask;
}

#endif //LJRE_BASE_HASH_H
