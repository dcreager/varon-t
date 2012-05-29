# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2012, RedJack, LLC.
# All rights reserved.
#
# Please see the LICENSE.txt file in this distribution for license
# details.
# ----------------------------------------------------------------------

include_directories(../include)

#-----------------------------------------------------------------------
# Build the library

set(LIBVRT_SRC
    libvrt/queue.c
    libvrt/yield.c
)

add_library(libvrt SHARED ${LIBVRT_SRC})
set_target_properties(libvrt PROPERTIES
    OUTPUT_NAME vrt
    SOVERSION 0.0.0)
target_link_libraries(libvrt ${CORK_LIBRARIES})

install(TARGETS libvrt
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib)

#-----------------------------------------------------------------------
# Generate the pkg-config file

set(prefix ${CMAKE_INSTALL_PREFIX})
configure_file(varon-t.pc.in varon-t.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/varon-t.pc
        DESTINATION lib/pkgconfig)
