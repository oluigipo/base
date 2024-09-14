#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

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
#define SignedSizeof(x) ((intz)sizeof(x))

#if defined(__clang__)
#	define Assume(...) __builtin_assume(__VA_ARGS__)
#	define Debugbreak() __builtin_debugtrap()
#	define Trap() __builtin_trap()
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
#	define Trap() __debugbreak()
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
#	define Trap() __builtin_trap()
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

struct Buffer
{
	uint8 const* data;
	intz size;
}
typedef Buffer;

typedef Buffer String;

struct Arena typedef Arena;
typedef bool ArenaCommitMemoryProc(Arena* arena, intz needed_size);

struct Arena
{
	intz size;
	intz offset;
	uint8* memory;
	
	// NOTE(ljre): Commit-as-you-go arena style.
	//             commit_memory_proc() function should return true if the specified needed_size is commited
	intz reserved;
	ArenaCommitMemoryProc* commit_memory_proc;
};

struct ArenaSavepoint
{
	Arena* arena;
	intz offset;
}
typedef ArenaSavepoint;

enum AllocatorMode
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
}
typedef AllocatorMode;

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
typedef void* AllocatorProc(void* instance, AllocatorMode mode, intz size, intz alignment, void* old_ptr, intz old_size, AllocatorError* out_err);
struct Allocator
{
	AllocatorProc* proc;
	void* instance;
};

Allocator typedef SingleAllocator;         // Alloc; single allocation
Allocator typedef SingleResizingAllocator; // Alloc, Resize; single allocation
Allocator typedef MultiAllocator;          // Alloc
Allocator typedef MultiResizingAllocator;  // Alloc, Resize, Free
Allocator typedef ArenaAllocator;          // Alloc, Pop

enum
{
	LOG_DEBUG = 0,
	LOG_INFO = 10,
	LOG_WARN = 20,
	LOG_ERROR = 30,
	LOG_FATAL = 40,
};

struct ThreadContextLogger typedef ThreadContextLogger;
typedef void ThreadContextLoggerProc(ThreadContextLogger* logger, int32 level, String string);

struct ThreadContextLogger
{
	ThreadContextLoggerProc* proc;
	void* user_data;
	int32 minimum_level;
};

typedef void ThreadContextAssertionFailureProc(String expr, String func, String file, int32 line);

struct ThreadContext
{
	Arena scratch[2]; // 4 MiB each
	ThreadContextLogger logger;
	ThreadContextAssertionFailureProc* assertion_failure_proc;
}
typedef ThreadContext;

#define INTSIZE_MAX PTRDIFF_MAX
#define INTSIZE_MIN PTRDIFF_MIN
#define UINTSIZE_MAX SIZE_MAX
#define INTZ_MAX PTRDIFF_MAX
#define INTZ_MIN PTRDIFF_MIN
#define UINTZ_MAX SIZE_MAX

#endif //COMMON_CONFIG_H