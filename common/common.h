/*
* NOTE(ljre):
*     The Common Headers. They're just independent declarations we use everywhere for any tool.
*/

#ifndef COMMON_H
#define COMMON_H
#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#	define _CRT_SECURE_NO_WARNINGS
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "common_basic.h"

//~ ASSERT
#ifndef Assert_IsDebuggerPresent_
#	ifdef _WIN32
EXTERN_C __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
#		define Assert_IsDebuggerPresent_() IsDebuggerPresent()
#	else
#		define Assert_IsDebuggerPresent_() true
#	endif
#endif

//- NOTE(ljre): SafeAssert -- always present, side-effects allowed, memory safety assert
#define SafeAssert(...) do {                                                  \
		if (Unlikely(!(__VA_ARGS__))) {                                       \
			if (Assert_IsDebuggerPresent_())                                  \
				Debugbreak();                                                 \
			AssertionFailure(#__VA_ARGS__, __func__, __FILE__, __LINE__); \
		}                                                                     \
	} while (0)

//- NOTE(ljre): Assert -- set to Assume() on release, logic safety assert
#ifndef CONFIG_DEBUG
#	define Assert(...) Assume(__VA_ARGS__)
#else //CONFIG_DEBUG
#	define Assert(...) do {                                               \
		if (Unlikely(!(__VA_ARGS__))) {                                   \
			if (Assert_IsDebuggerPresent_())                              \
				Debugbreak();                                             \
			AssertionFailure(#__VA_ARGS__, __func__, __FILE__, __LINE__); \
		}                                                                 \
	} while (0)
#endif //CONFIG_DEBUG

#ifdef CONFIG_OS_ANDROID
#	include <android/log.h>
//#	undef Trace
//#	define Trace() __android_log_print(ANDROID_LOG_INFO, "NativeExample", "[Trace] %s\n", __func__)
#	undef Assert_OnFailure
#	define Assert_OnFailure(expr, file, line, func) do { __android_log_print(ANDROID_LOG_FATAL, "NativeExample", "[Assert] file %s at line %i:\n\tFunction: %s\n\tExpr: %s\n", file, line, func, expr); Unreachable(); } while (0)
#	undef SafeAssert_OnFailure
#	define SafeAssert_OnFailure Assert_OnFailure
#	undef Assert_IsDebuggerPresent_
#	define Assert_IsDebuggerPresent_() false
#endif //CONFIG_OS_ANDROID

#ifndef __cplusplus
#	define Buf(x) (Buffer) BufInit(x)
#	define BUFNULL (Buffer) { 0 }
#	define BufNull (Buffer) { 0 }
#	define BufMake(size, data) (Buffer) { (const uint8*)(data), (intz)(size) }
#	define BufRange(begin, end) (Buffer) BufInitRange(begin, end)
#	define Str(x) (String) StrInit(x)
#	define STRNULL (String) { 0 }
#	define StrNull (String) { 0 }
#	define StrMake(size,data) (String) { (const uint8*)(data), (intz)(size) }
#	define StrRange(begin, end) (String) StrInitRange(begin, end)
#else //__cplusplus
#	define Buf(x) Buffer BufInit(x)
#	define BUFNULL Buffer {}
#	define BufNull Buffer {}
#	define BufMake(size, data) Buffer { (const uint8*)(data), (intz)(size) }
#	define BufRange(begin, end) Buffer BufInitRange(begin, end)
#	define Str(x) String StrInit(x)
#	define STRNULL String {}
#	define StrNull String {}
#	define StrMake(size, data) String { (const uint8*)(data), (intz)(size) }
#	define StrRange(begin, end) String StrInitRange(begin, end)
#endif //__cplusplus
#define BufInit(x) { (const uint8*)(x), SignedSizeof(x) }
#define BufInitRange(begin, end) { (const uint8*)(begin), (intz)((end) - (begin)) }
#define StrInit(x) { (const uint8*)(x), SignedSizeof(x) - 1 }
#define StrInitRange(begin, end) { (const uint8*)(begin), (intz)((end) - (begin)) }
#define StrFmt(x) (x).size, (char*)(x).data
#define StrMacro_(x) #x
#define StrMacro(x) StrMacro_(x)

#define StringVPrintfLocal(size, ...) StringVPrintf((char[size]) { 0 }, size, __VA_ARGS__)
#define StringPrintfLocal(size, ...) StringPrintf((char[size]) { 0 }, size, __VA_ARGS__)

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
		((Type*)ArenaPushMemoryAligned(arena, &(Type) __VA_ARGS__, SignedSizeof(Type), alignof(Type)))
#else //__cplusplus
#   define ArenaPushStructInit(arena, Type, ...) \
		((Type*)ArenaPushMemoryAligned(arena, &(Type const&) Type __VA_ARGS__, SignedSizeof(Type), alignof(Type)))
#endif //__cplusplus

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

static inline FORCE_INLINE String StringMake(intz size, void const* buffer);
static inline FORCE_INLINE String StringMakeRange(void const* start, void const* end);
static inline FORCE_INLINE Buffer BufferMake(intz size, void const* buffer);
static inline FORCE_INLINE Buffer BufferMakeRange(void const* start, void const* end);
static inline bool StringDecode(String str, intz* index, uint32* out_codepoint);
static inline intz StringEncodedCodepointSize(uint32 codepoint);
static inline intz StringEncode(uint8* buffer, intz size, uint32 codepoint);
static inline intz StringDecodedLength(String str);
static inline int32 StringCompare(String a, String b);
static inline bool StringEquals(String a, String b);
static inline bool StringEndsWith(String check, String s);
static inline bool StringStartsWith(String check, String s);
static inline String StringSubstr(String str, intsize index, intsize size);
static inline String StringSlice(String str, intsize begin, intsize end);
static inline String StringSliceEnd(String str, intsize count);
static inline String StringFromCString(char const* cstr);
static inline intsize StringIndexOf(String str, uint8 ch, intsize start_index);
static inline intsize StringIndexOfSubstr(String str, String substr, intsize start_index);
static inline String StringFromFixedBuffer(Buffer buf);

