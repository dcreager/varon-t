# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2012, RedJack, LLC.
# All rights reserved.
#
# Please see the COPYING file in this distribution for license
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

set(UTIL_SOURCES
    lib/integers.c
    lib/queue.c
)

macro(make_test test_name)
    add_executable(${test_name} ${test_name}.c ${UTIL_SOURCES})
    target_link_libraries(${test_name}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CHECK_LIBRARIES}
        libvrt
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro(make_test)

make_test(test-perf-dq)
make_test(test-vrt)

#-----------------------------------------------------------------------
# Command-line tests

find_program(
    CRAM_EXECUTABLE
    NAMES cram
    HINTS ENV CRAM_DIR
    PATH_SUFFIXES bin
    DOC "Cram test harness"
)

if (CRAM_EXECUTABLE)
    configure_file(ccram ${CMAKE_BINARY_DIR}/ccram COPYONLY)
    add_test(
        NAME cram-tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_BINARY_DIR}/ccram
            --cram ${CRAM_EXECUTABLE}
            --tests ${CMAKE_CURRENT_SOURCE_DIR}
    )
else (CRAM_EXECUTABLE)
    message(WARNING "Unable to find cram test harness; skipping tests.")
endif (CRAM_EXECUTABLE)
