/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "c99defs.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "bmem.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dynamic array.
 *
 * NOTE: Not type-safe when using directly.
 *       Specifying size per call with inline maximizes compiler optimizations
 *
 *       See DARRAY macro at the bottom of the file for slightly safer usage.
 */

#define DARRAY_INVALID ((size_t)-1)

struct darray {
	void *array;
	size_t num;
	size_t capacity;
};

static inline void darray_init(struct darray *dst)
{
	dst->array = NULL;
	dst->num = 0;
	dst->capacity = 0;
}

static inline void darray_free(struct darray *dst)
{
	bfree(dst->array);
	dst->array = NULL;
	dst->num = 0;
	dst->capacity = 0;
}

static inline size_t darray_alloc_size(const size_t element_size,
				       const struct darray *da)
{
	return element_size * da->num;
}

static inline void *darray_item(const size_t element_size,
				const struct darray *da, size_t idx)
{
	return (void *)(((uint8_t *)da->array) + element_size * idx);
}

static inline void *darray_end(const size_t element_size,
			       const struct darray *da)
{
	if (!da->num)
		return NULL;

	return darray_item(element_size, da, da->num - 1);
}

static inline void darray_reserve(const size_t element_size, struct darray *dst,
				  const size_t capacity)
{
	void *ptr;
	if (capacity == 0 || capacity <= dst->capacity)
		return;

	ptr = bmalloc(element_size * capacity);
	if (dst->array) {
		if (dst->num)
			memcpy(ptr, dst->array, element_size * dst->num);

		bfree(dst->array);
	}
	dst->array = ptr;
	dst->capacity = capacity;
}

static inline void darray_ensure_capacity(const size_t element_size,
					  struct darray *dst,
					  const size_t new_size)
{
	size_t new_cap;
	void *ptr;
	if (new_size <= dst->capacity)
		return;

	new_cap = (!dst->capacity) ? new_size : dst->capacity * 2;
	if (new_size > new_cap)
		new_cap = new_size;
	ptr = bmalloc(element_size * new_cap);
	if (dst->array) {
		if (dst->capacity)
			memcpy(ptr, dst->array, element_size * dst->capacity);

		bfree(dst->array);
	}
	dst->array = ptr;
	dst->capacity = new_cap;
}

static inline void darray_resize(const size_t element_size, struct darray *dst,
				 const size_t size)
{
	int b_clear;
	size_t old_num;

	if (size == dst->num) {
		return;
	} else if (size == 0) {
		dst->num = 0;
		return;
	}

	b_clear = size > dst->num;
	old_num = dst->num;

	darray_ensure_capacity(element_size, dst, size);
	dst->num = size;

	if (b_clear)
		memset(darray_item(element_size, dst, old_num), 0,
		       element_size * (dst->num - old_num));
}

static inline void darray_copy(const size_t element_size, struct darray *dst,
			       const struct darray *da)
{
	if (da->num == 0) {
		darray_free(dst);
	} else {
		darray_resize(element_size, dst, da->num);
		memcpy(dst->array, da->array, element_size * da->num);
	}
}

static inline void darray_copy_array(const size_t element_size,
				     struct darray *dst, const void *array,
				     const size_t num)
{
	darray_resize(element_size, dst, num);
	memcpy(dst->array, array, element_size * dst->num);
}

static inline void darray_move(struct darray *dst, struct darray *src)
{
	darray_free(dst);
	memcpy(dst, src, sizeof(struct darray));
	src->array = NULL;
	src->capacity = 0;
	src->num = 0;
}

static inline size_t darray_find(const size_t element_size,
				 const struct darray *da, const void *item,
				 const size_t idx)
{
	size_t i;

	assert(idx <= da->num);

	for (i = idx; i < da->num; i++) {
		void *compare = darray_item(element_size, da, i);
		if (memcmp(compare, item, element_size) == 0)
			return i;
	}

	return DARRAY_INVALID;
}

static inline size_t darray_push_back(const size_t element_size,
				      struct darray *dst, const void *item)
{
	darray_ensure_capacity(element_size, dst, ++dst->num);
	memcpy(darray_end(element_size, dst), item, element_size);

	return dst->num - 1;
}

static inline void *darray_push_back_new(const size_t element_size,
					 struct darray *dst)
{
	void *last;

	darray_ensure_capacity(element_size, dst, ++dst->num);

	last = darray_end(element_size, dst);
	memset(last, 0, element_size);
	return last;
}

