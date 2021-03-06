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

#include <string.h>
#include "../util/c99defs.h"
#include "../util/bmem.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Procedure call data structure
 *
 *   This is used to store parameters (and return value) sent to/from signals,
 * procedures, and callbacks.
 */

enum call_param_type {
	CALL_PARAM_TYPE_VOID,
	CALL_PARAM_TYPE_INT,
	CALL_PARAM_TYPE_FLOAT,
	CALL_PARAM_TYPE_BOOL,
	CALL_PARAM_TYPE_PTR,
	CALL_PARAM_TYPE_STRING
};

#define CALL_PARAM_IN (1 << 0)
#define CALL_PARAM_OUT (1 << 1)

struct calldata {
	uint8_t *stack;
	size_t size;     /* size of the stack, in bytes */
	size_t capacity; /* capacity of the stack, in bytes */
	bool fixed;      /* fixed size (using call stack) */
};

typedef struct calldata calldata_t;

static inline void calldata_init(struct calldata *data)
{
	memset(data, 0, sizeof(struct calldata));
}

static inline void calldata_clear(struct calldata *data);

static inline void calldata_init_fixed(struct calldata *data, uint8_t *stack,
				       size_t size)
{
	data->stack = stack;
	data->capacity = size;
	data->fixed = true;
	data->size = 0;
	calldata_clear(data);
}

static inline void calldata_free(struct calldata *data)
{
	if (!data->fixed)
		bfree(data->stack);
}

EXPORT bool calldata_get_data(const calldata_t *data, const char *name,
			      void *out, size_t size);
EXPORT void calldata_set_data(calldata_t *data, const char *name,
			      const void *in, size_t new_size);

static inline void calldata_clear(struct calldata *data)
{
	if (data->stack) {
		data->size = sizeof(size_t);
		memset(data->stack, 0, sizeof(size_t));
	}
}

static inline calldata_t *calldata_create(void)
{
	return (calldata_t *)bzalloc(sizeof(struct calldata));
}

static inline void calldata_destroy(calldata_t *cd)
{
	calldata_free(cd);
	bfree(cd);
}

/* ------------------------------------------------------------------------- */
/* NOTE: 'get' functions return true only if parameter exists, and is the
 *       same type.  They return false otherwise. */

static inline bool calldata_get_int(const calldata_t *data, const char *name,
				    long long *val)
{
	return calldata_get_data(data, name, val, sizeof(*val));
}

static inline bool calldata_get_float(const calldata_t *data, const char *name,
				      double *val)
{
	return calldata_get_data(data, name, val, sizeof(*val));
}

static inline bool calldata_get_bool(const calldata_t *data, const char *name,
				     bool *val)
{
	return calldata_get_data(data, name, val, sizeof(*val));
}

static inline bool calldata_get_ptr(const calldata_t *data, const char *name,
				    void *p_ptr)
{
	return calldata_get_data(data, name, p_ptr, sizeof(p_ptr));
}

EXPORT bool calldata_get_string(const calldata_t *data, const char *name,
				const char **str);

/* ------------------------------------------------------------------------- */
/* call if you know your data is valid */

static inline long long calldata_int(const calldata_t *data, const char *name)
{
	long long val = 0;
	calldata_get_int(data, name, &val);
	return val;
}

static inline double calldata_float(const calldata_t *data, const char *name)
{
	double val = 0.0;
	calldata_get_float(data, name, &val);
	return val;
}

static inline bool calldata_bool(const calldata_t *data, const char *name)
{
	bool val = false;
	calldata_get_bool(data, name, &val);
	return val;
}

static inline void *calldata_ptr(const calldata_t *data, const char *name)
{
	void *val = NULL;
	calldata_get_ptr(data, name, &val);
	return val;
}

static inline const char *calldata_string(const calldata_t *data,
					  const char *name)
{
	const char *val = NULL;
	calldata_get_string(data, name, &val);
	return val;
}

/* ------------------------------------------------------------------------- */

static inline void calldata_set_int(calldata_t *data, const char *name,
				    long long val)
{
	calldata_set_data(data, name, &val, sizeof(val));
}

static inline void calldata_set_float(calldata_t *data, const char *name,
				      double val)
{
	calldata_set_data(data, name, &val, sizeof(val));
}

static inline void calldata_set_bool(calldata_t *data, const char *name,
				     bool val)
{
	calldata_set_data(data, name, &val, sizeof(val));
}

static inline void calldata_set_ptr(calldata_t *data, const char *name,
				    void *ptr)
{
	calldata_set_data(data, name, &ptr, sizeof(ptr));
}

static inline void calldata_set_string(calldata_t *data, const char *name,
				       const char *str)
{
	if (str)
		calldata_set_data(data, name, str, strlen(str) + 1);
	else
		calldata_set_data(data, name, NULL, 0);
}

#ifdef __cplusplus
}
#endif
