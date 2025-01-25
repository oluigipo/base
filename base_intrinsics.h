#ifndef LJRE_BASE_INTRINSICS_H
#define LJRE_BASE_INTRINSICS_H

#include "base.h"
#include "base_assert.h"

static inline FORCE_INLINE int32 BitCtz64(uint64 i);
static inline FORCE_INLINE int32 BitCtz32(uint32 i);
static inline FORCE_INLINE int32 BitCtz16(uint16 i);
static inline FORCE_INLINE int32 BitCtz8(uint8 i);
static inline FORCE_INLINE int32 BitClz64(uint64 i);
static inline FORCE_INLINE int32 BitClz32(uint32 i);
static inline FORCE_INLINE int32 BitClz16(uint16 i);
static inline FORCE_INLINE int32 BitClz8(uint8 i);
static inline FORCE_INLINE int32 PopCnt64(uint64 x);
static inline FORCE_INLINE int32 PopCnt32(uint32 x);
static inline FORCE_INLINE int32 PopCnt16(uint16 x);
static inline FORCE_INLINE int32 PopCnt8(uint8 x);
static inline FORCE_INLINE uint64 ByteSwap64(uint64 x);
static inline FORCE_INLINE uint32 ByteSwap32(uint32 x);
static inline FORCE_INLINE uint16 ByteSwap16(uint16 x);
static inline FORCE_INLINE uint16  EncodeF16(float32 x);
static inline FORCE_INLINE float32 DecodeF16(uint16 x);
static inline intz MemoryStrlen(const char* restrict cstr);
static inline intz MemoryStrnlen(const char* restrict cstr, intz limit);
static inline int32 MemoryStrcmp(const char* left, const char* right);
static inline int32 MemoryStrncmp(const char* left, const char* right, intz limit);
static inline char* MemoryStrstr(const char* left, const char* right);
static inline char* MemoryStrnstr(const char* left, const char* right, intz limit);
static inline void const* MemoryFindByte(const void* buffer, uint8 byte, intz size);
static inline void* MemoryZeroSafe(void* restrict dst, intz size);
static inline FORCE_INLINE void* MemoryZero(void* restrict dst, intz size);
static inline FORCE_INLINE void* MemoryCopy(void* restrict dst, const void* restrict src, intz size);
static inline FORCE_INLINE void* MemoryMove(void* dst, const void* src, intz size);
static inline FORCE_INLINE void* MemorySet(void* restrict dst, uint8 byte, intz size);
static inline FORCE_INLINE int32 MemoryCompare(const void* left_, const void* right_, intz size);

#ifdef CONFIG_ARCH_X86FAMILY
#   include <immintrin.h>
#endif

#ifndef CONFIG_ARENA_DEFAULT_ALIGNMENT
#	define CONFIG_ARENA_DEFAULT_ALIGNMENT 16
#endif

static_assert(CONFIG_ARENA_DEFAULT_ALIGNMENT != 0 && IsPowerOf2(CONFIG_ARENA_DEFAULT_ALIGNMENT), "Default Arena alignment should always be non-zero and a power of two");

//-
#if defined(_MSC_VER) && !defined(__clang__)
#	pragma optimize("", off)
#endif
static inline void*
MemoryZeroSafe(void* restrict dst, intz size)
{
	Trace();
	MemoryZero(dst, size);
#if defined(__GNUC__) || defined(__clang__)
	__asm__ __volatile__ ("" : "+X"(dst) :: "memory");
#elif defined(_MSC_VER)
	_ReadWriteBarrier();
#endif
	return dst;
}
#if defined(_MSC_VER) && !defined(__clang__)
#	pragma optimize("", on)
#endif

//-
static inline FORCE_INLINE int32
GenericPopCnt64(uint64 x)
{
	x -= (x >> 1) & 0x5555555555555555u;
	x = (x & 0x3333333333333333u) + ((x >> 2) & 0x3333333333333333u);
	x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fu;
	return (x * 0x0101010101010101u) >> 56;
}

static inline FORCE_INLINE int32
GenericPopCnt32(uint32 x)
{
	x -= (x >> 1) & 0x55555555u;
	x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
	x = (x + (x >> 4)) & 0x0f0f0f0fu;
	return (x * 0x01010101u) >> 24;
}

