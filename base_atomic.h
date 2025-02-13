#ifndef LJRE_BASE_ATOMIC_H
#define LJRE_BASE_ATOMIC_H

#include "base.h"

#if defined(__clang__) || defined(__GNUC__)

#define _DEFINE_ATOMIC_PROCS(op, X) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32 X(int32, int32, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64 X(int64, int64, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void* Atomic ## op ## Ptr X(void*, intptr, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32Acq X(int32, int32, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64Acq X(int64, int64, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE void* Atomic ## op ## PtrAcq X(void*, intptr, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32Rel X(int32, int32, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64Rel X(int64, int64, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE void* Atomic ## op ## PtrRel X(void*, intptr, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32AcqRel X(int32, int32, __ATOMIC_ACQ_REL) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64AcqRel X(int64, int64, __ATOMIC_ACQ_REL) \
	static inline FORCE_INLINE void* Atomic ## op ## PtrAcqRel X(void*, intptr, __ATOMIC_ACQ_REL) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32Relaxed X(int32, int32, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64Relaxed X(int64, int64, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE void* Atomic ## op ## PtrRelaxed X(void*, intptr, __ATOMIC_RELAXED)
#define _DEFINE_ATOMIC_PROCS_WITHOUT_RELEASE(op, X) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32 X(int32, int32, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64 X(int64, int64, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void* Atomic ## op ## Ptr X(void*, intptr, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32Acq X(int32, int32, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64Acq X(int64, int64, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE void* Atomic ## op ## PtrAcq X(void*, intptr, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE int32 Atomic ## op ## 32Relaxed X(int32, int32, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE int64 Atomic ## op ## 64Relaxed X(int64, int64, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE void* Atomic ## op ## PtrRelaxed X(void*, intptr, __ATOMIC_RELAXED)

#define _DEFINE_ATOMIC_PROCS_RETURNING_VOID(op, X) \
	static inline FORCE_INLINE void Atomic ## op ## 32 X(int32, int32, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void Atomic ## op ## 64 X(int64, int64, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void Atomic ## op ## Ptr X(void*, intptr, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void Atomic ## op ## 32Acq X(int32, int32, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE void Atomic ## op ## 64Acq X(int64, int64, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE void Atomic ## op ## PtrAcq X(void*, intptr, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE void Atomic ## op ## 32Rel X(int32, int32, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE void Atomic ## op ## 64Rel X(int64, int64, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE void Atomic ## op ## PtrRel X(void*, intptr, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE void Atomic ## op ## 32AcqRel X(int32, int32, __ATOMIC_ACQ_REL) \
	static inline FORCE_INLINE void Atomic ## op ## 64AcqRel X(int64, int64, __ATOMIC_ACQ_REL) \
	static inline FORCE_INLINE void Atomic ## op ## PtrAcqRel X(void*, intptr, __ATOMIC_ACQ_REL) \
	static inline FORCE_INLINE void Atomic ## op ## 32Relaxed X(int32, int32, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE void Atomic ## op ## 64Relaxed X(int64, int64, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE void Atomic ## op ## PtrRelaxed X(void*, intptr, __ATOMIC_RELAXED)
#define _DEFINE_ATOMIC_PROCS_RETURNING_VOID_WITHOUT_ACQUIRE(op, X) \
	static inline FORCE_INLINE void Atomic ## op ## 32 X(int32, int32, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void Atomic ## op ## 64 X(int64, int64, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void Atomic ## op ## Ptr X(void*, intptr, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE void Atomic ## op ## 32Rel X(int32, int32, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE void Atomic ## op ## 64Rel X(int64, int64, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE void Atomic ## op ## PtrRel X(void*, intptr, __ATOMIC_RELEASE) \
	static inline FORCE_INLINE void Atomic ## op ## 32Relaxed X(int32, int32, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE void Atomic ## op ## 64Relaxed X(int64, int64, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE void Atomic ## op ## PtrRelaxed X(void*, intptr, __ATOMIC_RELAXED)

