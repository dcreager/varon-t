/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2011-2014, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>

#include <libcork/core.h>
#include <libcork/helpers/errors.h>
#include <pthread.h>

#include "vrt/queue.h"

#include "helpers.h"
#include "queue.h"

int
vrt_test_queue_threaded(struct vrt_queue *q, struct vrt_queue_client *clients,
                        vrt_clock *elapsed)
{
    vrt_clock  start_time;
    vrt_clock  end_time;

    vrt_get_clock(&start_time);

    size_t  i;
    size_t  client_count = 0;
    struct vrt_queue_client  *client;
    for (client = clients; client->run != NULL; client++) {
        client_count++;
    }

    pthread_t  *thread_ids;
    thread_ids = cork_calloc(client_count, sizeof(pthread_t));

    for (i = 0; i < cork_array_size(&q->producers); i++) {
        struct vrt_producer  *p = cork_array_at(&q->producers, i);
        p->yield = vrt_yield_strategy_threaded();
    }

    for (i = 0; i < cork_array_size(&q->consumers); i++) {
        struct vrt_consumer  *c = cork_array_at(&q->consumers, i);
        c->yield = vrt_yield_strategy_threaded();
    }

    for (i = 0; i < client_count; i++) {
        pthread_create(&thread_ids[i], NULL, clients[i].run, clients[i].ud);
    }

    for (i = 0; i < client_count; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    cork_cfree(thread_ids, client_count, sizeof(pthread_t));
    vrt_get_clock(&end_time);

    *elapsed = (end_time - start_time);
    return 0;
}

int
vrt_test_queue_threaded_spin(struct vrt_queue *q,
                                 struct vrt_queue_client *clients,
                                 vrt_clock *elapsed)
{
    vrt_clock  start_time;
    vrt_clock  end_time;

    vrt_get_clock(&start_time);

    size_t  i;
    size_t  client_count = 0;
    struct vrt_queue_client  *client;
    for (client = clients; client->run != NULL; client++) {
        client_count++;
    }

    pthread_t  *thread_ids;
    thread_ids = cork_calloc(client_count, sizeof(pthread_t));

    for (i = 0; i < cork_array_size(&q->producers); i++) {
        struct vrt_producer  *p = cork_array_at(&q->producers, i);
        p->yield = vrt_yield_strategy_spin_wait();
    }

    for (i = 0; i < cork_array_size(&q->consumers); i++) {
        struct vrt_consumer  *c = cork_array_at(&q->consumers, i);
        c->yield = vrt_yield_strategy_spin_wait();
    }

    for (i = 0; i < client_count; i++) {
        pthread_create(&thread_ids[i], NULL, clients[i].run, clients[i].ud);
    }

    for (i = 0; i < client_count; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    cork_cfree(thread_ids, client_count, sizeof(pthread_t));
    vrt_get_clock(&end_time);

    *elapsed = (end_time - start_time);
    return 0;
}

int
vrt_test_queue_threaded_hybrid(struct vrt_queue *q,
                                   struct vrt_queue_client *clients,
                                   vrt_clock *elapsed)
{
    vrt_clock  start_time;
    vrt_clock  end_time;

    vrt_get_clock(&start_time);

    size_t  i;
    size_t  client_count = 0;
    struct vrt_queue_client  *client;
    for (client = clients; client->run != NULL; client++) {
        client_count++;
    }

    pthread_t  *thread_ids;
    thread_ids = cork_calloc(client_count, sizeof(pthread_t));

    for (i = 0; i < cork_array_size(&q->producers); i++) {
        struct vrt_producer  *p = cork_array_at(&q->producers, i);
        p->yield = vrt_yield_strategy_hybrid();
    }

    for (i = 0; i < cork_array_size(&q->consumers); i++) {
        struct vrt_consumer  *c = cork_array_at(&q->consumers, i);
        c->yield = vrt_yield_strategy_hybrid();
    }

    for (i = 0; i < client_count; i++) {
        pthread_create(&thread_ids[i], NULL, clients[i].run, clients[i].ud);
    }

    for (i = 0; i < client_count; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    cork_cfree(thread_ids, client_count, sizeof(pthread_t));
    vrt_get_clock(&end_time);

    *elapsed = (end_time - start_time);
    return 0;
}