static inline FORCE_INLINE int32
GenericPopCnt16(uint16 x)
{ return GenericPopCnt32(x); }

static inline FORCE_INLINE int32
GenericPopCnt8(uint8 x)
{ return GenericPopCnt32(x); }

static inline FORCE_INLINE uint16
GenericByteSwap16(uint16 x)
{ return (uint16)((x >> 8) | (x << 8)); }

static inline FORCE_INLINE uint32
GenericByteSwap32(uint32 x)
{
	uint32 result;
	result = (x << 24) | (x >> 24) | (x >> 8 & 0xff00) | (x << 8 & 0xff0000);
	return result;
}

static inline FORCE_INLINE uint64
GenericByteSwap64(uint64 x)
{
	uint64 result;
	result  = (uint64)GenericByteSwap32(x >> 32);
	result |= (uint64)GenericByteSwap32((uint32)x) << 32;
	return result;
}

//~ NOTE(ljre): X86
#if defined(CONFIG_ARCH_X86FAMILY)
static inline FORCE_INLINE int32
BitCtz64(uint64 i)
{
	int32 result;
	
	if (i == 0)
		result = sizeof(i)*8;
	else
	{
#if defined(__GNUC__) || defined(__clang__)
		result = __builtin_ctzll(i);
#elif defined(_MSC_VER)
		_BitScanForward64((unsigned long*)&result, i);
#endif
	}
	
	return result;
}

static inline FORCE_INLINE int32
BitCtz32(uint32 i)
{
	int32 result;
	
	if (i == 0)
		result = sizeof(i)*8;
	else
	{
#if defined(__GNUC__) || defined(__clang__)
		result = __builtin_ctz(i);
#elif defined(_MSC_VER)
		_BitScanForward((unsigned long*)&result, i);
#endif
	}
	
	return result;
}

static inline FORCE_INLINE int32
BitCtz16(uint16 i)
{ return i == 0 ? sizeof(i)*8 : BitCtz32(i); }

static inline FORCE_INLINE int32
BitCtz8(uint8 i)
{ return i == 0 ? sizeof(i)*8 : BitCtz32(i); }

static inline FORCE_INLINE int32
BitClz64(uint64 i)
{
	int32 result;
	
	if (i == 0)
		result = sizeof(i)*8;
	else
	{
#if defined(__GNUC__) || defined(__clang__)
		result = __builtin_clzll(i);
#elif defined(_MSC_VER)
		_BitScanReverse((unsigned long*)&result, i);
		result = 63 - result;
#endif
	}
	
	return result;
}

static inline FORCE_INLINE int32
BitClz32(uint32 i)
{
	int32 result;
	
	if (i == 0)
		result = sizeof(i)*8;
	else
	{
#if defined(__GNUC__) || defined(__clang__)
		result = __builtin_clz(i);
#elif defined(_MSC_VER)
		_BitScanReverse((unsigned long*)&result, i);
		result = 31 - result;
#endif
	}
	
	return result;
}

static inline FORCE_INLINE int32
BitClz16(uint16 i)
{ return i == 0 ? sizeof(i)*8 : BitClz32(i) - 16; }

static inline FORCE_INLINE int32
BitClz8(uint8 i)
{ return i == 0 ? sizeof(i)*8 : BitClz32(i) - 24; }

#ifdef CONFIG_ARCH_INST_POPCNT
static inline FORCE_INLINE int32
PopCnt64(uint64 x)
{ return (int32)_mm_popcnt_u64(x); }

static inline FORCE_INLINE int32
PopCnt32(uint32 x)
{ return (int32)_mm_popcnt_u32(x); }

static inline FORCE_INLINE int32
PopCnt16(uint16 x)
{ return (int32)_mm_popcnt_u32(x); }

static inline FORCE_INLINE int32
PopCnt8(uint8 x)
{ return (int32)_mm_popcnt_u32(x); }
#else //CONFIG_ARCH_INST_POPCNT
static inline FORCE_INLINE int32
PopCnt64(uint64 x)
{ return GenericPopCnt64(x); }

static inline FORCE_INLINE int32
PopCnt32(uint32 x)
{ return GenericPopCnt32(x); }

