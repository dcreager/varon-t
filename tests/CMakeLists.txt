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
    target_link_libraries(${test_name}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CHECK_LIBRARIES}
        libvrt
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro(make_test)

make_test(test-vrt)

#-----------------------------------------------------------------------
# Command-line tests

# This will automatically find any test cases in tests/ that should be
# run using the run-script helper script.  This lets you pass in
# arbitary command-line options and stdin to a compiled executable, and
# verify its stdout and stderr.  The expectation is that your
# directories will be named tests/${category}/${test_name}

configure_file(run-test ${CMAKE_BINARY_DIR}/run-test COPYONLY)
configure_file(train ${CMAKE_BINARY_DIR}/train COPYONLY)

file(GLOB_RECURSE OUTPUT_TESTS command)
foreach(OUTPUT_TEST ${OUTPUT_TESTS})
    get_filename_component(test_dir ${OUTPUT_TEST} PATH)
    get_filename_component(test_suffix ${test_dir} NAME)
    get_filename_component(test_parent_dir ${test_dir} PATH)
    get_filename_component(test_prefix ${test_parent_dir} NAME)
    set(test_name "${test_prefix}:${test_suffix}")

    add_test(NAME ${test_name}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
             COMMAND ${CMAKE_BINARY_DIR}/run-test ${test_dir})
endforeach(OUTPUT_TEST ${OUTPUT_TESTS})
