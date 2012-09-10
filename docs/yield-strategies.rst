.. _yield-strategies:

.. highlight:: c

Yield strategies
================

Each producer and consumer must yield to other disruptor queue clients when an
operation will not immediately succeed. This prevents overwriting of values
and gives slower queue clients an opportunity to catch up. Custom yield
stratgies must implement the following interface:

.. type:: struct vrt_yield_strategy

    .. member:: int (\*yield)(struct vrt_yield_strategy \*self, bool first, const char \*queue_name, const char \*name)

        Yields control to other producer and consumer clients of the disruptor
        queue.

    .. member:: void (\*free)(struct vrt_yield_strategy \*self)

        Free allocated resources associated with this yield strategy


Varon-T has three built-in yielding strategies:

.. function:: struct vrt_yield_strategy \*vrt_yield_strategy_spin_wait(void)

    This simple yield strategy does a spin-loop while waiting for a queue
    operation that will succeed. This yield strategy requires each producer
    and consumer client execute in a separate thread.

.. function:: struct vrt_yield_strategy \*vrt_yield_strategy_threaded(void)

    This yield strategy uses a short spin-loop before yielding to other threads.
    It also requires each queue client execute in a separate thread.

.. function:: struct vrt_yield_strategy \*vrt_yield_strategy_hybrid(void)

    This strategy yields to other coroutines in the same thread for a initial
    wait cycles. It then utilizes more progressively intense yield loops.
