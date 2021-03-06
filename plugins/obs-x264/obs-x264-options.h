#pragma once

#include <stddef.h>

struct obs_x264_option {
	char *name;
	char *value;
};

struct obs_x264_options {
	size_t count;
	struct obs_x264_option *options;
	size_t ignored_word_count;
	char **ignored_words;
	char **input_words;
};

struct obs_x264_options obs_x264_parse_options(const char *options_string);
void obs_x264_free_options(struct obs_x264_options options);
