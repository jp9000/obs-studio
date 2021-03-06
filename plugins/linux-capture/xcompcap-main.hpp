#pragma once

struct XCompcapMain_private;

class XCompcapMain {
public:
	static bool init();
	static void deinit();

	static obs_properties_t *properties();
	static void defaults(obs_data_t *settings);

	XCompcapMain(obs_data_t *settings, obs_source_t *source);
	~XCompcapMain();

	void updateSettings(obs_data_t *settings);

	void tick(float seconds);
	void render(gs_effect_t *effect);

	uint32_t width();
	uint32_t height();

private:
	XCompcapMain_private *p;
};
