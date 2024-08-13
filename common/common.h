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

//~ CONFIG
#if defined(__cplusplus)
#	define API extern "C"
#else
#	define API
#endif

//- Detect arch
#if defined(__x86_64__) || defined(_M_X64)
#	define CONFIG_ARCH_AMD64
#	define CONFIG_ARCH_X86FAMILY
#	define CONFIG_SIZEOF_PTR 8
#	define CONFIG_CACHELINE_SIZE 64
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#	define CONFIG_ARCH_I686
#	define CONFIG_ARCH_X86FAMILY
#	define CONFIG_SIZEOF_PTR 4
#	define CONFIG_CACHELINE_SIZE 64
#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
#	define CONFIG_ARCH_ARMV7A
#	define CONFIG_ARCH_ARMFAMILY
#	define CONFIG_SIZEOF_PTR 4
#	define CONFIG_CACHELINE_SIZE 64
#elif defined(__aarch64__) || defined(_M_ARM64)
#	define CONFIG_ARCH_AARCH64
#	define CONFIG_ARCH_ARMFAMILY
#	define CONFIG_SIZEOF_PTR 8
#	define CONFIG_CACHELINE_SIZE 64
#else
#	error "Could not detect target arch"
#endif

#ifdef CONFIG_ARCH_X86FAMILY
#    if defined(__AVX2__)
#        define CONFIG_ARCH_SSELEVEL 51
#    elif defined(__AVX__)
#        define CONFIG_ARCH_SSELEVEL 50
#    elif defined(__SSE4_2__)
#        define CONFIG_ARCH_SSELEVEL 42
#    elif defined(__SSE4_1__)
#        define CONFIG_ARCH_SSELEVEL 41
#    elif defined(__SSSE3__)
#        define CONFIG_ARCH_SSELEVEL 31
#    elif defined(__SSE3__)
#        define CONFIG_ARCH_SSELEVEL 30
#    elif defined(__SSE2__) || defined(CONFIG_ARCH_AMD64)
#        define CONFIG_ARCH_SSELEVEL 20
#    endif
#    if CONFIG_ARCH_SSELEVEL >= 41 || defined(__POPCNT__)
#        define CONFIG_ARCH_INST_POPCNT
#        define CONFIG_ARCH_INST_LZCNT
#    endif
#endif //CONFIG_ARCH_X86FAMILY

//- Detect OS
#if defined(_WIN32)
#   define CONFIG_OS_WINDOWS
#elif defined(ANDROID) || defined(__ANDROID__)
#   define CONFIG_OS_ANDROID
#elif defined(__linux__)
#   define CONFIG_OS_LINUX
#endif

//- Tracy
#if defined(CONFIG_ENABLE_TRACY) && defined(__clang__) // NOTE(ljre): this won't work with MSVC...
#	define TRACY_ENABLE
#	define TRACY_MANUAL_LIFETIME
#	define TRACY_DELAYED_INIT
#	include <TracyC.h>
static inline void ___my_tracy_zone_end(TracyCZoneCtx* ctx) { TracyCZoneEnd(*ctx); }
#	define TraceCat__(x,y) x ## y
#	define TraceCat_(x,y) TraceCat__(x,y)
#	define Trace() TracyCZone(TraceCat_(_ctx,__LINE__) __attribute((cleanup(___my_tracy_zone_end))),true); ((void)TraceCat_(_ctx,__LINE__))
#	define TraceName(...) do { String a = (__VA_ARGS__); TracyCZoneName(TraceCat_(_ctx,__LINE__), (const char*)a.data, a.size); } while (0)
#	define TraceText(...) do { String a = (__VA_ARGS__); TracyCZoneText(TraceCat_(_ctx,__LINE__), (const char*)a.data, a.size); } while (0)
#	define TraceColor(...) TracyCZoneColor(TraceCat_(_ctx,__LINE__), (__VA_ARGS__))
#	define TraceF(sz, ...) do { char buf[sz]; uintsize len = StringPrintfBuffer(buf, sizeof(buf), __VA_ARGS__); TracyCZoneText(TraceCat_(_ctx,__LINE__), buf, len); } while (0)
#	define TraceFrameBegin() TracyCFrameMarkStart(0)
#	define TraceFrameEnd() TracyCFrameMarkEnd(0)
#	define TraceInit() ___tracy_startup_profiler()
#	define TraceDeinit() ___tracy_shutdown_profiler()
#else
#	define Trace() ((void)0)
// #include <stdio.h>
// static int ___space_count = 0;
// static inline void __dec_count(char const** arg)
// { --___space_count; }
// static inline void __enter_func(char const* arg)
// {
// 	static char const spaces[] = "                                              ";
// 	___space_count += 1;
// 	printf("callstack: %.*s%s\n", ___space_count, spaces, arg);
// }
// #	define Trace() char const* __my_func __attribute__((cleanup(__dec_count))) = __func__; __enter_func(__my_func);
#	define TraceName(...) ((void)0)
#	define TraceText(...) ((void)0)
#	define TraceColor(...) ((void)0)
#	define TraceF(sz, ...) ((void)0)
#	define TraceFrameBegin() ((void)0)
#	define TraceFrameEnd() ((void)0)
#	define TraceInit() ((void)0)
#	define TraceDeinit() ((void)0)
#endif

