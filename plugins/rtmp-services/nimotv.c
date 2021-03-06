#include <util/curl/curl-helper.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <util/dstr.h>
#include "util/base.h"
#include "nimotv.h"
#include <time.h>

struct nimotv_mem_struct {
	char *memory;
	size_t size;
};
static char *current_ingest = NULL;
static time_t last_time = -1;

static size_t nimotv_write_cb(void *contents, size_t size, size_t nmemb,
			      void *userp)
{
	size_t realsize = size * nmemb;
	struct nimotv_mem_struct *mem = (struct nimotv_mem_struct *)userp;

	char *ptr = realloc(mem->memory, mem->size + realsize + 1);
	if (ptr == NULL) {
		blog(LOG_WARNING, "nimotv_write_cb: realloc returned NULL");
		return 0;
	}

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

static char *load_ingest(const char *json)
{
	json_t *root = json_loads(json, 0, NULL);
	char *ingest = NULL;
	if (!root)
		return ingest;

	json_t *recommended_ingests = json_object_get(root, "ingests");
	if (recommended_ingests) {
		json_t *recommended = json_array_get(recommended_ingests, 0);
		if (recommended) {
			json_t *item_url = json_object_get(recommended, "url");
			if (item_url) {
				const char *url = json_string_value(item_url);
				ingest = bstrdup(url);
			}
		}
	}

	json_decref(root);
	return ingest;
}

const char *nimotv_get_ingest(const char *key)
{
	if (current_ingest != NULL) {
		time_t now = time(NULL);
		double diff = difftime(now, last_time);
		if (diff < 2) {
			blog(LOG_INFO,
			     "nimotv_get_ingest: returning ingest from cache: %s",
			     current_ingest);
			return current_ingest;
		}
	}

	CURL *curl_handle;
	CURLcode res;
	struct nimotv_mem_struct chunk;
	struct dstr uri;
	long response_code;

	curl_handle = curl_easy_init();
	chunk.memory = malloc(1); /* will be grown as needed by realloc */
	chunk.size = 0;           /* no data at this point */

	char *encoded_key = curl_easy_escape(NULL, key, 0);
	dstr_init(&uri);
	dstr_copy(&uri, "https://globalcdnweb.nimo.tv/api/ingests/nimo?id=");
	dstr_ncat(&uri, encoded_key, strlen(encoded_key));
	curl_free(encoded_key);

	curl_easy_setopt(curl_handle, CURLOPT_URL, uri.array);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, true);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 3L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, nimotv_write_cb);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_obs_set_revoke_setting(curl_handle);

#if LIBCURL_VERSION_NUM >= 0x072400
	// A lot of servers don't yet support ALPN
	curl_easy_setopt(curl_handle, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif

	res = curl_easy_perform(curl_handle);
	dstr_free(&uri);

	if (res != CURLE_OK) {
		blog(LOG_WARNING,
		     "nimotv_get_ingest: curl_easy_perform() failed: %s",
		     curl_easy_strerror(res));
		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
		return NULL;
	}

	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code != 200) {
		blog(LOG_WARNING,
		     "nimotv_get_ingest: curl_easy_perform() returned code: %ld",
		     response_code);
		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
		return NULL;
	}

	curl_easy_cleanup(curl_handle);

	if (chunk.size == 0) {
		blog(LOG_WARNING,
		     "nimotv_get_ingest: curl_easy_perform() returned empty response");
		free(chunk.memory);
		return NULL;
	}

	if (current_ingest != NULL) {
		bfree(current_ingest);
	}

	current_ingest = load_ingest(chunk.memory);
	last_time = time(NULL);

	free(chunk.memory);
	blog(LOG_INFO, "nimotv_get_ingest: returning ingest: %s",
	     current_ingest);

	return current_ingest;
}
