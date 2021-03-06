3-Component Vector
==================

.. code:: cpp

   #include <graphics/vec3.h>

.. type:: struct vec3

   Two component vector structure.

.. member:: float vec3.x

   X component

.. member:: float vec3.y

   Y component

.. member:: float vec3.z

   Z component

.. member:: float vec3.ptr[3]

   Unioned array of all components

---------------------

.. function:: void vec3_zero(struct vec3 *dst)

   Zeroes a vector

   :param dst: Destination

---------------------

.. function:: void vec3_set(struct vec3 *dst, float x, float y)

   Sets the individual components of a 3-component vector.

   :param dst: Destination
   :param x:   X component
   :param y:   Y component
   :param y:   Z component

---------------------

.. function:: void vec3_copy(struct vec3 *dst, const struct vec3 *v)

   Copies a vector

   :param dst: Destination
   :param v:   Vector to copy

---------------------

.. function:: void vec3_from_vec4(struct vec3 *dst, const struct vec4 *v)

   Creates a 3-component vector from a 4-component vector

   :param dst: 3-component vector destination
   :param v:   4-component vector

---------------------

.. function:: void vec3_add(struct vec3 *dst, const struct vec3 *v1, const struct vec3 *v2)

   Adds two vectors

   :param dst: Destination
   :param v1:  Vector 1
   :param v2:  Vector 2

---------------------

.. function:: void vec3_sub(struct vec3 *dst, const struct vec3 *v1, const struct vec3 *v2)

   Subtracts two vectors

   :param dst: Destination
   :param v1:  Vector being subtracted from
   :param v2:  Vector being subtracted

---------------------

.. function:: void vec3_mul(struct vec3 *dst, const struct vec3 *v1, const struct vec3 *v2)

   Multiplies two vectors

   :param dst: Destination
   :param v1:  Vector 1
   :param v2:  Vector 2

---------------------

.. function:: void vec3_div(struct vec3 *dst, const struct vec3 *v1, const struct vec3 *v2)

   Divides two vectors

   :param dst: Destination
   :param v1:  Dividend
   :param v2:  Divisor

---------------------

.. function:: void vec3_addf(struct vec3 *dst, const struct vec3 *v, float f)

   Adds a floating point to all components

   :param dst: Destination
   :param dst: Vector
   :param f:   Floating point

---------------------

.. function:: void vec3_subf(struct vec3 *dst, const struct vec3 *v, float f)

   Subtracts a floating point from all components

   :param dst: Destination
   :param v:   Vector being subtracted from
   :param f:   Floating point being subtracted
   
---------------------

.. function:: void vec3_mulf(struct vec3 *dst, const struct vec3 *v, float f)

   Multiplies a floating point with all components

   :param dst: Destination
   :param dst: Vector
   :param f:   Floating point

---------------------

.. function:: void vec3_divf(struct vec3 *dst, const struct vec3 *v, float f)

   Divides a floating point from all components

   :param dst: Destination
   :param v:   Vector (dividend)
   :param f:   Floating point (divisor)

---------------------

.. function:: void vec3_neg(struct vec3 *dst, const struct vec3 *v)

   Negates a vector

   :param dst: Destination
   :param v:   Vector to negate

---------------------

.. function:: float vec3_dot(const struct vec3 *v1, const struct vec3 *v2)

   Performs a dot product between two vectors

   :param v1: Vector 1
   :param v2: Vector 2
   :return:   Result of the dot product

---------------------

.. function:: void vec3_cross(struct vec3 *dst, const struct vec3 *v1, const struct vec3 *v2)

   Performs a cross product between two vectors

   :param dst: Destination
   :param v1:  Vector 1
   :param v2:  Vector 2

---------------------

.. function:: float vec3_len(const struct vec3 *v)

   Gets the length of a vector

   :param v: Vector
   :return:  The vector's length

---------------------

.. function:: float vec3_dist(const struct vec3 *v1, const struct vec3 *v2)

   Gets the distance between two vectors

   :param v1: Vector 1
   :param v2: Vector 2
   :return:   Distance between the two vectors

---------------------

.. function:: void vec3_minf(struct vec3 *dst, const struct vec3 *v, float val)

   Gets the minimum values between a vector's components and a floating point

   :param dst: Destination
   :param v:   Vector
   :param val: Floating point

---------------------

.. function:: void vec3_min(struct vec3 *dst, const struct vec3 *v, const struct vec3 *min_v)

   Gets the minimum values between two vectors

   :param dst:   Destination
   :param v:     Vector 1
   :param min_v: Vector 2

---------------------

.. function:: void vec3_maxf(struct vec3 *dst, const struct vec3 *v, float val)

   Gets the maximum values between a vector's components and a floating point

   :param dst: Destination
   :param v:   Vector
   :param val: Floating point

---------------------

.. function:: void vec3_max(struct vec3 *dst, const struct vec3 *v, const struct vec3 *max_v)

   Gets the maximum values between two vectors

   :param dst:   Destination
   :param v:     Vector 1
   :param max_v: Vector 2

---------------------

.. function:: void vec3_abs(struct vec3 *dst, const struct vec3 *v)

   Gets the absolute values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: void vec3_floor(struct vec3 *dst, const struct vec3 *v)

   Gets the floor values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: void vec3_ceil(struct vec3 *dst, const struct vec3 *v)

   Gets the ceiling values of each component

   :param dst: Destination
   :param v:   Vector

---------------------

.. function:: int vec3_close(const struct vec3 *v1, const struct vec3 *v2, float epsilon)

   Compares two vectors

   :param v1:      Vector 1
   :param v2:      Vector 2
   :param epsilon: Maximum precision for comparison

---------------------

.. function:: void vec3_norm(struct vec3 *dst, const struct vec3 *v)

   Normalizes a vector

   :param dst: Destination
   :param v:   Vector to normalize

---------------------

.. function:: void vec3_transform(struct vec3 *dst, const struct vec3 *v, const struct matrix4 *m)

   Transforms a vector

   :param dst: Destination
   :param v:   Vector
   :param m:   Matrix

---------------------

.. function:: void vec3_rotate(struct vec3 *dst, const struct vec3 *v, const struct matrix3 *m)

   Rotates a vector

   :param dst: Destination
   :param v:   Vector
   :param m:   Matrix

---------------------

.. function:: void vec3_rand(struct vec3 *dst, int positive_only)

   Generates a random vector

   :param dst:           Destination
   :param positive_only: *true* if positive only, *false* otherwise
