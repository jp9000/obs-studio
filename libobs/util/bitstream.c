#include "bitstream.h"

#include <stdlib.h>
#include <string.h>

void bitstream_reader_init(struct bitstream_reader *r, uint8_t *data,
			   size_t len)
{
	memset(r, 0, sizeof(struct bitstream_reader));
	r->buf = data;
	r->subPos = 0x80;
	r->len = len;
}

uint8_t bitstream_reader_read_bit(struct bitstream_reader *r)
{
	if (r->pos >= r->len)
		return 0;

	uint8_t bit = (*(r->buf + r->pos) & r->subPos) == r->subPos ? 1 : 0;

	r->subPos >>= 0x1;
	if (r->subPos == 0) {
		r->subPos = 0x80;
		r->pos++;
	}

	return bit;
}

uint8_t bitstream_reader_read_bits(struct bitstream_reader *r, int bits)
{
	uint8_t res = 0;

	for (int i = 1; i <= bits; i++) {
		res <<= 1;
		res |= bitstream_reader_read_bit(r);
	}

	return res;
}

uint8_t bitstream_reader_r8(struct bitstream_reader *r)
{
	return bitstream_reader_read_bits(r, 8);
}

uint16_t bitstream_reader_r16(struct bitstream_reader *r)
{
	uint8_t b = bitstream_reader_read_bits(r, 8);
	return ((uint16_t)b << 8) | bitstream_reader_read_bits(r, 8);
}
