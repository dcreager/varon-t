/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_VALUE_H
#define VRT_VALUE_H

#include <libcork/core.h>


/*-----------------------------------------------------------------------
 * Value objects
 */

enum vrt_value_special {
    VRT_VALUE_NONE = 0,
    VRT_VALUE_EOF,
    VRT_VALUE_HOLE,
    VRT_VALUE_FLUSH
};

struct vrt_value;

/** The ID of a value within the queue that manages it */
typedef int  vrt_value_id;

/* Each Varon-T disruptor queue manages a list of _values_.  The queue
 * manages the lifecycle of the value.  Each value type must implement the
 * following interface.  */
struct vrt_value_type {
    /** Allocate an instance of this type. */
    struct vrt_value *
    (*new_value)(struct vrt_value_type *type);

    /** Free an instance of this type. */
    void
    (*free_value)(struct vrt_value_type *type, struct vrt_value *value);
};

/** Instantiate a new value of the given type. */
#define vrt_value_new(type) \
    ((type)->new_value((type)))

/** Free a value of the given type. */
#define vrt_value_free(type, value) \
    ((type)->free_value((type), (value)))

/** The superclass of a value that's managed by a Varon-T queue. */
struct vrt_value {
    vrt_value_id  id;
    int  special;
};


#endif /* VRT_VALUE_H */
