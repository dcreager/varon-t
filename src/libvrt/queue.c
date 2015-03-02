/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012-2015, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#include <assert.h>

#include <bowsprit.h>
#include <clogger.h>
#include <libcork/core.h>
#include <libcork/ds.h>
#include <libcork/helpers/errors.h>

#include "vrt/atomic.h"
#include "vrt/queue.h"
#include "vrt/yield.h"

#define CLOG_CHANNEL  "vrt"


#define MINIMUM_QUEUE_SIZE  16
#define DEFAULT_QUEUE_SIZE  65536
#define DEFAULT_BATCH_SIZE  4096


/*-----------------------------------------------------------------------
 * Tests
 */

static unsigned int  starting_value = 0;

void
vrt_testing_mode(void)
{
    starting_value = INT_MAX - (2 * DEFAULT_BATCH_SIZE);
}


/*-----------------------------------------------------------------------
 * Dummy statistics
 */

/* If a queue client doesn't provide us with a Bowsprit context, we can't create
 * any statistics objects.  But, we want to make sure that each bws_derive
 * pointer in our producers and consumers always points at a valid object, so
 * that we can blindly increase them without having to add a bunch of `!= NULL`
 * checks.  This dummy instance is what we use as a fallback.  We never read its
 * contents, so we can reuse it for all of the queues that don't have a Bowsprit
 * context. */

static struct bws_derive  dummy_derive;


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
    q->ctx = NULL;

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
    q->last_consumed_id = starting_value;
    q->last_claimed_id.value = q->last_consumed_id;
    q->cursor.value = q->last_consumed_id;
    q->value_type = value_type;

    q->values = cork_calloc(value_count, sizeof(struct vrt_value *));
    clog_debug("[%s] Create queue with %u entries", q->name, value_count);

    cork_pointer_array_init(&q->producers, (cork_free_f) vrt_producer_free);
    cork_pointer_array_init(&q->consumers, (cork_free_f) vrt_consumer_free);

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
    unsigned int  value_count = q->value_mask + 1;

    if (q->name != NULL) {
        cork_strfree(q->name);
    }

    cork_array_done(&q->producers);
    cork_array_done(&q->consumers);

    if (q->values != NULL) {
        for (i = 0; i < value_count; i++) {
            if (q->values[i] != NULL) {
                vrt_value_free(q->value_type, q->values[i]);
            }
        }

        cork_cfree(q->values, value_count, sizeof(struct vrt_value *));
    }

    cork_delete(struct vrt_queue, q);
}

