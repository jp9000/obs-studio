4-Component Vector
==================

.. code:: cpp

   #include <graphics/vec4.h>

.. type:: struct vec4

   Two component vector structure.

.. member:: float vec4.x

   X component

.. member:: float vec4.y

   Y component

.. member:: float vec4.z

   Z component

.. member:: float vec4.w

   W component

.. member:: float vec4.ptr[4]

   Unioned array of all components

---------------------

.. function:: void vec4_zero(struct vec4 *dst)

   Zeroes a vector

   :param dst: Destination

---------------------

.. function:: void vec4_set(struct vec4 *dst, float x, float y, float z, float w)

   Sets the individual components of a 4-component vector.

   :param dst: Destination
   :param x:   X component
   :param y:   Y component
   :param z:   Z component
   :param w:   W component

---------------------

.. function:: void vec4_copy(struct vec4 *dst, const struct vec4 *v)

   Copies a vector

   :param dst: Destination
   :param v:   Vector to copy

---------------------

.. function:: void vec4_from_vec3(struct vec4 *dst, const struct vec3 *v)

   Creates a 4-component vector from a 3-component vector

   :param dst: 4-component vector destination
   :param v:   3-component vector

---------------------

.. function:: void vec4_add(struct vec4 *dst, const struct vec4 *v1, const struct vec4 *v2)

   Adds two vectors

   :param dst: Destination
   :param v1:  Vector 1
   :param v2:  Vector 2

---------------------

.. function:: void vec4_sub(struct vec4 *dst, const struct vec4 *v1, const struct vec4 *v2)

   Subtracts two vectors

   :param dst: Destination
   :param v1:  Vector being subtracted from
   :param v2:  Vector being subtracted

---------------------

.. function:: void vec4_mul(struct vec4 *dst, const struct vec4 *v1, const struct vec4 *v2)

   Multiplies two vectors

   :param dst: Destination
   :param v1:  Vector 1
   :param v2:  Vector 2

---------------------

.. function:: void vec4_div(struct vec4 *dst, const struct vec4 *v1, const struct vec4 *v2)

   Divides two vectors

   :param dst: Destination
   :param v1:  Dividend
   :param v2:  Divisor

---------------------

.. function:: void vec4_addf(struct vec4 *dst, const struct vec4 *v, float f)

   Adds a floating point to all components

   :param dst: Destination
   :param dst: Vector
   :param f:   Floating point

---------------------

.. function:: void vec4_subf(struct vec4 *dst, const struct vec4 *v, float f)

   Subtracts a floating point from all components

   :param dst: Destination
   :param v:   Vector being subtracted from
   :param f:   Floating point being subtracted
   
---------------------

.. function:: void vec4_mulf(struct vec4 *dst, const struct vec4 *v, float f)

   Multiplies a floating point with all components

   :param dst: Destination
   :param dst: Vector
   :param f:   Floating point

---------------------

.. function:: void vec4_divf(struct vec4 *dst, const struct vec4 *v, float f)

   Divides a floating point from all components

   :param dst: Destination
   :param v:   Vector (dividend)
   :param f:   Floating point (divisor)

---------------------

.. function:: void vec4_neg(struct vec4 *dst, const struct vec4 *v)

   Negates a vector

   :param dst: Destination
   :param v:   Vector to negate

---------------------

.. function:: float vec4_dot(const struct vec4 *v1, const struct vec4 *v2)

   Performs a dot product between two vectors

   :param v1: Vector 1
   :param v2: Vector 2
   :return:   Result of the dot product

---------------------

.. function:: float vec4_len(const struct vec4 *v)

   Gets the length of a vector

   :param v: Vector
   :return:  The vector's length

---------------------

.. function:: float vec4_dist(const struct vec4 *v1, const struct vec4 *v2)

   Gets the distance between two vectors

   :param v1: Vector 1
   :param v2: Vector 2
   :return:   Distance between the two vectors

---------------------

.. function:: void vec4_minf(struct vec4 *dst, const struct vec4 *v, float val)

   Gets the minimum values between a vector's components and a floating point

   :param dst: Destination
   :param v:   Vector
   :param val: Floating point

---------------------

.. function:: void vec4_min(struct vec4 *dst, const struct vec4 *v, const struct vec4 *min_v)

   Gets the minimum values between two vectors

   :param dst:   Destination
   :param v:     Vector 1
   :param min_v: Vector 2

---------------------

.. function:: void vec4_maxf(struct vec4 *dst, const struct vec4 *v, float val)

   Gets the maximum values between a vector's components and a floating point

   :param dst: Destination
   :param v:   Vector
   :param val: Floating point

---------------------

.. function:: void vec4_max(struct vec4 *dst, const struct vec4 *v, const struct vec4 *max_v)

   Gets the maximum values between two vectors

   :param dst:   Destination
   :param v:     Vector 1
   :param max_v: Vector 2

---------------------

.. function:: void vec4_abs(struct vec4 *dst, const struct vec4 *v)

   Gets the absolute values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: void vec4_floor(struct vec4 *dst, const struct vec4 *v)

   Gets the floor values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: void vec4_ceil(struct vec4 *dst, const struct vec4 *v)

   Gets the ceiling values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: int vec4_close(const struct vec4 *v1, const struct vec4 *v2, float epsilon)

   Compares two vectors

   :param v1:      Vector 1
   :param v2:      Vector 2
   :param epsilon: Maximum precision for comparison

---------------------

.. function:: void vec4_norm(struct vec4 *dst, const struct vec4 *v)

   Normalizes a vector

   :param dst: Destination
   :param v:   Vector to normalize

---------------------

.. function:: void vec4_transform(struct vec4 *dst, const struct vec4 *v, const struct matrix4 *m)

   Transforms a vector

   :param dst: Destination
   :param v:   Vector
   :param m:   Matrix
