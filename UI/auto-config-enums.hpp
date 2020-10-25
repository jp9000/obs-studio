#pragma once

namespace AutoConfig {

enum WizardPage {
	StartPage,
	VideoPage,
	StreamPage,
	TestPage,
};

enum class SetupType {
	Invalid,
	Streaming,
	Recording,
	VirtualCam,
};

enum class FPSType : int {
	PreferHighFPS,
	PreferHighRes,
	UseCurrent,
	fps30,
	fps60,
};
}
