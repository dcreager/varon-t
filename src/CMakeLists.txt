# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2011, RedJack, LLC.
# All rights reserved.
#
# Please see the LICENSE.txt file in this distribution for license
# details.
# ----------------------------------------------------------------------

include_directories(../include)

#-----------------------------------------------------------------------
# Build the library

file(GLOB_RECURSE LIBVARON_T_SRC libvaron-t/*.c)

add_library(libvaron_t SHARED ${LIBVARON_T_SRC})
set_target_properties(libvaron_t PROPERTIES
    OUTPUT_NAME varon-t
    SOVERSION 0.0.0)
target_link_libraries(libvaron_t
    ${CORK_LIBRARIES})

install(TARGETS libvaron_t
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib)

#-----------------------------------------------------------------------
# Generate the pkg-config file

set(prefix ${CMAKE_INSTALL_PREFIX})
configure_file(varon-t.pc.in varon-t.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/varon-t.pc
        DESTINATION lib/pkgconfig)
