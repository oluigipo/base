#ifndef LJRE_BASE_CHECKED_H
#define LJRE_BASE_CHECKED_H

#include "base.h"

static inline bool
CheckedAddI64(int64* out, int64 lhs, int64 rhs)
{
	return !__builtin_add_overflow(lhs, rhs, out);
}

static inline bool
CheckedAddU64(uint64* out, uint64 lhs, uint64 rhs)
{
	return !__builtin_add_overflow(lhs, rhs, out);
}

static inline bool
CheckedAddI32(int32* out, int32 lhs, int32 rhs)
{
	return !__builtin_add_overflow(lhs, rhs, out);
}

static inline bool
CheckedAddU32(uint32* out, uint32 lhs, uint32 rhs)
{
	return !__builtin_add_overflow(lhs, rhs, out);
}

static inline bool
CheckedSubI64(int64* out, int64 lhs, int64 rhs)
{
	return !__builtin_sub_overflow(lhs, rhs, out);
}

static inline bool
CheckedSubI32(int32* out, int32 lhs, int32 rhs)
{
	return !__builtin_sub_overflow(lhs, rhs, out);
}

static inline bool
CheckedMulI64(int64* out, int64 lhs, int64 rhs)
{
	return !__builtin_mul_overflow(lhs, rhs, out);
}

static inline bool
CheckedMulI32(int32* out, int32 lhs, int32 rhs)
{
	return !__builtin_mul_overflow(lhs, rhs, out);
}

#endif //LJRE_BASE_CHECKED_H
