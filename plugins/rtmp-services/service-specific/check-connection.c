#include "check-connection.h"
#include <string.h>

#include <util/base.h>
#include <util/bmem.h>
#include <util/dstr.h>

#include <util/platform.h>

#include <curl/curl.h>
#include <util/curl/curl-helper.h>

#ifndef NSEC_TO_MSEC
#define NSEC_TO_MSEC 1000000ULL
#endif // NSEC_TO_MSEC
// Keeps a connection timeout.
static long g_default_timeout = 3L;
// Gets current time in milliseconds.
static uint64_t now()
{
	return os_gettime_ns() / NSEC_TO_MSEC;
}
// Connects to ingest server.
static bool check_connection(const char *url)
{
	struct dstr uri = {0};
	CURLcode code = CURLE_FAILED_INIT;
	long response_code = 0;
	CURL *handle = curl_easy_init();

	if (handle) {
		dstr_init(&uri);
		dstr_copy(&uri, url);
		curl_easy_setopt(handle, CURLOPT_URL, uri.array);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, true);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(handle, CURLOPT_TIMEOUT, g_default_timeout);
		curl_obs_set_revoke_setting(handle);
#if LIBCURL_VERSION_NUM >= 0x072400
		curl_easy_setopt(handle, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif // LIBCURL_VERSION_NUM >= 0x072400
		code = curl_easy_perform(handle);
		// Releasing allocated memory.
		dstr_free(&uri);
		if (code != CURLE_OK) {
			blog(LOG_WARNING,
			     "check-connection.c: [check_connection] connection failed: %s to %s",
			     curl_easy_strerror(code), url);
		} else {
			curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE,
					  &response_code);
			if (response_code != 200) {
				blog(LOG_WARNING,
				     "check-connection.c: curl_easy_perform() returned "
				     "code: %ld",
				     response_code);
			}
		}
		curl_easy_cleanup(handle);
	}
	return (code == CURLE_OK);
}

static char *get_host_name(const char *url, const char *protocol)
{
	if (url && protocol) {
		const size_t protocol_length = strlen(protocol);
		for (size_t i = protocol_length; i < strlen(url); ++i) {
			if (url[i] == ':' || url[i] == '/') {
				return bstrdup_n(url + protocol_length,
						 i - protocol_length);
			}
		}
	}
	return NULL;
}

uint64_t connection_time(const char *url)
{
	// Getting start time of the operation.
	const uint64_t start_time = now();
	// Checking connection to host.
	const bool status = check_connection(url);
	return (status) ? (now() - start_time) : INT32_MAX;
}
