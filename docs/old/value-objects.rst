.. _value-objects:

.. highlight:: c

Value objects
=============

Each Varon-T disruptor queue manages a list of *values*, which are allocated
and controlled by the disruptor queue. This increases performance of the queue
and application by eliminating the need to perform allocations on a per
object basis. Another way to think of the disruptor queue is a pool of value
objects available to your applications.

The value interface is simple and is a superclass of a value managed by a
Varon-T disruptor queue.

.. type:: struct vrt_value

        An oqaque type that serves as a superclass for ring buffer values.


Each *value type* in an application must implement the following interface:

.. type:: struct vrt_value_type

    .. member:: cork_hash  type_id

        A type identifier for this value type. The :ref:`cork-hash
        <libcork:cork-hash>` utility in libcork will generate a sufficient
        hash value for this field given a string identifier.

    .. member:: struct vrt_value \*(\*new_value)(const struct vrt_value_type \*type)

        Allocates, iniatializes, and returns an instance of *type*.

    .. member:: void (\*free_value)(const struct vrt_value_type \*type, struct vrt_value \*value)

        Frees any resources used by *value*, which must be an instance of
        *type*.


.. _example_value:

Example: Integer values
-----------------------

The following is a simple implementation of a new value type for storing
integer values in a Varon-T disruptor queue.

::

    #include <libcork/core.h>

    struct vrt_integer_value {
        struct vrt_value  parent;
        int64_t  value;
    };

    static struct vrt_value *
    vrt_integer__new_value(const struct vrt_value_type *type)
    {
        struct vrt_integer_value  *self = cork_new(struct vrt_integer_value);
        return &self->parent;
    }

    static void
    vrt_integer__free_value(const struct vrt_value_type *type, struct vrt_value *value)
    {
        struct vrt_integer_value  *self =
            cork_container_of(value, struct vrt_integer_value, parent);
        free(self);
    }

    /* The following hash value is produced by the cork-hash utility function */
    #define VRT_INTEGER_TYPE 0xcd6e0682

    static struct vrt_value_type  _vrt_integer_type = {
        type = VRT_INTEGER_TYPE,
        vrt_integer__new_value,
        vrt_integer__free_value
    };

    const struct vrt_value_type *
    vrt_integer_type(void)
    {
        return &_vrt_integer_type;
    }

The implementation is straightforward and depends on the `libcork`_ library. A
few details about this implementation are worth mentioning:

* The implementation uses embedded C ``structs`` to contain or "subclass" the
  :c:type:`vrt_value` type within :c:type:`vrt_integer_value`. The disruptor
  queue library can then operate efficiently on pointers to the contained or
  "superclass" ``struct``. However, your application will need a pointer to the
  container ``struct`` when given a pointer to the contained ``struct`` in order
  to perform application specific computations. That is the purpose of
  :c:func:`cork_container_of`.
* The :c:type:`_vrt_integer_type` does not require additional fields beyond
  the :c:type:`vrt_value_type` interface. Therefore, it is a ``static`` value
  type instance and accessible through :c:func:`vrt_integer_type`.

.. _libcork: http://libcork.readthedocs.org/en/latest/
