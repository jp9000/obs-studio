#include <file-updater/file-updater.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs-module.h>
#include <util/dstr.h>
#include <jansson.h>

#include <curl/curl.h>
#include <pthread.h>

#if defined(_WIN32)
#include <windows.h>
#endif // defined(_WIN32)

#include "onlyfans.h"
#include "check-connection.h"

typedef struct onlyfans_ingest onlyfans_ingest_t;

static update_info_t *onlyfans_update_info = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool ingests_refreshed = false;
static bool ingests_refreshing = false;
static bool ingests_loaded = false;
static bool update_stopped = false;
static pthread_t thread;
static char *stream_key = NULL;
//!< Keeps default a name of ingests file.
static const char *default_ingests_file_name = "onlyfans_servers.json";
static const char *default_ingests_temp_file_name =
	"onlyfans_servers.temp.json";
//!< Keeps a variable name of destination.
static const char *default_destination_url_name = "ONLYFANS_DEST_URL";
//!< Keeps a destination to get a list of ingests.
static const char *default_destination_url =
	"https://gateway.onlyfans.com/rtmp-servers";
//!< Keeps a destination url parameter name.
static const char *default_destination_url_param = "key";
// Keeps a list of supported protocols.
static const char *supported_protocols[] = {"srtmp"};

extern const char *get_module_name(void);
extern void onlyfans_ingests_refresh(int seconds);
extern void load_onlyfans_data(void);
extern struct obs_service_info rtmp_common_service;

struct ingest {
	char *name;
	char *title;
	char *type;
	char *url;
	char *test_url;
	int64_t rtt;
};
typedef struct ingest ingest_t;

static DARRAY(struct ingest) cur_ingests;

// Releases allocated memory.
static void free_ingests(void)
{
	for (size_t i = 0; i < cur_ingests.num; i++) {
		ingest_t *ingest = (ingest_t *)(cur_ingests.array + i);
		bfree(ingest->name);
		bfree(ingest->type);
		bfree(ingest->url);
		bfree(ingest->test_url);
		if (ingest->title) {
			bfree(ingest->title);
		}
	}
	da_free(cur_ingests);
}

// Sorts a list of ingests by RTT.
static void bubble_sort_by_rtt(ingest_t *data, size_t count)
{
	ingest_t tmp;
	for (size_t i = 0; i < count; ++i) {
		for (size_t j = count - 1; j >= i + 1; --j) {
			if (data[j].rtt < data[j - 1].rtt) {
				tmp = data[j];
				data[j] = data[j - 1];
				data[j - 1] = tmp;
			}
		}
	}
}

// Updating RTT field in the list of ingests.
static bool onlyfans_ingests_update_rtt()
{
	bool success = true;
	// Getting a count of ingests.
	const size_t ingests_count = onlyfans_ingest_count();
	for (size_t i = 0; i < ingests_count; ++i) {
		// Getting ingest.
		ingest_t *ingest = cur_ingests.array + i;
		if (ingest) {
			blog(LOG_INFO, "[start] Updating RTT value for [%s]",
			     ingest->name);
			// Updating ingest RTT.
			ingest->rtt = connection_time(ingest->test_url);
			if (success) {
				// Setting a status of updating RTT.
				success = ingest->rtt != INT32_MAX;
			}
			blog(LOG_INFO, "[done] Updating RTT value for [%s]: %d",
			     ingest->name, (int32_t)ingest->rtt);
		}
	}
	// Sorting the list of ingests by RTT.
	bubble_sort_by_rtt(cur_ingests.array, ingests_count);
	return success;
}

// Builds a new JSON object.
static json_t *build_object(const ingest_t *ingest)
{
	json_t *object = json_object();
	if (object != NULL) {
		json_object_set(object, "name", json_string(ingest->name));
		json_object_set(object, "title", json_string(ingest->title));
		json_object_set(object, "type", json_string(ingest->type));
		json_object_set(object, "url", json_string(ingest->url));
		json_object_set(object, "test_url",
				json_string(ingest->test_url));
		json_object_set(object, "rtt", json_integer(ingest->rtt));
	}
	return object;
}

