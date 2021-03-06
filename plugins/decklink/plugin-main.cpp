#include <obs-module.h>
#include "decklink-devices.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("decklink", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Blackmagic DeckLink source";
}

extern struct obs_source_info create_decklink_source_info();
struct obs_source_info decklink_source_info;

extern struct obs_output_info create_decklink_output_info();
struct obs_output_info decklink_output_info;

void log_sdk_version()
{
	IDeckLinkIterator *deckLinkIterator;
	IDeckLinkAPIInformation *deckLinkAPIInformation;
	HRESULT result;

	deckLinkIterator = CreateDeckLinkIteratorInstance();
	if (deckLinkIterator == NULL) {
		blog(LOG_WARNING,
		     "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed");
		return;
	}

	result = deckLinkIterator->QueryInterface(
		IID_IDeckLinkAPIInformation, (void **)&deckLinkAPIInformation);
	if (result == S_OK) {
		decklink_string_t deckLinkVersion;
		deckLinkAPIInformation->GetString(BMDDeckLinkAPIVersion,
						  &deckLinkVersion);

		blog(LOG_INFO, "Decklink API Compiled version %s",
		     BLACKMAGIC_DECKLINK_API_VERSION_STRING);

		std::string versionString;
		DeckLinkStringToStdString(deckLinkVersion, versionString);

		blog(LOG_INFO, "Decklink API Installed version %s",
		     versionString.c_str());

		deckLinkAPIInformation->Release();
	}
}

bool obs_module_load(void)
{
	log_sdk_version();

	deviceEnum = new DeckLinkDeviceDiscovery();
	if (!deviceEnum->Init())
		return true;

	decklink_source_info = create_decklink_source_info();
	obs_register_source(&decklink_source_info);

	decklink_output_info = create_decklink_output_info();
	obs_register_output(&decklink_output_info);

	return true;
}

void obs_module_unload(void)
{
	delete deviceEnum;
}
