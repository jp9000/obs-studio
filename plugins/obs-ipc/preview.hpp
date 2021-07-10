#pragma once

#include <stdint.h>
#include <obs.hpp>
#include <map>

typedef void (*texchange_cb)(uint32_t handle, void *param);
typedef void (*present_cb)(void *param);

class Preview {
public:
	Preview();
	~Preview();

	void SetSize(uint32_t width, uint32_t height);
	void AddTexChangeCallback(texchange_cb cb, void *param);
	void AddPresentCallback(present_cb cb, void *param);
	uint32_t GetHandle();

private:
	static void DrawCallback(void *param, uint32_t cx, uint32_t cy);
	void SignalTexChange();
	void SignalDraw();

	std::map<texchange_cb, void *> texCallbacks;
	std::map<present_cb, void *> presCallbacks;

	gs_texture_t *tex = NULL;
	gs_texrender_t *texrender = NULL;
	uint32_t w = 200;
	uint32_t h = 200;

	uint32_t texW = 0;
	uint32_t texH = 0;

	uint32_t texSizes[5] = {128, 256, 512, 1024, 2048};

	uint32_t handle = 0;
};