void
vrt_queue_set_bws_ctx(struct vrt_queue *q, struct bws_ctx *ctx)
{
    q->ctx = ctx;
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
    vrt_value_id  wrapped_id = p->last_claimed_id - vrt_queue_size(q);
    if (vrt_mod_lt(q->last_consumed_id, wrapped_id)) {
        clog_debug("<%s> Wait for value %d to be consumed",
                   p->name, wrapped_id);
        vrt_value_id  minimum = vrt_queue_find_last_consumed_id(q);
        while (vrt_mod_lt(minimum, wrapped_id)) {
            clog_trace("<%s> Last consumed value is %d (wait)",
                       p->name, minimum);
            bws_derive_inc(p->yields);
            rii_check(vrt_yield_strategy_yield
                      (p->yield, first, q->name, p->name));
            first = false;
            minimum = vrt_queue_find_last_consumed_id(q);
        }
        bws_derive_inc(p->claimed_batches);
        q->last_consumed_id = minimum;
        clog_debug("<%s> Last consumed value is %d", p->name, minimum);
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
        clog_trace("<%s> Claim value %d (single-threaded)",
                   p->name, p->last_claimed_id);
    } else {
        clog_trace("<%s> Claim values %d-%d (single-threaded)",
                   p->name, p->last_claimed_id - p->batch_size + 1,
                   p->last_claimed_id);
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
        clog_trace("<%s> Claim value %d (multi-threaded)",
                   p->name, p->last_claimed_id);
    } else {
        clog_trace("<%s> Claim values %d-%d (multi-threaded)",
                   p->name, p->last_produced_id + 1, p->last_claimed_id);
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
    clog_debug("<%s> Signal publication of value %d (single-threaded)",
               p->name, last_published_id);
    vrt_queue_set_cursor(q, last_published_id);
    return 0;
}

static int
vrt_publish_multi_threaded(struct vrt_queue *q, struct vrt_producer *p,
                           vrt_value_id last_published_id)
{
    bool  first = true;
    vrt_value_id  expected_cursor;
    vrt_value_id  current_cursor;

    /* If there are multiple publisherthen we have to wait until all
     * of the values before the chunk that we claimed have been
     * published.  (If we don't, there will be a hole in the sequence of
     * published records.) */
    expected_cursor = last_published_id - p->batch_size;
    current_cursor = vrt_queue_get_cursor(q);
    clog_debug("<%s> Wait for value %d to be published",
               p->name, expected_cursor);

    while (vrt_mod_lt(current_cursor, expected_cursor)) {
        clog_trace("<%s> Last published value is %d (wait)",
                   p->name, current_cursor);
        bws_derive_inc(p->yields);
        rii_check(vrt_yield_strategy_yield
                  (p->yield, first, q->name, p->name));
        first = false;
        current_cursor = vrt_queue_get_cursor(q);
    }

    clog_debug("<%s> Last published value is %d", p->name, current_cursor);
    clog_debug("<%s> Signal publication of value %d (multi-threaded)",
               p->name, last_published_id);
    vrt_queue_set_cursor(q, last_published_id);
    return 0;
}

static int
vrt_queue_add_producer(struct vrt_queue *q, struct vrt_producer *p)
{
    clog_debug("[%s] Add producer %s", q->name, p->name);

    /* Add the producer to the queue's array and assign its index. */
    cork_array_append(&q->producers, p);
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
    clog_debug("[%s] Add consumer %s", q->name, c->name);

    /* Add the consumer to the queue's array and assign its index. */
    cork_array_append(&q->consumers, c);
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
    struct vrt_producer  *p;
    unsigned int  maximum_batch_size;

    p = cork_new(struct vrt_producer);
    memset(p, 0, sizeof(struct vrt_producer));

    p->name = cork_strdup(name);
    ei_check(vrt_queue_add_producer(q, p));

    if (batch_size == 0) {
        batch_size = DEFAULT_BATCH_SIZE;
    }
    maximum_batch_size = vrt_queue_size(q) / 4;
    if (batch_size > maximum_batch_size) {
        batch_size = maximum_batch_size;
    }
    clog_trace("<%s> Batch size is %u", name, batch_size);

    p->last_produced_id = starting_value;
    p->last_claimed_id = starting_value;
    p->batch_size = batch_size;
    p->yield = NULL;

    if (q->ctx == NULL) {
        p->claims = &dummy_derive;
        p->claimed_batches = &dummy_derive;
        p->flushes = &dummy_derive;
        p->flushed_holes = &dummy_derive;
        p->publishes = &dummy_derive;
        p->published_batches = &dummy_derive;
        p->skips = &dummy_derive;
        p->yields = &dummy_derive;
    } else {
        struct bws_plugin  *plugin = bws_plugin_new(q->ctx, q->name, p->name);
        p->claims =
            bws_derive_new(plugin, "total_objects", "claims");
        p->claimed_batches =
            bws_derive_new(plugin, "total_objects", "claimed_batches");
        p->flushes =
            bws_derive_new(plugin, "total_objects", "flushes");
        p->flushed_holes =
            bws_derive_new(plugin, "total_objects", "flushed_holes");
        p->publishes =
            bws_derive_new(plugin, "total_objects", "publishes");
        p->published_batches =
            bws_derive_new(plugin, "total_objects", "published_batches");
        p->yields =
            bws_derive_new(plugin, "contextswitch", NULL);
    }

    return p;

error:
    if (p->name != NULL) {
        cork_strfree(p->name);
    }

    cork_delete(struct vrt_producer, p);
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

    cork_delete(struct vrt_producer, p);
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
    clog_trace("<%s> Claimed value %d (%d is available)\n",
               p->name, p->last_produced_id, p->last_claimed_id);
    return 0;
}

int
vrt_producer_claim(struct vrt_producer *p, struct vrt_value **value)
{
    struct vrt_value  *v;
    bws_derive_inc(p->claims);
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
    bws_derive_inc(p->publishes);
    if (p->last_produced_id == p->last_claimed_id) {
        bws_derive_inc(p->published_batches);
        return p->publish(p->queue, p, p->last_claimed_id);
    } else {
        clog_trace("<%s> Wait to publish %d until end of batch (at %d)",
                   p->name, p->last_produced_id, p->last_claimed_id);
        return 0;
    }
}

int
vrt_producer_skip(struct vrt_producer *p)
{
    struct vrt_value  *v;
    bws_derive_inc(p->skips);
    clog_trace("<%s> Skip %d", p->name, p->last_produced_id);
    v = vrt_queue_get(p->queue, p->last_produced_id);
    v->special = VRT_VALUE_HOLE;
    return vrt_producer_publish(p);
}

int
vrt_producer_flush(struct vrt_producer *p)
{
    struct vrt_value  *v;
    bws_derive_inc(p->flushes);

    if (p->last_produced_id == p->last_claimed_id) {
        /* We don't have any queue entries that we've claimed but haven't used,
         * so there's nothing to flush. */
        return 0;
    }

    /* Claim a value to fill in a FLUSH control message. */
    rii_check(vrt_producer_claim_raw(p->queue, p));
    clog_trace("<%s> Flush %d", p->name, p->last_produced_id);
    v = vrt_queue_get(p->queue, p->last_produced_id);
    v->special = VRT_VALUE_FLUSH;

    /* If we've claimed more value than we've produced, fill in the
     * remainder with holes. */
    if (vrt_mod_lt(p->last_produced_id, p->last_claimed_id)) {
        vrt_value_id  i;
        clog_trace("<%s> Holes %d-%d",
                   p->name, p->last_produced_id + 1, p->last_claimed_id);
        for (i = p->last_produced_id + 1;
             vrt_mod_le(i, p->last_claimed_id); i++) {
            struct vrt_value  *v = vrt_queue_get(p->queue, i);
            v->id = i;
            v->special = VRT_VALUE_HOLE;
            bws_derive_inc(p->flushed_holes);
        }
        p->last_produced_id = p->last_claimed_id;
    }

    /* Then publish the whole chunk. */
    bws_derive_inc(p->published_batches);
    return p->publish(p->queue, p, p->last_claimed_id);
}

int
vrt_producer_eof(struct vrt_producer *p)
{
    struct vrt_value  *v;
    rii_check(vrt_producer_claim_raw(p->queue, p));
    clog_debug("<%s> EOF %d", p->name, p->last_produced_id);
    v = vrt_queue_get(p->queue, p->last_produced_id);
    v->id = p->last_produced_id;
    v->special = VRT_VALUE_EOF;
    rii_check(vrt_producer_publish(p));
    return vrt_producer_flush(p);
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
    c->cursor.value = starting_value;
    c->last_available_id = starting_value;
    c->current_id = starting_value;
    c->eof_count = 0;

    if (q->ctx == NULL) {
        c->consumed = &dummy_derive;
        c->eofs = &dummy_derive;
        c->flushes = &dummy_derive;
        c->holes = &dummy_derive;
        c->received_batches = &dummy_derive;
        c->values = &dummy_derive;
        c->yields = &dummy_derive;
    } else {
        struct bws_plugin  *plugin = bws_plugin_new(q->ctx, q->name, c->name);
        c->consumed =
            bws_derive_new(plugin, "total_objects", "consumed");
        c->eofs =
            bws_derive_new(plugin, "total_objects", "eofs");
        c->flushes =
            bws_derive_new(plugin, "total_objects", "flushes");
        c->holes =
            bws_derive_new(plugin, "total_objects", "holes");
        c->received_batches =
            bws_derive_new(plugin, "total_objects", "received_batches");
        c->values =
            bws_derive_new(plugin, "total_objects", "values");
        c->yields =
            bws_derive_new(plugin, "contextswitch", NULL);
    }

    return c;

error:
    if (c->name != NULL) {
        cork_strfree(c->name);
    }

    cork_array_done(&c->dependencies);
    cork_delete(struct vrt_consumer, c);
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
    cork_delete(struct vrt_consumer, c);
}

#define vrt_consumer_find_last_dependent_id(c) \
    (vrt_minimum_cursor(&(c)->dependencies))

/* Retrieves the next value from the consumer's queue.  When this
 * returns c->current_id will be the ID of the next value.  You can
 * retrieve the value using vrt_queue_get. */
static int
vrt_consumer_next_raw(struct vrt_queue *q, struct vrt_consumer *c)
{
    /* We've just finished processing the current_id'th value. */
    vrt_value_id  last_consumed_id = c->current_id++;

    /* If we know there are values available that we haven't yet
     * consumed, go ahead and return one. */
    if (vrt_mod_le(c->current_id, c->last_available_id)) {
        clog_trace("<%s> Next value is %d (already available)",
                   c->name, c->current_id);
        bws_derive_inc(c->consumed);
        return 0;
    }

    /* We've run out of values that we know can been processed.  Notify
     * the world how much we've processed so far. */
    clog_debug("<%s> Signal consumption of %d", c->name, last_consumed_id);
    vrt_consumer_set_cursor(c, last_consumed_id);

    /* Check to see if there are any more values that we can process. */
    if (cork_array_is_empty(&c->dependencies)) {
        bool  first = true;
        vrt_value_id  last_available_id;
        clog_debug("<%s> Wait for value %d", c->name, c->current_id);

        /* If we don't have any dependencies check the queue itself to see how
         * many values have been published. */
        last_available_id = vrt_queue_get_cursor(q);
        while (vrt_mod_le(last_available_id, last_consumed_id)) {
            clog_trace("<%s> Last available value is %d (wait)",
                       c->name, last_available_id);
            bws_derive_inc(c->yields);
            rii_check(vrt_yield_strategy_yield
                      (c->yield, first, q->name, c->name));
            first = false;
            last_available_id = vrt_queue_get_cursor(q);
        }
        c->last_available_id = last_available_id;
        clog_debug("<%s> Last available value is %d",
                   c->name, last_available_id);
    } else {
        bool  first = true;
        vrt_value_id  last_available_id;
        clog_debug("<%s> Wait for value %d from dependencies",
                   c->name, c->current_id);

        /* If there are dependencies we can only process what they've *all*
         * finished processing. */
        last_available_id = vrt_consumer_find_last_dependent_id(c);
        while (vrt_mod_le(last_available_id, last_consumed_id)) {
            clog_trace("<%s> Last available value is %d (wait)",
                       c->name, last_available_id);
            bws_derive_inc(c->yields);
            rii_check(vrt_yield_strategy_yield
                      (c->yield, first, q->name, c->name));
            first = false;
            last_available_id = vrt_consumer_find_last_dependent_id(c);
        }
        c->last_available_id = last_available_id;
        clog_debug("<%s> Last available value is %d",
                   c->name, last_available_id);
    }

    bws_derive_inc(c->received_batches);

    /* Once we fall through to here, we know that there are additional
     * values that we can process. */
    clog_trace("<%s> Next value is %d", c->name, c->current_id);
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
                bws_derive_inc(c->values);
                *value = v;
                return 0;

            case VRT_VALUE_EOF:
                bws_derive_inc(c->eofs);
                producer_count = cork_array_size(&c->queue->producers);
                c->eof_count++;
                clog_debug("<%s> Detected EOF (%u of %u) at value %d",
                           c->name, c->eof_count, producer_count,
                           c->current_id);

                if (c->eof_count == producer_count) {
                    /* We've run out of values that we know can been
                     * processed.  Notify the world how much we've
                     * processed so far. */
                    clog_debug("<%s> Signal consumption of %d",
                               c->name, c->current_id);
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
                bws_derive_inc(c->holes);
                break;

            case VRT_VALUE_FLUSH:
                /* Return the FLUSH control message. */
                bws_derive_inc(c->flushes);
                return VRT_QUEUE_FLUSH;

            default:
                cork_unreachable();
        }
    } while (true);
}