static inline intz StringVPrintfBuffer(char* buf, intz len, const char* fmt, va_list args);
static inline intz StringPrintfBuffer(char* buf, intz len, const char* fmt, ...);
static inline String StringVPrintf(char* buf, intz len, const char* fmt, va_list args);
static inline String StringPrintf(char* buf, intz len, const char* fmt, ...);
static inline intz StringVPrintfSize(const char* fmt, va_list args);
static inline intz StringPrintfSize(const char* fmt, ...);

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

static inline FORCE_INLINE uint64 HashString(String memory);
static inline FORCE_INLINE uint32 HashInt32(uint32 x);
static inline FORCE_INLINE uint64 HashInt64(uint64 x);
static inline FORCE_INLINE intz   HashMsi(uint32 log2_of_cap, uint64 hash, intz index);

static inline Allocator AllocatorFromArena(Arena* arena);
static inline Allocator NullAllocator(void);
static inline bool IsNullAllocator(Allocator allocator);

static inline Arena*         ScratchArena(intz conflict_count, Arena* const conflicts[]);
static inline void           Log(int32 level, char const* fmt, ...);
static inline ThreadContext* ThisThreadContext(void);
NO_RETURN static inline void FORCE_NOINLINE AssertionFailure(char const* expr, char const* func, char const* file, int32 line);

//~ API Implementation
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

static inline FORCE_INLINE String
StringMake(intz size, void const* buffer)
{
	return StrMake(size, buffer);
}

static inline FORCE_INLINE String
StringMakeRange(void const* start, void const* end)
{
	return StrRange(start, end);
}

static inline FORCE_INLINE Buffer
BufferMake(intz size, void const* buffer)
{
	return BufMake(size, buffer);
}

static inline FORCE_INLINE Buffer
BufferMakeRange(void const* start, void const* end)
{
	return BufRange(start, end);
}

static inline bool
StringDecode(String str, intz* index, uint32* out_codepoint)
{
	const uint8* head = str.data + *index;
	const uint8* const end = str.data + str.size;
	
	if (head >= end)
		return false;
	
	uint8 byte = *head++;
	if (!byte || byte == 0xff)
		return false;
	
	int32 size =  BitClz8(~byte);
	if (Unlikely(size == 1 || size > 4 || head + size - 1 > end))
		return false;
	
	uint32 result = 0;
	if (size == 0)
		result = byte;
	else
	{
		result |= (byte << size & 0xff) >> size;
		
		switch (size)
		{
			case 4: result = (result << 6) | (*head++ & 0x3f);
			case 3: result = (result << 6) | (*head++ & 0x3f);
			case 2: result = (result << 6) | (*head++ & 0x3f);
		}
	}
	
	*index = (intsize)(head - str.data);
	*out_codepoint = result;
	return true;
}

static inline intz
StringEncodedCodepointSize(uint32 codepoint)
{
	if (codepoint < 128)
		return 1;
	if (codepoint < 128+1920)
		return 2;
	if (codepoint < 128+1920+61440)
		return 3;
	return 4;
}

static inline intz
StringEncode(uint8* buffer, intz size, uint32 codepoint)
{
	intz needed = StringEncodedCodepointSize(codepoint);
	if (size < needed)
		return 0;
	
	switch (needed)
	{
		case 1: buffer[0] = codepoint & 0x7f; break;
		
		case 2:
		{
			buffer[0] = (codepoint>>6 & 0x1f) | 0xc0;
			buffer[1] = (codepoint>>0 & 0x3f) | 0x80;
		} break;
		
		case 3:
		{
			buffer[0] = (codepoint>>12 & 0x0f) | 0xe0;
			buffer[1] = (codepoint>>6  & 0x3f) | 0x80;
			buffer[2] = (codepoint>>0  & 0x3f) | 0x80;
		} break;
		
		case 4:
		{
			buffer[0] = (codepoint>>18 & 0x07) | 0xf0;
			buffer[1] = (codepoint>>12 & 0x3f) | 0x80;
			buffer[2] = (codepoint>>6  & 0x3f) | 0x80;
			buffer[3] = (codepoint>>0  & 0x3f) | 0x80;
		} break;
		
		default: Unreachable(); break;
	}
	
	return needed;
}

// TODO(ljre): make this better (?)
static inline intz
StringDecodedLength(String str)
{
	Trace();
	intz len = 0;
	
	for (intz i = 0; i < str.size; ++i)
		len += ((str.data[i] & 0x80) == 0 || (str.data[i] & 0x40) != 0);
	
	return len;
}

static inline int32
StringCompare(String a, String b)
{
	Trace();
	int32 result = MemoryCompare(a.data, b.data, Min(a.size, b.size));
	
	if (result == 0 && a.size != b.size)
		return (a.size > b.size) ? 1 : -1;
	
	return result;
}

static inline bool
StringEquals(String a, String b)
{
	Trace();
	if (a.size != b.size)
		return false;
	if (a.data == b.data)
		return true;

	return MemoryCompare(a.data, b.data, a.size) == 0;
}

static inline bool
StringEndsWith(String check, String s)
{
	Trace();
	if (!check.size)
		return (s.size == 0);
	if (s.size > check.size)
		return false;
	
	String substr = {
		check.data + (check.size - s.size),
		s.size,
	};
	
	return MemoryCompare(substr.data, s.data, substr.size) == 0;
}

