#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

#include <obs.h>

struct audio_repack;

typedef int (*audio_repack_func_t)(struct audio_repack *, const uint8_t *,
				   uint32_t);

struct audio_repack {
	uint8_t *packet_buffer;
	uint32_t packet_size;

	uint32_t base_src_size;
	uint32_t base_dst_size;
	uint32_t extra_dst_size;

	audio_repack_func_t repack_func;
};

enum _audio_repack_mode {
	repack_mode_8to3ch = 0,
	repack_mode_8to4ch,
	repack_mode_8to5ch,
	repack_mode_8to6ch,
	repack_mode_8to5ch_swap,
	repack_mode_8to6ch_swap,
	repack_mode_8ch_swap,
	repack_mode_8ch,
};

typedef enum _audio_repack_mode audio_repack_mode_t;

extern int audio_repack_init(struct audio_repack *repack,
			     audio_repack_mode_t repack_mode,
			     uint8_t sample_bit);
extern void audio_repack_free(struct audio_repack *repack);

#ifdef __cplusplus
}
#endif
