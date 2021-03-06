#pragma once

#include <util/darray.h>

struct update_info;
typedef struct update_info update_info_t;

struct file_download_data {
	const char *name;
	int version;

	DARRAY(uint8_t) buffer;
};

typedef bool (*confirm_file_callback_t)(void *param,
					struct file_download_data *file);

update_info_t *update_info_create(const char *log_prefix,
				  const char *user_agent,
				  const char *update_url, const char *local_dir,
				  const char *cache_dir,
				  confirm_file_callback_t confirm_callback,
				  void *param);
update_info_t *update_info_create_single(
	const char *log_prefix, const char *user_agent, const char *file_url,
	confirm_file_callback_t confirm_callback, void *param);
void update_info_destroy(update_info_t *info);