static inline FORCE_INLINE int32
PopCnt16(uint16 x)
{ return GenericPopCnt16(x); }

static inline FORCE_INLINE int32
PopCnt8(uint8 x)
{ return GenericPopCnt8(x); }
#endif //CONFIG_ARCH_INST_POPCNT

//- ByteSwap
static inline FORCE_INLINE uint16
ByteSwap16(uint16 x)
{ return (uint16)((x >> 8) | (x << 8)); }

static inline FORCE_INLINE uint32
ByteSwap32(uint32 x)
{
	uint32 result;
	
#if defined(__GNUC__) || defined(__clang__)
	result = __builtin_bswap32(x);
#elif defined (_MSC_VER)
	extern unsigned long _byteswap_ulong(unsigned long);
	
	result = _byteswap_ulong(x);
#endif
	
	return result;
}

static inline FORCE_INLINE uint64
ByteSwap64(uint64 x)
{
	uint64 result;
	
#if defined(__GNUC__) || defined(__clang__)
	result = __builtin_bswap64(x);
#elif defined (_MSC_VER)
	extern unsigned __int64 _byteswap_uint64(unsigned __int64);
	
	result = _byteswap_uint64(x);
#endif
	
	return result;
}

static inline FORCE_INLINE uint16
EncodeF16(float32 x)
{
	union
	{
		float32 f;
		uint32 i;
	} cvt = { x };
	uint32 sign = cvt.i >> 31;
	uint32 exponent = (cvt.i >> 23) & 0xff;
	uint32 mantissa = cvt.i & 0x7fffff;
	
	exponent = Clamp(exponent, 128-16, 128+16);
	exponent -= 128-16;
	mantissa >>= 13;
	
	uint32 result = 0;
	result |= sign << 15;
	result |= exponent << 10;
	result |= mantissa;
	return (uint16)result;
}

static inline FORCE_INLINE float32
DecodeF16(uint16 x)
{
	uint32 sign = x >> 15;
	uint32 exponent = (x >> 10) & 0x1f;
	uint32 mantissa = x & 0x3ff;
	
	exponent += 128-16;
	mantissa <<= 13;
	
	uint32 result = 0;
	result |= sign << 31;
	result |= exponent << 23;
	result |= mantissa;
	
	union
	{
		uint32 i;
		float32 f;
	} cvt = { result };
	return cvt.f;
}

#elif defined(CONFIG_ARCH_ARMFAMILY)
//~ NOTE(ljre): ARM
#ifdef CONFIG_ARCH_AARCH64
#	include <arm_neon.h>
#endif
#ifdef CONFIG_DONT_USE_CRT
#	error "ARM implementation needs CRT"
#endif

static inline FORCE_INLINE int32 BitCtz64(uint64 i) { return __builtin_ctzll(i); }
static inline FORCE_INLINE int32 BitCtz32(uint32 i) { return __builtin_ctz(i); }
static inline FORCE_INLINE int32 BitCtz16(uint16 i) { return __builtin_ctz(i); }
static inline FORCE_INLINE int32 BitCtz8(uint8 i) { return __builtin_ctz(i); }

static inline FORCE_INLINE int32 BitClz64(uint64 i) { return __builtin_clzll(i); }
static inline FORCE_INLINE int32 BitClz32(uint32 i) { return __builtin_clz(i); }
static inline FORCE_INLINE int32 BitClz16(uint16 i) { return __builtin_clz(i)-16; }
static inline FORCE_INLINE int32 BitClz8(uint8 i) { return __builtin_clz(i)-24; }

#ifdef CONFIG_ARCH_AARCH64
static inline FORCE_INLINE int32 PopCnt64(uint64 x) { return __builtin_popcountll(x); }
static inline FORCE_INLINE int32 PopCnt32(uint32 x) { return __builtin_popcount(x); }
static inline FORCE_INLINE int32 PopCnt16(uint16 x) { return __builtin_popcount(x); }
static inline FORCE_INLINE int32 PopCnt8(uint8 x) { return __builtin_popcount(x); }
#else //CONFIG_ARCH_AARCH64
static inline FORCE_INLINE int32 PopCnt64(uint64 x) { return GenericPopCnt64(x); }
static inline FORCE_INLINE int32 PopCnt32(uint32 x) { return GenericPopCnt32(x); }
static inline FORCE_INLINE int32 PopCnt16(uint16 x) { return GenericPopCnt16(x); }
static inline FORCE_INLINE int32 PopCnt8(uint8 x) { return GenericPopCnt8(x); }
#endif //CONFIG_ARCH_AARCH64