static inline bool
StringStartsWith(String check, String s)
{
	Trace();
	if (s.size > check.size)
		return false;
	
	return MemoryCompare(check.data, s.data, s.size) == 0;
}

static inline String
StringSubstr(String str, intz index, intz size)
{
	if (!str.size)
		return str; // NOTE(ljre): adding a 0 offset to a null pointer is UB
	index = Clamp(index, 0, str.size);
	str.data += index;
	str.size -= index;

	if (size < 0)
		size = str.size + size + 1;
	str.size = Clamp(size, 0, str.size);
	
	return str;
}

static inline String
StringFromCString(char const* cstr)
{
	Trace();
	return StrMake(MemoryStrlen(cstr), cstr);
}

static inline String
StringSlice(String str, intz begin, intz end)
{
	if (!str.size)
		return str; // NOTE(ljre): adding a 0 offset to a null pointer is UB
	if (begin < 0)
		begin = (intz)str.size + begin + 1;
	if (end < 0)
		end = (intz)str.size + end + 1;
	
	begin = ClampMax(begin, (intz)str.size);
	end   = Clamp(end, begin, (intz)str.size);

	str.data += begin;
	str.size = end - begin;
	return str;
}

static inline String
StringSliceEnd(String str, intz count)
{
	if (!str.size)
		return str; // NOTE(ljre): adding a 0 offset to a null pointer is UB
	count = ClampMax(count, (intz)str.size);
	str.data += (intz)str.size - count;
	str.size = count;
	return str;
}

static inline intz
StringIndexOf(String str, uint8 ch, intz start_index)
{
	Trace();
	if (start_index < 0)
		start_index = 0;
	if (start_index >= (intz)str.size)
		return -1;
	if (!str.size)
		return -1;
	
	uint8 const* data = str.data + start_index;
	intz size = str.size - start_index;
	uint8 const* found_ch = (uint8 const*)MemoryFindByte(data, ch, size);
	if (found_ch)
		return found_ch - data;
	return -1;
}

static inline intsize
StringIndexOfSubstr(String str, String substr, intsize start_index)
{
	Trace();
	if (start_index < 0)
		start_index = 0;
	if (start_index >= (intsize)str.size)
		return -1;
	if (substr.size > str.size)
		return -1;
	if (!substr.size || !str.size)
		return -1;
	
	uint8 const* data = str.data;
	intz size = str.size;
	intz min_allowed_size = size - substr.size;
	
	for (intsize offset = start_index; offset < min_allowed_size; ++offset)
	{
		uint8 const* found_ch = (uint8 const*)MemoryFindByte(data + offset, substr.data[0], min_allowed_size - offset);
		if (!found_ch)
			return -1;
		offset = found_ch - data;
		if (MemoryCompare(data + offset, substr.data, substr.size))
			return offset;
	}
	
	return -1;
}

static inline String
StringFromFixedBuffer(Buffer buf)
{
	Trace();
	if (!buf.size)
		return buf;

	uint8 const* zero = (uint8 const*)MemoryFindByte(buf.data, 0, buf.size);
	String result = buf;
	if (zero)
		result.size = zero - buf.data;
	return result;
}

static inline FORCE_INLINE intz StringPrintfFunc_(char* buf, intz buf_size, const char* restrict fmt, va_list args);

static inline intz
StringVPrintfBuffer(char* buf, intz len, const char* fmt, va_list args)
{
	Trace();
	Assert(buf && len >= 0);
	
	return StringPrintfFunc_(buf, len, fmt, args);
}

static inline intz
StringPrintfBuffer(char* buf, intz len, const char* fmt, ...)
{
	Trace();
	Assert(buf && len >= 0);
	
	va_list args;
	va_start(args, fmt);
	
	intz result = StringPrintfFunc_(buf, len, fmt, args);
	
	va_end(args);
	return result;
}

static inline String
StringVPrintf(char* buf, intz len, const char* fmt, va_list args)
{
	Trace();
	Assert(buf && len);
	
	String result = {
		(uint8*)buf,
		StringPrintfFunc_(buf, len, fmt, args),
	};
	
	return result;
}

static inline String
StringPrintf(char* buf, intz len, const char* fmt, ...)
{
	Trace();
	Assert(buf && len);
	
	va_list args;
	va_start(args, fmt);
	
	String result = {
		(uint8*)buf,
		StringPrintfFunc_(buf, len, fmt, args),
	};
	
	va_end(args);
	
	return result;
}

static inline intz
StringVPrintfSize(const char* fmt, va_list args)
{
	Trace();
	return StringPrintfFunc_(NULL, 0, fmt, args);
}

static inline intz
StringPrintfSize(const char* fmt, ...)
{
	Trace();
	va_list args;
	va_start(args, fmt);
	
	intz result = StringPrintfFunc_(NULL, 0, fmt, args);
	
	va_end(args);
	return result;
}

//- NOTE(ljre): Actual implementation for snprintf alternative.
#define String_STDSP_SPECIAL 0x7000
static inline int32 String_stbsp__real_to_str(char const** start, uint32* len, char* out, int32* decimal_pos, float64 value, uint32 frac_digits);

static inline FORCE_INLINE void
StringWriteBuf_(char** p, char* end, intz* count, const char* restrict buf, intsize bufsize)
{
	Assert(bufsize >= 0);
	
	if (Likely(bufsize > 0))
	{
		if (end)
		{
			bufsize = Min(end - *p, bufsize);
			MemoryCopy(*p, buf, bufsize);
			*p += bufsize;
		}
		
		*count += bufsize;
	}
}