static void dump_cache(const json_t *json, const char *file_name)
{
	char *onlyfans_cache = obs_module_config_path(file_name);
	if (onlyfans_cache) {
		char *dump = json_dumps(json, JSON_COMPACT);
		if (dump) {
			char *cache_new = obs_module_config_path(
				default_ingests_temp_file_name);
			os_quick_write_utf8_file(cache_new, dump, strlen(dump),
						 false);
			os_safe_replace(onlyfans_cache, cache_new, NULL);
			bfree(cache_new);
			//<???> bfree(dump);
		}
		// Releasing allocated memory.
		bfree(onlyfans_cache);
	}
}

static void save_cache_ingests()
{
	const size_t ingest_count = onlyfans_ingest_count();
	if (ingest_count > 0) {
		json_t *root = json_object();
		json_t *ingests = json_array();

		for (size_t i = 0; i < ingest_count; ++i) {
			const ingest_t *ingest = cur_ingests.array + i;
			// Adding a new ingest.
			json_t *object = build_object(ingest);
			if (object != NULL) {
				// Appending a new ingest.
				json_array_append(ingests, object);
				json_decref(object);
			}
		}
		// Appending the array into root object.
		json_object_set(root, "servers", ingests);
		// Dumping the cache into a file.
		dump_cache(root, default_ingests_file_name);

		json_decref(ingests);
		json_decref(root);
	}
}

static bool load_ingests_from_protocol(const char *protocol, json_t *root,
				       bool need_clear_before)
{
	if (!protocol || !root) {
		return false;
	}
	json_t *proto = json_object_get(root, protocol);
	if (!proto) {
		return false;
	}
	// Getting a list of servers.
	json_t *servers = json_object_get(proto, "ServerList");
	if (servers) {
		const size_t array_size = json_array_size(servers);
		if (array_size > 0 && need_clear_before) {
			// Clearing a list of servers.
			free_ingests();
		}
		for (size_t i = 0; i < array_size; ++i) {
			// Getting a server.
			json_t *item = json_array_get(servers, i);
			if (item) {
				ingest_t ingest = {.title = NULL,
						   .url = NULL,
						   .rtt = INT32_MAX,
						   .name = NULL,
						   .test_url = NULL,
						   .type = NULL};
				struct dstr url = {0};
				struct dstr test_url = {0};
				// Reading a set of fields.
				json_t *item_name =
					json_object_get(item, "name");
				json_t *item_title =
					json_object_get(item, "title");
				json_t *item_url =
					json_object_get(item, "rtmp_url");
				json_t *item_test_url =
					json_object_get(item, "url");
				json_t *item_rtt = json_object_get(item, "rtt");

				if (!item_name || !item_url || !item_test_url) {
					blog(LOG_WARNING,
					     "onlyfans.c: [load_ingests_from_protocol] invalid object: %s",
					     "Name or URL or Test URL are empty");
					continue;
				}
				const char *url_str =
					json_string_value(item_url);
				const char *test_url_str =
					json_string_value(item_test_url);
				const char *name_str =
					json_string_value(item_name);
				const char *title_str =
					json_string_value(item_title);

				dstr_copy(&url, url_str);
				dstr_copy(&test_url, test_url_str);

				ingest.name = bstrdup(name_str);
				ingest.type = bstrdup(protocol);
				ingest.url = url.array;
				ingest.test_url = test_url.array;
				ingest.rtt =
					(item_rtt != NULL)
						? json_integer_value(item_rtt)
						: INT32_MAX;
				if (title_str) {
					ingest.title = bstrdup(title_str);
				}
				// Adding a new ingest object.
				da_push_back(cur_ingests, &ingest);
			}
		}
		return (cur_ingests.num > 0);
	}
	return false;
}

