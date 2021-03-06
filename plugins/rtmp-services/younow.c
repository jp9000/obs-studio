#include <util/curl/curl-helper.h>
#include <stdlib.h>
#include <string.h>

#include <util/dstr.h>
#include "util/base.h"
#include "younow.h"

struct younow_mem_struct {
	char *memory;
	size_t size;
};

static char *current_ingest = NULL;

static size_t younow_write_cb(void *contents, size_t size, size_t nmemb,
			      void *userp)
{
	size_t realsize = size * nmemb;
	struct younow_mem_struct *mem = (struct younow_mem_struct *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		blog(LOG_WARNING, "yyounow_write_cb: realloc returned NULL");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

const char *younow_get_ingest(const char *server, const char *key)
{
	CURL *curl_handle;
	CURLcode res;
	struct younow_mem_struct chunk;
	struct dstr uri;
	long response_code;

	// find the delimiter in stream key
	const char *delim = strchr(key, '_');
	if (delim == NULL) {
		blog(LOG_WARNING,
		     "younow_get_ingest: delimiter not found in stream key");
		return server;
	}

	curl_handle = curl_easy_init();

	chunk.memory = malloc(1); /* will be grown as needed by realloc */
	chunk.size = 0;           /* no data at this point */

	dstr_init(&uri);
	dstr_copy(&uri, server);
	dstr_ncat(&uri, key, delim - key);

	curl_easy_setopt(curl_handle, CURLOPT_URL, uri.array);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, true);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 3L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, younow_write_cb);
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
		     "younow_get_ingest: curl_easy_perform() failed: %s",
		     curl_easy_strerror(res));
		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
		return server;
	}

	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code != 200) {
		blog(LOG_WARNING,
		     "younow_get_ingest: curl_easy_perform() returned code: %ld",
		     response_code);
		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
		return server;
	}

	curl_easy_cleanup(curl_handle);

	if (chunk.size == 0) {
		blog(LOG_WARNING,
		     "younow_get_ingest: curl_easy_perform() returned empty response");
		free(chunk.memory);
		return server;
	}

	if (current_ingest) {
		free(current_ingest);
		current_ingest = NULL;
	}

	current_ingest = strdup(chunk.memory);
	free(chunk.memory);
	blog(LOG_INFO, "younow_get_ingest: returning ingest: %s",
	     current_ingest);
	return current_ingest;
}
