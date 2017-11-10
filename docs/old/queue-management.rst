.. _queue-management:

.. highlight:: c

Disruptor queue management
==========================

A disruptor queue in Varon-T is a high-performance ring buffer with memory
barriers and yielding strategies to coordinate producers and consumers of
value instances. A distinguishing feature of disruptor queues is that value
instances are pre-allocated by the library, usually with size of a power of 2.
The result is disruptor queues do little to no memory allocation during
execution, and your application requests access from the library to value
instances.

A Varon-T distruptor queue is defined by the following interface:

.. type:: struct vrt_queue

    .. member:: const char  \*name

        An alphanumeric name for the queue.

    .. member:: struct vrt_value  \**values

        The array of values managed by the queue (see :doc:`value objects
        <value-objects>` for a discussion of custom values.)

    .. member:: unsigned int  value_mask

        A mask that is always equal to ``|queue| - 1`` and used to determine
        the queue size efficiently through the calculation ``x % value_count``.
        This works because the actual queue size is always a power of 2.

    .. member:: const struct vrt_value_type  \*value_type

        The type of values managed by the queue (see :doc:`value objects
        <value-objects>` for a discussion of custom value types.)

    .. member:: vrt_producer_array  producers
                vrt_consumers_array  consumers

        Arrays of producers and consumers feeding this queue

    .. member:: vrt_value_id  last_consumed_id

        The ID of the last guaranteed value instanced processed by each
        consumer.

    .. member:: struct vrt_padded_int  last_claimed_id

        The ID of the last value instance claimed by a producer. This is only
        updated for disruptor queues with multiple producers. It is expected
        that single producer disruptor queues will track this value internal
        to the producer instance.

    .. member:: vrt_padded_int  cursor

        The next value instance ID that can written into the queue.


Built-in operations
-------------------

We support several built-in operations for queue management.

.. function:: struct vrt_queue \* vrt_queue_new(const char \*name, const struct vrt_value_type \*value_type, unsigned int value_count)

        Construct a new disruptor queue called ``name`` that stores
        ``value_count`` objects of type ``value_type`` in a ring buffer.

.. function:: void vrt_queue_free(struct vrt_queue \*q)

        Free the memory associated with ``q``.

.. function:: static inline vrt_value_id vrt_queue_get_cursor(struct vrt_queue \*q)

        Return the ID of the value instance that was most recently published
        into the queue. Since this function involves a memory barrier, it
        should be used sparingly.

.. function:: #define vrt_queue_size(q)

        Return the number of values managed by the queue.

.. function:: #define vrt_queue_get(q, id)

        Return the value instance with the given ID


Built-in result codes
---------------------

The following result codes are used to indicate various disruptor queue states.

.. var:: VRT_QUEUE_EOF

        Signify that no more data will be sent through the queue.

.. var:: VRT_QUEUE_FLUSH

        Signify that an upstream producer has requested a flush operation.
