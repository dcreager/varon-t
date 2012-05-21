/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2011, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the LICENSE.txt file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#include <libcork/core.h>
#include <libcork/helpers/errors.h>

#include "vrt/queue.h"
#include "integers.h"

static struct vrt_value *
vrt_value_int_new(struct vrt_value_type *type)
{
    struct vrt_value_int  *self = cork_new(struct vrt_value_int);
    return &self->parent;
}

static void
vrt_value_int_free(struct vrt_value_type *type, struct vrt_value *vself)
{
    struct vrt_value_int  *self =
        cork_container_of(vself, struct vrt_value_int, parent);
    free(self);
}

static struct vrt_value_type  _vrt_value_type_int = {
    vrt_value_int_new,
    vrt_value_int_free
};


struct vrt_value_type *
vrt_value_type_int(void)
{
    return &_vrt_value_type_int;
}