static bool load_ingests_from_net(const char *json, bool write_file)
{
	// Loading JSON.
	json_t *root = json_loads(json, 0, NULL);
	if (!root) {
		return false;
	}
	bool need_clear_before = true;
	// Loading a list of supported protocols.
	for (size_t i = 0;
	     i < sizeof(supported_protocols) / sizeof(supported_protocols[0]);
	     ++i) {
		// Loading protocol section.
		load_ingests_from_protocol(supported_protocols[i], root,
					   need_clear_before);
		// Setting a new state.
		need_clear_before = false;
	}
	if (write_file) {
		// Saving a list of ingests.
		save_cache_ingests();
	}
	// Releasing the object.
	json_decref(root);
	return onlyfans_ingest_count() > 0;
}

static char *get_host_name(const char *url)
{
	const size_t length = strlen(url);
	size_t start = 0, last = 0;
	for (start = 0; start < length; ++start) {
		if ((url[start] == ':') &&
		    (start < last - 2 && url[start + 1] == '/' &&
		     url[start + 2] == '/')) {
			start += 3;
			for (last = start; last < length; ++last) {
				if (url[last] == '/') {
					break;
				}
			}
			break;
		}
	}
	if (last > start) {
		return bstrdup_n(url + start, last - start);
	}
	return NULL;
}

static char *os_getenv(const char *name)
{
#if defined(_WIN32)
	char *ptr = NULL;
	WCHAR buffer[1024] = {0};
	wchar_t *buffer_name = NULL;
	//
	os_utf8_to_wcs_ptr(name, strlen(name), &buffer_name);
	// Reading env.
	const DWORD wrote =
		GetEnvironmentVariable(buffer_name, buffer, sizeof(buffer) - 1);
	if (wrote > 0) {
		os_wcs_to_utf8_ptr(buffer, 0, &ptr);
		return bstrdup(ptr);
	}
	return NULL;
#else
	return getenv(name) ? bstrdup(getenv(name)) : NULL;
#endif // defined(_WIN32)
}

static char *build_test_url(const char *url)
{
	struct dstr test_url = {0};
	// Getting a server host.
	char *host = get_host_name(url);
	if (host) {
		dstr_copy(&test_url, "https://");
		dstr_cat(&test_url, host);
		dstr_cat(&test_url, "/nr-test");
		// Releasing allocated memory.
		bfree(host);
		return test_url.array;
	}
	return NULL;
}

static bool load_ingests(const char *json, bool write_file)
{
	json_t *root;
	json_t *servers;
	size_t count = 0;
	// Loading JSON.
	root = json_loads(json, 0, NULL);
	if (!root) {
		return false;
	}
	// Getting a list of servers.
	servers = json_object_get(root, "servers");
	if (servers) {
		// Getting a count of servers.
		count = json_array_size(servers);
		if (count > 0) {
			// Removing a list of servers.
			free_ingests();
		}
		for (size_t i = 0; i < count; i++) {
			json_t *item = json_array_get(servers, i);
			json_t *item_name = json_object_get(item, "name");
			json_t *item_title = json_object_get(item, "title");
			json_t *item_type = json_object_get(item, "type");
			json_t *item_url = json_object_get(item, "url");
			json_t *item_test_url =
				json_object_get(item, "test_url");
			ingest_t server = {0};
			struct dstr url = {0};
			struct dstr rtmp_url = {0};

			if (!item_name || !item_url) {
				continue;
			}
			const char *url_str = json_string_value(item_url);
			const char *rtmp_url_str =
				json_string_value(item_test_url);
			const char *name_str = json_string_value(item_name);
			const char *type_str = json_string_value(item_type);
			const char *title_str = json_string_value(item_title);

			dstr_copy(&url, url_str);
			dstr_copy(&rtmp_url, rtmp_url_str);

			server.name = bstrdup(name_str);
			server.type = bstrdup(type_str);
			server.url = url.array;
			server.test_url = rtmp_url.array;
			server.rtt = INT32_MAX;
			if (title_str) {
				server.title = bstrdup(title_str);
			}
			// Adding a new server.
			da_push_back(cur_ingests, &server);
		}
		if (write_file) {
			// Saving a list of ingests.
			save_cache_ingests();
		}
	}
	// Releasing allocated memory.
	json_decref(root);
	return (onlyfans_ingest_count() > 0);
}

