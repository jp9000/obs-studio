#pragma once

#include "audio-repack.h"

class AudioRepacker {
	struct audio_repack arepack;

public:
	inline AudioRepacker(audio_repack_mode_t repack_mode)
	{
		audio_repack_init(&arepack, repack_mode, 16);
	}
	inline ~AudioRepacker() { audio_repack_free(&arepack); }

	inline int repack(const uint8_t *src, uint32_t frame_size)
	{
		return (*arepack.repack_func)(&arepack, src, frame_size);
	}

	inline operator struct audio_repack *() { return &arepack; }
	inline struct audio_repack *operator->() { return &arepack; }
};
