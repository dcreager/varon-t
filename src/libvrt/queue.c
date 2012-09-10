/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#include <libcork/core.h>
#include <libcork/ds.h>
#include <libcork/helpers/errors.h>

#include "vrt/atomic.h"
#include "vrt/queue.h"
#include "vrt/yield.h"


#ifndef VRT_DEBUG_QUEUE
#define VRT_DEBUG_QUEUE 0
#endif
#if VRT_DEBUG_QUEUE
#include <stdio.h>
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG(...) /* do nothing */
#endif


#define MINIMUM_QUEUE_SIZE  16
#define DEFAULT_QUEUE_SIZE  65536
#define DEFAULT_BATCH_SIZE  4096
#define DEFAULT_STARTING_VALUE  (INT_MAX - 2*DEFAULT_BATCH_SIZE)


/*-----------------------------------------------------------------------
 * Queues
 */

/** Returns the smallest power of 2 that is >= in. */
static inline unsigned int
min_power_of_2(unsigned int in)
{
    unsigned int  v = in;
    unsigned int  r = 1;
    while (v >>= 1) {
        r <<= 1;
    }
    if (r != in) {
        r <<= 1;
    }
    return r;
}


struct vrt_queue *
vrt_queue_new(const char *name, struct vrt_value_type *value_type,
              unsigned int size)
{
    struct vrt_queue  *q = cork_new(struct vrt_queue);
    memset(q, 0, sizeof(struct vrt_queue));
    q->name = cork_strdup(name);

    unsigned int  value_count;
    if (size == 0) {
        value_count = DEFAULT_QUEUE_SIZE;
    } else {
        if (size < MINIMUM_QUEUE_SIZE) {
            size = MINIMUM_QUEUE_SIZE;
        }
        value_count = min_power_of_2(size);
    }
    q->value_mask = value_count - 1;
    q->last_consumed_id = DEFAULT_STARTING_VALUE;
    q->last_claimed_id.value = q->last_consumed_id;
    q->cursor.value = q->last_consumed_id;
    q->value_type = value_type;

    q->values = cork_calloc(value_count, sizeof(struct vrt_value *));
    DEBUG("[%s] Created queue with %u values\n", q->name, value_count);

    cork_array_init(&q->producers);
    cork_array_init(&q->consumers);

    unsigned int  i;
    for (i = 0; i < value_count; i++) {
        q->values[i] = vrt_value_new(value_type);
        cork_abort_if_null(q->values[i], "Cannot allocate values");
    }

    return q;
}

void
vrt_queue_free(struct vrt_queue *q)
{
    unsigned int  i;

    if (q->name != NULL) {
        cork_strfree(q->name);
    }

    for (i = 0; i < cork_array_size(&q->producers); i++) {
        struct vrt_producer  *p = cork_array_at(&q->producers, i);
        if (p != NULL) {
            vrt_producer_free(p);
        }
    }
    cork_array_done(&q->producers);

    for (i = 0; i < cork_array_size(&q->consumers); i++) {
        struct vrt_consumer  *c = cork_array_at(&q->consumers, i);
        if (c != NULL) {
            vrt_consumer_free(c);
        }
    }
    cork_array_done(&q->consumers);

    if (q->values != NULL) {
        for (i = 0; i <= q->value_mask; i++) {
            if (q->values[i] != NULL) {
                vrt_value_free(q->value_type, q->values[i]);
            }
        }

        free(q->values);
    }

    free(q);
}

static vrt_value_id
vrt_minimum_cursor(vrt_consumer_array *cs)
{
    /* We know there's always at least one consumer */
    unsigned int  i;
    vrt_value_id  minimum =
        vrt_consumer_get_cursor(cork_array_at(cs, 0));
    for (i = 1; i < cork_array_size(cs); i++) {
        vrt_value_id  id =
            vrt_consumer_get_cursor(cork_array_at(cs, i));
        if (vrt_mod_lt(id, minimum)) {
            minimum = id;
        }
    }
    return minimum;
}

#define vrt_queue_find_last_consumed_id(q) \
    (vrt_minimum_cursor(&(q)->consumers))