static inline FORCE_INLINE void
StringFillBuf_(char** p, char* end, intz* count, char fill, intsize fillsize)
{
	Assert(fillsize >= 0);
	
	if (Likely(fillsize > 0))
	{
		if (end)
		{
			fillsize = Min(end - *p, fillsize);
			MemorySet(*p, (uint8)fill, fillsize);
			*p += fillsize;
		}
		
		*count += fillsize;
	}
}

static inline FORCE_INLINE intz
StringPrintfFunc_(char* buf, intz buf_size, const char* restrict fmt, va_list args)
{
	Trace();
	intz count = 0;
	const char* fmt_end = fmt + MemoryStrlen(fmt);
	
	SafeAssert(!buf ? buf_size <= 0 : true);
	
	char* p = buf;
	char* p_end = buf_size ? buf + buf_size : buf; // NOTE(ljre): avoid "(T*)NULL + 0" UB.
	
	while (*fmt)
	{
		//- Copy leading chars
		const char* percent_addr = (const char*)MemoryFindByte(fmt, '%', fmt_end - fmt);
		
		if (percent_addr)
		{
			StringWriteBuf_(&p, p_end, &count, fmt, percent_addr - fmt);
			fmt = percent_addr;
		}
		else
		{
			StringWriteBuf_(&p, p_end, &count, fmt, fmt_end - fmt);
			fmt = fmt_end;
			break;
		}
		
		if (*fmt != '%')
			break;
		++fmt;
		
		//- Parse padding
		int32 leading_padding = -1;
		int32 trailling_padding = -1;
		
		if (*fmt >= '1' && *fmt <= '9')
		{
			leading_padding = 0;
			
			do
			{
				leading_padding *= 10;
				leading_padding += *fmt - '0';
				++fmt;
			}
			while (*fmt >= '0' && *fmt <= '9');
		}
		
		if (*fmt == '.')
		{
			++fmt;
			trailling_padding = 0;
			
			if (*fmt == '*')
			{
				trailling_padding = va_arg(args, int32);
				++fmt;
			}
			else while (*fmt >= '0' && *fmt <= '9')
			{
				trailling_padding *= 10;
				trailling_padding += *fmt - '0';
				++fmt;
			}
		}
		
		//- Parse format specifier
		char tmpbuf[128] = { 0 };
		const char* write_buf = tmpbuf;
		intz write_count = 0;
		
		bool handled_write = false;
		char prefix_char = 0;
		char padding_char = ' ';
		
		switch (*fmt++)
		{
			default: write_count = 1; tmpbuf[0] = fmt[-1]; Assert(false); break;
			case '%': write_count = 1; tmpbuf[0] = '%'; break;
			case '0': write_count = 1; tmpbuf[0] = 0; break;
			case 'c':
			{
				int32 arg = va_arg(args, int32);
				write_count = 1;
				tmpbuf[0] = (char)arg;
			} break;
			
			//case 'i':
			//case 'I':
			//case 'Z':
			{
				int64 arg;
				const char* intmin;
				
				if (0) case 'i': { arg = va_arg(args, int32); intmin = "-2147483648";          }
				if (0) case 'I': { arg = va_arg(args, int64); intmin = "-9223372036854775808"; }
				// TODO(ljre): correct this
				if (0) case 'Z': { arg = va_arg(args, intsize); intmin = "this hould be PTRDIFF_MIN as string"; }
				
				padding_char = '0';
				
				if (arg == INT64_MIN ||
					arg == INT32_MIN && fmt[-1] == 'i' ||
					arg == INTSIZE_MIN && fmt[-1] == 'Z')
				{
					write_buf = intmin;
					write_count = MemoryStrlen(intmin);
					
					break;
				}
				else if (arg < 0)
				{
					arg = -arg;
					prefix_char = '-';
				}
				else if (arg == 0)
				{
					tmpbuf[0] = '0';
					write_count = 1;
					
					break;
				}
				
				int32 index = sizeof(tmpbuf);
				
				while (index > 0 && arg > 0)
				{
					tmpbuf[--index] = (arg % 10) + '0';
					arg /= 10;
				}
				
				write_count = SignedSizeof(tmpbuf) - index;
				write_buf = tmpbuf + index;
			} break;
			
			//case 'u':
			//case 'U':
			//case 'z':
			{
				uint64 arg;
				
				if (0) case 'u': arg = va_arg(args, uint32);
				if (0) case 'U': arg = va_arg(args, uint64);
				if (0) case 'z': arg = va_arg(args, uintsize);
				
				padding_char = '0';
				
				if (arg == 0)
				{
					tmpbuf[0] = '0';
					write_count = 1;
					break;
				}
				
				int32 index = sizeof(tmpbuf);
				
				while (index > 0 && arg > 0)
				{
					tmpbuf[--index] = (arg % 10) + '0';
					arg /= 10;
				}
				
				write_count = SignedSizeof(tmpbuf) - index;
				write_buf = tmpbuf + index;
			} break;
			
			//case 'x':
			//case 'X':
			//case 'p':
			{
				uint64 arg;
				
				if (0) case 'x': arg = va_arg(args, uint32);
				if (0) case 'X': arg = va_arg(args, uint64);
				if (0) case 'p': arg = va_arg(args, uintptr);
				
				padding_char = '0';
				
				if (arg == 0)
				{
					tmpbuf[0] = '0';
					write_count = 1;
					break;
				}
				
				const char* chars = "0123456789ABCDEF";
				int32 index = sizeof(tmpbuf);
				
				while (index > 0 && arg > 0)
				{
					tmpbuf[--index] = chars[arg & 0xf];
					arg >>= 4;
				}
				
				write_count = SignedSizeof(tmpbuf) - index;
				write_buf = tmpbuf + index;
			} break;
			
			case 's':
			{
				const char* arg = va_arg(args, const char*);
				intz len;
				
				if (!arg)
				{
					arg = "(null)";
					len = 6;
				}
				else if (trailling_padding >= 0)
				{
					const char* found = (const char*)MemoryFindByte(arg, 0, trailling_padding);
					
					if (found)
						len = found - arg;
					else
						len = trailling_padding;
					
					trailling_padding = -1;
				}
				else
					len = MemoryStrlen(arg);
				
				write_buf = arg;
				write_count = len;
			} break;
			
			case 'S':
			{
				String arg = va_arg(args, String);
				
				if (trailling_padding >= 0)
				{
					arg.size = Min(arg.size, trailling_padding);
					trailling_padding = -1;
				}
				
				write_buf = (const char*)arg.data;
				write_count = (intsize)arg.size;
			} break;
			
			case 'f':
			{
				float64 arg = va_arg(args, float64);
				
				if (trailling_padding == -1)
					trailling_padding = 8;
				
				char* start;
				uint32 length_;
				int32 decimal_pos;
				
				static_assert(sizeof(tmpbuf) > 64 + 32, "stbsp__real_to_str takes just the final 64 bytes of the buffer because we might need to add a prefix to the string");
				
				bool negative = String_stbsp__real_to_str((const char**)&start, &length_, tmpbuf+sizeof(tmpbuf)-64, &decimal_pos, arg, (uint32)trailling_padding);
				intz length = (intz)length_;
				
				if (decimal_pos == String_STDSP_SPECIAL)
				{
					if (start[0] == 'I' && negative)
					{
						write_buf = "-Inf";
						write_count = 4;
					}
					else
					{
						write_buf = start;
						write_count = 3;
					}
					
					break;
				}
				
				padding_char = '0';
				if (negative)
					prefix_char = '-';
				
				if (decimal_pos <= 0)
				{
					start -= -decimal_pos + 2;
					length += -decimal_pos + 2;
					
					SafeAssert(start >= tmpbuf);
					
					write_buf = start;
					write_count = length;
					
					*start++ = '0';
					*start++ = '.';
					
					while (decimal_pos++ < 0)
						*start++ = '0';
				}
				else
				{
					handled_write = true;
					if (negative)
						StringFillBuf_(&p, p_end, &count, '-', 1);
					
					if ((int32)length <= decimal_pos)
					{
						if (leading_padding != -1 && decimal_pos < leading_padding)
							StringFillBuf_(&p, p_end, &count, '0', leading_padding - decimal_pos);
						
						StringWriteBuf_(&p, p_end, &count, start, length);
						StringFillBuf_(&p, p_end, &count, '0', decimal_pos - length);
						StringFillBuf_(&p, p_end, &count, '.', 1);
						StringFillBuf_(&p, p_end, &count, '0', 2);
					}
					else
					{
						if (leading_padding != -1 && (int32)length - decimal_pos < leading_padding)
							StringFillBuf_(&p, p_end, &count, '0', leading_padding - (length - decimal_pos));
						
						StringWriteBuf_(&p, p_end, &count, start, decimal_pos);
						StringFillBuf_(&p, p_end, &count, '.', 1);
						StringWriteBuf_(&p, p_end, &count, start + decimal_pos, length - decimal_pos);
					}
				}
			} break;
		}
		
		if (!handled_write)
		{
			if (prefix_char)
				StringWriteBuf_(&p, p_end, &count, &prefix_char, 1);
			if (leading_padding != -1 && write_count < leading_padding)
				StringFillBuf_(&p, p_end, &count, padding_char, leading_padding - write_count);
			if (write_count)
				StringWriteBuf_(&p, p_end, &count, write_buf, write_count);
		}
	}
	
	return count;
}

