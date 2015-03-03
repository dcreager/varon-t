/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2012-2015, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the COPYING file in this distribution for license details.
 * ----------------------------------------------------------------------
 */

#ifndef VRT_QUEUE_H
#define VRT_QUEUE_H

#include <bowsprit.h>
#include <libcork/core.h>
#include <libcork/ds.h>

#include <vrt/atomic.h>
#include <vrt/value.h>
#include <vrt/yield.h>


/*-----------------------------------------------------------------------
 * Tests
 */

/* This function sets some internal state so that we exercise more interesting
 * behavior during test cases. */
void
vrt_testing_mode(void);


/*-----------------------------------------------------------------------
 * Queues
 */

/** The result code used to signify that no more data is will be sent through a
 * queue. */
#define VRT_QUEUE_EOF  -2

/** The result code used to signify that an upstream producer has requested a
 * FLUSH. */
#define VRT_QUEUE_FLUSH  -3

struct vrt_producer;
struct vrt_consumer;

typedef cork_array(struct vrt_producer *)  vrt_producer_array;
typedef cork_array(struct vrt_consumer *)  vrt_consumer_array;

/** A FIFO queue modeled after the Java Disruptor project. */
struct vrt_queue {
    /** The array of values managed by this queue. */
    struct vrt_value  **values;

    /** One less than the size of this queue.  The actual value count
     * will always be a power of 2, so this value will always be an
     * AND-mask that lets you easily calculate (x % value_count). */
    unsigned int  value_mask;

    /** The type of values managed by this queue. */
    struct vrt_value_type  *value_type;

    /** The producers feeding this queue. */
    vrt_producer_array  producers;

    /** The consumers feeding this queue. */
    vrt_consumer_array  consumers;

    /** The last item that we know every consumer has finished
     * processing. */
    vrt_value_id  last_consumed_id;

    /** The last item that has been claimed by a producer.  This will
     * only be updated if we have multiple producers; if there's only
     * one, it doesn't need to coordinate with anyone, and keeps track
     * of its last claimed value internally. */
    struct vrt_padded_int  last_claimed_id;

    /** The next value ID that can be written into the queue. */
    struct vrt_padded_int  cursor;

    /** A name for the queue */
    const char  *name;

    /* A Bowsprit context for this queue */
    struct bws_ctx  *ctx;
};

/** Allocate a new queue. */
struct vrt_queue *
vrt_queue_new(const char *name, struct vrt_value_type *value_type,
              unsigned int value_count);

/** Free a queue. */
void
vrt_queue_free(struct vrt_queue *q);

/* Have the queue keep track of various statistics using the given Bowsprit
 * context. */
void
vrt_queue_set_bws_ctx(struct vrt_queue *q, struct bws_ctx *ctx);

/* Compare two integers on the modular-arithmetic ring that fits into an int. */
#define vrt_mod_lt(a, b) (0 < ((b)-(a)))
#define vrt_mod_le(a, b) (0 <= ((b)-(a)))

/** Return the number of values managed by the queue. */
#define vrt_queue_size(q) \
    ((q)->value_mask + 1)

/** Retrieve the value with the given ID. */
#define vrt_queue_get(q, id) \
    ((q)->values[(id) & (q)->value_mask])

/** Return the ID of the value that was most recently published into the
 * queue.  This function involves a memory barrier, and so it should be
 * called sparingly. */
CORK_ATTR_UNUSED
static inline vrt_value_id
vrt_queue_get_cursor(struct vrt_queue *q)
{
    return vrt_padded_int_get(&q->cursor);
}

/** Set the ID of the value that was most recently published into the
 * queue.  This function involves a memory barrier, and so it should be
 * called sparingly.  Moreover, it's an interal method; client code
 * shouldn't need to call this directly. */
CORK_ATTR_UNUSED
static inline void
vrt_queue_set_cursor(struct vrt_queue *q, vrt_value_id value)
{
    vrt_padded_int_set(&q->cursor, value);
}


/*-----------------------------------------------------------------------
 * Producers
 */

