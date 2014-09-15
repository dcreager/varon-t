.. _example:

.. highlight:: c

Example: Summing integers
=========================

This example uses a Varon-T disruptor queue to produce one million integers and
sum them up. The queue uses a single producer and a single consumer, each
using the :c:func:`vrt_yield_strategy_threaded` described in
:doc:`yield strategies <yield-strategies>`.

Recall that Varon-T depends on the `libcork`_ library, which is included in
the following block.

::

    #include <stdlib.h>
    #include <stdio.h>
    #include <pthread.h>

    #include <libcork/core.h>
    #include <libcork/helpers/errors.h>
    #include <vrt.h>


The first coding task is to define the integer value and value types based on
:c:type:`vrt_value` and :c:type:`vrt_value_type`. The following code
demonstrates "subclassing :c:type:`vrt_value` as an embedded C struct. The
value type, however, is provided as a static instance of
:c:type:`vrt_value_type`, which has an interface to two functions responsible
for allocating and deallocating value instances.
:c:func:`vrt_integer_value_type(void)` is a helper function for accessing
the static value type :c:type:`_vrt_integer_value_type`.

::

    /* --------------------------------------------------------------
     * Integer value and type
     */

    struct vrt_integer_value {
        struct vrt_value  parent;
        int32_t  value;
    };

    static struct vrt_value *
    vrt_integer_value_new(struct vrt_value_type *type)
    {
        struct vrt_integer_value  *self = cork_new(struct vrt_integer_value);
        return &self->parent;
    }

    static void
    vrt_integer_value_free(struct vrt_value_type *type, struct vrt_value *vself)
    {
        struct vrt_integer_value  *iself =
            cork_container_of(vself, struct vrt_integer_value, parent);
        free(iself);
    }

    static struct vrt_value_type  _vrt_integer_value_type = {
        vrt_integer_value_new,
        vrt_integer_value_free
    };

    static struct vrt_value_type *
    vrt_integer_value_type(void)
    {
        return &_vrt_integer_value_type;
    }


An integer generator is a straightforward implementation with an embedded
producer pointer and a field to store the number of integers to generate.
The `for` loop simply iterates over the number of integers to produce, claims
a value instance from the queue, populates the `value` field of the value
instance with the current integer, and publishes the value instance back to the
queue. After all integers are published, the generator then pushes an `EOF`
signal to the queue to indicate it has finished.

::

    /* --------------------------------------------------------------
     * Integer producer
     */

    struct integer_generator {
        struct vrt_producer  *p;
        int64_t  count;
    };

    void *
    generate_integers(void *ud)
    {
        struct integer_generator  *c = ud;
        int32_t  i;
        for (i = 0; i < c->count; i++) {
            struct vrt_value  *vvalue;
            struct vrt_integer_value  *ivalue;
            rpi_check(vrt_producer_claim(c->p, &vvalue));
            ivalue = cork_container_of
                        (vvalue, struct vrt_integer_value, parent);
            ivalue->value = i;
            rpi_check(vrt_producer_publish(c->p));
        }
        rpi_check(vrt_producer_eof(c->p));
        return NULL;
    }


A summing consumer is similar to the generator producer in a straightforward
implementation. A "summer" is comprised of a consumer client and field for
tracking the sum. The consumer iterates over the available value instances
in the queue until an :token:`EOF` is encountered. The :token:`value` from each 
value instance is added to the current sum.

::

    /* --------------------------------------------------------------
     * Integer consumer
     */

    struct integer_summer {
        struct vrt_consumer  *c;
        int64_t  *sum;
    };

    void *
    sum_integers(void *ud)
    {
        int rc;
        struct integer_summer  *c = ud;
        struct vrt_value  *vvalue;
        int64_t  sum = 0;
        while ((rc = vrt_consumer_next(c->c, &vvalue)) != VRT_QUEUE_EOF) {
            if (rc == 0) {
                struct vrt_integer_value  *ivalue =
                    cork_container_of(vvalue, struct vrt_integer_value, parent);
                sum += ivalue->value;
            }
        }
        if (rc == VRT_QUEUE_EOF) {
            *c->sum = sum;
        }
        return NULL;
    }


The disruptor queue is implemented where each client (producer and consumer)
executes in a separate thread. The :c:type:`vrt_queue_client` structure is
a wrapper around queue clients that generalizes :c:func:`vrt_queue_threaded`,
and it is demonstrated as a design pattern. The critical steps are thread
management (create and join) and configuration of the appropriate yield
strategies for producers and consumers.

::

    /* --------------------------------------------------------------
     * Threaded queue
     */

    struct vrt_queue_client {
        void *(*run)(void *);
        void *ud;
    };

    int
    vrt_queue_threaded(struct vrt_queue *q, struct vrt_queue_client *clients)
    {
        size_t  i;
        size_t  client_count = 0;
        struct vrt_queue_client  *client;
        for (client = clients; client->run != NULL; client++) {
            client_count++;
        }

        pthread_t  *tids;
        tids = cork_calloc(client_count, sizeof(pthread_t));

        /* Choose a yield strategy */
        for (i = 0; i < cork_array_size(&q->producers); i++) {
            struct vrt_producer  *p = cork_array_at(&q->producers, i);
            p->yield = vrt_yield_strategy_threaded();
        }

        for (i = 0; i < cork_array_size(&q->consumers); i++) {
            struct vrt_consumer  *c = cork_array_at(&q->consumers, i);
            c->yield = vrt_yield_strategy_threaded();
        }

        /* Create the client threads */
        for (i = 0; i < client_count; i++) {
            pthread_create(&tids[i], NULL, clients[i].run, clients[i].ud);
        }

        for (i = 0; i < client_count; i++) {
            pthread_join(tids[i], NULL);
        }

        free(tids);
        return 0;
    }


The main function drives the disuptor queue. After successful allocation of
the queue, producer, and consumer, the generator and summer are configured
and added to the queue as the clients. Recall that each application client
(generator and summer) has an embedded queue-specific client (producer and
consumer, respectively). The disruptor queue is invoked through the call
:c:func:`vrt_queue_threaded`. Note that :token:`result` corresponds to
:token:`sum` in :c:type:`integer_summer`.

::

    int
    main(int argc, const char **argv)
    {
        struct vrt_queue  *q;
        struct vrt_producer  *p;
        struct vrt_consumer  *c;
        int64_t  result;
        size_t  QUEUE_SIZE = 64;

        /* Note that the parameter for queue size is a power of 2. */
        rip_check(q = vrt_queue_new("queue_sum", vrt_integer_value_type(),
                                    QUEUE_SIZE));
        rip_check(p = vrt_producer_new("generator", 4, q));
        rip_check(c = vrt_consumer_new("summer", q));

        struct integer_generator  integer_generator = {
            p, 1000000
        };

        struct integer_summer  integer_summer = {
            c, &result
        };

        struct vrt_queue_client  clients[] = {
            { generate_integers, &integer_generator },
            { sum_integers, &integer_summer },
            { NULL, NULL }
        };

        rii_check(vrt_queue_threaded(q, clients));

        fprintf(stdout, "Result: %" PRId64 "\n", result);
        vrt_queue_free(q);
        return 0;
    }

.. _libcork: http://libcork.readthedocs.org/en/latest/
