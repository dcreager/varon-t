.. _introduction:

Introduction
============

These days, *message passing* is a popular strategy for implementing
concurrent data processing applications.  In this model, you decompose
a large processing task into separate steps that can execute
concurrently, and which communicate solely by passing messages or data
items between each other.  This model is useful because it gives you an
intuitive way to structure a large processing task; but more
importantly, it also allows you to easily exploit any parallelism that
is supported by your computing infrastructure.

In order to use the message passing model, you need an efficient data
structure for passing messages between the processing elements of your
application.  Varon-T is a C library that provides the *disruptor
queue* (originally implemented in the `Disruptor`_ Java library), which
is a particularly efficient `FIFO queue`_ implementation.  Disruptor
queues achieve their efficiency through a number of related techniques:

- Objects are stored in a *circular ring buffer*, which means that we
  use a constant amount of memory, regardless of the number of data
  records that we process.

- Objects are stored directly *inline* in the slots of the ring buffer,
  and their life cycle is controlled by the disruptor queue, not the
  application.  This eliminates any per-record memory allocation
  overhead.

- The ring buffer's storage is implemented as a regular C array, so the
  data instances are all adjacent to each other in memory.  This allows
  us to take advantage of cache striding.

- In most cases, the producers and consumers of a queue can orchestrate
  with each other without needing any costly locks, or even atomic CAS
  operations.  Instead, they only require relatively cheap *memory
  barriers*.

- Multiple consumers are able to drain a single queue, even when there's
  a temporal constraint on the consumers --- i.e., where a "downstream"
  consumer must wait to process a particular record until an "upstream"
  consumer has finished with it.  By sharing a single queue, you
  eliminate additional memory allocations and copies.

.. _Disruptor: http://code.google.com/p/disruptor/
.. _FIFO queue: http://en.wikipedia.org/wiki/Queue_(data_structure)