static inline size_t darray_push_back_array(const size_t element_size,
					    struct darray *dst,
					    const void *array, const size_t num)
{
	size_t old_num;
	if (!dst)
		return 0;
	if (!array || !num)
		return dst->num;

	old_num = dst->num;
	darray_resize(element_size, dst, dst->num + num);
	memcpy(darray_item(element_size, dst, old_num), array,
	       element_size * num);

	return old_num;
}

static inline size_t darray_push_back_darray(const size_t element_size,
					     struct darray *dst,
					     const struct darray *da)
{
	return darray_push_back_array(element_size, dst, da->array, da->num);
}

static inline void darray_insert(const size_t element_size, struct darray *dst,
				 const size_t idx, const void *item)
{
	void *new_item;
	size_t move_count;

	assert(idx <= dst->num);

	if (idx == dst->num) {
		darray_push_back(element_size, dst, item);
		return;
	}

	move_count = dst->num - idx;
	darray_ensure_capacity(element_size, dst, ++dst->num);

	new_item = darray_item(element_size, dst, idx);

	memmove(darray_item(element_size, dst, idx + 1), new_item,
		move_count * element_size);
	memcpy(new_item, item, element_size);
}

static inline void *darray_insert_new(const size_t element_size,
				      struct darray *dst, const size_t idx)
{
	void *item;
	size_t move_count;

	assert(idx <= dst->num);
	if (idx == dst->num)
		return darray_push_back_new(element_size, dst);

	item = darray_item(element_size, dst, idx);

	move_count = dst->num - idx;
	darray_ensure_capacity(element_size, dst, ++dst->num);
	memmove(darray_item(element_size, dst, idx + 1), item,
		move_count * element_size);

	memset(item, 0, element_size);
	return item;
}

static inline void darray_insert_array(const size_t element_size,
				       struct darray *dst, const size_t idx,
				       const void *array, const size_t num)
{
	size_t old_num;

	assert(array != NULL);
	assert(num != 0);
	assert(idx <= dst->num);

	old_num = dst->num;
	darray_resize(element_size, dst, dst->num + num);

	memmove(darray_item(element_size, dst, idx + num),
		darray_item(element_size, dst, idx),
		element_size * (old_num - idx));
	memcpy(darray_item(element_size, dst, idx), array, element_size * num);
}

static inline void darray_insert_darray(const size_t element_size,
					struct darray *dst, const size_t idx,
					const struct darray *da)
{
	darray_insert_array(element_size, dst, idx, da->array, da->num);
}

static inline void darray_erase(const size_t element_size, struct darray *dst,
				const size_t idx)
{
	assert(idx < dst->num);

	if (idx >= dst->num || !--dst->num)
		return;

	memmove(darray_item(element_size, dst, idx),
		darray_item(element_size, dst, idx + 1),
		element_size * (dst->num - idx));
}

static inline void darray_erase_item(const size_t element_size,
				     struct darray *dst, const void *item)
{
	size_t idx = darray_find(element_size, dst, item, 0);
	if (idx != DARRAY_INVALID)
		darray_erase(element_size, dst, idx);
}

static inline void darray_erase_range(const size_t element_size,
				      struct darray *dst, const size_t start,
				      const size_t end)
{
	size_t count, move_count;

	assert(start <= dst->num);
	assert(end <= dst->num);
	assert(end > start);

	count = end - start;
	if (count == 1) {
		darray_erase(element_size, dst, start);
		return;
	} else if (count == dst->num) {
		dst->num = 0;
		return;
	}

	move_count = dst->num - end;
	if (move_count)
		memmove(darray_item(element_size, dst, start),
			darray_item(element_size, dst, end),
			move_count * element_size);

	dst->num -= count;
}

static inline void darray_pop_back(const size_t element_size,
				   struct darray *dst)
{
	assert(dst->num != 0);

	if (dst->num)
		darray_erase(element_size, dst, dst->num - 1);
}

static inline void darray_join(const size_t element_size, struct darray *dst,
			       struct darray *da)
{
	darray_push_back_darray(element_size, dst, da);
	darray_free(da);
}

static inline void darray_split(const size_t element_size, struct darray *dst1,
				struct darray *dst2, const struct darray *da,
				const size_t idx)
{
	struct darray temp;

	assert(da->num >= idx);
	assert(dst1 != dst2);

	darray_init(&temp);
	darray_copy(element_size, &temp, da);

	darray_free(dst1);
	darray_free(dst2);

	if (da->num) {
		if (idx)
			darray_copy_array(element_size, dst1, temp.array,
					  temp.num);
		if (idx < temp.num - 1)
			darray_copy_array(element_size, dst2,
					  darray_item(element_size, &temp, idx),
					  temp.num - idx);
	}

	darray_free(&temp);
}

