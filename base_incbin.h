#ifndef LJRE_BASE_INCBIN_H
#define LJRE_BASE_INCBIN_H

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

#endif // LJRE_BASE_INCBIN_H
