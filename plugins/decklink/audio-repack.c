#include "audio-repack.h"

#include <util/sse-intrin.h>

int check_buffer(struct audio_repack *repack, uint32_t frame_count)
{
	const uint32_t new_size =
		frame_count * repack->base_dst_size + repack->extra_dst_size;

	if (repack->packet_size < new_size) {
		repack->packet_buffer =
			brealloc(repack->packet_buffer, new_size);
		if (!repack->packet_buffer)
			return -1;

		repack->packet_size = new_size;
	}

	return 0;
}

/*
 * Squash arrays.
 * For instance:
 * 2.1:
 *
 * | FL | FR | LFE | emp | emp | emp |emp |emp |
 * |    |    |
 * | FL | FR | LFE |
*/

int repack_squash(struct audio_repack *repack, const uint8_t *bsrc,
		  uint32_t frame_count)
{
	if (check_buffer(repack, frame_count) < 0)
		return -1;

	int squash = repack->extra_dst_size;
	const __m128i *src = (__m128i *)bsrc;
	const __m128i *esrc = src + frame_count;
	uint16_t *dst = (uint16_t *)repack->packet_buffer;

	/*  Audio needs squashing in order to avoid resampling issues.
	 * The condition checks for 7.1 audio for which no squash is needed.
	 */
	if (squash > 0) {
		while (src != esrc) {
			__m128i target = _mm_load_si128(src++);
			_mm_storeu_si128((__m128i *)dst, target);
			dst += 8 - squash;
		}
	}

	return 0;
}

int repack_squash_swap(struct audio_repack *repack, const uint8_t *bsrc,
		       uint32_t frame_count)
{
	if (check_buffer(repack, frame_count) < 0)
		return -1;
	int squash = repack->extra_dst_size;
	const __m128i *src = (__m128i *)bsrc;
	const __m128i *esrc = src + frame_count;
	uint16_t *dst = (uint16_t *)repack->packet_buffer;
	while (src != esrc) {
		__m128i target = _mm_load_si128(src++);
		__m128i buf =
			_mm_shufflelo_epi16(target, _MM_SHUFFLE(2, 3, 1, 0));
		_mm_storeu_si128((__m128i *)dst, buf);
		dst += 8 - squash;
	}
	return 0;
}

int audio_repack_init(struct audio_repack *repack,
		      audio_repack_mode_t repack_mode, uint8_t sample_bit)
{
	memset(repack, 0, sizeof(*repack));

	if (sample_bit != 16)
		return -1;
	int _audio_repack_ch[8] = {3, 4, 5, 6, 5, 6, 8, 8};
	repack->base_src_size = 8 * (16 / 8);
	repack->base_dst_size = _audio_repack_ch[repack_mode] * (16 / 8);
	repack->extra_dst_size = 8 - _audio_repack_ch[repack_mode];
	repack->repack_func = &repack_squash;
	if (repack_mode == repack_mode_8to5ch_swap ||
	    repack_mode == repack_mode_8to6ch_swap ||
	    repack_mode == repack_mode_8ch_swap)
		repack->repack_func = &repack_squash_swap;

	return 0;
}

void audio_repack_free(struct audio_repack *repack)
{
	if (repack->packet_buffer)
		bfree(repack->packet_buffer);

	memset(repack, 0, sizeof(*repack));
}
