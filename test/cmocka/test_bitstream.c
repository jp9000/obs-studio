#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <util/bitstream.h>

static void bitstream_test(void **state)
{
	struct bitstream_reader reader;
	uint8_t data[6] = {0x34, 0xff, 0xe1, 0x23, 0x91, 0x45};

	// set len to one less than the array to show that we stop reading at that len
	bitstream_reader_init(&reader, data, 5);

	assert_int_equal(bitstream_reader_read_bits(&reader, 8), 0x34);
	assert_int_equal(bitstream_reader_read_bits(&reader, 1), 1);
	assert_int_equal(bitstream_reader_read_bits(&reader, 3), 7);
	assert_int_equal(bitstream_reader_read_bits(&reader, 4), 0xF);
	assert_int_equal(bitstream_reader_r8(&reader), 0xe1);
	assert_int_equal(bitstream_reader_r16(&reader), 0x2391);

	// test reached end
	assert_int_equal(bitstream_reader_r8(&reader), 0);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(bitstream_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