static inline FORCE_INLINE uint64 ByteSwap64(uint64 x) { return __builtin_bswap64(x); }
static inline FORCE_INLINE uint32 ByteSwap32(uint32 x) { return __builtin_bswap32(x); }
static inline FORCE_INLINE uint16 ByteSwap16(uint16 x) { return __builtin_bswap16(x); }

#ifdef CONFIG_ARCH_AARCH64
static inline FORCE_INLINE void
MemoryCopyX16(void* restrict dst, const void* restrict src)
{ vst1q_u64((uint64*)dst, vld1q_u64((const uint64*)src)); }
#else //CONFIG_ARCH_AARCH64
static inline FORCE_INLINE void
MemoryCopyX16(void* restrict dst, const void* restrict src)
{
	((uint32*)dst)[0] = ((const uint32*)src)[0];
	((uint32*)dst)[1] = ((const uint32*)src)[1];
	((uint32*)dst)[2] = ((const uint32*)src)[2];
	((uint32*)dst)[3] = ((const uint32*)src)[3];
}
#endif //CONFIG_ARCH_AARCH64

#else //CONFIG_ARCH_*
#	error "Unknown architecture"
#endif //CONFIG_ARCH_*

#include <string.h>

static inline FORCE_INLINE void*
MemoryCopy(void* restrict dst, const void* restrict src, intz size)
{ Trace(); Assert(size >= 0); return size == 0 ? dst : memcpy(dst, src, (size_t)size); }

static inline FORCE_INLINE void*
MemoryMove(void* dst, const void* src, intz size)
{ Trace(); Assert(size >= 0); return size == 0 ? dst : memmove(dst, src, (size_t)size); }

static inline FORCE_INLINE void*
MemorySet(void* restrict dst, uint8 byte, intz size)
{ Trace(); Assert(size >= 0); return size == 0 ? dst : memset(dst, byte, (size_t)size); }

static inline FORCE_INLINE int32
MemoryCompare(const void* left_, const void* right_, intz size)
{ Trace(); Assert(size >= 0); return size == 0 ? 0 : memcmp(left_, right_, (size_t)size); }

static inline intz
MemoryStrlen(const char* restrict cstr)
{
	Trace();
	size_t result = strlen(cstr);
	Assert(result <= INTZ_MAX);
	return (intz)result;
}

static inline intz
MemoryStrnlen(const char* restrict cstr, intz limit)
{
	Trace();
	Assert(limit >= 0);
	size_t result = strnlen(cstr, (size_t)limit);
	Assert(result <= INTZ_MAX);
	return (intz)result;
}

static inline int32
MemoryStrcmp(const char* left, const char* right)
{ Trace(); return strcmp(left, right); }

static inline int32
MemoryStrncmp(const char* left, const char* right, intz limit)
{
	Trace();
	Assert(limit >= 0);
	return strncmp(left, right, (size_t)limit);
}

static inline char*
MemoryStrstr(const char* left, const char* right)
{ Trace(); return (char*)strstr(left, right); }

static inline const void*
MemoryFindByte(const void* buffer, uint8 byte, intz size)
{
	Trace();
	Assert(size >= 0);
	return memchr(buffer, byte, (size_t)size);
}

static inline FORCE_INLINE void*
MemoryZero(void* restrict dst, intz size)
{
	Trace();
	Assert(size >= 0);
	return memset(dst, 0, (size_t)size);
}

static inline char*
MemoryStrnstr(const char* left, const char* right, intz limit)
{
	Trace();
	
	for (; *left && limit > 0; ++left)
	{
		const char* it_left = left;
		const char* it_right = right;
		intz local_limit = limit--;
		while (*it_left == *it_right)
		{
			if (!*it_left)
				return (char*)it_left;
			if (--local_limit == 0)
				return (char*)it_left;
			++it_left;
			++it_right;
		}
	}
	
	return NULL;
}

#endif //LJRE_BASE_INTRINSICS_H