/* Waits for the slot given by the producer's last_claimed_id to become
 * free.  (This happens when every consumer has finished processing the
 * previous value that would've used the same slot in the ring buffer. */
static int
vrt_wait_for_slot(struct vrt_queue *q, struct vrt_producer *p)
{
    bool  first = true;
    vrt_value_id  wrapped_id =
        p->last_claimed_id - vrt_queue_size(q);
    DEBUG("[%s] %s: Waiting for value %d to be consumed\n",
          q->name, p->name, wrapped_id);
    if (vrt_mod_lt(q->last_consumed_id, wrapped_id)) {
        vrt_value_id  minimum = vrt_queue_find_last_consumed_id(q);
        while (vrt_mod_lt(minimum, wrapped_id)) {
            DEBUG("[%s] %s: Last consumed value is %d\n",
                  q->name, p->name, minimum);
#if VRT_QUEUE_STATS
            p->yield_count++;
#endif
            rii_check(vrt_yield_strategy_yield
                      (p->yield, first, q->name, p->name));
            first = false;
            minimum = vrt_queue_find_last_consumed_id(q);
        }
#if VRT_QUEUE_STATS
        p->batch_count++;
#endif
        q->last_consumed_id = minimum;
    }

    return 0;
}


static int
vrt_claim_single_threaded(struct vrt_queue *q, struct vrt_producer *p)
{
    /* If there's only a single producer, we can just grab the next
     * batch of values in sequence. */
    p->last_claimed_id += p->batch_size;
    if (p->batch_size == 1) {
        DEBUG("[%s] %s: Claiming value %d\n",
              q->name, p->name, p->last_claimed_id);
    } else {
        DEBUG("[%s] %s: Claiming values %d-%d\n",
              q->name, p->name,
              p->last_claimed_id - p->batch_size + 1, p->last_claimed_id);
    }

    /* But we do have to wait until the slots for these new values are
     * free. */
    return vrt_wait_for_slot(q, p);
}

static int
vrt_claim_multi_threaded(struct vrt_queue *q, struct vrt_producer *p)
{
    /* If there are multiple producerwe have to use an atomic
     * increment to claim the next batch of records. */
    p->last_claimed_id =
        vrt_padded_int_atomic_add(&q->last_claimed_id, p->batch_size);
    p->last_produced_id = p->last_claimed_id - p->batch_size;
    if (p->batch_size == 1) {
        DEBUG("[%s] %s: xClaiming value %d\n",
              q->name, p->name, p->last_claimed_id);
    } else {
        DEBUG("[%s] %s: xClaiming values %d-%d\n",
              q->name, p->name,
              p->last_produced_id + 1, p->last_claimed_id);
    }

    /* Then wait until the slots for these new values are free. */
    return vrt_wait_for_slot(q, p);
}

static int
vrt_publish_single_threaded(struct vrt_queue *q, struct vrt_producer *p,
                            vrt_value_id last_published_id)
{
    /* If there's only a single producer, we can just update the queue's
     * cursor.  We don't have to wait for anything, because the claim
     * function will have already ensured that this slot was free to
     * fill in and publish. */
    DEBUG("[%s] %s: Publishing value %d\n",
          q->name, p->name, last_published_id);
    vrt_queue_set_cursor(q, last_published_id);
    return 0;
}

static int
vrt_publish_multi_threaded(struct vrt_queue *q, struct vrt_producer *p,
                           vrt_value_id last_published_id)
{
    /* If there are multiple publisherthen we have to wait until all
     * of the values before the chunk that we claimed have been
     * published.  (If we don't, there will be a hole in the sequence of
     * published records.) */
    vrt_value_id  expected_cursor = last_published_id - p->batch_size;
    DEBUG("[%s] %s: Waiting for value %d to be published\n",
          q->name, p->name, expected_cursor);
    vrt_value_id  current_cursor = vrt_queue_get_cursor(q);
    bool  first = true;

    while (vrt_mod_lt(current_cursor, expected_cursor)) {
        rii_check(vrt_yield_strategy_yield
                  (p->yield, first, q->name, p->name));
        first = false;
        current_cursor = vrt_queue_get_cursor(q);
    }

    DEBUG("[%s] %s: Publishing value %d\n",
          q->name, p->name, last_published_id);
    vrt_queue_set_cursor(q, last_published_id);
    return 0;
}

