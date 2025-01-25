#ifndef LJRE_BASE_ASSERT_H
#define LJRE_BASE_ASSERT_H

#include "base.h"

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

#endif //LJRE_BASE_ASSERT_H
