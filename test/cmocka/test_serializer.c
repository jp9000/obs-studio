#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <util/array-serializer.h>

static void serialize_test(void **state)
{
	UNUSED_PARAMETER(state);
	struct array_output_data output;
	struct serializer s;

	array_output_serializer_init(&s, &output);

	s_w8(&s, 0x01);
	s_w8(&s, 0xff);
	s_w8(&s, 0xe1);

	assert_int_equal(output.bytes.num, 3);
	uint8_t expected[3] = {0x01, 0xff, 0xe1};
	assert_memory_equal(output.bytes.array, expected, 3);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(serialize_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
