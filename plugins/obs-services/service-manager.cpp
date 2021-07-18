#include "service-manager.hpp"

#include "plugin.hpp"

extern "C" {
#include <jansson.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <obs-module.h>
}

#include "json-format-ver.hpp"

static inline int get_int_val(json_t *service, const char *key)
{
	json_t *integer_val = json_object_get(service, key);
	if (!integer_val || !json_is_integer(integer_val))
		return 0;

	return (int)json_integer_value(integer_val);
}

static inline const char *get_string_val(json_t *service, const char *key)
{
	json_t *str_val = json_object_get(service, key);
	if (!str_val || !json_is_string(str_val))
		return NULL;

	return json_string_value(str_val);
}

static json_t *open_services_list(const char *file)
{
	char *file_data = os_quick_read_utf8_file(file);
	json_error_t error;
	json_t *root;
	json_t *list;
	int format_ver;

	if (!file_data)
		return NULL;

	root = json_loads(file_data, JSON_REJECT_DUPLICATES, &error);
	bfree(file_data);

	if (!root) {
		blog(LOG_WARNING, "%s: Error reading JSON file (%d): %s",
		     __func__, error.line, error.text);
		return NULL;
	}

	format_ver = get_int_val(root, "format_version");

	if (format_ver != SERVICES_FORMAT_VERSION) {
		blog(LOG_DEBUG, "%s: Wrong format version (%d), expected %d",
		     __func__, format_ver, SERVICES_FORMAT_VERSION);
		json_decref(root);
		return NULL;
	}

	list = json_object_get(root, "services");
	if (list)
		json_incref(list);
	json_decref(root);

	if (!list) {
		blog(LOG_WARNING, "%s: No services list", __func__);
		return NULL;
	}

	return list;
}

static json_t *get_services_list(void)
{
	char *file;
	json_t *list = NULL;

	file = obs_module_config_path("services.json");
	if (file) {
		list = open_services_list(file);
		bfree(file);
	}

	if (!list) {
		file = obs_module_file("services.json");
		if (file) {
			list = open_services_list(file);
			bfree(file);
		}
	}

	return list;
}

service_manager::~service_manager()
{
	_factories.clear();
}

void service_manager::register_services()
{
	json_t *services_list = get_services_list();
	json_t *service;
	size_t idx;

	json_array_foreach (services_list, idx, service) {
		const char *id = get_string_val(service, "id");

		if (id != NULL) {
			blog(LOG_DEBUG, "%s: Loading service with id \"%s\"",
			     __func__, id);
			_factories.emplace(
				id, std::make_shared<service_factory>(service));
			blog(LOG_DEBUG, "%s: Service with id \"%s\" was loaded",
			     __func__, id);
		} else
			blog(LOG_ERROR,
			     "%s: Unable to load as service JSON object n°%zu",
			     __func__, idx);
	}

	json_decref(service);
	json_decref(services_list);
}

std::shared_ptr<service_manager> _service_manager_instance = nullptr;

void service_manager::initialize()
{
	if (!_service_manager_instance) {
		_service_manager_instance = std::make_shared<service_manager>();
		_service_manager_instance->register_services();
	}
}

void service_manager::finalize()
{
	_service_manager_instance.reset();
}