/**
 * A producer is an object that feeds values into a queue.  The queue
 * manages the storage of the objects, however, so a producer works by
 * "claiming" the next free object in the queue.  It then fills in the
 * object as needed, and finally "publishes" the filled-in object, which
 * makes it available to the queue's consumers.  The object is
 * considered live until all consumers inform the queue that they're
 * done with the object.  At that point, the value's slot in the queue's
 * array can be reused by another object.
 */
struct vrt_producer {
    /** The queue that this producer feeds */
    struct vrt_queue  *queue;

    /** The index of this producer within its queue */
    unsigned int  index;

    /** The ID of the last value that was returned by the producer. */
    vrt_value_id  last_produced_id;

    /**
     * The ID of the last value that's currently claimed by the
     * producer.  This field is undefined if the producer hasn't claimed
     * a value yet.
     */
    vrt_value_id  last_claimed_id;

    /**
     * The function that the producer will use to claim a value ID from
     * the queue.  This is filled in by the vrt_queue_add_producer
     * function.  The particular function used depends on how many
     * producers are connected to the queue.  (If there's only one, we
     * can use a faster implementation.)
     *
     * This function won't return until there's a value to give to the
     * producer.  If the queue is currently full, this function will
     * call the producer's yield method to allow other producers and
     * consumers to run.
     */
    int
    (*claim)(struct vrt_queue *q, struct vrt_producer *self);

    /**
     * The function that the producer will use to publish a value ID to
     * the queue.  This is filled in by the vrt_queue_add_producer
     * function.  The particular function used depends on how many
     * producers are connected to the queue.  (If there's only one, we
     * can use a faster implementation.)
     */
    int
    (*publish)(struct vrt_queue *q, struct vrt_producer *self,
               vrt_value_id last_published_id);

    /** The number of values to claim at once. */
    unsigned int  batch_size;

    /** The yield strategy to use when the producer operations would
     * block. */
    struct vrt_yield_strategy  *yield;

    /** A name for the producer */
    const char  *name;

    /* The number of queue slots that we've claimed */
    struct bws_derive  *claims;

    /* The number of batches that we've claimed */
    struct bws_derive  *claimed_batches;

    /* The number of flush messages that we've published */
    struct bws_derive  *flushes;

    /* The number of queue slots that we've marked as holes as part of flushing
     * a batch */
    struct bws_derive  *flushed_holes;

    /* The number of values that we've published */
    struct bws_derive  *publishes;

    /* The number of batches that we've published */
    struct bws_derive  *published_batches;

    /* The number of claimed queue slots that we've skipped without filling with
     * a value */
    struct bws_derive  *skips;

    /* The number of times that we've yielded during a blocking operation */
    struct bws_derive  *yields;
};

/** Allocate a new producer that will feed the given queue.  The
 * producer will claim batch_size values at a time.  If batch_size is 0,
 * then we'll calculate a reasonable default batch size. */
struct vrt_producer *
vrt_producer_new(const char *name, unsigned int batch_size,
                 struct vrt_queue *q);

/** Free a producer */
void
vrt_producer_free(struct vrt_producer *p);

/** Claim the next value managed by the producer's queue.  If this
 * returns without an error, a value instance will be loaded into @ref
 * value.  The caller has full control over the contents of this value. */
int
vrt_producer_claim(struct vrt_producer *p, struct vrt_value **value);

/** Publish the most recently claimed value.  This function won't return
 * until the value is successfully published to the queue's consumers.
 * Once this function returns, the caller no longer has any rights to
 * the claimed value.  (Even for reading!)  The queue is allowed to
 * overwrite its contents at will. */
int
vrt_producer_publish(struct vrt_producer *p);

/** Skip the value that was just claimed. */
int
vrt_producer_skip(struct vrt_producer *p);

/** Signal that this producer won't produce any more values. */
int
vrt_producer_eof(struct vrt_producer *p);

int
vrt_producer_flush(struct vrt_producer *p);


/*-----------------------------------------------------------------------
 * Consumers
 */

