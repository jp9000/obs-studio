#pragma once

#include "c99defs.h"

/*
 *   General programmable serialization functions.  (A shared interface to
 * various reading/writing to/from different inputs/outputs)
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bitstream_reader {
	uint8_t pos;
	uint8_t subPos;
	uint8_t *buf;
	size_t len;
};

EXPORT void bitstream_reader_init(struct bitstream_reader *r, uint8_t *data,
				  size_t len);
EXPORT uint8_t bitstream_reader_read_bits(struct bitstream_reader *r, int bits);
EXPORT uint8_t bitstream_reader_r8(struct bitstream_reader *r);
EXPORT uint16_t bitstream_reader_r16(struct bitstream_reader *r);

#ifdef __cplusplus
}
#endif
