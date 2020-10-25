#pragma once
#include "auto-config-enums.hpp"

namespace AutoConfig {

struct AutoConfigModel {

	// Constants for UI
	static int wizardMinWidth(void) { return 500; }

	// What is the wizard setting up:
	SetupType setupType = SetupType::Streaming;

	// AutoConfigVideoPage
	AutoConfig::FPSType fpsType = AutoConfig::FPSType::PreferHighFPS;
	int specificFPSNum = 0;
	int specificFPSDen = 0;
	int baseResolutionCX = 1920;
	int baseResolutionCY = 1080;
	bool preferHighFPS = false;
};

} //namespace AutoConfig
