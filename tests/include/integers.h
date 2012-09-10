/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_TESTS_INTEGERS
#define VRT_TESTS_INTEGERS

/* A sample vrt_value_type that stores a single int64_t value. */

#include <stdlib.h>

#include <libcork/core.h>
#include <libcork/helpers/errors.h>

#include "vrt/queue.h"
#include "vrt/value.h"


/*-----------------------------------------------------------------------
 * Integer value type
 */

struct vrt_value_int {
    struct vrt_value  parent;
    int32_t  value;
};

struct vrt_value_type *
vrt_value_type_int(void);


/*-----------------------------------------------------------------------
 * Generate processor
 */

struct generate_config {
    struct vrt_producer  *p;
    int64_t  count;
};

CORK_ATTR_UNUSED
static void *
generate_integers(void *ud)
{
    struct generate_config  *c = ud;
    int32_t  i;
    for (i = 0; i < c->count; i++) {
        struct vrt_value  *vvalue;
        struct vrt_value_int  *value;
        rpi_check(vrt_producer_claim(c->p, &vvalue));
        value = cork_container_of(vvalue, struct vrt_value_int, parent);
        value->value = i;
        rpi_check(vrt_producer_publish(c->p));
    }

    /* Send an EOF */
    rpi_check(vrt_producer_eof(c->p));
    return NULL;
}


/*-----------------------------------------------------------------------
 * Multiply processor
 */

struct multiply_config {
    struct vrt_consumer  *c;
    int64_t  multiplicand;
};

CORK_ATTR_UNUSED
static void *
multiply_integers(void *ud)
{
    int  rc;
    struct multiply_config  *c = ud;
    struct vrt_value  *vvalue;
    while ((rc = vrt_consumer_next(c->c, &vvalue)) != VRT_QUEUE_EOF) {
        if (rc == 0) {
            struct vrt_value_int  *value =
                cork_container_of(vvalue, struct vrt_value_int, parent);
            /* We can update the value because downstream processors will be
             * dependent on us. */
            value->value *= c->multiplicand;
        }
    }
    return NULL;
}


/*-----------------------------------------------------------------------
 * Sum processor
 */

struct sum_config {
    struct vrt_consumer  *c;
    int64_t  *result;
};

CORK_ATTR_UNUSED
static void *
sum_integers(void *ud)
{
    int  rc;
    struct sum_config  *c = ud;
    struct vrt_value  *vvalue;
    int64_t  sum = 0;
    while ((rc = vrt_consumer_next(c->c, &vvalue)) != VRT_QUEUE_EOF) {
        if (rc == 0) {
            struct vrt_value_int  *value =
                cork_container_of(vvalue, struct vrt_value_int, parent);
            sum += value->value;
        }
    }
    if (rc == VRT_QUEUE_EOF) {
        /* Got an EOF; store the sum in the requested destination */
        *c->result = sum;
    }
    return NULL;
}


/*-----------------------------------------------------------------------
 * Noop processor
 */

struct noop_config {
    struct vrt_consumer  *c;
    int64_t  *result;
};

CORK_ATTR_UNUSED
static void *
noop_integers(void *ud)
{
    int  rc;
    struct noop_config  *c = ud;
    struct vrt_value  *vvalue;
    int32_t  v = 0;
    while ((rc = vrt_consumer_next(c->c, &vvalue)) != VRT_QUEUE_EOF) {
        if (rc == 0) {
            /*
            struct vrt_value_int  *value =
                cork_container_of(vvalue, struct vrt_value_int, parent);
                v = value->value;
             */
        }
    }
    if (rc == VRT_QUEUE_EOF) {
        /* On EOF, copy the latest value in to the result location */
        *c->result = v;
    }
    return NULL;
}

#endif /* VRT_TESTS_INTEGERS */