static bool onlyfans_ingest_update(void *param, struct file_download_data *data)
{
	blog(LOG_INFO, "[start] Downloading a list of servers.");
	bool success = false;
	// Locking the section.
	onlyfans_ingests_lock();
	success = load_ingests_from_net((const char *)data->buffer.array, true);
	// Unlocking the section.
	onlyfans_ingests_unlock();
	// Setting a new state of loading a list of servers.
	os_atomic_set_bool(&ingests_loaded, success);
	if (os_atomic_load_bool(&ingests_loaded)) {
		// Locking the section.
		onlyfans_ingests_lock();
		// Updating RTT of ingests.
		success = onlyfans_ingests_update_rtt();
		// Unlocking the section.
		onlyfans_ingests_unlock();
		// Setting a new state of refreshing RTT values.
		os_atomic_set_bool(&ingests_refreshed, success);
	}
	UNUSED_PARAMETER(param);
	// Changed a state.
	os_atomic_set_bool(&ingests_refreshing, false);
	blog(LOG_INFO, "[done] Downloading a list of servers.");
	return success;
}

void onlyfans_ingests_lock(void)
{
	pthread_mutex_lock(&mutex);
}

void onlyfans_ingests_unlock(void)
{
	pthread_mutex_unlock(&mutex);
}

size_t onlyfans_ingest_count(void)
{
	return cur_ingests.num;
}

struct onlyfans_ingest get_onlyfans_ingest(size_t idx)
{
	onlyfans_ingest_t ingest = {
		.name = NULL, .url = NULL, .rtt = INT32_MAX};
	// Getting a count of ingests.
	const size_t count = onlyfans_ingest_count();
	if (idx < count) {
		// Getting ingest by index.
		const ingest_t *server = cur_ingests.array + idx;
		// Setting server info.
		ingest.name = (server->title) ? server->title : server->name;
		ingest.url = server->url;
		ingest.rtt = server->rtt;
	}
	return ingest;
}

struct onlyfans_ingest get_onlyfans_ingest_by_url(const char *url)
{
	onlyfans_ingest_t ingest = {
		.name = NULL, .url = NULL, .rtt = INT32_MAX};
	// Getting a count of ingests.
	const size_t count = onlyfans_ingest_count();
	for (size_t i = 0; i < count; ++i) {
		// Getting ingest by index.
		const ingest_t *server = cur_ingests.array + i;
		if (strcmp(server->url, url) == 0) {
			// Setting server info.
			ingest.name = (server->title) ? server->title
						      : server->name;
			ingest.url = server->url;
			ingest.rtt = server->rtt;
		}
	}
	return ingest;
}

void add_onlyfans_ingest(const char *name, const char *title, const char *url)
{
	ingest_t ingest = {.name = bstrdup(name),
			   .url = bstrdup(url),
			   .type = bstrdup("config"),
			   .test_url = build_test_url(url),
			   .rtt = INT32_MAX};
	if (title) {
		ingest.title = bstrdup(title);
	}
	// Adding a new ingest object.
	da_push_back(cur_ingests, &ingest);
}

void init_onlyfans_data(void)
{
	da_init(cur_ingests);
	pthread_mutex_init(&mutex, NULL);
#if defined(_WIN32)
	// Loading a list of ingests.
	load_onlyfans_data();
#endif // defined(_WIN32)
}

//!< Sets a new stream key.
void set_stream_key(const char *key)
{
	if (stream_key) {
		bfree(stream_key);
		stream_key = NULL;
	}
	if (key) {
		// Setting a new stream key.
		stream_key = bstrdup(key);
	}
}

void onlyfans_update_rtt()
{
	if (!os_atomic_load_bool(&ingests_refreshed)) {
		// Locking the list of ingests.
		onlyfans_ingests_lock();
		// Updating RTT of ingests.
		onlyfans_ingests_update_rtt();
		// Unlocking the list of ingests.
		onlyfans_ingests_unlock();
	}
}

