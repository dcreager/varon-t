/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012-2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_TESTS_HELPERS
#define VRT_TESTS_HELPERS

#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <libcork/core.h>


#define DESCRIBE_TEST \
    fprintf(stderr, "--- %s\n", __func__);


/*-----------------------------------------------------------------------
 * Allocators
 */

/* Use a custom allocator that debugs every allocation. */

CORK_ATTR_UNUSED
static void
setup_allocator(void)
{
    struct cork_alloc  *debug = cork_debug_alloc_new(cork_allocator);
    cork_set_allocator(debug);
}


/*-----------------------------------------------------------------------
 * Error reporting
 */

#define fail_if_error(call) \
    do { \
        call; \
        if (cork_error_occurred()) { \
            fail("%s", cork_error_message()); \
        } \
    } while (0)

#define fail_unless_error(call, msg) \
    do { \
        call; \
        if (!cork_error_occurred()) { \
            fail(msg); \
        } \
    } while (0)


/* Functions for calculating the amount of time it takes to execute a
 * test case. */

/* microseconds */
typedef uint64_t  vrt_clock;

#define vrt_get_clock(clk) \
    do { \
        struct timeval  __tv; \
        gettimeofday(&__tv, NULL); \
        *(clk) = __tv.tv_sec * 1000000 + __tv.tv_usec; \
    } while (0)

#define vrt_report_clock(clk, iterations) \
    do { \
        printf("%" PRIu64 " usec\t%.0lf iterations/sec\n", \
               (clk), (((double) (iterations)) / (clk) * 1000000)); \
    } while (0)


#endif /* VRT_TESTS_HELPERS */
