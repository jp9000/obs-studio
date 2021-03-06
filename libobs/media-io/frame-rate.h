#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct media_frames_per_second {
	uint32_t numerator;
	uint32_t denominator;
};

static inline double
media_frames_per_second_to_frame_interval(struct media_frames_per_second fps)
{
	return (double)fps.denominator / fps.numerator;
}

static inline double
media_frames_per_second_to_fps(struct media_frames_per_second fps)
{
	return (double)fps.numerator / fps.denominator;
}

static inline bool
media_frames_per_second_is_valid(struct media_frames_per_second fps)
{
	return fps.numerator && fps.denominator;
}

#ifdef __cplusplus
}
#endif
