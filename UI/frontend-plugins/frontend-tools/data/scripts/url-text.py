import obspython as obs
import urllib.request
import urllib.error

url         = ""
interval    = 30
source_name = ""

# ------------------------------------------------------------

def update_text():
	global url
	global interval
	global source_name

	source = obs.obs_get_source_by_name(source_name)
	if source is not None:
		try:
			with urllib.request.urlopen(url) as response:
				data = response.read()
				text = data.decode('utf-8')

				settings = obs.obs_data_create()
				obs.obs_data_set_string(settings, "text", text)
				obs.obs_source_update(source, settings)
				obs.obs_data_release(settings)

		except urllib.error.URLError as err:
			obs.script_log(obs.LOG_WARNING, "Error opening URL '" + url + "': " + err.reason)
			obs.remove_current_callback()

		obs.obs_source_release(source)

def refresh_pressed(props, prop):
	update_text()

# ------------------------------------------------------------

def script_description():
	return "Updates a text source to the text retrieved from a URL at every specified interval.\n\nBy Jim"

def script_update(settings):
	global url
	global interval
	global source_name

	url         = obs.obs_data_get_string(settings, "url")
	interval    = obs.obs_data_get_int(settings, "interval")
	source_name = obs.obs_data_get_string(settings, "source")

	obs.timer_remove(update_text)

	if url != "" and source_name != "":
		obs.timer_add(update_text, interval * 1000)

def script_defaults(settings):
	obs.obs_data_set_default_int(settings, "interval", 30)

def script_properties():
	props = obs.obs_properties_create()

	obs.obs_properties_add_text(props, "url", "URL", obs.OBS_TEXT_DEFAULT)
	obs.obs_properties_add_int(props, "interval", "Update Interval (seconds)", 5, 3600, 1)

	p = obs.obs_properties_add_list(props, "source", "Text Source", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
	sources = obs.obs_enum_sources()
	if sources is not None:
		for source in sources:
			source_id = obs.obs_source_get_unversioned_id(source)
			if source_id == "text_gdiplus" or source_id == "text_ft2_source":
				name = obs.obs_source_get_name(source)
				obs.obs_property_list_add_string(p, name, name)

		obs.source_list_release(sources)

	obs.obs_properties_add_button(props, "button", "Refresh", refresh_pressed)
	return props