void onlyfans_ingests_refresh(int seconds)
{
	if (os_atomic_load_bool(&ingests_refreshed)) {
		return;
	}
	blog(LOG_INFO, "[start] Refreshing a list of servers.");
	if (!os_atomic_load_bool(&ingests_refreshing)) {
		os_atomic_set_bool(&ingests_refreshing, true);
		os_atomic_set_bool(&ingests_loaded, false);
		// Reading a variable.
		char *dest_url = os_getenv(default_destination_url_name);
		if (!dest_url) {
			// Getting destination url.
			dest_url = bstrdup(default_destination_url);
		}
		struct dstr dest_uri = {0};
		if (stream_key) {
			dstr_copy(&dest_uri, dest_url);
			dstr_cat(&dest_uri, "?");
			dstr_cat(&dest_uri, default_destination_url_param);
			dstr_cat(&dest_uri, "=");
			dstr_cat(&dest_uri, stream_key);
		} else {
			dstr_copy(&dest_uri, dest_url);
		}
		blog(LOG_INFO, "Dest uri: %s", dest_uri.array);
		onlyfans_update_info = update_info_create_single(
			"[onlyfans ingest update] ", get_module_name(),
			dest_uri.array, onlyfans_ingest_update, NULL);
		// Releasing allocated memory.
		dstr_free(&dest_uri);
		bfree(dest_url);
	}

	/* wait five seconds max when loading ingests for the first time */
	if (!os_atomic_load_bool(&ingests_loaded)) {
		for (int i = 0; i < seconds * 100; i++) {
			if (os_atomic_load_bool(&ingests_refreshed)) {
				break;
			}
			os_sleep_ms(10);
		}
	}
	os_atomic_set_bool(&ingests_refreshing, false);
	blog(LOG_INFO, "[done] Refreshing a list of servers.");
}

void load_onlyfans_data(void)
{
	blog(LOG_DEBUG,
	     "onlyfans.c: [load_onlyfans_data] [start] loading a list of servers");
	char *onlyfans_cache =
		obs_module_config_path(default_ingests_file_name);
	// Setting a new state.
	os_atomic_set_bool(&ingests_loaded, false);
	// Trying to get a list of servers.
	onlyfans_ingests_refresh(5);
	if (!os_atomic_load_bool(&ingests_loaded) &&
	    !os_atomic_load_bool(&ingests_refreshing)) {
		if (onlyfans_cache && os_file_exists(onlyfans_cache)) {
			blog(LOG_WARNING,
			     "[start] Loading a list of servers from cache [%s]",
			     onlyfans_cache);
			bool success = false;
			char *data = os_quick_read_utf8_file(onlyfans_cache);
			if (data) {
				onlyfans_ingests_lock();
				success = load_ingests(data, false);
				onlyfans_ingests_unlock();
				// Releasing allocated memory.
				bfree(data);
			}
			os_atomic_set_bool(&ingests_loaded, success);
			blog(LOG_WARNING,
			     "[done] Loading a list of servers from cache [%s]: %s",
			     onlyfans_cache, (success) ? "success" : "fail");
		} else {
			blog(LOG_ERROR,
			     "Load a list of server failed: no cache file found");
		}
	}
	if (onlyfans_cache) {
		// Releasing allocated resources.
		bfree(onlyfans_cache);
	}
	blog(LOG_DEBUG,
	     "onlyfans.c: [load_onlyfans_data] [done] loading a list of servers");
}

void unload_onlyfans_data(void)
{
	os_atomic_set_bool(&update_stopped, true);
	pthread_join(thread, NULL);
	// Saving OnlyFans cache.
	save_cache_ingests();
	update_info_destroy(onlyfans_update_info);
	// Releasing allocated resource.
	free_ingests();
	if (stream_key) {
		bfree(stream_key);
		stream_key = NULL;
	}
	pthread_mutex_destroy(&mutex);
}
