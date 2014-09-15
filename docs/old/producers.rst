.. _producers:

.. highlight:: c

Producers
=========

A producer is a queue client that feeds values into a disruptor queue. The
queue manages the allocation of memory for value instances in the queue,
however, so the produces "claims" the next free value instance in the queue.
Once claimed, the producer copies or fills in the value instance and "publishes"
the value instances, making it available for consumer clients. At the point of
publishing the value instance availability, the producer relinquishes its claim
to the value instance. The value instance is then considered active and
available until all consumer clients inform the disruptor queue they are
finished with the value instance. At this point, the disruptor queue makes the
value instance's slot in the queue array available for reuse and claim by
a producer.

A Varon-T producer client must implement the following interface:

.. type:: struct vrt_producer

    .. member:: struct vrt_queue  \*queue

        A pointer to the disruptor queue fed by this producer client.

    .. member:: unsigned int  index

        The index of this producer client within *queue*.

    .. member:: vrt_value_id  last_produced_id

        The ID of the last value instance returned by the producer.

    .. member:: vrt_value_id  last_claimed_id

        The ID of the last value instance in the distruptor queue currently
        claimed by the producer.

    .. member:: int (\*claim)(struct vrt_queue \*q, struct vrt_producer \*self)

        This is the function the producer will call to claim a value instance in
        the disruptor queue. The function blocks until there is a value to
        return to the producer. If the queue is currently full, then this
        function will call the producers's yield method to permit other
        producers and consumer clients execution time.

    .. member:: int (\*publish)(struct vrt_queue \*q, struct vrt_producer \*self, vrt_value_id last_published_id)

        This is the function the producer will use to publish a value instance
        ID to the disruptor queue.

    .. member:: unsigned int batch_size

        The number of value instances in a disruptor queue the producers will
        claim in a single call to claim.

    .. member:: struct vrt_yield_strategy \*yield

        A pointer to the function implementing the producer's
        :doc:`yield strategy <yield-strategies>`.

    .. member:: const char  \*name

        A name for the producer.

    .. member:: unsigned int  batch_count

        The number of batch value instances the producer has fed. Note that
        :token:`VRT_QUEUE_STATS` must be defined as ``true``.

    .. member:: unsigned int  yield_count

        The number of times the producer has yield whilst waiting to claim a
        value instances. Note that :token:`VRT_QUEUE_STATS` must be defined
        as ``true``.


Built-in producer operations
----------------------------

We provide the following built-in producer operations:

.. function:: struct vrt_producer \* vrt_producer_new(const char \*name, unsigned int batch_size, struct vrt_queue \*q)

    Allocate a new producer instance to feed the given queue *q* and initialize
    to claim *batch_size* values at a time. If *batch_size* is set to 0, then
    a reasonable default batch size is calculated.

.. function:: void vrt_producer_free(struct vrt_producer \*p)

    Free a producer instance and any associated resources.

.. function:: int vrt_producer_claim(struct vrt_producer \*p, struct vrt_value \**value)

    Claim the next available value instance managed by the producer's queue. If
    this funtion returns no error (*0*), then a value instance is loaded into
    *value* and the caller has complete control over its contents.

.. function:: int vrt_producer_publish(struct vrt_producer \*p)

    Publish the most recently claimed value. This function will no return until
    the value is successfully published to the queue's consumers. Immediately
    upon return, the relinquishes all rights to the claimed value, including
    for reading values. The queue has complete control and can overwrite the
    value's contents at any time.

.. function:: int vrt_producer_skip(struct vrt_producer \*p)

    Skip over the most recently claimed value.

.. function:: int vrt_producer_eof(struct vrt_producer \*p)

    Signal that this producer will no longer produce any new values for its
    queue.

.. function:: int vrt_producer_flush(struct vrt_producer \*p)

   Signals that this producer is flushing any claimed values back to the queue.

.. function:: void vrt_producer_report(struct vrt_producer \*p)

    Prints statistics about the producer's batch and yields to standard output.