//~ NOTE(ljre): All this code is derived from stb_sprint! Licensed under Unlicense.
static inline void
String_stbsp__copyfp(void* to, const void* f)
{
	*(uint64*)to = *(uint64*)f;
}

static inline void
String_stbsp__ddmulthi(float64* restrict oh, float64* restrict ol, float64 xh, float64 yh)
{
	float64 ahi = 0, alo, bhi = 0, blo;
	int64 bt;
	
	*oh = xh * yh;
	String_stbsp__copyfp(&bt, &xh);
	bt &= ((~(uint64)0) << 27);
	String_stbsp__copyfp(&ahi, &bt);
	alo = xh - ahi;
	String_stbsp__copyfp(&bt, &yh);
	bt &= ((~(uint64)0) << 27);
	String_stbsp__copyfp(&bhi, &bt);
	blo = yh - bhi;
	*ol = ((ahi * bhi - *oh) + ahi * blo + alo * bhi) + alo * blo;
}

static inline void
String_stbsp__ddmultlos(float64* restrict oh, float64* restrict ol, float64 xh, float64 yl)
{
	*ol = *ol + (xh * yl);
}

static inline void
String_stbsp__ddmultlo(float64* restrict oh, float64* restrict ol, float64 xh, float64 xl, float64 yh, float64 yl)
{
	*ol = *ol + (xh * yl + xl * yh);
}

static inline void
String_stbsp__ddrenorm(float64* restrict oh, float64* restrict ol)
{
	float64 s;
	s = *oh + *ol;
	*ol = *ol - (s - *oh);
	*oh = s;
}

static inline void
String_stbsp__ddtoS64(int64* restrict ob, float64 xh, float64 xl)
{
	float64 ahi = 0, alo, vh, t;
	*ob = (int64)xh;
	vh = (float64)*ob;
	ahi = (xh - vh);
	t = (ahi - xh);
	alo = (xh - (ahi - t)) - (vh + t);
	*ob += (int64)(ahi + alo + xl);
}

