/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012-2015, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdio.h>
#include <libcork/core.h>
#include <libcork/ds.h>
#include <vrt.h>

#include "helpers.h"
#include "integers.h"
#include "queue.h"

/* These performance parameters are taken directly from the LMAX Disruptor
 * Queue code. Note that they are different from the parameters given in the
 * LMAX technical paper available online.
 */

#define RUNS  3
#define QUEUE_SIZE  8 * 1024
#define BATCH_SIZE  10
#define DEFAULT_GENERATE_COUNT  1000000

static uint64_t  GENERATE_COUNT = DEFAULT_GENERATE_COUNT;

/* Unicast: 1P -> 1C */
static int
unicast_test(uint32_t queue_size, uint64_t batch_size,
             int (*run_func)
                 (struct vrt_queue *, struct vrt_queue_client *, vrt_clock *))
{
    int64_t  result = 0;
    struct vrt_queue  *q;
    struct vrt_producer  *p;
    struct vrt_consumer  *c;
    vrt_clock  elapsed;

    q = vrt_queue_new("queue_noop", vrt_value_type_int(), queue_size);
    p = vrt_producer_new("generate", batch_size, q);
    c = vrt_consumer_new("noop", q);

    struct generate_config  gc = {
        p, GENERATE_COUNT
    };

    struct noop_config nc = {
        c, &result
    };

    struct vrt_queue_client  clients[] = {
        {generate_integers, &gc},
        {noop_integers, &nc},
        {NULL, NULL}
    };

    run_func(q, clients, &elapsed);
    /*fprintf(stdout, "Result: %" PRId64 "\n", result);*/
    vrt_report_clock(elapsed, GENERATE_COUNT);
    vrt_queue_free(q);
    return 0;
}

/* Three-step Pipeline: 1P -> 1C -> 1C -> 1C */
CORK_ATTR_UNUSED
static int
three_step_pipeline_test(uint32_t queue_size, uint64_t batch_size,
                   int (*run_func)
                   (struct vrt_queue *, struct vrt_queue_client *, vrt_clock *))
{
    return 0;
}


/* Sequencer: 3P -> 1C */
CORK_ATTR_UNUSED
static int
sequencer_test(uint32_t queue_size, uint64_t batch_size,
               int (*run_func)
                   (struct vrt_queue *, struct vrt_queue_client *, vrt_clock *))
{
    int64_t  result = 0;
    struct vrt_queue  *q;
    struct vrt_producer  *p1;
    struct vrt_producer  *p2;
    struct vrt_producer  *p3;
    struct vrt_consumer  *c;
    vrt_clock  elapsed;

    q = vrt_queue_new("queue_noop", vrt_value_type_int(), queue_size);
    p1 = vrt_producer_new("generate_1", batch_size, q);
    p2 = vrt_producer_new("generate_2", batch_size, q);
    p3 = vrt_producer_new("generate_3", batch_size, q);
    c = vrt_consumer_new("noop", q);

    struct generate_config  gc1 = {
        p1, GENERATE_COUNT
    };

    struct generate_config  gc2 = {
        p2, GENERATE_COUNT
    };

    struct generate_config  gc3 = {
        p3, GENERATE_COUNT
    };

    struct noop_config nc = {
        c, &result
    };

    struct vrt_queue_client  clients[] = {
        {generate_integers, &gc1},
        {generate_integers, &gc2},
        {generate_integers, &gc3},
        {noop_integers, &nc},
        {NULL, NULL}
    };

    run_func(q, clients, &elapsed);
    /*fprintf(stdout, "Result: %" PRId64 "\n", result);*/
    vrt_report_clock(elapsed, GENERATE_COUNT);
    vrt_queue_free(q);
    return 0;
}

/* Multcast: 1P -> 3C */
CORK_ATTR_UNUSED
static int
multicast_test(uint32_t queue_size, uint64_t batch_size,
               int (*run_func)
                   (struct vrt_queue *, struct vrt_queue_client *, vrt_clock *))
{
    int64_t  result = 0;
    struct vrt_queue  *q;
    struct vrt_producer  *p;
    struct vrt_consumer  *c1;
    struct vrt_consumer  *c2;
    struct vrt_consumer  *c3;
    vrt_clock  elapsed;

    q = vrt_queue_new("queue_noop", vrt_value_type_int(), queue_size);
    p = vrt_producer_new("generate", batch_size, q);
    c1 = vrt_consumer_new("noop_1", q);
    c2 = vrt_consumer_new("noop_2", q);
    c3 = vrt_consumer_new("noop_3", q);

    struct generate_config  gc = {
        p, GENERATE_COUNT
    };

    struct noop_config nc1 = {
        c1, &result
    };

    struct noop_config nc2 = {
        c2, &result
    };

    struct noop_config nc3 = {
        c3, &result
    };

    struct vrt_queue_client  clients[] = {
        {generate_integers, &gc},
        {noop_integers, &nc1},
        {noop_integers, &nc2},
        {noop_integers, &nc3},
        {NULL, NULL}
    };

    run_func(q, clients, &elapsed);
    /*fprintf(stdout, "Result: %" PRId64 "\n", result);*/
    vrt_report_clock(elapsed, GENERATE_COUNT);
    vrt_queue_free(q);
    return 0;

}

