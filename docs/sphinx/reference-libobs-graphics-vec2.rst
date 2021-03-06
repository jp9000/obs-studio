2-Component Vector
==================

.. code:: cpp

   #include <graphics/vec2.h>

.. type:: struct vec2

   Two component vector structure.

.. member:: float vec2.x

   X component

.. member:: float vec2.y

   Y component

.. member:: float vec2.ptr[2]

   Unioned array of both components

---------------------

.. function:: void vec2_zero(struct vec2 *dst)

   Zeroes a vector

   :param dst: Destination

---------------------

.. function:: void vec2_set(struct vec2 *dst, float x, float y)

   Sets the individual components of a 2-component vector.

   :param dst: Destination
   :param x:   X component
   :param y:   Y component

---------------------

.. function:: void vec2_copy(struct vec2 *dst, const struct vec2 *v)

   Copies a vector

   :param dst: Destination
   :param v:   Vector to copy

---------------------

.. function:: void vec2_add(struct vec2 *dst, const struct vec2 *v1, const struct vec2 *v2)

   Adds two vectors

   :param dst: Destination
   :param v1:  Vector 1
   :param v2:  Vector 2

---------------------

.. function:: void vec2_sub(struct vec2 *dst, const struct vec2 *v1, const struct vec2 *v2)

   Subtracts two vectors

   :param dst: Destination
   :param v1:  Vector being subtracted from
   :param v2:  Vector being subtracted

---------------------

.. function:: void vec2_mul(struct vec2 *dst, const struct vec2 *v1, const struct vec2 *v2)

   Multiplies two vectors

   :param dst: Destination
   :param v1:  Vector 1
   :param v2:  Vector 2

---------------------

.. function:: void vec2_div(struct vec2 *dst, const struct vec2 *v1, const struct vec2 *v2)

   Divides two vectors

   :param dst: Destination
   :param v1:  Dividend
   :param v2:  Divisor

---------------------

.. function:: void vec2_addf(struct vec2 *dst, const struct vec2 *v, float f)

   Adds a floating point to all components

   :param dst: Destination
   :param dst: Vector
   :param f:   Floating point

---------------------

.. function:: void vec2_subf(struct vec2 *dst, const struct vec2 *v, float f)

   Subtracts a floating point from all components

   :param dst: Destination
   :param v:   Vector being subtracted from
   :param f:   Floating point being subtracted
   
---------------------

.. function:: void vec2_mulf(struct vec2 *dst, const struct vec2 *v, float f)

   Multiplies a floating point with all components

   :param dst: Destination
   :param dst: Vector
   :param f:   Floating point

---------------------

.. function:: void vec2_divf(struct vec2 *dst, const struct vec2 *v, float f)

   Divides a floating point from all components

   :param dst: Destination
   :param v:   Vector (dividend)
   :param f:   Floating point (divisor)

---------------------

.. function:: void vec2_neg(struct vec2 *dst, const struct vec2 *v)

   Negates a vector

   :param dst: Destination
   :param v:   Vector to negate

---------------------

.. function:: float vec2_dot(const struct vec2 *v1, const struct vec2 *v2)

   Performs a dot product between two vectors

   :param v1: Vector 1
   :param v2: Vector 2
   :return:   Result of the dot product

---------------------

.. function:: float vec2_len(const struct vec2 *v)

   Gets the length of a vector

   :param v: Vector
   :return:  The vector's length

---------------------

.. function:: float vec2_dist(const struct vec2 *v1, const struct vec2 *v2)

   Gets the distance between two vectors

   :param v1: Vector 1
   :param v2: Vector 2
   :return:   Distance between the two vectors

---------------------

.. function:: void vec2_minf(struct vec2 *dst, const struct vec2 *v, float val)

   Gets the minimum values between a vector's components and a floating point

   :param dst: Destination
   :param v:   Vector
   :param val: Floating point

---------------------

.. function:: void vec2_min(struct vec2 *dst, const struct vec2 *v, const struct vec2 *min_v)

   Gets the minimum values between two vectors

   :param dst:   Destination
   :param v:     Vector 1
   :param min_v: Vector 2

---------------------

.. function:: void vec2_maxf(struct vec2 *dst, const struct vec2 *v, float val)

   Gets the maximum values between a vector's components and a floating point

   :param dst: Destination
   :param v:   Vector
   :param val: Floating point

---------------------

.. function:: void vec2_max(struct vec2 *dst, const struct vec2 *v, const struct vec2 *max_v)

   Gets the maximum values between two vectors

   :param dst:   Destination
   :param v:     Vector 1
   :param max_v: Vector 2

---------------------

.. function:: void vec2_abs(struct vec2 *dst, const struct vec2 *v)

   Gets the absolute values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: void vec2_floor(struct vec2 *dst, const struct vec2 *v)

   Gets the floor values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: void vec2_ceil(struct vec2 *dst, const struct vec2 *v)

   Gets the ceiling values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: int vec2_close(const struct vec2 *v1, const struct vec2 *v2, float epsilon)

   Compares two vectors

   :param v1:      Vector 1
   :param v2:      Vector 2
   :param epsilon: Maximum precision for comparison

---------------------

.. function:: void vec2_norm(struct vec2 *dst, const struct vec2 *v)

   Normalizes a vector

   :param dst: Destination
   :param v:   Vector to normalize
