#include "util.hpp"

const char *bmd_video_connection_to_name(BMDVideoConnection connection)
{
	switch (connection) {
	case bmdVideoConnectionSDI:
		return "SDI";
	case bmdVideoConnectionHDMI:
		return "HDMI";
	case bmdVideoConnectionOpticalSDI:
		return "Optical SDI";
	case bmdVideoConnectionComponent:
		return "Component";
	case bmdVideoConnectionComposite:
		return "Composite";
	case bmdVideoConnectionSVideo:
		return "S-Video";
	default:
		return "Unknown";
	}
}

const char *bmd_audio_connection_to_name(BMDAudioConnection connection)
{
	switch (connection) {
	case bmdAudioConnectionEmbedded:
		return "Embedded";
	case bmdAudioConnectionAESEBU:
		return "AES/EBU";
	case bmdAudioConnectionAnalog:
		return "Analog";
	case bmdAudioConnectionAnalogXLR:
		return "Analog XLR";
	case bmdAudioConnectionAnalogRCA:
		return "Analog RCA";
	case bmdAudioConnectionMicrophone:
		return "Microphone";
	case bmdAudioConnectionHeadphones:
		return "Headphones";
	default:
		return "Unknown";
	}
}