static int
vrt_queue_add_producer(struct vrt_queue *q, struct vrt_producer *p)
{
    /* Add the producer to the queue's array and assign its index. */
    rii_check(cork_array_append(&q->producers, p));
    p->queue = q;
    p->index = cork_array_size(&q->producers) - 1;

    /* Choose the right claim and publish implementations for this
     * producer. */
    if (p->index == 0) {
        /* If this is the first producer, use faster claim and publish
         * methods that are optimized for the single-producer case. */
        p->claim = vrt_claim_single_threaded;
        p->publish = vrt_publish_single_threaded;
    } else {
        /* Otherwise we need to use slower, but multiple-producer-
         * capable, implementations of claim and publish. */
        p->claim = vrt_claim_multi_threaded;
        p->publish = vrt_publish_multi_threaded;

        /* If this is the second producer, then we need to update the
         * first producer to also use the slower implementations. */
        if (p->index == 1) {
            struct vrt_producer  *first = cork_array_at(&q->producers, 0);
            first->claim = vrt_claim_multi_threaded;
            first->publish = vrt_publish_multi_threaded;
        }
    }

    return 0;
}

static int
vrt_queue_add_consumer(struct vrt_queue *q, struct vrt_consumer *c)
{
    /* Add the consumer to the queue's array and assign its index. */
    rii_check(cork_array_append(&q->consumers, c));
    c->queue = q;
    c->index = cork_array_size(&q->consumers) - 1;
    return 0;
}


/*-----------------------------------------------------------------------
 * Producers
 */

struct vrt_producer *
vrt_producer_new(const char *name, unsigned int batch_size,
                 struct vrt_queue *q)
{
    struct vrt_producer  *p = cork_new(struct vrt_producer);
    memset(p, 0, sizeof(struct vrt_producer));

    p->name = cork_strdup(name);
    ei_check(vrt_queue_add_producer(q, p));

    if (batch_size == 0) {
        batch_size = DEFAULT_BATCH_SIZE;
    }
    unsigned int  maximum_batch_size = vrt_queue_size(q) / 4;
    if (batch_size > maximum_batch_size) {
        batch_size = maximum_batch_size;
    }

    p->last_produced_id = DEFAULT_STARTING_VALUE;
    p->last_claimed_id = DEFAULT_STARTING_VALUE;
    p->batch_size = batch_size;
    p->yield = NULL;
#if VRT_QUEUE_STATS
    p->batch_count = 0;
    p->yield_count = 0;
#endif
    return p;

error:
    if (p->name != NULL) {
        cork_strfree(p->name);
    }

    free(p);
    return NULL;
}

void
vrt_producer_free(struct vrt_producer *p)
{
    if (p->name != NULL) {
        cork_strfree(p->name);
    }

    if (p->yield != NULL) {
        vrt_yield_strategy_free(p->yield);
    }

    free(p);
}

/* Claims the next ID that this producer can fill in.  The new value's
 * ID will be stored in p->last_produced_id.  You can get the value
 * itself using vrt_queue_get. */
static int
vrt_producer_claim_raw(struct vrt_queue *q, struct vrt_producer *p)
{
    if (p->last_produced_id == p->last_claimed_id) {
        rii_check(p->claim(q, p));
    }
    p->last_produced_id++;
    DEBUG("[%s] %s: Returning value %d (%d)\n",
          q->name, p->name, p->last_produced_id, p->last_claimed_id);
    return 0;
}

int
vrt_producer_claim(struct vrt_producer *p, struct vrt_value **value)
{
    struct vrt_value  *v;
    rii_check(vrt_producer_claim_raw(p->queue, p));
    v = vrt_queue_get(p->queue, p->last_produced_id);
    v->id = p->last_produced_id;
    v->special = VRT_VALUE_NONE;
    *value = v;
    return 0;
}