static void
String_stbsp__raise_to_power10(float64* restrict ohi, float64* restrict olo, float64 d, int32 power) // power can be -323 to +350
{
	static float64 const stbsp__bot[23] = {
		1e+000, 1e+001, 1e+002, 1e+003, 1e+004, 1e+005, 1e+006, 1e+007, 1e+008, 1e+009, 1e+010, 1e+011,
		1e+012, 1e+013, 1e+014, 1e+015, 1e+016, 1e+017, 1e+018, 1e+019, 1e+020, 1e+021, 1e+022
	};
	static float64 const stbsp__negbot[22] = {
		1e-001, 1e-002, 1e-003, 1e-004, 1e-005, 1e-006, 1e-007, 1e-008, 1e-009, 1e-010, 1e-011,
		1e-012, 1e-013, 1e-014, 1e-015, 1e-016, 1e-017, 1e-018, 1e-019, 1e-020, 1e-021, 1e-022
	};
	static float64 const stbsp__negboterr[22] = {
		-5.551115123125783e-018,  -2.0816681711721684e-019, -2.0816681711721686e-020, -4.7921736023859299e-021, -8.1803053914031305e-022, 4.5251888174113741e-023,
		4.5251888174113739e-024,  -2.0922560830128471e-025, -6.2281591457779853e-026, -3.6432197315497743e-027, 6.0503030718060191e-028,  2.0113352370744385e-029,
		-3.0373745563400371e-030, 1.1806906454401013e-032,  -7.7705399876661076e-032, 2.0902213275965398e-033,  -7.1542424054621921e-034, -7.1542424054621926e-035,
		2.4754073164739869e-036,  5.4846728545790429e-037,  9.2462547772103625e-038,  -4.8596774326570872e-039
	};
	static float64 const stbsp__top[13] = {
		1e+023, 1e+046, 1e+069, 1e+092, 1e+115, 1e+138, 1e+161, 1e+184, 1e+207, 1e+230, 1e+253, 1e+276, 1e+299
	};
	static float64 const stbsp__negtop[13] = {
		1e-023, 1e-046, 1e-069, 1e-092, 1e-115, 1e-138, 1e-161, 1e-184, 1e-207, 1e-230, 1e-253, 1e-276, 1e-299
	};
	static float64 const stbsp__toperr[13] = {
		8388608,
		6.8601809640529717e+028,
		-7.253143638152921e+052,
		-4.3377296974619174e+075,
		-1.5559416129466825e+098,
		-3.2841562489204913e+121,
		-3.7745893248228135e+144,
		-1.7356668416969134e+167,
		-3.8893577551088374e+190,
		-9.9566444326005119e+213,
		6.3641293062232429e+236,
		-5.2069140800249813e+259,
		-5.2504760255204387e+282
	};
	static float64 const stbsp__negtoperr[13] = {
		3.9565301985100693e-040,  -2.299904345391321e-063,  3.6506201437945798e-086,  1.1875228833981544e-109,
		-5.0644902316928607e-132, -6.7156837247865426e-155, -2.812077463003139e-178,  -5.7778912386589953e-201,
		7.4997100559334532e-224,  -4.6439668915134491e-247, -6.3691100762962136e-270, -9.436808465446358e-293,
		8.0970921678014997e-317
	};
	
	float64 ph, pl;
	if ((power >= 0) && (power <= 22)) {
		String_stbsp__ddmulthi(&ph, &pl, d, stbsp__bot[power]);
	} else {
		int32 e, et, eb;
		double p2h, p2l;
		
		e = power;
		if (power < 0)
			e = -e;
		et = (e * 0x2c9) >> 14; /* %23 */
		if (et > 13)
			et = 13;
		eb = e - (et * 23);
		
		ph = d;
		pl = 0.0;
		if (power < 0) {
			if (eb) {
				--eb;
				String_stbsp__ddmulthi(&ph, &pl, d, stbsp__negbot[eb]);
				String_stbsp__ddmultlos(&ph, &pl, d, stbsp__negboterr[eb]);
			}
			if (et) {
				String_stbsp__ddrenorm(&ph, &pl);
				--et;
				String_stbsp__ddmulthi(&p2h, &p2l, ph, stbsp__negtop[et]);
				String_stbsp__ddmultlo(&p2h, &p2l, ph, pl, stbsp__negtop[et], stbsp__negtoperr[et]);
				ph = p2h;
				pl = p2l;
			}
		} else {
			if (eb) {
				e = eb;
				if (eb > 22)
					eb = 22;
				e -= eb;
				String_stbsp__ddmulthi(&ph, &pl, d, stbsp__bot[eb]);
				if (e) {
					String_stbsp__ddrenorm(&ph, &pl);
					String_stbsp__ddmulthi(&p2h, &p2l, ph, stbsp__bot[e]);
					String_stbsp__ddmultlos(&p2h, &p2l, stbsp__bot[e], pl);
					ph = p2h;
					pl = p2l;
				}
			}
			if (et) {
				String_stbsp__ddrenorm(&ph, &pl);
				--et;
				String_stbsp__ddmulthi(&p2h, &p2l, ph, stbsp__top[et]);
				String_stbsp__ddmultlo(&p2h, &p2l, ph, pl, stbsp__top[et], stbsp__toperr[et]);
				ph = p2h;
				pl = p2l;
			}
		}
	}
	String_stbsp__ddrenorm(&ph, &pl);
	*ohi = ph;
	*olo = pl;
}

