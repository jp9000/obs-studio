#include "../platform.hpp"
#include <util/apple/cfstring-utils.h>

bool DeckLinkStringToStdString(decklink_string_t input, std::string &output)
{
	const CFStringRef string = static_cast<CFStringRef>(input);

	char *buffer = cfstr_copy_cstr(string, kCFStringEncodingASCII);

	if (buffer)
		output = std::string(buffer);

	bfree(buffer);
	CFRelease(string);

	return (buffer != NULL);
}
