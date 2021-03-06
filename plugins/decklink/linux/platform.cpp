#include "../platform.hpp"

bool DeckLinkStringToStdString(decklink_string_t input, std::string &output)
{
	if (input == nullptr)
		return false;

	output = std::string(input);
	free((void *)input);

	return true;
}
