#ifndef LJRE_BASE_H
#define LJRE_BASE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#ifndef API
#	define API EXTERN_C
#endif

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

#define AlignUp(x, mask) (((x) + (mask)) & ~(mask))
#define AlignDown(x, mask) ((x) & ~(mask))
#define IsPowerOf2(x) ( ((x) & (x)-1) == 0 )
#define ArrayLength(x) ((intz)(sizeof(x) / sizeof(x)[0]))
#define Min(x,y) ((x) < (y) ? (x) : (y))
#define Max(x,y) ((x) > (y) ? (x) : (y))
#define ClampMax Min
#define ClampMin Max
#define Clamp(x,min,max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#define SignedSizeof(x) ((intz)sizeof(x))

#ifndef Trace
#	define Trace() ((void)0)
#	define TraceName(...) ((void)0)
#	define TraceText(...) ((void)0)
#	define TraceColor(...) ((void)0)
#	define TraceF(sz, ...) ((void)0)
#	define TraceFrameBegin() ((void)0)
#	define TraceFrameEnd() ((void)0)
#	define TraceInit() ((void)0)
#	define TraceDeinit() ((void)0)
#endif

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

typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef uint64_t  uint64;
typedef uintptr_t uintptr;
typedef size_t    uintz;
typedef int8_t    int8;
typedef int16_t   int16;
typedef int32_t   int32;
typedef int64_t   int64;
typedef intptr_t  intptr;
typedef ptrdiff_t intz;
typedef float     float32;
typedef double    float64;

#define INTZ_MAX PTRDIFF_MAX
#define INTZ_MIN PTRDIFF_MIN
#define UINTZ_MAX SIZE_MAX

struct Buffer
{
	uint8 const* data;
	intz size;
}
typedef Buffer, String;

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
typedef void ThreadContextLoggerProc(ThreadContextLogger* logger, int32 level, char const* fmt, va_list args);

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

EXTERN_C thread_local ThreadContext g_thread_context_;

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
	thread_ctx->logger.proc(&thread_ctx->logger, level, fmt, args);
	va_end(args);
}

NO_RETURN static inline void FORCE_NOINLINE
AssertionFailure(char const* expr, char const* func, char const* file, int32 line)
{
	ThreadContext* thread_ctx = ThisThreadContext();
	if (thread_ctx->assertion_failure_proc)
	{
		String expr_str = { (uint8 const*)expr };
        for (; *expr++; ++expr_str.size);
		String func_str = { (uint8 const*)func };
        for (; *func++; ++func_str.size);
		String file_str = { (uint8 const*)file };
        for (; *file++; ++file_str.size);
		thread_ctx->assertion_failure_proc(expr_str, func_str, file_str, line);
	}
	Trap();
}

// ===========================================================================
// ===========================================================================
// Buffers and strings

#ifndef __cplusplus
#	define Buf(x) (Buffer) BufInit(x)
#	define BUFNULL (Buffer) { 0 }
#	define BufMake(size, data) (Buffer) { (const uint8*)(data), (intz)(size) }
#	define BufRange(begin, end) (Buffer) BufInitRange(begin, end)
#	define Str(x) (String) StrInit(x)
#	define STRNULL (String) { 0 }
#	define StrMake(size,data) (String) { (const uint8*)(data), (intz)(size) }
#	define StrRange(begin, end) (String) StrInitRange(begin, end)
#else //__cplusplus
#	define Buf(x) Buffer BufInit(x)
#	define BUFNULL Buffer {}
#	define BufMake(size, data) Buffer { (const uint8*)(data), (intz)(size) }
#	define BufRange(begin, end) Buffer BufInitRange(begin, end)
#	define Str(x) String StrInit(x)
#	define STRNULL String {}
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

#ifdef __cplusplus
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
		SafeAssert(count <= INTZ_MAX / sizeof(T));
		return { (uint8 const*)data, count * SignedSizeof(T) };
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

static inline Slice<uint8 const>
SliceFromBuffer(Buffer buf)
{
	return { buf.data, buf.size };
}

template <typename T>
static inline Slice<T const>
SliceFromTypedBuffer(Buffer buf)
{
	return {
		buf.data,
		buf.size / SignedSizeof(T),
	};
}
#endif

#endif