static inline void darray_move_item(const size_t element_size,
				    struct darray *dst, const size_t from,
				    const size_t to)
{
	void *temp, *p_from, *p_to;

	if (from == to)
		return;

	temp = malloc(element_size);
	if (!temp)
		bcrash("darray_move_item: out of memory");

	p_from = darray_item(element_size, dst, from);
	p_to = darray_item(element_size, dst, to);

	memcpy(temp, p_from, element_size);

	if (to < from)
		memmove(darray_item(element_size, dst, to + 1), p_to,
			element_size * (from - to));
	else
		memmove(p_from, darray_item(element_size, dst, from + 1),
			element_size * (to - from));

	memcpy(p_to, temp, element_size);
	free(temp);
}

static inline void darray_swap(const size_t element_size, struct darray *dst,
			       const size_t a, const size_t b)
{
	void *temp, *a_ptr, *b_ptr;

	assert(a < dst->num);
	assert(b < dst->num);

	if (a == b)
		return;

	temp = malloc(element_size);
	if (!temp)
		bcrash("darray_swap: out of memory");

	a_ptr = darray_item(element_size, dst, a);
	b_ptr = darray_item(element_size, dst, b);

	memcpy(temp, a_ptr, element_size);
	memcpy(a_ptr, b_ptr, element_size);
	memcpy(b_ptr, temp, element_size);

	free(temp);
}

/*
 * Defines to make dynamic arrays more type-safe.
 * Note: Still not 100% type-safe but much better than using darray directly
 *       Makes it a little easier to use as well.
 *
 *       I did -not- want to use a gigantic macro to generate a crapload of
 *       typesafe inline functions per type.  It just feels like a mess to me.
 */

#define DARRAY(type)                     \
	union {                          \
		struct darray da;        \
		struct {                 \
			type *array;     \
			size_t num;      \
			size_t capacity; \
		};                       \
	}

#define da_init(v) darray_init(&v.da)

#define da_free(v) darray_free(&v.da)

#define da_alloc_size(v) (sizeof(*v.array) * v.num)

#define da_end(v) darray_end(sizeof(*v.array), &v.da)

#define da_reserve(v, capacity) \
	darray_reserve(sizeof(*v.array), &v.da, capacity)

#define da_resize(v, size) darray_resize(sizeof(*v.array), &v.da, size)

#define da_copy(dst, src) darray_copy(sizeof(*dst.array), &dst.da, &src.da)

#define da_copy_array(dst, src_array, n) \
	darray_copy_array(sizeof(*dst.array), &dst.da, src_array, n)

#define da_move(dst, src) darray_move(&dst.da, &src.da)

#define da_find(v, item, idx) darray_find(sizeof(*v.array), &v.da, item, idx)

#define da_push_back(v, item) darray_push_back(sizeof(*v.array), &v.da, item)

#define da_push_back_new(v) darray_push_back_new(sizeof(*v.array), &v.da)

#define da_push_back_array(dst, src_array, n) \
	darray_push_back_array(sizeof(*dst.array), &dst.da, src_array, n)

#define da_push_back_da(dst, src) \
	darray_push_back_darray(sizeof(*dst.array), &dst.da, &src.da)

#define da_insert(v, idx, item) \
	darray_insert(sizeof(*v.array), &v.da, idx, item)

#define da_insert_new(v, idx) darray_insert_new(sizeof(*v.array), &v.da, idx)

#define da_insert_array(dst, idx, src_array, n) \
	darray_insert_array(sizeof(*dst.array), &dst.da, idx, src_array, n)

#define da_insert_da(dst, idx, src) \
	darray_insert_darray(sizeof(*dst.array), &dst.da, idx, &src.da)

#define da_erase(dst, idx) darray_erase(sizeof(*dst.array), &dst.da, idx)

#define da_erase_item(dst, item) \
	darray_erase_item(sizeof(*dst.array), &dst.da, item)

#define da_erase_range(dst, from, to) \
	darray_erase_range(sizeof(*dst.array), &dst.da, from, to)

#define da_pop_back(dst) darray_pop_back(sizeof(*dst.array), &dst.da);

#define da_join(dst, src) darray_join(sizeof(*dst.array), &dst.da, &src.da)

#define da_split(dst1, dst2, src, idx) \
	darray_split(sizeof(*src.array), &dst1.da, &dst2.da, &src.da, idx)

#define da_move_item(v, from, to) \
	darray_move_item(sizeof(*v.array), &v.da, from, to)

#define da_swap(v, idx1, idx2) darray_swap(sizeof(*v.array), &v.da, idx1, idx2)

#ifdef __cplusplus
}
#endif
