/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012-2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <unistd.h>

#include <libcork/core.h>

#include "vrt/yield.h"


#ifndef VRT_DEBUG_YIELD
#define VRT_DEBUG_YIELD 0
#endif
#if VRT_DEBUG_YIELD
#include <stdio.h>
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG(...) /* do nothing */
#endif


#define SPIN_COUNT_BEFORE_YIELDING  100


#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define PAUSE() \
        __asm__ __volatile__ ("pause");
#else
#define PAUSE()  /* do nothing */
#endif

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#if defined(__APPLE__)
#include <pthread.h>
#define THREAD_YIELD   pthread_yield_np
#elif defined(__linux__) || defined(BSD)
#include <sched.h>
#define THREAD_YIELD   sched_yield
#else
#error "Unknown hybrid yield implementation"
#endif


/*-----------------------------------------------------------------------
 * Thread yielding strategy
 */

struct vrt_thread_yield_strategy {
    struct vrt_yield_strategy  parent;
    int  counter;
};

static void
vrt_thread_yield_free(struct vrt_yield_strategy *vys)
{
    struct vrt_thread_yield_strategy  *ys =
        cork_container_of(vys, struct vrt_thread_yield_strategy, parent);
    cork_delete(struct vrt_thread_yield_strategy, ys);
}

static int
vrt_thread_yield(struct vrt_yield_strategy *vys, bool first,
                     const char *queue_name, const char *name)
{
    struct vrt_thread_yield_strategy  *ys =
        cork_container_of(vys, struct vrt_thread_yield_strategy, parent);

    if (first) {
        ys->counter = SPIN_COUNT_BEFORE_YIELDING;
    } else {
        if (ys->counter == 0) {
            DEBUG("[%s] %s: Yielding to other threads\n", queue_name, name);
            THREAD_YIELD();
        } else {
            ys->counter--;
            PAUSE();
        }
    }

    return 0;
}

struct vrt_yield_strategy *
vrt_yield_strategy_threaded(void)
{
    struct vrt_thread_yield_strategy  *vs =
        cork_new(struct vrt_thread_yield_strategy);
    vs->parent.yield = vrt_thread_yield;
    vs->parent.free = vrt_thread_yield_free;
    return &vs->parent;
}


/*-----------------------------------------------------------------------
 * Spin-wait yielding strategy
 */

static int
vrt_spin_wait_yield(struct vrt_yield_strategy *vys, bool first,
                        const char *queue_name, const char *name)
{
    /* For a spin-wait, we just immediately return and let the
     * producer/consumer try again. */
    PAUSE();
    return 0;
}

static void
vrt_spin_wait_free(struct vrt_yield_strategy *vys)
{
    /* No-op; this is a static object */
}

static const struct vrt_yield_strategy  vrt_spin_wait_strategy = {
    vrt_spin_wait_yield,
    vrt_spin_wait_free
};

struct vrt_yield_strategy *
vrt_yield_strategy_spin_wait(void)
{
    return (struct vrt_yield_strategy *) &vrt_spin_wait_strategy;
}


/*-----------------------------------------------------------------------
 * Hybrid yielding strategy
 */

struct vrt_hybrid_yield_strategy {
    struct vrt_yield_strategy  parent;
    int  counter;
};

static void
vrt_hybrid_yield_free(struct vrt_yield_strategy *vys)
{
    struct vrt_hybrid_yield_strategy  *ys =
        cork_container_of(vys, struct vrt_hybrid_yield_strategy, parent);
    cork_delete(struct vrt_hybrid_yield_strategy, ys);
}

static int
vrt_hybrid_yield(struct vrt_yield_strategy *vys, bool first,
                     const char *queue_name, const char *name)
{
    /* Adapted from
     * http://www.1024cores.net/home/lock-free-algorithms/tricks/spinning */
    struct vrt_hybrid_yield_strategy  *ys =
        cork_container_of(vys, struct vrt_hybrid_yield_strategy, parent);

    if (first) {
        ys->counter = 0;
    } else if (ys->counter < 10) {
        /* Spin-wait */
        PAUSE();
    } else if (ys->counter < 20) {
        /* A more intense spin-wait */
        int  i;
        for (i = 0; i < 50; i++) {
            PAUSE();
        }
    } else if (ys->counter < 22) {
        THREAD_YIELD();
    } else if (ys->counter < 24) {
        usleep(0);
    } else if (ys->counter < 26) {
        usleep(1);
    } else {
        usleep((ys->counter - 25) * 10);
    }

    ys->counter++;
    return 0;
}

struct vrt_yield_strategy *
vrt_yield_strategy_hybrid(void)
{
    struct vrt_hybrid_yield_strategy  *vs =
        cork_new(struct vrt_hybrid_yield_strategy);
    vs->parent.yield = vrt_hybrid_yield;
    vs->parent.free = vrt_hybrid_yield_free;
    return &vs->parent;
}
