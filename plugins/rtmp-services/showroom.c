#include <util/curl/curl-helper.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <util/dstr.h>
#include <util/darray.h>
#include "util/base.h"
#include <obs-module.h>
#include <util/platform.h>
#include "showroom.h"
#include <util/threading.h>

struct showroom_ingest_info {
	char *access_key;
	uint64_t last_time;
	struct showroom_ingest ingest;
};

static DARRAY(struct showroom_ingest_info) cur_ingests = {0};

struct showroom_ingest invalid_ingest = {"", ""};

void free_showroom_data(void)
{
	for (size_t i = 0; i < cur_ingests.num; i++) {
		struct showroom_ingest_info *info = &cur_ingests.array[i];
		bfree(info->access_key);
		bfree((void *)info->ingest.key);
		bfree((void *)info->ingest.url);
	}

	da_free(cur_ingests);
}

static size_t showroom_write_cb(void *data, size_t size, size_t nmemb,
				void *user_pointer)
{
	struct dstr *json = user_pointer;
	size_t realsize = size * nmemb;
	dstr_ncat(json, data, realsize);
	return realsize;
}

static struct showroom_ingest_info *find_ingest(const char *access_key)
{
	struct showroom_ingest_info *ret = NULL;
	for (size_t i = 0; i < cur_ingests.num; i++) {
		struct showroom_ingest_info *info = &cur_ingests.array[i];
		if (strcmp(info->access_key, access_key) == 0) {
			ret = info;
			break;
		}
	}

	return ret;
}

#ifndef SEC_TO_NSEC
#define SEC_TO_NSEC 1000000000ULL
#endif

static struct showroom_ingest_info *get_ingest_from_json(char *str,
							 const char *access_key)
{
	json_error_t error;
	json_t *root;
	root = json_loads(str, JSON_REJECT_DUPLICATES, &error);
	if (!root) {
		return NULL;
	}

	const char *url_str =
		json_string_value(json_object_get(root, "streaming_url_rtmp"));
	const char *key_str =
		json_string_value(json_object_get(root, "streaming_key"));

	struct showroom_ingest_info *info = find_ingest(access_key);
	if (!info) {
		info = da_push_back_new(cur_ingests);
		info->access_key = bstrdup(access_key);
	}

	bfree((void *)info->ingest.url);
	bfree((void *)info->ingest.key);
	info->ingest.url = bstrdup(url_str);
	info->ingest.key = bstrdup(key_str);
	info->last_time = os_gettime_ns() / SEC_TO_NSEC;

	json_decref(root);
	return info;
}

struct showroom_ingest *showroom_get_ingest(const char *server,
					    const char *access_key)
{
	struct showroom_ingest_info *info = find_ingest(access_key);
	CURL *curl_handle;
	CURLcode res;
	struct dstr json = {0};
	struct dstr uri = {0};
	long response_code;

	if (info) {
		/* this function is called a bunch of times for the same data,
		 * so in order to prevent multiple unnecessary queries in a
		 * short period of time, return the same data for 10 seconds */

		uint64_t ts_sec = os_gettime_ns() / SEC_TO_NSEC;
		if (ts_sec - info->last_time < 10) {
			return &info->ingest;
		} else {
			info = NULL;
		}
	}

	curl_handle = curl_easy_init();

	dstr_copy(&uri, server);
	dstr_cat(&uri, access_key);
	curl_easy_setopt(curl_handle, CURLOPT_URL, uri.array);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, true);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, showroom_write_cb);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&json);
	curl_obs_set_revoke_setting(curl_handle);

#if LIBCURL_VERSION_NUM >= 0x072400
	curl_easy_setopt(curl_handle, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif

	res = curl_easy_perform(curl_handle);
	dstr_free(&uri);
	if (res != CURLE_OK) {
		blog(LOG_WARNING,
		     "showroom_get_ingest: curl_easy_perform() failed: %s",
		     curl_easy_strerror(res));
		goto cleanup;
	}

	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code != 200) {
		blog(LOG_WARNING,
		     "showroom_get_ingest: curl_easy_perform() returned "
		     "code: %ld",
		     response_code);
		goto cleanup;
	}

	if (json.len == 0) {
		blog(LOG_WARNING,
		     "showroom_get_ingest: curl_easy_perform() returned "
		     "empty response");
		goto cleanup;
	}

	info = get_ingest_from_json(json.array, access_key);

cleanup:
	curl_easy_cleanup(curl_handle);
	dstr_free(&json);
	return info ? &info->ingest : &invalid_ingest;
}
