/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <clogger.h>
#include <libcork/core.h>

#include <check.h>

#include "vrt.h"

#include "helpers.h"
#include "integers.h"
#include "queue.h"


/*----------------------------------------------------------------------
 * Sum test
 */

#define DEFAULT_GENERATE_COUNT  10
static int64_t  GENERATE_COUNT = DEFAULT_GENERATE_COUNT;

#define RUN_TEST(queue_size, batch_size, run_func) \
    DESCRIBE_TEST; \
    int64_t  result; \
    \
    struct vrt_queue  *q; \
    struct vrt_producer  *p; \
    struct vrt_consumer  *c; \
    vrt_clock  elapsed; \
    \
    fail_if_error(q = vrt_queue_new \
                      ("queue_sum", vrt_value_type_int(), \
                       queue_size)); \
    fail_if_error(p = vrt_producer_new \
                      ("generate", batch_size, q)); \
    fail_if_error(c = vrt_consumer_new("sum", q)); \
    \
    struct generate_config  generate_config = { \
        p, GENERATE_COUNT \
    }; \
    struct sum_config  sum_config = { \
        c, &result \
    }; \
    \
    struct vrt_queue_client  clients[] = { \
        { generate_integers, &generate_config }, \
        { sum_integers, &sum_config }, \
        { NULL, NULL } \
    }; \
    \
    fail_if_error(run_func(q, clients, &elapsed)); \
    fprintf(stdout, "Result: %" PRId64 "\n", result); \
    vrt_report_clock(elapsed, GENERATE_COUNT); \
    vrt_queue_free(q);



START_TEST(test_sum_threaded_small)
{
    RUN_TEST(16, 4, vrt_test_queue_threaded);
}
END_TEST

START_TEST(test_sum_threaded)
{
    RUN_TEST(0, 0, vrt_test_queue_threaded);
}
END_TEST


START_TEST(test_sum_threaded_spin_small)
{
    RUN_TEST(16, 4, vrt_test_queue_threaded_spin);
}
END_TEST

START_TEST(test_sum_threaded_spin)
{
    RUN_TEST(0, 0, vrt_test_queue_threaded_spin);
}
END_TEST


START_TEST(test_sum_threaded_hybrid_small)
{
    RUN_TEST(16, 4, vrt_test_queue_threaded_hybrid);
}
END_TEST

START_TEST(test_sum_threaded_hybrid)
{
    RUN_TEST(0, 0, vrt_test_queue_threaded_hybrid);
}
END_TEST


/*----------------------------------------------------------------------
 * Testing harness
 */

Suite *
test_suite()
{
    Suite  *s = suite_create("varon-t");

    TCase  *tc_vrt = tcase_create("varon-t");
    tcase_add_test(tc_vrt, test_sum_threaded);
    tcase_add_test(tc_vrt, test_sum_threaded_small);
    tcase_add_test(tc_vrt, test_sum_threaded_spin);
    tcase_add_test(tc_vrt, test_sum_threaded_spin_small);
    tcase_add_test(tc_vrt, test_sum_threaded_hybrid);
    tcase_add_test(tc_vrt, test_sum_threaded_hybrid_small);
    suite_add_tcase(s, tc_vrt);

    return s;
}

int
main(int argc, const char **argv)
{
    int number_failed;
    Suite  *suite = test_suite();
    SRunner  *runner = srunner_create(suite);

    setup_allocator();

    if (argc > 1) {
        if (sscanf(argv[1], "%" PRId64, &GENERATE_COUNT) != 1) {
            fprintf(stderr, "Invalid record count: \"%s\"\n", argv[1]);
            return -1;
        }
    }

    vrt_testing_mode();
    clog_setup_logging();
    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0)? EXIT_SUCCESS: EXIT_FAILURE;
}