int
vrt_producer_publish(struct vrt_producer *p)
{
#if 0
    DEBUG("[%s] %s: Pre-publishing value %d\n",
          p->queue->name, p->name, p->last_produced_id);
#endif
    if (p->last_produced_id == p->last_claimed_id) {
        return p->publish(p->queue, p, p->last_claimed_id);
    } else {
        return 0;
    }
}

int
vrt_producer_skip(struct vrt_producer *p)
{
    struct vrt_value  *v;
    v = vrt_queue_get(p->queue, p->last_produced_id);
    v->special = VRT_VALUE_HOLE;
    return vrt_producer_publish(p);
}

int
vrt_producer_flush(struct vrt_producer *p)
{
    /* Claim a value to fill in a FLUSH control message. */
    struct vrt_value  *v;
    rii_check(vrt_producer_claim_raw(p->queue, p));
    v = vrt_queue_get(p->queue, p->last_produced_id);
    v->special = VRT_VALUE_FLUSH;

    /* If we've claimed more value than we've produced, fill in the
     * remainder with holes. */
    if (vrt_mod_lt(p->last_produced_id, p->last_claimed_id)) {
        vrt_value_id  i;
        DEBUG("[%s] %s: Filling in holes for values %d-%d\n",
              p->queue->name, p->name,
              p->last_produced_id + 1, p->last_claimed_id);
        for (i = p->last_produced_id + 1;
             vrt_mod_le(i, p->last_claimed_id); i++) {
            struct vrt_value  *v = vrt_queue_get(p->queue, i);
            v->id = i;
            v->special = VRT_VALUE_HOLE;
        }
        p->last_produced_id = p->last_claimed_id;
    }

    /* Then publish the whole chunk. */
    return p->publish(p->queue, p, p->last_claimed_id);
}

int
vrt_producer_eof(struct vrt_producer *p)
{
    struct vrt_value  *v;
    rii_check(vrt_producer_claim_raw(p->queue, p));
    DEBUG("[%s] %s: Signaling EOF at value %d\n",
          p->queue->name, p->name, p->last_produced_id);
    v = vrt_queue_get(p->queue, p->last_produced_id);
    v->id = p->last_produced_id;
    v->special = VRT_VALUE_EOF;
    rii_check(vrt_producer_publish(p));
    return vrt_producer_flush(p);
}

void
vrt_report_producer(struct vrt_producer *p)
{
#if VRT_QUEUE_STATS
    printf("Producer %s:\n"
           "  Batches: %zu (%.3lf records/batch)\n"
           "  Yields:  %zu\n",
           p->name, p->batch_count,
           ((double) p->queue->last_produced_id.value) / (p->batch_count),
           p->yield_count);
#endif
}


/*-----------------------------------------------------------------------
 * Consumers
 */

struct vrt_consumer *
vrt_consumer_new(const char *name, struct vrt_queue *q)
{
    struct vrt_consumer  *c = cork_new(struct vrt_consumer);
    memset(c, 0, sizeof(struct vrt_consumer));
    c->name = cork_strdup(name);
    cork_array_init(&c->dependencies);

    ei_check(vrt_queue_add_consumer(q, c));
    c->cursor.value = DEFAULT_STARTING_VALUE;
    c->last_available_id = DEFAULT_STARTING_VALUE;
    c->current_id = DEFAULT_STARTING_VALUE;
    c->eof_count = 0;
#if VRT_QUEUE_STATS
    c->batch_count = 0;
    c->yield_count = 0;
#endif
    return c;

error:
    if (c->name != NULL) {
        cork_strfree(c->name);
    }

    cork_array_done(&c->dependencies);
    free(c);
    return NULL;
}

void
vrt_consumer_free(struct vrt_consumer *c)
{
    if (c->name != NULL) {
        cork_strfree(c->name);
    }

    if (c->yield != NULL) {
        vrt_yield_strategy_free(c->yield);
    }

    cork_array_done(&c->dependencies);
    free(c);
}

#define vrt_consumer_find_last_dependent_id(c) \
    (vrt_minimum_cursor(&(c)->dependencies))

/* Retrieves the next value from the consumer's queue.  When this
 * returnc->current_id will be the ID of the next value.  You can
 * retrieve the value using vrt_queue_get. */