#define _DEFINE_ATOMIC_PROCS_RETURNING_BOOL_WITH_TWO_ORDERINGS(op, X) \
	static inline FORCE_INLINE bool Atomic ## op ## 32 X(int32, int32, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE bool Atomic ## op ## 64 X(int64, int64, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE bool Atomic ## op ## Ptr X(void*, intptr, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) \
	static inline FORCE_INLINE bool Atomic ## op ## 32Acq X(int32, int32, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## 64Acq X(int64, int64, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## PtrAcq X(void*, intptr, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## 32Rel X(int32, int32, __ATOMIC_RELEASE, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE bool Atomic ## op ## 64Rel X(int64, int64, __ATOMIC_RELEASE, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE bool Atomic ## op ## PtrRel X(void*, intptr, __ATOMIC_RELEASE, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE bool Atomic ## op ## 32RelOrAcq X(int32, int32, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## 64RelOrAcq X(int64, int64, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## PtrRelOrAcq X(void*, intptr, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## 32AcqRel X(int32, int32, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## 64AcqRel X(int64, int64, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## PtrAcqRel X(void*, intptr, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) \
	static inline FORCE_INLINE bool Atomic ## op ## 32Relaxed X(int32, int32, __ATOMIC_RELAXED, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE bool Atomic ## op ## 64Relaxed X(int64, int64, __ATOMIC_RELAXED, __ATOMIC_RELAXED) \
	static inline FORCE_INLINE bool Atomic ## op ## PtrRelaxed X(void*, intptr, __ATOMIC_RELAXED, __ATOMIC_RELAXED)

#define X_LOAD(Type, ArithType, ordering)             (void* ptr) { return __atomic_load_n((Type*)ptr, ordering); }
#define X_STORE(Type, ArithType, ordering)            (void* ptr, Type x) { __atomic_store_n((Type*)ptr, x, ordering); }
#define X_EXCHANGE(Type, ArithType, ordering)         (void* ptr, Type x) { return __atomic_exchange_n((Type*)ptr, x, ordering); }
#define X_COMPARE_EXCHANGE(Type, ArithType, ordering1, ordering2) (void* ptr, Type* expected, Type x) { return __atomic_compare_exchange_n((Type*)ptr, expected, x, false, ordering1, ordering2); }
#define X_ADD_FETCH(Type, ArithType, ordering)        (void* ptr, Type x) { return __atomic_add_fetch((Type*)ptr, (ArithType)x, ordering); }
#define X_SUB_FETCH(Type, ArithType, ordering)        (void* ptr, Type x) { return __atomic_sub_fetch((Type*)ptr, (ArithType)x, ordering); }
#define X_AND_FETCH(Type, ArithType, ordering)        (void* ptr, Type x) { return (Type)__atomic_and_fetch((ArithType*)ptr, (ArithType)x, ordering); }
#define X_OR_FETCH(Type, ArithType, ordering)         (void* ptr, Type x) { return (Type)__atomic_or_fetch((ArithType*)ptr, (ArithType)x, ordering); }
#define X_XOR_FETCH(Type, ArithType, ordering)        (void* ptr, Type x) { return (Type)__atomic_xor_fetch((ArithType*)ptr, (ArithType)x, ordering); }
#define X_INC(Type, ArithType, ordering)              (void* ptr) { return (Type)__atomic_add_fetch((Type*)ptr, (ArithType)1, ordering); }
#define X_DEC(Type, ArithType, ordering)              (void* ptr) { return (Type)__atomic_sub_fetch((Type*)ptr, (ArithType)1, ordering); }

_DEFINE_ATOMIC_PROCS_WITHOUT_RELEASE(Load, X_LOAD);
_DEFINE_ATOMIC_PROCS_RETURNING_VOID_WITHOUT_ACQUIRE(Store, X_STORE);
_DEFINE_ATOMIC_PROCS(Exchange, X_EXCHANGE);
_DEFINE_ATOMIC_PROCS_RETURNING_BOOL_WITH_TWO_ORDERINGS(CompareExchange, X_COMPARE_EXCHANGE);
_DEFINE_ATOMIC_PROCS(AddFetch, X_ADD_FETCH);
_DEFINE_ATOMIC_PROCS(SubFetch, X_SUB_FETCH);
_DEFINE_ATOMIC_PROCS(AndFetch, X_AND_FETCH);
_DEFINE_ATOMIC_PROCS(OrFetch, X_OR_FETCH);
_DEFINE_ATOMIC_PROCS(XorFetch, X_XOR_FETCH);
_DEFINE_ATOMIC_PROCS(Inc, X_INC);
_DEFINE_ATOMIC_PROCS(Dec, X_DEC);

#undef X_LOAD
#undef X_STORE
#undef X_EXCHANGE
#undef X_COMPARE_EXCHANGE
#undef X_ADD_FETCH
#undef X_SUB_FETCH
#undef X_AND_FETCH
#undef X_OR_FETCH
#undef X_XOR_FETCH
#undef X_INC
#undef X_DEC

#elif defined(_MSC_VER)

// TODO(ljre)

#endif

#endif