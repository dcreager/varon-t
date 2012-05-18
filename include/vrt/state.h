/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the LICENSE.txt file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_STATE
#define VRT_STATE

#include <errno.h>
#include <string.h>

#include <libcork/core.h>


/*-----------------------------------------------------------------------
 * Debugging messages
 */

#ifndef WANT_DEBUG
#define WANT_DEBUG 0
#endif
#if WANT_DEBUG
#define DEBUG(...) printf(__VA_ARGS__)
#else
#define DEBUG(...) /* do nothing */
#endif


/*-----------------------------------------------------------------------
 * Error handling
 */

/** @brief The error class for generic pan-Varon-T errors */
/* hash of "vrt/state.h" */
#define VRT_ERROR  0x58d56d51

/** @brief Error codes for generic pan-Varon-T errors */
enum vrt_error {
    /** @brief An error with the arguments given to a function */
    VRT_ARGUMENTS_ERROR,
    /** @brief An error in the data received by a computation */
    VRT_MALFORMED_DATA_ERROR,
    /** @brief An I/O error reading or writing a file */
    VRT_IO_ERROR,
    /** @brief An error occurred while executing some nested Lua */
    VRT_LUA_ERROR
};

#define vrt_io_error() \
    cork_error_set \
        (VRT_ERROR, VRT_IO_ERROR, \
         "%s", strerror(errno))


#endif /* VRT_STATE */
