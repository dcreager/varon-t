# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2012, RedJack, LLC.
# All rights reserved.
#
# Please see the LICENSE.txt file in this distribution for license
# details.
# ----------------------------------------------------------------------

include_directories(../include)
include_directories(./include)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/../src)

#-----------------------------------------------------------------------
# Check for prerequisite libraries

find_package(PkgConfig)

pkg_check_modules(CHECK REQUIRED check)
include_directories(${CHECK_INCLUDE_DIRS})
link_directories(${CHECK_LIBRARY_DIRS})

#-----------------------------------------------------------------------
# Build the test cases

file(GLOB UTIL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lib/*.c)
macro(make_test test_name)
    add_executable(${test_name} ${test_name}.c ${UTIL_SOURCES})
    target_link_libraries(${test_name} ${CHECK_LIBRARIES} libvrt)
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro(make_test)

make_test(test-vrt)
