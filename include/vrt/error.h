/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the LICENSE.txt file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_ERROR_H
#define VRT_ERROR_H

#include <errno.h>
#include <string.h>

#include <libcork/core.h>


/*-----------------------------------------------------------------------
 * Error handling
 */

/* hash of "vrt.h" */
#define VRT_ERROR  0x2fe5c7ca

enum vrt_error {
    /* An error with the arguments given to a function */
    VRT_ARGUMENTS_ERROR,
    /* An error in the data received by a computation */
    VRT_MALFORMED_DATA_ERROR,
    /* An I/O error reading or writing a file */
    VRT_IO_ERROR
};

#define vrt_error(code, ...)  (cork_error_set(VRT_ERROR, code, __VA_ARGS__))
#define vrt_io_error() \
    cork_error_set \
        (VRT_ERROR, VRT_IO_ERROR, \
         "%s", strerror(errno))


#endif /* VRT_ERROR_H */