//- IncludeBinary
// https://gist.github.com/mmozeiko/ed9655cf50341553d282
#if defined(__clang__) || defined(__GNUC__)
#ifdef _WIN32
#	define IncludeBinary_Section ".rdata, \"dr\""
#	ifdef _WIN64
#	    define IncludeBinary_Name(name) #name
#	else
#	    define IncludeBinary_Name(name) "_" #name
#	endif
#else
#	define IncludeBinary_Section ".rodata"
#	define IncludeBinary_Name(name) #name
#endif
// this aligns start address to 16 and terminates byte array with explict 0
// which is not really needed, feel free to change it to whatever you want/need
#define IncludeBinary(name, file) \
	__asm__( \
		".section " IncludeBinary_Section "\n" \
		".global " IncludeBinary_Name(name) "_begin\n" \
		".balign 16\n" \
		IncludeBinary_Name(name) "_begin:\n" \
		".incbin \"" file "\"\n" \
		\
		".global " IncludeBinary_Name(name) "_end\n" \
		".balign 1\n" \
		IncludeBinary_Name(name) "_end:\n" \
		".byte 0\n" \
	); \
extern __attribute__((aligned(16))) const unsigned char name ## _begin[]; \
extern const unsigned char name ## _end[];
#endif //defined(__clang__) || defined(__GNUC__)

//~ DEFS
#define AlignUp(x, mask) (((x) + (mask)) & ~(mask))
#define AlignDown(x, mask) ((x) & ~(mask))
#define IsPowerOf2(x) ( ((x) & (x)-1) == 0 )
#define ArrayLength(x) ((intsize)(sizeof(x) / sizeof(x)[0]))
#define Min(x,y) ((x) < (y) ? (x) : (y))
#define Max(x,y) ((x) > (y) ? (x) : (y))
#define ClampMax Min
#define ClampMin Max
#define Clamp(x,min,max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

#if defined(__clang__)
#	define Assume(...) __builtin_assume(__VA_ARGS__)
#	define Debugbreak() __builtin_debugtrap()
#	define Likely(...) __builtin_expect(!!(__VA_ARGS__), 1)
#	define Unlikely(...) __builtin_expect((__VA_ARGS__), 0)
#	define Unreachable() __builtin_unreachable()
#	define DisableWarnings() \
	_Pragma("clang diagnostic push")\
	_Pragma("clang diagnostic ignored \"-Weverything\"")
#	define ReenableWarnings() \
	_Pragma("clang diagnostic pop")
#	define FORCE_INLINE __attribute__((always_inline))
#	define FORCE_NOINLINE __attribute__((noinline))
#	define Alignas(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
#	define Assume(...) __assume(__VA_ARGS__)
#	define Debugbreak() __debugbreak()
#	define Likely(...) (__VA_ARGS__)
#	define Unlikely(...) (__VA_ARGS__)
#	define Unreachable() __assume(0)
#	define DisableWarnings() \
	__pragma(warning(push, 0))
#	define ReenableWarnings() \
	__pragma(warning(pop))
#    define FORCE_INLINE __forceinline
#    define FORCE_INLINE __declspec(noinline)
#	define Alignas(x) __declspec(align(x))
#elif defined(__GNUC__)
#	define Assume(...) do { if (!(__VA_ARGS__)) __builtin_unreachable(); } while (0)
#	define Debugbreak() __asm__ __volatile__ ("int $3")
#	define Likely(...) __builtin_expect(!!(__VA_ARGS__), 1)
#	define Unlikely(...) __builtin_expect((__VA_ARGS__), 0)
#	define Unreachable() __builtin_unreachable()
#	define DisableWarnings() \
	_Pragma("GCC diagnostic push")\
	_Pragma("GCC diagnostic ignored \"-Wall\"")\
	_Pragma("GCC diagnostic ignored \"-Wextra\"")
#	define ReenableWarnings() \
	_Pragma("GCC diagnostic pop")
#	define FORCE_INLINE __attribute__((always_inline))
#	define FORCE_NOINLINE __attribute__((noipa))
#	define Alignas(x) __attribute__((aligned(x)))
#else
#	define Assume(...) ((void)0)
#	define Debugbreak() ((void)0)
#	define Likely(...) (__VA_ARGS__)
#	define Unlikely(...) (__VA_ARGS__)
#	define Unreachable() ((void)0)
#	define DisableWarnings()
#	define ReenableWarnings()
#	define FORCE_INLINE
#	define FORCE_NOINLINE
#	define Alignas(x)
#endif

#ifdef __cplusplus
#	define restrict
#	define EXTERN_C extern "C"
#	define NO_RETURN [[noreturn]]
#else
#	define EXTERN_C extern
#	define NO_RETURN _Noreturn
#	define alignas _Alignas
#	define alignof _Alignof
#	define static_assert _Static_assert
#	ifdef _MSC_VER
#		define thread_local __declspec(thread)
#	else
#		define thread_local _Thread_local
#	endif
#endif