static int
vrt_consumer_next_raw(struct vrt_queue *q, struct vrt_consumer *c)
{
    /* We've just finished processing the current_id'th value. */
    vrt_value_id  last_consumed_id = c->current_id++;

    /* If we know there are values available that we haven't yet
     * consumed, go ahead and return one. */
    if (vrt_mod_le(c->current_id, c->last_available_id)) {
        return 0;
    }

    /* We've run out of values that we know can been processed.  Notify
     * the world how much we've processed so far. */
    vrt_consumer_set_cursor(c, last_consumed_id);

    /* Check to see if there are any more values that we can process. */
    if (cork_array_is_empty(&c->dependencies)) {
        DEBUG("[%s] %s: Waiting for value %d from queue\n",
              q->name, c->name, c->current_id);
        /* If we don't have any dependenciewe check the queue itself to
         * see how many values have been published. */
        bool  first = true;
        vrt_value_id  last_available_id =
            vrt_queue_get_cursor(q);
        while (vrt_mod_le(last_available_id, last_consumed_id)) {
#if VRT_QUEUE_STATS
            c->yield_count++;
#endif
            rii_check(vrt_yield_strategy_yield
                      (c->yield, first, q->name, c->name));
            first = false;
            last_available_id = vrt_queue_get_cursor(q);
        }
        c->last_available_id = last_available_id;
    } else {
        DEBUG("[%s] %s: Waiting for value %d from dependencies\n",
              q->name, c->name, c->current_id);
        /* If there are dependenciewe can only process what they've
         * *all* finished processing. */
        bool  first = true;
        vrt_value_id  last_available_id =
            vrt_consumer_find_last_dependent_id(c);
        while (vrt_mod_le(last_available_id, last_consumed_id)) {
#if VRT_QUEUE_STATS
            c->yield_count++;
#endif
            rii_check(vrt_yield_strategy_yield
                      (c->yield, first, q->name, c->name));
            first = false;
            last_available_id = vrt_consumer_find_last_dependent_id(c);
        }
        c->last_available_id = last_available_id;
    }

#if VRT_QUEUE_STATS
    c->batch_count++;
#endif

    /* Once we fall through to here, we know that there are additional
     * values that we can process. */
    DEBUG("[%s] %s: Value %d ready for processing\n",
          q->name, c->name, c->last_available_id);
    return 0;
}

int
vrt_consumer_next(struct vrt_consumer *c, struct vrt_value **value)
{
    do {
        unsigned int  producer_count;
        struct vrt_value  *v;
        rii_check(vrt_consumer_next_raw(c->queue, c));
        v = vrt_queue_get(c->queue, c->current_id);

        switch (v->special) {
            case VRT_VALUE_NONE:
                DEBUG("[%s] %s: Processing value %d\n",
                      c->queue->name, c->name, c->current_id);
                *value = v;
                return 0;

            case VRT_VALUE_EOF:
                producer_count = cork_array_size(&c->queue->producers);
                c->eof_count++;
                DEBUG("[%s] %s: Detected EOF (%u of %u) at value %d\n",
                      c->queue->name, c->name,
                      c->eof_count, producer_count, c->current_id);

                if (c->eof_count == producer_count) {
                    /* We've run out of values that we know can been
                     * processed.  Notify the world how much we've
                     * processed so far. */
                    vrt_consumer_set_cursor(c, c->current_id);
                    return VRT_QUEUE_EOF;
                } else {
                    /* There are other producers still producing values,
                     * so we should repeat the loop to grab the next
                     * value. */
                    break;
                }

            case VRT_VALUE_HOLE:
                /* Repeat the loop to grab the next value. */
                break;

            case VRT_VALUE_FLUSH:
                /* Return the FLUSH control message. */
                return VRT_QUEUE_FLUSH;

            default:
                cork_unreachable();
        }
    } while (true);
}

void
vrt_report_consumer(struct vrt_consumer *c)
{
#if VRT_QUEUE_STATS
    printf("Consumer %s:\n"
           "  Batches: %zu (%.3lf records/batch)\n"
           "  Yields:  %zu\n",
           c->name, c->batch_count,
           ((double) c->last_produced_id) / (c->batch_count),
           c->yield_count);
#endif
}