static inline int32
String_stbsp__real_to_str(char const** start, uint32* len, char out[64], int32* decimal_pos, float64 value, uint32 frac_digits)
{
	Trace();
	static uint64 const stbsp__powten[20] = {
		1,
		10,
		100,
		1000,
		10000,
		100000,
		1000000,
		10000000,
		100000000,
		1000000000,
		10000000000ULL,
		100000000000ULL,
		1000000000000ULL,
		10000000000000ULL,
		100000000000000ULL,
		1000000000000000ULL,
		10000000000000000ULL,
		100000000000000000ULL,
		1000000000000000000ULL,
		10000000000000000000ULL
	};
	
	static const struct
	{
		alignas(2) char pair[201];
	}
	stbsp__digitpair =
	{
		"00010203040506070809101112131415161718192021222324"
			"25262728293031323334353637383940414243444546474849"
			"50515253545556575859606162636465666768697071727374"
			"75767778798081828384858687888990919293949596979899"
	};
	
	float64 d;
	int64 bits = 0;
	int32 expo, e, ng, tens;
	
	d = value;
	String_stbsp__copyfp(&bits, &d);
	
	expo = (int32)((bits >> 52) & 2047);
	ng = (int32)((uint64) bits >> 63);
	if (ng)
		d = -d;
	
	if (expo == 2047) // is nan or inf?
	{
		*start = ((uint64)bits & ((((uint64)1) << 52) - 1)) ? "NaN" : "Inf";
		*decimal_pos = String_STDSP_SPECIAL;
		*len = 3;
		return ng;
	}
	
	if (expo == 0) // is zero or denormal
	{
		if (((uint64) bits << 1) == 0) // do zero
		{
			*decimal_pos = 1;
			*start = out;
			out[0] = '0';
			*len = 1;
			return ng;
		}
		// find the right expo for denormals
		{
			int64 v = ((uint64)1) << 51;
			while ((bits & v) == 0) {
				--expo;
				v >>= 1;
			}
		}
	}
	
	// find the decimal exponent as well as the decimal bits of the value
	{
		float64 ph, pl;
		
		// log10 estimate - very specifically tweaked to hit or undershoot by no more than 1 of log10 of all expos 1..2046
		tens = expo - 1023;
		tens = (tens < 0) ? ((tens * 617) / 2048) : (((tens * 1233) / 4096) + 1);
		
		// move the significant bits into position and stick them into an int
		String_stbsp__raise_to_power10(&ph, &pl, d, 18 - tens);
		
		// get full as much precision from double-double as possible
		String_stbsp__ddtoS64(&bits, ph, pl);
		
		// check if we undershot
		if (((uint64)bits) >= 1000000000000000000ULL/*stbsp__tento19th*/)
			++tens;
	}
	
	// now do the rounding in integer land
	frac_digits = (frac_digits & 0x80000000) ? ((frac_digits & 0x7ffffff) + 1) : ((uint32)tens + frac_digits);
	if ((frac_digits < 24)) {
		uint32 dg = 1;
		if ((uint64)bits >= stbsp__powten[9])
			dg = 10;
		while ((uint64)bits >= stbsp__powten[dg]) {
			++dg;
			if (dg == 20)
				goto noround;
		}
		if (frac_digits < dg) {
			uint64 r;
			// add 0.5 at the right position and round
			e = (int32)(dg - frac_digits);
			if ((uint32)e >= 24)
				goto noround;
			r = stbsp__powten[e];
			bits = bits + (int64)(r / 2);
			if ((uint64)bits >= stbsp__powten[dg])
				++tens;
			bits /= r;
		}
		noround:;
	}
	
	// kill long trailing runs of zeros
	if (bits) {
		uint32 n;
		for (;;) {
			if (bits <= 0xffffffff)
				break;
			if (bits % 1000)
				goto donez;
			bits /= 1000;
		}
		n = (uint32)bits;
		while ((n % 1000) == 0)
			n /= 1000;
		bits = n;
		donez:;
	}
	
	// convert to string
	out += 64;
	e = 0;
	for (;;) {
		uint32 n;
		char* o = out - 8;
		// do the conversion in chunks of U32s (avoid most 64-bit divides, worth it, constant denomiators be damned)
		if (bits >= 100000000) {
			n = (uint32)(bits % 100000000);
			bits /= 100000000;
		} else {
			n = (uint32)bits;
			bits = 0;
		}
		while (n) {
			out -= 2;
			*(uint16*)out = *(uint16*)&stbsp__digitpair.pair[(n % 100) * 2];
			n /= 100;
			e += 2;
		}
		if (bits == 0) {
			if ((e) && (out[0] == '0')) {
				++out;
				--e;
			}
			break;
		}
		while (out != o) {
			*--out = '0';
			++e;
		}
	}
	
	*decimal_pos = tens;
	*start = out;
	*len = (uint32)e;
	return ng;
}

#undef String_STDSP_SPECIAL

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
	SafeAssert((intsize)size >= 0);
	
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
ArenaAllocatorProc(void* instance, AllocatorMode mode, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
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
			intsize old_offset = (uint8*)old_ptr - arena->memory;
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

			intsize old_offset = (uint8*)old_ptr - arena->memory;
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
				intsize amount_to_copy = Min(size, old_size);
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
			intsize old_offset = (uint8*)old_ptr - arena->memory;
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
				intsize amount_to_copy = Min(size, old_size);
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

static inline void*
AllocatorAlloc(Allocator* allocator, intsize size, intsize alignment, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_Alloc, size, alignment, NULL, 0, out_err);
}

static inline String
AllocatorCloneString(Allocator* allocator, String str, AllocatorError* out_err)
{
	uint8* ptr = allocator->proc(allocator->instance, AllocatorMode_Alloc, str.size, 1, NULL, 0, out_err);
	if (ptr)
	{
		MemoryCopy(ptr, str.data, str.size);
		return StringMake(str.size, ptr);
	}
	return StrNull;
}