//~ COMPILER CAPS
#if defined(__cplusplus) || __STDC_VERSION__ >= 202000
#	define CONFIG_HAS_ENHANCED_ENUMS 1
#endif

#ifdef CONFIG_HAS_ENHANCED_ENUMS
#	define CONFIG_IF_ENHANCED_ENUMS(...) __VA_ARGS__
#else
#	define CONFIG_IF_ENHANCED_ENUMS(...)
#endif

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
#ifndef SafeAssert_OnFailure
#	define SafeAssert_OnFailure(expr, file, line, func) Unreachable()
#endif
#define SafeAssert(...) do {                                                  \
		bool not_ok = !(__VA_ARGS__);                                         \
		if (Unlikely(not_ok)) {                                               \
			if (Assert_IsDebuggerPresent_())                                  \
				Debugbreak();                                                 \
			SafeAssert_OnFailure(#__VA_ARGS__, __FILE__, __LINE__, __func__); \
		}                                                                     \
	} while (0)

//- NOTE(ljre): Assert -- set to Assume() on release, logic safety assert
#ifndef CONFIG_DEBUG
#	define Assert(...) Assume(__VA_ARGS__)
#else //CONFIG_DEBUG
#	ifndef Assert_OnFailure
#		define Assert_OnFailure(expr, file, line, func) Unreachable()
#	endif
#	define Assert(...) do {                                               \
		if (Unlikely(!(__VA_ARGS__))) {                                   \
			if (Assert_IsDebuggerPresent_())                              \
				Debugbreak();                                             \
			Assert_OnFailure(#__VA_ARGS__, __FILE__, __LINE__, __func__); \
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

//~ API Declaration
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef uint64_t  uint64;
typedef uintptr_t uintptr;
typedef size_t    uintsize;
typedef int8_t    int8;
typedef int16_t   int16;
typedef int32_t   int32;
typedef int64_t   int64;
typedef intptr_t  intptr;
typedef ptrdiff_t intsize;
typedef float     float32;
typedef double    float64;

typedef ptrdiff_t intz;
typedef size_t    uintz;

enum
{
	ATOMIC_RELAXED,
	ATOMIC_CONSUME,
	ATOMIC_ACQUIRE,
	ATOMIC_RELEASE,
	ATOMIC_ACQ_REL,
	ATOMIC_SEQ_CST,
};

#if defined(__GNUC__) || defined(__clang__)
static_assert(ATOMIC_RELAXED == __ATOMIC_RELAXED, "must be same");
static_assert(ATOMIC_CONSUME == __ATOMIC_CONSUME, "must be same");
static_assert(ATOMIC_ACQUIRE == __ATOMIC_ACQUIRE, "must be same");
static_assert(ATOMIC_RELEASE == __ATOMIC_RELEASE, "must be same");
static_assert(ATOMIC_ACQ_REL == __ATOMIC_ACQ_REL, "must be same");
static_assert(ATOMIC_SEQ_CST == __ATOMIC_SEQ_CST, "must be same");
#endif //defined(__GNUC__) || defined(__clang__)

struct Buffer
{
	uint8 const* data;
	uintsize size;
}
typedef Buffer;

typedef Buffer String;

struct Arena typedef Arena;
typedef bool ArenaCommitMemoryProc(Arena* arena, uintsize needed_size);

struct Arena
{
	uintsize size;
	uintsize offset;
	uint8* memory;
	
	// NOTE(ljre): Commit-as-you-go arena style.
	//             commit_memory_proc() function should return true if the specified needed_size is commited
	uintsize reserved;
	ArenaCommitMemoryProc* commit_memory_proc;
};

struct ArenaSavepoint
{
	Arena* arena;
	uintsize offset;
}
typedef ArenaSavepoint;

enum AllocatorMode CONFIG_IF_ENHANCED_ENUMS(: uint8)
{
	// requires (size, alignment), returns a valid pointer
	AllocatorMode_Alloc,
	// requires (old_ptr, old_size), returns NULL
	AllocatorMode_Free,
	// requires (), returns NULL
	AllocatorMode_FreeAll,
	// requires (size, alignment, old_ptr, old_size), returns a valid pointer
	AllocatorMode_Resize,
	// requires(old_ptr), returns NULL
	// NOTE(ljre): old_ptr needs to be a valid pointer to a uint32
	AllocatorMode_QueryFeatures,
	AllocatorMode_QueryInfo,
	// same as Alloc
	AllocatorMode_AllocNonZeroed,
	// same as Resize
	AllocatorMode_ResizeNonZeroed,
	// requires (old_ptr), returns NULL
	// NOTE(ljre): this is valid for arena allocators, where one can call Alloc(0, 0) to receive a pointer
	//             to the end of the arena and then call Pop(ptr) to free all allocations made after the
	//             call to Alloc.
	AllocatorMode_Pop,
};

#ifdef CONFIG_HAS_ENHANCED_ENUMS
typedef enum AllocatorMode AllocatorMode;
#else
typedef uint8 AllocatorMode;
#endif

enum AllocatorError
{
	AllocatorError_Ok = 0,
	AllocatorError_OutOfMemory,
	AllocatorError_InvalidPointer,
	AllocatorError_InvalidArgument,
	AllocatorError_ModeNotImplemented,
}
typedef AllocatorError;

struct Allocator typedef Allocator;
typedef void* AllocatorProc(Allocator* allocator, AllocatorMode mode, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err);
struct Allocator
{
	AllocatorProc* proc;
	void* instance;
	void* odin_proc;
};

#define INTSIZE_MAX PTRDIFF_MAX
#define INTSIZE_MIN PTRDIFF_MIN
#define UINTSIZE_MAX SIZE_MAX
#define INTZ_MAX PTRDIFF_MAX
#define INTZ_MIN PTRDIFF_MIN
#define UINTZ_MAX SIZE_MAX

#ifndef __cplusplus
#	define Buf(x) (Buffer) BufInit(x)
#	define BufNull (Buffer) { 0 }
#	define BufMake(size, data) (Buffer) { (const uint8*)(data), (uintsize)(size) }
#	define BufRange(begin, end) (Buffer) BufInitRange(begin, end)
#	define Str(x) (String) StrInit(x)
#	define StrNull (String) { 0 }
#	define StrMake(size,data) (String) { (const uint8*)(data), (uintsize)(size) }
#	define StrRange(begin, end) (String) StrInitRange(begin, end)
#else //__cplusplus
#	define Buf(x) Buffer BufInit(x)
#	define BufNull Buffer {}
#	define BufMake(size, data) Buffer { (const uint8*)(data), (uintsize)(size) }
#	define BufRange(begin, end) Buffer BufInitRange(begin, end)
#	define Str(x) String StrInit(x)
#	define StrNull String {}
#	define StrMake(size, data) String { (const uint8*)(data), (uintsize)(size) }
#	define StrRange(begin, end) String StrInitRange(begin, end)
#endif //__cplusplus
#define BufInit(x) { (const uint8*)(x), sizeof(x) }
#define BufInitRange(begin, end) { (const uint8*)(begin), (uintsize)((end) - (begin)) }
#define StrInit(x) { (const uint8*)(x), sizeof(x) - 1 }
#define StrInitRange(begin, end) { (const uint8*)(begin), (uintsize)((end) - (begin)) }
#define StrFmt(x) (x).size, (char*)(x).data
#define StrMacro_(x) #x
#define StrMacro(x) StrMacro_(x)

#define StringVPrintfLocal(size, ...) StringVPrintf((char[size]) { 0 }, size, __VA_ARGS__)
#define StringPrintfLocal(size, ...) StringPrintf((char[size]) { 0 }, size, __VA_ARGS__)

#define ArenaPushStruct(arena, Type) \
	((Type*)ArenaPushAligned(arena, sizeof(Type), alignof(Type)))
#define ArenaPushStructData(arena, Type, ...) \
	((Type*)MemoryCopy(ArenaPushDirtyAligned(arena, sizeof(Type), alignof(Type)), __VA_ARGS__, sizeof(Type)))
#define ArenaPushArray(arena, Type, count) \
	((Type*)ArenaPushAligned(arena, sizeof(Type)*(count), alignof(Type)))
#define ArenaPushArrayData(arena, Type, data, count) \
	((Type*)MemoryCopy(ArenaPushDirtyAligned(arena, sizeof(Type)*(count), alignof(Type)), data, sizeof(Type)*(count)))
#define ArenaPushData(arena, data) \
	MemoryCopy(ArenaPushDirtyAligned(arena, sizeof*(data), 1), data, sizeof*(data))
#define ArenaPushDataArray(arena, data, count) \
	MemoryCopy(ArenaPushDirtyAligned(arena, sizeof*(data)*(count), 1), data, sizeof*(data)*(count))
#define ArenaTempScope(arena_) \
	(ArenaSavepoint _temp__ = { arena_, (arena_)->offset }; _temp__.arena; _temp__.arena->offset = _temp__.offset, _temp__.arena = NULL)
#ifndef __cplusplus
#   define ArenaPushStructInit(arena, Type, ...) \
		((Type*)ArenaPushMemoryAligned(arena, &(Type) __VA_ARGS__, sizeof(Type), alignof(Type)))
#else //__cplusplus
#   define ArenaPushStructInit(arena, Type, ...) \
		((Type*)ArenaPushMemoryAligned(arena, &(Type const&) Type __VA_ARGS__, sizeof(Type), alignof(Type)))
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
static inline uintsize MemoryStrlen(const char* restrict cstr);
static inline uintsize MemoryStrnlen(const char* restrict cstr, uintsize limit);
static inline int32 MemoryStrcmp(const char* left, const char* right);
static inline int32 MemoryStrncmp(const char* left, const char* right, uintsize limit);
static inline char* MemoryStrstr(const char* left, const char* right);
static inline char* MemoryStrnstr(const char* left, const char* right, uintsize limit);
static inline void const* MemoryFindByte(const void* buffer, uint8 byte, uintsize size);
static inline void* MemoryZeroSafe(void* restrict dst, uintsize size);
static inline FORCE_INLINE void* MemoryZero(void* restrict dst, uintsize size);
static inline FORCE_INLINE void* MemoryCopy(void* restrict dst, const void* restrict src, uintsize size);
static inline FORCE_INLINE void* MemoryMove(void* dst, const void* src, uintsize size);
static inline FORCE_INLINE void* MemorySet(void* restrict dst, uint8 byte, uintsize size);
static inline FORCE_INLINE int32 MemoryCompare(const void* left_, const void* right_, uintsize size);

static inline uint32 StringDecode(String str, intsize* index);
static inline uint32 StringEncodedCodepointSize(uint32 codepoint);
static inline uintsize StringEncode(uint8* buffer, uintsize size, uint32 codepoint);
static inline uintsize StringDecodedLength(String str);
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

static inline uintsize StringVPrintfBuffer(char* buf, uintsize len, const char* fmt, va_list args);
static inline uintsize StringPrintfBuffer(char* buf, uintsize len, const char* fmt, ...);
static inline String StringVPrintf(char* buf, uintsize len, const char* fmt, va_list args);
static inline String StringPrintf(char* buf, uintsize len, const char* fmt, ...);
static inline uintsize StringVPrintfSize(const char* fmt, va_list args);
static inline uintsize StringPrintfSize(const char* fmt, ...);

static inline Arena ArenaFromMemory(void* memory, uintsize size);
static inline void* ArenaPush(Arena* arena, uintsize size);
static inline void* ArenaPushDirty(Arena* arena, uintsize size);
static inline void* ArenaPushAligned(Arena* arena, uintsize size, uintsize alignment);
static inline void* ArenaPushDirtyAligned(Arena* arena, uintsize size, uintsize alignment);
static inline void* ArenaPushMemory(Arena* arena, void const* buf, uintsize size);
static inline void* ArenaPushMemoryAligned(Arena* arena, void const* buf, uintsize size, uintsize alignment);
static inline String ArenaPushString(Arena* arena, String str);
static inline String ArenaPushStringAligned(Arena* arena, String str, uintsize alignment);
static inline char* ArenaPushCString(Arena* arena, String str);
static inline String ArenaVPrintf(Arena* arena, const char* fmt, va_list args);
static inline String ArenaPrintf(Arena* arena, const char* fmt, ...);
static inline void  ArenaPop(Arena* arena, void* ptr);
static inline void* ArenaEndAligned(Arena* arena, uintsize alignment);
static inline void  ArenaClear(Arena* arena);
static inline void* ArenaEnd(Arena* arena);
static inline ArenaSavepoint ArenaSave(Arena* arena);
static inline void           ArenaRestore(ArenaSavepoint savepoint);

static inline FORCE_INLINE uint64 HashString(String memory);
static inline FORCE_INLINE uint32 HashInt32(uint32 x);
static inline FORCE_INLINE uint64 HashInt64(uint64 x);
static inline FORCE_INLINE int32  HashMsi(uint32 log2_of_cap, uint64 hash, int32 index);

static inline Allocator AllocatorFromArena(Arena* arena);
static inline Allocator NullAllocator(void);
static inline bool IsNullAllocator(Allocator allocator);

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
MemoryZeroSafe(void* restrict dst, uintsize size)
{
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
MemoryCopy(void* restrict dst, const void* restrict src, uintsize size)
{ Trace(); return memcpy(dst, src, size); }

static inline FORCE_INLINE void*
MemoryMove(void* dst, const void* src, uintsize size)
{ Trace(); return memmove(dst, src, size); }

static inline FORCE_INLINE void*
MemorySet(void* restrict dst, uint8 byte, uintsize size)
{ Trace(); return memset(dst, byte, size); }

static inline FORCE_INLINE int32
MemoryCompare(const void* left_, const void* right_, uintsize size)
{ Trace(); return memcmp(left_, right_, size); }

static inline uintsize
MemoryStrlen(const char* restrict cstr)
{ Trace(); return strlen(cstr); }

static inline uintsize
MemoryStrnlen(const char* restrict cstr, uintsize limit)
{ Trace(); return strnlen(cstr, limit); }

static inline int32
MemoryStrcmp(const char* left, const char* right)
{ Trace(); return strcmp(left, right); }

static inline int32
MemoryStrncmp(const char* left, const char* right, uintsize limit)
{ Trace(); return strncmp(left, right, limit); }

static inline char*
MemoryStrstr(const char* left, const char* right)
{ Trace(); return (char*)strstr(left, right); }

static inline const void*
MemoryFindByte(const void* buffer, uint8 byte, uintsize size)
{ Trace(); return memchr(buffer, byte, size); }

static inline FORCE_INLINE void*
MemoryZero(void* restrict dst, uintsize size)
{ Trace(); return memset(dst, 0, size); }

static inline char*
MemoryStrnstr(const char* left, const char* right, uintsize limit)
{
	Trace();
	
	for (; *left && limit; ++left)
	{
		const char* it_left = left;
		const char* it_right = right;
		uintsize local_limit = limit--;
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

static inline uint32
StringDecode(String str, intsize* index)
{
	const uint8* head = str.data + *index;
	const uint8* const end = str.data + str.size;
	
	if (head >= end)
		return 0;
	
	uint8 byte = *head++;
	if (!byte || byte == 0xff)
		return 0;
	
	int32 size =  BitClz8(~byte);
	if (Unlikely(size == 1 || size > 4 || head + size - 1 > end))
		return 0;
	
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
	return result;
}

static inline uint32
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

static inline uintsize
StringEncode(uint8* buffer, uintsize size, uint32 codepoint)
{
	uint32 needed = StringEncodedCodepointSize(codepoint);
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
static inline uintsize
StringDecodedLength(String str)
{
	uintsize len = 0;
	
	intsize it = 0;
	while (StringDecode(str, &it))
		++len;
	
	return len;
}

static inline int32
StringCompare(String a, String b)
{
	int32 result = MemoryCompare(a.data, b.data, Min(a.size, b.size));
	
	if (result == 0 && a.size != b.size)
		return (a.size > b.size) ? 1 : -1;
	
	return result;
}

static inline bool
StringEquals(String a, String b)
{
	if (a.size != b.size)
		return false;
	
	return MemoryCompare(a.data, b.data, a.size) == 0;
}

static inline bool
StringEndsWith(String check, String s)
{
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
	if (s.size > check.size)
		return false;
	
	return MemoryCompare(check.data, s.data, s.size) == 0;
}

static inline String
StringSubstr(String str, intsize index, intsize size)
{
	if (index >= str.size)
		return StrNull;
	
	str.data += index;
	str.size -= index;
	
	if (size < 0)
		size = str.size + size + 1;
	if (size < 0)
		str.size = 0;
	else if (size < str.size)
		str.size = size;
	
	return str;
}

static inline String
StringFromCString(char const* cstr)
{
	return StrMake(MemoryStrlen(cstr), cstr);
}

static inline String
StringSlice(String str, intsize begin, intsize end)
{
	if (begin < 0)
		begin = (intsize)str.size + begin + 1;
	if (end < 0)
		end = (intsize)str.size + end + 1;
	if (begin >= end)
		return StrNull;
	
	begin = ClampMax(begin, (intsize)str.size);
	end   = ClampMax(end,   (intsize)str.size);
	
	str.data += begin;
	str.size = end - begin;
	return str;
}

static inline String
StringSliceEnd(String str, intsize count)
{
	count = ClampMax(count, (intsize)str.size);
	str.data += (intsize)str.size - count;
	str.size = count;
	return str;
}

static inline intsize
StringIndexOf(String str, uint8 ch, intsize start_index)
{
	if (start_index < 0)
		start_index = 0;
	if (start_index >= (intsize)str.size)
		return -1;
	if (!str.size)
		return -1;
	
	uint8 const* data = str.data + start_index;
	uintsize size = str.size - start_index;
	uint8 const* found_ch = (uint8 const*)MemoryFindByte(data, ch, size);
	if (found_ch)
		return found_ch - data;
	return -1;
}

static inline intsize
StringIndexOfSubstr(String str, String substr, intsize start_index)
{
	if (start_index < 0)
		start_index = 0;
	if (start_index >= (intsize)str.size)
		return -1;
	if (substr.size > str.size)
		return -1;
	if (!substr.size || !str.size)
		return -1;
	
	uint8 const* data = str.data;
	uintsize size = str.size;
	uintsize min_allowed_size = size - substr.size;
	
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
	if (!buf.size)
		return StrNull;

	uint8 const* zero = (uint8 const*)MemoryFindByte(buf.data, 0, buf.size);
	String result = buf;
	if (zero)
		result.size = zero - buf.data;
	return result;
}

static inline FORCE_INLINE uintsize StringPrintfFunc_(char* buf, uintsize buf_size, const char* restrict fmt, va_list args);

static inline uintsize
StringVPrintfBuffer(char* buf, uintsize len, const char* fmt, va_list args)
{
	Assert(buf && len);
	
	return StringPrintfFunc_(buf, len, fmt, args);
}

static inline uintsize
StringPrintfBuffer(char* buf, uintsize len, const char* fmt, ...)
{
	Assert(buf && len);
	
	va_list args;
	va_start(args, fmt);
	
	uintsize result = StringPrintfFunc_(buf, len, fmt, args);
	
	va_end(args);
	return result;
}

static inline String
StringVPrintf(char* buf, uintsize len, const char* fmt, va_list args)
{
	Assert(buf && len);
	
	String result = {
		(uint8*)buf,
		StringPrintfFunc_(buf, len, fmt, args),
	};
	
	return result;
}

static inline String
StringPrintf(char* buf, uintsize len, const char* fmt, ...)
{
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

static inline uintsize
StringVPrintfSize(const char* fmt, va_list args)
{
	return StringPrintfFunc_(NULL, 0, fmt, args);
}

static inline uintsize
StringPrintfSize(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	uintsize result = StringPrintfFunc_(NULL, 0, fmt, args);
	
	va_end(args);
	return result;
}

//- NOTE(ljre): Actual implementation for snprintf alternative.
#define String_STDSP_SPECIAL 0x7000
static inline int32 String_stbsp__real_to_str(char const** start, uint32* len, char* out, int32* decimal_pos, float64 value, uint32 frac_digits);

static inline FORCE_INLINE void
StringWriteBuf_(char** p, char* end, uintsize* count, const char* restrict buf, intsize bufsize)
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
StringFillBuf_(char** p, char* end, uintsize* count, char fill, intsize fillsize)
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

static inline FORCE_INLINE uintsize
StringPrintfFunc_(char* buf, uintsize buf_size, const char* restrict fmt, va_list args)
{
	uintsize count = 0;
	const char* fmt_end = fmt + MemoryStrlen(fmt);
	
	SafeAssert(!buf ? buf_size == 0 : true);
	
	char* p = buf;
	char* p_end = buf + buf_size;
	
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
		
		if (*fmt >= '0' && *fmt <= '9')
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
		intsize write_count = 0;
		
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
				
				write_count = sizeof(tmpbuf) - index;
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
				
				write_count = sizeof(tmpbuf) - index;
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
				
				write_count = sizeof(tmpbuf) - index;
				write_buf = tmpbuf + index;
			} break;
			
			case 's':
			{
				const char* arg = va_arg(args, const char*);
				uintsize len;
				
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
				uint32 length;
				int32 decimal_pos;
				
				static_assert(sizeof(tmpbuf) > 64 + 32, "stbsp__real_to_str takes just the final 64 bytes of the buffer because we might need to add a prefix to the string");
				
				bool negative = String_stbsp__real_to_str((const char**)&start, &length, tmpbuf+sizeof(tmpbuf)-64, &decimal_pos, arg, trailling_padding);
				
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
		*start = (bits & ((((uint64)1) << 52) - 1)) ? "NaN" : "Inf";
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
	frac_digits = (frac_digits & 0x80000000) ? ((frac_digits & 0x7ffffff) + 1) : (tens + frac_digits);
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
			e = dg - frac_digits;
			if ((uint32)e >= 24)
				goto noround;
			r = stbsp__powten[e];
			bits = bits + (r / 2);
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
	*len = e;
	return ng;
}

#undef String_STDSP_SPECIAL

static inline Arena
ArenaFromMemory(void* memory, uintsize size)
{
	Assert(((uintptr)memory & ~(CONFIG_ARENA_DEFAULT_ALIGNMENT-1)) == (uintptr)memory);
	
	Arena result = {
		.size = size,
		.offset = 0,
		.memory = (uint8*)memory,
	};
	
	return result;
}

static inline void*
ArenaEndAligned(Arena* arena, uintsize alignment)
{
	Assert(alignment != 0 && IsPowerOf2(alignment));
	
	uintptr target_offset = AlignUp((uintptr)arena->memory + arena->offset, alignment-1) - (uintptr)arena->memory;
	arena->offset = target_offset;
	return arena->memory + arena->offset;
}

static inline void*
ArenaPushDirtyAligned(Arena* arena, uintsize size, uintsize alignment)
{
	Assert(alignment != 0 && IsPowerOf2(alignment));
	SafeAssert((intsize)size > 0);
	
	uintptr target_offset = AlignUp((uintptr)arena->memory + arena->offset, alignment-1) - (uintptr)arena->memory;
	uintsize needed = target_offset + size;
	
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
	
	uintsize new_offset = p - arena->memory;
	arena->offset = new_offset;
}

static inline String
ArenaVPrintf(Arena* arena, char const* fmt, va_list args)
{
	va_list args2;
	va_copy(args2, args);
	
	uintsize size = StringVPrintfSize(fmt, args2);
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
ArenaPushDirty(Arena* arena, uintsize size)
{ return ArenaPushDirtyAligned(arena, size, CONFIG_ARENA_DEFAULT_ALIGNMENT); }

static inline void*
ArenaPushAligned(Arena* arena, uintsize size, uintsize alignment)
{
	void* data = ArenaPushDirtyAligned(arena, size, alignment);
	if (data)
		MemoryZero(data, size);
	return data;
}

static inline void*
ArenaPush(Arena* arena, uintsize size)
{
	void* data = ArenaPushDirtyAligned(arena, size, CONFIG_ARENA_DEFAULT_ALIGNMENT);
	if (data)
		MemoryZero(data, size);
	return data;
}

static inline void*
ArenaPushMemory(Arena* arena, const void* buf, uintsize size)
{
	void* data = ArenaPushDirtyAligned(arena, size, 1);
	if (data)
		MemoryCopy(data, buf, size);
	return data;
}

static inline void*
ArenaPushMemoryAligned(Arena* arena, const void* buf, uintsize size, uintsize alignment)
{
	void* data = ArenaPushDirtyAligned(arena, size, alignment);
	if (data)
		MemoryCopy(data, buf, size);
	return data;
}

static inline String
ArenaPushString(Arena* arena, String str)
{
	void* data = ArenaPushDirtyAligned(arena, str.size, 1);
	String result = { 0 };
	if (data)
		result = StrMake(str.size, MemoryCopy(data, str.data, str.size));
	return result;
}

static inline String
ArenaPushStringAligned(Arena* arena, String str, uintsize alignment)
{
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
ArenaAllocatorProc(Allocator* allocator, AllocatorMode mode, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	Trace();
	void* result = NULL;
	AllocatorError error = AllocatorError_Ok;
	Arena* arena = (Arena*)allocator->instance;

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

static void*
NullAllocatorProc(Allocator* allocator, AllocatorMode mode, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	if (out_err)
		*out_err = AllocatorError_OutOfMemory;
	return NULL;
}

static inline Allocator
NullAllocator(void)
{
	return (Allocator) { NullAllocatorProc };
}

static inline bool
IsNullAllocator(Allocator allocator)
{
	return allocator.proc == NullAllocatorProc;
}

static inline void*
AllocatorAlloc(Allocator* allocator, intsize size, intsize alignment, AllocatorError* out_err)
{
	return allocator->proc(allocator, AllocatorMode_Alloc, size, alignment, NULL, 0, out_err);
}

static inline void*
AllocatorResize(Allocator* allocator, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	return allocator->proc(allocator, AllocatorMode_Resize, size, alignment, old_ptr, old_size, out_err);
}

static inline void*
AllocatorAllocNonZeroed(Allocator* allocator, intsize size, intsize alignment, AllocatorError* out_err)
{
	return allocator->proc(allocator, AllocatorMode_AllocNonZeroed, size, alignment, NULL, 0, out_err);
}

static inline void*
AllocatorResizeNonZeroed(Allocator* allocator, intsize size, intsize alignment, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	return allocator->proc(allocator, AllocatorMode_ResizeNonZeroed, size, alignment, old_ptr, old_size, out_err);
}

static inline void
AllocatorFree(Allocator* allocator, void* old_ptr, intsize old_size, AllocatorError* out_err)
{
	allocator->proc(allocator, AllocatorMode_Free, 0, 0, old_ptr, old_size, out_err);
}

static inline void
AllocatorFreeAll(Allocator* allocator, AllocatorError* out_err)
{
	allocator->proc(allocator, AllocatorMode_FreeAll, 0, 0, NULL, 0, out_err);
}

static inline void
AllocatorPop(Allocator* allocator, void* old_ptr, AllocatorError* out_err)
{
	allocator->proc(allocator, AllocatorMode_Pop, 0, 0, old_ptr, 0, out_err);
}

static inline bool
AllocatorResizeOk(Allocator* allocator, intsize size, intsize alignment, void* inout_ptr, intsize old_size, AllocatorError* out_err)
{
	void* new_ptr = allocator->proc(allocator, AllocatorMode_Resize, size, alignment, *(void**)inout_ptr, old_size, out_err);
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
	void* new_ptr = allocator->proc(allocator, AllocatorMode_ResizeNonZeroed, size, alignment, *(void**)inout_ptr, old_size, out_err);
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
	return allocator->proc(allocator, AllocatorMode_Alloc, count*size, alignment, NULL, 0, out_err);
}

static inline void*
AllocatorResizeArray(Allocator* allocator, intsize count, intsize size, intsize alignment, void* old_ptr, intsize old_count, AllocatorError* out_err)
{
	SafeAssert(!size || (count >= 0 && count <= INTSIZE_MAX / size && old_count >= 0 && old_count <= INTSIZE_MAX / size));
	return allocator->proc(allocator, AllocatorMode_Resize, count*size, alignment, old_ptr, old_count*size, out_err);
}

static inline bool
AllocatorResizeArrayOk(Allocator* allocator, intsize count, intsize size, intsize alignment, void* inout_ptr, intsize old_count, AllocatorError* out_err)
{
	SafeAssert(!size || (count >= 0 && count <= INTSIZE_MAX / size && old_count >= 0 && old_count <= INTSIZE_MAX / size));
	void* new_ptr = allocator->proc(allocator, AllocatorMode_Resize, count*size, alignment, *(void**)inout_ptr, old_count*size, out_err);
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
	allocator->proc(allocator, AllocatorMode_Free, 0, 0, old_ptr, old_count*size, out_err);
}

// NOTE(ljre): FNV-1a implementation.
static inline FORCE_INLINE uint64
HashString(String memory)
{
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
static inline FORCE_INLINE int32
HashMsi(uint32 log2_of_cap, uint64 hash, int32 index)
{
	uint32 exp = log2_of_cap;
	uint32 mask = (1u << exp) - 1;
	uint32 step = (uint32)(hash >> (64 - exp)) | 1;
	return (index + step) & mask;
}

#endif //COMMON_H
