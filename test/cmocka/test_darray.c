#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <util/darray.h>

static void array_basic_test(void **state)
{
	DARRAY(uint8_t) testarray;
	da_init(testarray);

	uint8_t t = 1;
	da_push_back_array(testarray, &t, sizeof(uint8_t));

	assert_int_equal(testarray.num, 1);
	assert_memory_equal(testarray.array, &t, 1);

	da_free(testarray);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(array_basic_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
