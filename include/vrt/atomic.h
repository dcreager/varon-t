/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_ATOMIC
#define VRT_ATOMIC

#include <libcork/core.h>
#include <libcork/threads.h>


/*
 * __sync_synchronize doesn't emit a memory fence instruction in GCC <=
 * 4.3.  It also implements a full memory barrier.  So, on x86_64, which
 * has separate lfence and sfence instructions, we use hard-coded
 * assembly, regardless of GCC version.  On i386, we can't guarantee
 * that lfence/sfence are available, so we have to use a full memory
 * barrier anyway.  We use the GCC intrinsics if we can; otherwise, we
 * fall back on assembly.
 *
 * [1] http://gcc.gnu.org/bugzilla/show_bug.cgi?id=36793
 * */

#if defined(__GNUC__) && defined(__x86_64__)

CORK_ATTR_UNUSED
static inline void
vrt_atomic_read_barrier(void)
{
    __asm__ __volatile__ ("lfence" ::: "memory");
}

CORK_ATTR_UNUSED
static inline void
vrt_atomic_write_barrier(void)
{
    __asm__ __volatile__ ("sfence" ::: "memory");
}

#elif (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) > 40300

CORK_ATTR_UNUSED
static inline void
vrt_atomic_read_barrier(void)
{
    __sync_synchronize();
}

CORK_ATTR_UNUSED
static inline void
vrt_atomic_write_barrier(void)
{
    __sync_synchronize();
}

#elif defined(__GNUC__) && defined(__i386__)

CORK_ATTR_UNUSED
static inline void
vrt_atomic_read_barrier(void)
{
    int  a = 0;
    __asm__ __volatile__ ("lock orl $0, %0" : "+m" (a));
}

CORK_ATTR_UNUSED
static inline void
vrt_atomic_write_barrier(void)
{
    int  a = 0;
    __asm__ __volatile__ ("lock orl $0, %0" : "+m" (a));
}

#else
#error "No memory barrier implementation!"
#endif


/*-----------------------------------------------------------------------
 * Padded values
 */

/* An int that's guaranteed to be padded to at least a 64-byte cache line. */
struct vrt_padded_int {
    char  __pad0[64 - sizeof(int)];
    volatile int  value;
    char  __pad1[64 - sizeof(int)];
};

CORK_ATTR_UNUSED
static inline int
vrt_padded_int_get(struct vrt_padded_int *padded)
{
    /* We need a read barrier before reading */
    vrt_atomic_read_barrier();
    return padded->value;
}

CORK_ATTR_UNUSED
static inline void
vrt_padded_int_set(struct vrt_padded_int *padded, int v)
{
    /* We need a write barrier after writing */
    padded->value = v;
    vrt_atomic_write_barrier();
}

CORK_ATTR_UNUSED
static inline int
vrt_padded_int_atomic_add(struct vrt_padded_int *padded, int delta)
{
    /* The atomic instruction includes a memory barrier already */
    return cork_int_atomic_add(&padded->value, delta);
}


#endif /* VRT_ATOMIC */
