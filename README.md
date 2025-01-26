## base foundation
This is my base layer, a "personal standard library" per se.
The basic ideas in it are:
- Null-terminated strings should be avoided whenever possible, always use counted strings;
- Subscripts (indices, sizes, counts) should be signed, _not_ unsigned;
- Arenas trivialize a lot of allocation patterns, specially temporary dynamic allocations;
- String functions should almost always return empty slices into the input string instead of returning `STRNULL`. This avoids UBs when doing pointer arithmetic;
- Errors are returned as the optional, last output parameter. If passed `NULL`, then it traps on error;
- `MemoryCopy()` and friends are just like `memcpy()` and others, except they check for `size == 0` and early exit (because UB);
- A `ThreadContext` global for general temporary allocation, logging, and panic handling. All configurable;
- My own `printf` replacement to support all those custom data types;
