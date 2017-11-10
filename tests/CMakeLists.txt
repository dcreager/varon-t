# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------
# Copyright © 2011-2015, RedJack, LLC.
# All rights reserved.
#
# Please see the COPYING file in this distribution for license details.
# ----------------------------------------------------------------------

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

#-----------------------------------------------------------------------
# Build the test cases

set(UTIL_SOURCES
    lib/integers.c
    lib/queue.c
)

macro(make_test test_name)
    add_c_executable(
        ${test_name}
        SKIP_INSTALL
        OUTPUT_NAME ${test_name}
        SOURCES
            ${test_name}.c
            ${UTIL_SOURCES}
        LIBRARIES
            check
            threads
        LOCAL_LIBRARIES
            libvrt
    )
    add_test(${test_name} ${test_name})
endmacro(make_test)

make_test(test-perf-dq)
make_test(test-vrt)

#-----------------------------------------------------------------------
# Command-line tests

find_package(PythonInterp)

if (PYTHON_EXECUTABLE)
    configure_file(ccram ${CMAKE_BINARY_DIR}/ccram COPYONLY)
    file(GLOB_RECURSE TESTS "${CMAKE_CURRENT_SOURCE_DIR}/*.t")
    foreach(TEST ${TESTS})
        get_filename_component(TEST_NAME "${TEST}" NAME_WE)
        add_test(
            ${TEST_NAME}
            ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}
            ${CMAKE_BINARY_DIR}/ccram
                --python ${PYTHON_EXECUTABLE}
                --root ${CMAKE_SOURCE_DIR}
                --tests ${TEST}
        )
    endforeach(TEST)
else (PYTHON_EXECUTABLE)
    message(WARNING "Unable to find Python; skipping cram tests.")
endif (PYTHON_EXECUTABLE)
