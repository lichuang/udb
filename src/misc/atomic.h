#ifndef _UDB_ATOMIC_H_
#define _UDB_ATOMIC_H_

#ifndef __has_extension
#define __has_extension(x) 0 /* compatibility with non-clang compilers */
#endif
#if GCC_VERSION >= 4007000 ||                                                  \
    (__has_extension(c_atomic) && __has_extension(c_atomic_store_n))
#define AtomicLoad(PTR) __atomic_load_n((PTR), __ATOMIC_RELAXED)
#define AtomicStore(PTR, VAL) __atomic_store_n((PTR), (VAL), __ATOMIC_RELAXED)
#else
#define AtomicLoad(PTR) (*(PTR))
#define AtomicStore(PTR, VAL) (*(PTR) = (VAL))
#endif

#endif /* _UDB_ATOMIC_H_ */