/**
 * A consumer is an object that drains values from a queue.  The
 * consumer must check the queue's cursor to see determine the ID of the
 * most recently published value.  The consumer also maintains the ID of
 * the last value that it extracted.  This allows the consumer to know
 * which entries in the queue are safe to be read.
 *
 * The producers of the queue peek at each consumer's cursor to
 * determine when it's safe to write to the queue.  (This is how we
 * protect against wrapping around within the queue's ring buffer.)
 * This means that access to the consumer's cursor must be thread-safe.
 * You must *never* access the cursor field directly; you must *always*
 * use the vrt_consumer_get_cursor and vrt_consumer_set_cursor
 * functions.
 *
 * One simplifying assumption that we have to make is that when the
 * consumer's client calls next_entry, it has completely finished with
 * all previous values.  You cannot save the value pointer to be used
 * later on, since it will almost certainly be overwritten later on by a
 * different value.  The consumer's client is responsible for extracting
 * any needed contents and stashing them into some other bit of storage
 * before retrieving the next value.
 */
struct vrt_consumer {
    /** The queue that this consumer feeds */
    struct vrt_queue  *queue;

    /** The index of this consumer within its queue */
    unsigned int  index;

    /** The last value that we've told that world that we've finished
     * consuming */
    struct vrt_padded_int  cursor;

    /** The last value that we know is available for processing.  This
     * field is not thread-safe, and allows us to process a chunk of
     * values without yielding. */
    vrt_value_id  last_available_id;

    /** The value that's currently being consumed. */
    vrt_value_id  current_id;

    /** The number of EOFs seen by this consumer. */
    unsigned int  eof_count;

    /** Any consumers that this consumer depends on.  This consumer
     * won't be allowed to process a value until all of its dependent
     * consumers have processed it. */
    vrt_consumer_array  dependencies;

    /** The yield strategy to use when the consumer operations would
     * block. */
    struct vrt_yield_strategy  *yield;

    /** A name for the consumer */
    const char  *name;

    /* The number of queue slots that we've consumed */
    struct bws_derive  *consumed;

    /* The number of EOF messages we've received */
    struct bws_derive  *eofs;

    /* The number of flush messages we've received */
    struct bws_derive  *flushes;

    /* The number of holes that we've received */
    struct bws_derive  *holes;

    /* The number of batches that we've received */
    struct bws_derive  *received_batches;

    /* The number of regular values that we've received */
    struct bws_derive  *values;

    /* The number of times that we've yielded during a blocking operation */
    struct bws_derive  *yields;
};

/** Allocate a new consumer that will drain the given queue. */
struct vrt_consumer *
vrt_consumer_new(const char *name, struct vrt_queue *q);

/** Free a consumer */
void
vrt_consumer_free(struct vrt_consumer *c);

/** Adds a dependency to a consumer */
#define vrt_consumer_add_dependency(c1, c2) \
    (cork_array_append(&(c1)->dependencies, (c2)))

/** Retrieve the next value from the consumer's queue.  If this function
 * returns successfully, then @ref value will be filled in with the next
 * value in the queue.  The caller then has full read access to the
 * contents of that value.  The value instance will only be valid until
 * the next call to @c vrt_consumer_next.  At that point, the queue
 * is free to overwrite the contents of the value at will. */
int
vrt_consumer_next(struct vrt_consumer *c, struct vrt_value **value);

/** Return the ID of the value that was most recently processed by this
 * consumer.  This function involves a memory barrier, and so it should
 * be called sparingly. */
CORK_ATTR_UNUSED
static inline vrt_value_id
vrt_consumer_get_cursor(struct vrt_consumer *c)
{
    return vrt_padded_int_get(&c->cursor);
}

/** Set the ID of the value that was most recently processed by this
 * consumer.  This function involves a memory barrier, and so it should
 * be called sparingly.  Moreover, it's an interal method; client code
 * shouldn't need to call this directly. */
CORK_ATTR_UNUSED
static inline void
vrt_consumer_set_cursor(struct vrt_consumer *c, vrt_value_id value)
{
    vrt_padded_int_set(&c->cursor, value);
}


#endif /* VRT_QUEUE_H */
