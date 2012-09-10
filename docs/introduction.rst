.. _introduction:

Introduction
============

*Message passing* is currently a popular approach for implementing
concurrent data processing applications.  In this model, you decompose
a large processing task into separate steps that execute concurrently
and communicate solely by passing messages or data items between one
another.  This concurrency model is an intuitive way to structure a large
processing task to exploit parallelism in a shared memory environment
without incurring the complexity and overhead costs associated with
multi-threaded applications.

In order to use a message passing model, you need an efficient data
structure for passing messages between the processing elements of your
application.  A common approach is to utilize queues for storing and
retrieving messages. *Varon-T* is a C library that implements a *disruptor
queue* (originally implemented in the `Disruptor`_ Java library), which
is a particularly efficient `FIFO queue`_ implementation.  Disruptor
queues achieve their efficiency through a number of related techniques:

- Objects are stored in a *ring buffer*, which uses a fixed amount of
  memory regardless of the number of data records processed.

- Objects are stored directly *inline* in the slots of the ring buffer,
  and their life cycle is controlled by the disruptor queue, not the
  application.  This eliminates any per-record memory allocation
  overhead.

- The ring buffer's storage is implemented as a regular C array, so the
  data instances are all adjacent to each other in memory.  This allows
  us to take advantage of cache striding and locality.

- In most cases, the producers and consumers of a queue are coordinated
  without needing any costly locks, or even atomic CAS operations.  Instead,
  they only require relatively cheap *memory barriers*.

- Multiple consumers are able to drain a single queue, even when there's
  a temporal constraint on the consumers --- i.e., where a "downstream"
  consumer must wait to process a particular record until an "upstream"
  consumer has finished with it.  By sharing a single queue, you
  eliminate additional memory allocations and copies.

.. _Disruptor: http://code.google.com/p/disruptor/
.. _FIFO queue: http://en.wikipedia.org/wiki/Queue_(data_structure)
