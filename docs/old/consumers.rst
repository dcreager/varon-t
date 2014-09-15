.. _consumers:

.. highlight:: c

Consumers
=========

A consumer is a disruptor queue client that processes or "drains" values from
the queue. A disruptor queue may more than one consumer in a typical
application. Each consumer must check the queue's cursor to determine the ID
of the recently published value instance, and each must maintain an ID of the
last value instance that is extracted. This is the mechanism consumers use to
ensure safe processing of value instances in the queue.

We protect against wrapping around the queue's ring buffer by enabling
producers with an ability to peek at each consumer's cursor. This implies that
access to a consumer's cursor must be thread-safe. Consumers never accesss
their cursors directly. They must always use :c:func:`vrt_consumer_get_cursor`
and :c:func:`vrt_consumer_set_cursor`, and then very sparingly.

Varon-T disruptor queue consumers adhere to the following interface:

.. type:: struct vrt_consumer

    .. member:: const char \*name

        An identifier name for this consumer.

    .. member:: struct vrt_queue  \*queue

        The disruptor queue that feeds this consumer

    .. member:: unsigned int  index

        The index of this consumer within the queue

    .. member:: struct vrt_padded_int  cursor

        The last value publically acknowledged as consumed by the consumer. Note
        that this field is never directly access by the consumer.

    .. member:: vrt_value_id  last_available_id

        The ID of the last value instance guaranteed available for processing.
        This field is **not** thread-safe and allows the consumer to process
        a group of value instances without yielding.

    .. member:: vrt_value_id  current_id

        The ID of the value instance currently consumed.

    .. member:: unsigned int  eof_count

        The number of EOFs seen by this consumer.

    .. member:: vrt_consumer_array  dependencies

        A list of consumers upon which this consumer depends. A consumer may
        not process a value instance until all dependency consumers have
        processed it.

    .. member:: struct vrt_yield_strategy  \*yield

        The yield strategy used by this consumer during a blocking operation.

    .. member:: unsigned int  batch_count

        The number of batches of values to process. Used only if
        :token:`VRT_QUEUE_STATS` is true.

    .. member:: unsigned int  yield_count

        The number of time the consumer has yield whilst waiting for a value
        instances. Used only if :token:`VRT_QUEUE_STATS` is true.


Built-in consumer operations
----------------------------

We provide the following built-in consumer operations:

.. function:: struct vrt_consumer \* vrt_consumer_new(const char \*name, struct vrt_queue \*q)

    Allocate a new consumer to process value instances from a queue.

.. function:: void vrt_consumer_free(struct vrt_consumer \*c)

    Free a consumer.

.. function:: #define vrt_consumer_add_dependency(c1, c2)

    Add a consumer dependency ``c2`` to ``c1``.

.. function:: int vrt_consumer_next(struct vrt_consumer \*c, struct vrt_value \**value)

    Retrieve the next value from the consumer's queue.  If this function
    returns successfully, then ``value`` will be filled in with the next
    value in the queue.  The caller then has full read access to the
    contents of that value.  The value instance will only be valid until
    the next call to ``vrt_consumer_next``.  At that point, the queue
    is free to overwrite the contents of the value at will.
    Client cannot save the value pointer to be used later on, since it will
    almost certainly be overwritten later on by a different value. The
    consumer's client is responsible for extracting desired contents and
    stashing them into another storage location before retrieving the next
    value.

.. function:: void vrt_report_consumer(struct vrt_consumer \*c)

    Prints statistics about the consumer's batches and yields to standard
    output.