static inline String
AllocatorCloneBuffer(Allocator* allocator, intz alignment, Buffer buf, AllocatorError* out_err)
{
	uint8* ptr = allocator->proc(allocator->instance, AllocatorMode_Alloc, buf.size, alignment, NULL, 0, out_err);
	if (ptr)
	{
		MemoryCopy(ptr, buf.data, buf.size);
		return BufMake(buf.size, ptr);
	}
	return StrNull;
}

static inline void*
AllocatorResize(Allocator* allocator, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_Resize, size, alignment, old_ptr, old_size, out_err);
}

static inline void*
AllocatorAllocNonZeroed(Allocator* allocator, intsize size, intsize alignment, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_AllocNonZeroed, size, alignment, NULL, 0, out_err);
}

static inline void*
AllocatorResizeNonZeroed(Allocator* allocator, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	return allocator->proc(allocator->instance, AllocatorMode_ResizeNonZeroed, size, alignment, old_ptr, old_size, out_err);
}

static inline void
AllocatorFree(Allocator* allocator, void* old_ptr, intsize old_size, AllocatorError* out_err)
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
AllocatorResizeOk(Allocator* allocator, intsize size, intsize alignment, void* inout_ptr, intsize old_size, AllocatorError* out_err)
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
AllocatorResizeNonZeroedOk(Allocator* allocator, intsize size, intsize alignment, void* inout_ptr, intsize old_size, AllocatorError* out_err)
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
AllocatorAllocArray(Allocator* allocator, intsize count, intsize size, intsize alignment, AllocatorError* out_err)
{
	SafeAssert(!size || count >= 0 && count <= INTSIZE_MAX / size);
	return allocator->proc(allocator->instance, AllocatorMode_Alloc, count*size, alignment, NULL, 0, out_err);
}

static inline void*
AllocatorResizeArray(Allocator* allocator, intsize count, intsize size, intsize alignment, void* old_ptr, intsize old_count, AllocatorError* out_err)
{
	SafeAssert(!size || (count >= 0 && count <= INTSIZE_MAX / size && old_count >= 0 && old_count <= INTSIZE_MAX / size));
	return allocator->proc(allocator->instance, AllocatorMode_Resize, count*size, alignment, old_ptr, old_count*size, out_err);
}

static inline bool
AllocatorResizeArrayOk(Allocator* allocator, intsize count, intsize size, intsize alignment, void* inout_ptr, intsize old_count, AllocatorError* out_err)
{
	SafeAssert(!size || (count >= 0 && count <= INTSIZE_MAX / size && old_count >= 0 && old_count <= INTSIZE_MAX / size));
	void* new_ptr = allocator->proc(allocator->instance, AllocatorMode_Resize, count*size, alignment, *(void**)inout_ptr, old_count*size, out_err);
	if (new_ptr || !size)
	{
		*(void**)inout_ptr = new_ptr;
		return true;
	}
	return false;
}

static inline void
AllocatorFreeArray(Allocator* allocator, intsize size, void* old_ptr, intsize old_count, AllocatorError* out_err)
{
	SafeAssert(!size || old_count >= 0 && old_count <= INTSIZE_MAX / size);
	allocator->proc(allocator->instance, AllocatorMode_Free, 0, 0, old_ptr, old_count*size, out_err);
}

// NOTE(ljre): FNV-1a implementation.
static inline FORCE_INLINE uint64
HashString(String memory)
{
	Trace();
	uint64 result = 14695981039346656037u;
	
	for (uintsize i = 0; i < memory.size; ++i)
	{
		uint64 value = (uint64)memory.data[i] & 0xff;
		
		result ^= value;
		result *= 1099511628211u;
	}
	
	return result;
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

// NOTE(ljre): Thread context stuff
#ifdef _MSC_VER
__declspec(selectany) EXTERN_C thread_local
#elif defined(__GNUC__)
__attribute__((weak)) EXTERN_C thread_local
#endif
ThreadContext g_thread_context_;

static inline ThreadContext*
ThisThreadContext(void)
{
	ThreadContext* ctx = &g_thread_context_;
	return ctx;
}

static inline Arena*
ScratchArena(intz conflict_count, Arena* const conflicts[])
{
	ThreadContext* thread_context = ThisThreadContext();
	for (intz i = 0; i < ArrayLength(thread_context->scratch); ++i)
	{
		Arena* arena = &thread_context->scratch[i];
		bool ok = true;
		for (intz j = 0; j < conflict_count; ++j)
		{
			if (arena == conflicts[j])
			{
				ok = false;
				break;
			}
		}
		if (ok)
			return arena;
	}
	return NULL;
}

static inline void
Log(int32 level, char const* fmt, ...)
{
	ThreadContext* thread_ctx = ThisThreadContext();
	if (!thread_ctx->logger.proc || level < thread_ctx->logger.minimum_level)
		return;

	va_list args;
	va_start(args, fmt);
	Arena* scratch_arena = ScratchArena(0, NULL);
	for ArenaTempScope(scratch_arena)
	{
		String str = ArenaVPrintf(scratch_arena, fmt, args);
		thread_ctx->logger.proc(&thread_ctx->logger, level, str);
	}
	va_end(args);
}

NO_RETURN static inline void FORCE_NOINLINE
AssertionFailure(char const* expr, char const* func, char const* file, int32 line)
{
	Trace();
	ThreadContext* thread_ctx = ThisThreadContext();
	if (thread_ctx->assertion_failure_proc)
	{
		String expr_str = StringFromCString(expr);
		String func_str = StringFromCString(func);
		String file_str = StringFromCString(file);
		thread_ctx->assertion_failure_proc(expr_str, func_str, file_str, line);
	}
	Trap();
}

#endif //COMMON_H
