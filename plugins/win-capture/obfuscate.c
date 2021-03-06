#ifdef _MSC_VER
#pragma warning(disable : 4152) /* casting func ptr to void */
#endif

#include <stdbool.h>
#include <windows.h>
#include "obfuscate.h"

#define LOWER_HALFBYTE(x) ((x)&0xF)
#define UPPER_HALFBYTE(x) (((x) >> 4) & 0xF)

static void deobfuscate_str(char *str, uint64_t val)
{
	uint8_t *dec_val = (uint8_t *)&val;
	int i = 0;

	while (*str != 0) {
		int pos = i / 2;
		bool bottom = (i % 2) == 0;
		uint8_t *ch = (uint8_t *)str;
		uint8_t xor = bottom ? LOWER_HALFBYTE(dec_val[pos])
				     : UPPER_HALFBYTE(dec_val[pos]);

		*ch ^= xor;

		if (++i == sizeof(uint64_t) * 2)
			i = 0;

		str++;
	}
}

void *get_obfuscated_func(HMODULE module, const char *str, uint64_t val)
{
	char new_name[128];
	strcpy(new_name, str);
	deobfuscate_str(new_name, val);
	return GetProcAddress(module, new_name);
}
