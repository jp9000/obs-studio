#include <util/curl/curl-helper.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/darray.h>
#include <util/dstr.h>
#include <obs-data.h>
#include "file-updater.h"

#define warn(msg, ...) \
	blog(LOG_WARNING, "%s" msg, info->log_prefix, ##__VA_ARGS__)
#define info(msg, ...) \
	blog(LOG_WARNING, "%s" msg, info->log_prefix, ##__VA_ARGS__)

struct update_info {
	char error[CURL_ERROR_SIZE];
	struct curl_slist *header;
	DARRAY(uint8_t) file_data;
	char *user_agent;
	CURL *curl;
	char *url;

	/* directories */
	char *local;
	char *cache;
	char *temp;

	const char *remote_url;
	obs_data_t *local_package;
	obs_data_t *cache_package;
	obs_data_t *remote_package;

	char *etag_local;
	char *etag_remote;

	confirm_file_callback_t callback;
	void *param;

	pthread_t thread;
	bool thread_created;
	char *log_prefix;
};

void update_info_destroy(struct update_info *info)
{
	if (!info)
		return;

	if (info->thread_created)
		pthread_join(info->thread, NULL);

	da_free(info->file_data);
	bfree(info->log_prefix);
	bfree(info->user_agent);
	bfree(info->temp);
	bfree(info->cache);
	bfree(info->local);
	bfree(info->url);

	if (info->header)
		curl_slist_free_all(info->header);
	if (info->curl)
		curl_easy_cleanup(info->curl);
	if (info->local_package)
		obs_data_release(info->local_package);
	if (info->cache_package)
		obs_data_release(info->cache_package);
	if (info->remote_package)
		obs_data_release(info->remote_package);
	bfree(info);
}

static size_t http_write(uint8_t *ptr, size_t size, size_t nmemb,
			 struct update_info *info)
{
	size_t total = size * nmemb;
	if (total)
		da_push_back_array(info->file_data, ptr, total);

	return total;
}

static size_t http_header(char *buffer, size_t size, size_t nitems,
			  struct update_info *info)
{
	if (!strncmp(buffer, "ETag: ", 6)) {
		char *etag = buffer + 6;
		if (*etag) {
			char *etag_clean, *p;

			etag_clean = bstrdup(etag);

			p = strchr(etag_clean, '\r');
			if (p)
				*p = 0;

			p = strchr(etag_clean, '\n');
			if (p)
				*p = 0;

			info->etag_remote = etag_clean;
		}
	}
	return nitems * size;
}

static bool do_http_request(struct update_info *info, const char *url,
			    long *response_code)
{
	CURLcode code;
	uint8_t null_terminator = 0;

	da_resize(info->file_data, 0);
	curl_easy_setopt(info->curl, CURLOPT_URL, url);
	curl_easy_setopt(info->curl, CURLOPT_HTTPHEADER, info->header);
	curl_easy_setopt(info->curl, CURLOPT_ERRORBUFFER, info->error);
	curl_easy_setopt(info->curl, CURLOPT_WRITEFUNCTION, http_write);
	curl_easy_setopt(info->curl, CURLOPT_WRITEDATA, info);
	curl_easy_setopt(info->curl, CURLOPT_FAILONERROR, true);
	curl_easy_setopt(info->curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(info->curl, CURLOPT_ACCEPT_ENCODING, "");
	curl_obs_set_revoke_setting(info->curl);

	if (!info->remote_url) {
		// We only care about headers from the main package file
		curl_easy_setopt(info->curl, CURLOPT_HEADERFUNCTION,
				 http_header);
		curl_easy_setopt(info->curl, CURLOPT_HEADERDATA, info);
	}

#if LIBCURL_VERSION_NUM >= 0x072400
	// A lot of servers don't yet support ALPN
	curl_easy_setopt(info->curl, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif

	code = curl_easy_perform(info->curl);
	if (code != CURLE_OK) {
		warn("Remote update of URL \"%s\" failed: %s", url,
		     info->error);
		return false;
	}

	if (curl_easy_getinfo(info->curl, CURLINFO_RESPONSE_CODE,
			      response_code) != CURLE_OK)
		return false;

	if (*response_code >= 400) {
		warn("Remote update of URL \"%s\" failed: HTTP/%ld", url,
		     *response_code);
		return false;
	}

	da_push_back(info->file_data, &null_terminator);

	return true;
}

static char *get_path(const char *dir, const char *file)
{
	struct dstr str = {0};

	dstr_copy(&str, dir);

	if (str.array && dstr_end(&str) != '/' && dstr_end(&str) != '\\')
		dstr_cat_ch(&str, '/');

	dstr_cat(&str, file);
	return str.array;
}

static inline obs_data_t *get_package(const char *base_path, const char *file)
{
	char *full_path = get_path(base_path, file);
	obs_data_t *package = obs_data_create_from_json_file(full_path);
	bfree(full_path);
	return package;
}

static bool init_update(struct update_info *info)
{
	struct dstr user_agent = {0};

	info->curl = curl_easy_init();
	if (!info->curl) {
		warn("Could not initialize Curl");
		return false;
	}

	info->local_package = get_package(info->local, "package.json");
	info->cache_package = get_package(info->cache, "package.json");

	obs_data_t *metadata = get_package(info->cache, "meta.json");
	if (metadata) {
		const char *etag = obs_data_get_string(metadata, "etag");
		if (etag) {
			struct dstr if_none_match = {0};
			dstr_copy(&if_none_match, "If-None-Match: ");
			dstr_cat(&if_none_match, etag);

			info->etag_local = bstrdup(etag);

			info->header = curl_slist_append(info->header,
							 if_none_match.array);

			dstr_free(&if_none_match);
		}

		obs_data_release(metadata);
	}

	dstr_copy(&user_agent, "User-Agent: ");
	dstr_cat(&user_agent, info->user_agent);

	info->header = curl_slist_append(info->header, user_agent.array);

	dstr_free(&user_agent);
	return true;
}

static void copy_local_to_cache(struct update_info *info, const char *file)
{
	char *local_file_path = get_path(info->local, file);
	char *cache_file_path = get_path(info->cache, file);
	char *temp_file_path = get_path(info->temp, file);

	os_copyfile(local_file_path, temp_file_path);
	os_unlink(cache_file_path);
	os_rename(temp_file_path, cache_file_path);

	bfree(local_file_path);
	bfree(cache_file_path);
	bfree(temp_file_path);
}

static void enum_files(obs_data_t *package,
		       bool (*enum_func)(void *param, obs_data_t *file),
		       void *param)
{
	obs_data_array_t *array = obs_data_get_array(package, "files");
	size_t num;

	if (!array)
		return;

	num = obs_data_array_count(array);

	for (size_t i = 0; i < num; i++) {
		obs_data_t *file = obs_data_array_item(array, i);
		bool continue_enum = enum_func(param, file);
		obs_data_release(file);

		if (!continue_enum)
			break;
	}

	obs_data_array_release(array);
}

struct file_update_data {
	const char *name;
	int version;
	bool newer;
	bool found;
};

static bool newer_than_cache(void *param, obs_data_t *cache_file)
{
	struct file_update_data *input = param;
	const char *name = obs_data_get_string(cache_file, "name");
	int version = (int)obs_data_get_int(cache_file, "version");

	if (strcmp(input->name, name) == 0) {
		input->found = true;
		input->newer = input->version > version;
		return false;
	}

	return true;
}

static bool update_files_to_local(void *param, obs_data_t *local_file)
{
	struct update_info *info = param;
	struct file_update_data data = {
		.name = obs_data_get_string(local_file, "name"),
		.version = (int)obs_data_get_int(local_file, "version")};

	enum_files(info->cache_package, newer_than_cache, &data);
	if (data.newer || !data.found)
		copy_local_to_cache(info, data.name);

	return true;
}

static int update_local_version(struct update_info *info)
{
	int local_version;
	int cache_version = 0;

	local_version = (int)obs_data_get_int(info->local_package, "version");
	cache_version = (int)obs_data_get_int(info->cache_package, "version");

	/* if local cached version is out of date, copy new version */
	if (cache_version < local_version) {
		enum_files(info->local_package, update_files_to_local, info);
		copy_local_to_cache(info, "package.json");

		obs_data_release(info->cache_package);
		obs_data_addref(info->local_package);
		info->cache_package = info->local_package;

		return local_version;
	}

	return cache_version;
}

static inline bool do_relative_http_request(struct update_info *info,
					    const char *url, const char *file)
{
	long response_code;
	char *full_url = get_path(url, file);
	bool success = do_http_request(info, full_url, &response_code);
	bfree(full_url);
	return success && response_code == 200;
}

static inline void write_file_data(struct update_info *info,
				   const char *base_path, const char *file)
{
	char *full_path = get_path(base_path, file);
	os_quick_write_utf8_file(full_path, (char *)info->file_data.array,
				 info->file_data.num - 1, false);
	bfree(full_path);
}

static inline void replace_file(const char *src_base_path,
				const char *dst_base_path, const char *file)
{
	char *src_path = get_path(src_base_path, file);
	char *dst_path = get_path(dst_base_path, file);

	if (src_path && dst_path) {
		os_unlink(dst_path);
		os_rename(src_path, dst_path);
	}

	bfree(dst_path);
	bfree(src_path);
}

static bool update_remote_files(void *param, obs_data_t *remote_file)
{
	struct update_info *info = param;

	struct file_update_data data = {
		.name = obs_data_get_string(remote_file, "name"),
		.version = (int)obs_data_get_int(remote_file, "version")};

	enum_files(info->cache_package, newer_than_cache, &data);
	if (!data.newer && data.found)
		return true;

	if (!do_relative_http_request(info, info->remote_url, data.name))
		return true;

	if (info->callback) {
		struct file_download_data download_data;
		bool confirm;

		download_data.name = data.name;
		download_data.version = data.version;
		download_data.buffer.da = info->file_data.da;

		confirm = info->callback(info->param, &download_data);

		info->file_data.da = download_data.buffer.da;

		if (!confirm) {
			info("Update file '%s' (version %d) rejected",
			     data.name, data.version);
			return true;
		}
	}

	write_file_data(info, info->temp, data.name);
	replace_file(info->temp, info->cache, data.name);

	info("Successfully updated file '%s' (version %d)", data.name,
	     data.version);
	return true;
}

static void update_save_metadata(struct update_info *info)
{
	struct dstr path = {0};

	if (!info->etag_remote)
		return;

	dstr_copy(&path, info->cache);
	dstr_cat(&path, "meta.json");

	obs_data_t *data;
	data = obs_data_create();
	obs_data_set_string(data, "etag", info->etag_remote);
	obs_data_save_json(data, path.array);
	obs_data_release(data);

	dstr_free(&path);
}

static void update_remote_version(struct update_info *info, int cur_version)
{
	int remote_version;
	long response_code;

	if (!do_http_request(info, info->url, &response_code))
		return;

	if (response_code == 304)
		return;

	if (!info->file_data.array || info->file_data.array[0] != '{') {
		warn("Remote package does not exist or is not valid json");
		return;
	}

	update_save_metadata(info);

	info->remote_package =
		obs_data_create_from_json((char *)info->file_data.array);
	if (!info->remote_package) {
		warn("Failed to initialize remote package json");
		return;
	}

	remote_version = (int)obs_data_get_int(info->remote_package, "version");
	if (remote_version <= cur_version)
		return;

	write_file_data(info, info->temp, "package.json");

	info->remote_url = obs_data_get_string(info->remote_package, "url");
	if (!info->remote_url) {
		warn("No remote url in package file");
		return;
	}

	/* download new files */
	enum_files(info->remote_package, update_remote_files, info);

	replace_file(info->temp, info->cache, "package.json");

	info("Successfully updated package (version %d)", remote_version);
	return;
}

static void *update_thread(void *data)
{
	struct update_info *info = data;
	int cur_version;

	if (!init_update(info))
		return NULL;

	cur_version = update_local_version(info);
	update_remote_version(info, cur_version);
	os_rmdir(info->temp);

	if (info->etag_local)
		bfree(info->etag_local);
	if (info->etag_remote)
		bfree(info->etag_remote);

	return NULL;
}

update_info_t *update_info_create(const char *log_prefix,
				  const char *user_agent,
				  const char *update_url, const char *local_dir,
				  const char *cache_dir,
				  confirm_file_callback_t confirm_callback,
				  void *param)
{
	struct update_info *info;
	struct dstr dir = {0};

	if (!log_prefix)
		log_prefix = "";

	if (os_mkdir(cache_dir) < 0) {
		blog(LOG_WARNING, "%sCould not create cache directory %s",
		     log_prefix, cache_dir);
		return NULL;
	}

	dstr_copy(&dir, cache_dir);
	if (dstr_end(&dir) != '/' && dstr_end(&dir) != '\\')
		dstr_cat_ch(&dir, '/');
	dstr_cat(&dir, ".temp");

	if (os_mkdir(dir.array) < 0) {
		blog(LOG_WARNING, "%sCould not create temp directory %s",
		     log_prefix, cache_dir);
		dstr_free(&dir);
		return NULL;
	}

	info = bzalloc(sizeof(*info));
	info->log_prefix = bstrdup(log_prefix);
	info->user_agent = bstrdup(user_agent);
	info->temp = dir.array;
	info->local = bstrdup(local_dir);
	info->cache = bstrdup(cache_dir);
	info->url = get_path(update_url, "package.json");
	info->callback = confirm_callback;
	info->param = param;

	if (pthread_create(&info->thread, NULL, update_thread, info) == 0)
		info->thread_created = true;

	return info;
}

static void *single_file_thread(void *data)
{
	struct update_info *info = data;
	struct file_download_data download_data;
	long response_code;

	info->curl = curl_easy_init();
	if (!info->curl) {
		warn("Could not initialize Curl");
		return NULL;
	}

	if (!do_http_request(info, info->url, &response_code))
		return NULL;
	if (!info->file_data.array || !info->file_data.array[0])
		return NULL;

	download_data.name = info->url;
	download_data.version = 0;
	download_data.buffer.da = info->file_data.da;
	info->callback(info->param, &download_data);
	info->file_data.da = download_data.buffer.da;
	return NULL;
}

update_info_t *
update_info_create_single(const char *log_prefix, const char *user_agent,
			  const char *file_url,
			  confirm_file_callback_t confirm_callback, void *param)
{
	struct update_info *info;

	if (!log_prefix)
		log_prefix = "";

	info = bzalloc(sizeof(*info));
	info->log_prefix = bstrdup(log_prefix);
	info->user_agent = bstrdup(user_agent);
	info->url = bstrdup(file_url);
	info->callback = confirm_callback;
	info->param = param;

	if (pthread_create(&info->thread, NULL, single_file_thread, info) == 0)
		info->thread_created = true;

	return info;
}