/* Diamond: 1P -> 2C -> 1C */
CORK_ATTR_UNUSED
static int
diamond_test(uint32_t queue_size, uint64_t batch_size,
             int (*run_func)
                 (struct vrt_queue *, struct vrt_queue_client *, vrt_clock *))
{
    return 0;
}

int
main(int argc, const char * argv[])
{
    unsigned int i = 0;
    uint32_t  batch_size = 0;
#define MAX_BATCH_SIZE  1024

    setup_allocator();

    /* 1-1 Unicast test (batched) */
    for (batch_size = 64; batch_size <= MAX_BATCH_SIZE; batch_size <<= 1) {

        fprintf(stdout, "\n1-1 UNICAST TEST (BATCH SIZE = %" PRIu32 ")\n"
                        "====================================\n",
                        batch_size);

        fprintf(stdout, "vrt_test_queue_threaded\n"
                        "-----------------------\n");
        for (i = 1; i <= RUNS; i++) {
            fprintf(stdout, "run %" PRIu32 ": ", i);
            unicast_test(QUEUE_SIZE, batch_size,
                         vrt_test_queue_threaded);
        }

        fprintf(stdout, "\nvrt_test_queue_threaded_spin\n"
                        "----------------------------\n");
        for (i = 1; i <= RUNS; i++) {
            fprintf(stdout, "run %" PRIu32 ": ", i);
            unicast_test(QUEUE_SIZE, batch_size,
                         vrt_test_queue_threaded_spin);
        }

        fprintf(stdout, "\nvrt_test_queue_threaded_hybrid\n"
                        "------------------------------\n");
        for (i = 1; i <= RUNS; i++) {
            fprintf(stdout, "run %" PRIu32 ": ", i);
            unicast_test(QUEUE_SIZE, batch_size, 
                         vrt_test_queue_threaded_hybrid);
        }
    }

    /* 1-1 Unicast test (unbatched) */
    fprintf(stdout, "\n1-1 UNICAST TEST (UNBATCHED)\n"
                    "============================\n");

    fprintf(stdout, "vrt_test_queue_threaded\n"
                      "-----------------------\n");
    for (i = 1; i <= RUNS; i++) {
        fprintf(stdout, "run %" PRIu32 ": ", i);
        unicast_test(QUEUE_SIZE, 1, vrt_test_queue_threaded);
    }

    fprintf(stdout, "\nvrt_test_queue_threaded_spin\n"
                      "----------------------------\n");
    for (i = 1; i <= RUNS; i++) {
        fprintf(stdout, "run %" PRIu32 ": ", i);
        unicast_test(QUEUE_SIZE, 1, vrt_test_queue_threaded_spin);
    }

    fprintf(stdout, "\nvrt_test_queue_threaded_hybrid\n"
                      "------------------------------\n");
    for (i = 1; i <= RUNS; i++) {
        fprintf(stdout, "run %" PRIu32 ": ", i);
        unicast_test(QUEUE_SIZE, 1, vrt_test_queue_threaded_hybrid);
    }


    /* 1-3 Multicast test */
    fprintf(stdout, "\n1-3 MULTICAST TEST (UNBATCHED)\n"
                      "==============================\n");

    fprintf(stdout, "vrt_test_queue_threaded\n"
                      "-----------------------\n");
    for (i = 1; i <= RUNS; i++) {
        fprintf(stdout, "run %" PRIu32 ": ", i);
        multicast_test(QUEUE_SIZE, 1, vrt_test_queue_threaded);
    }

    fprintf(stdout, "\nvrt_test_queue_threaded_spin\n"
                      "----------------------------\n");
    for (i = 1; i <= RUNS; i++) {
        fprintf(stdout, "run %" PRIu32 ": ", i);
        multicast_test(QUEUE_SIZE, 1, vrt_test_queue_threaded_spin);
    }

    fprintf(stdout, "\nvrt_test_queue_threaded_hybrid\n"
                      "------------------------------\n");
    for (i = 1; i <= RUNS; i++) {
        fprintf(stdout, "run %" PRIu32 ": ", i);
        multicast_test(QUEUE_SIZE, 1, vrt_test_queue_threaded_hybrid);
    }

    return EXIT_SUCCESS;
}

