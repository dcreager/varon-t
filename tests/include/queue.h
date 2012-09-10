/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_TESTS_QUEUE
#define VRT_TESTS_QUEUE

/*
 * A collection of functions for running vrt_queue tests using a
 * variety of different execution strategies.
 */

#include <libcork/core.h>

#include "vrt/queue.h"

#include "helpers.h"


/** A struct that represents each of the clients of a queue in a
 * particular test case. */
struct vrt_queue_client {
    /** The function to run */
    void *
    (*run)(void *);

    /** The parameter to pass into the function */
    void  *ud;
};


/** Run each client in a separate thread */
int
vrt_test_queue_threaded(struct vrt_queue *q,
                        struct vrt_queue_client *clients,
                        vrt_clock *elapsed);

/** Run each client in a separate thread, but use spin-waits instead of
 * thread yields */
int
vrt_test_queue_threaded_spin(struct vrt_queue *q,
                             struct vrt_queue_client *clients,
                             vrt_clock *elapsed);

/** Run each client in a separate thread, but use the hybrid yield
 * strategy */
int
vrt_test_queue_threaded_hybrid(struct vrt_queue *q,
                               struct vrt_queue_client *clients,
                               vrt_clock *elapsed);


#endif /* VRT_TESTS_QUEUE */
