/******************************************************************************
    Copyright (C) 2019-2020 by Dillon Pentz <dillon@vodbox.io>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "importers.hpp"

using namespace std;
using namespace json11;

static string translate_key(const string &sl_key)
{
	if (sl_key.substr(0, 6) == "Numpad" && sl_key.size() == 7) {
		return "OBS_KEY_NUM" + sl_key.substr(6);
	} else if (sl_key.substr(0, 3) == "Key") {
		return "OBS_KEY_" + sl_key.substr(3);
	} else if (sl_key.substr(0, 5) == "Digit") {
		return "OBS_KEY_" + sl_key.substr(5);
	} else if (sl_key[0] == 'F' && sl_key.size() < 4) {
		return "OBS_KEY_" + sl_key;
	}

#define add_translation(str, out) \
	if (sl_key == str) {      \
		return out;       \
	}

	add_translation("Backquote", "OBS_KEY_ASCIITILDE");
	add_translation("Backspace", "OBS_KEY_BACKSPACE");
	add_translation("Tab", "OBS_KEY_TAB");
	add_translation("Space", "OBS_KEY_SPACE");
	add_translation("Period", "OBS_KEY_PERIOD");
	add_translation("Slash", "OBS_KEY_SLASH");
	add_translation("Backslash", "OBS_KEY_BACKSLASH");
	add_translation("Minus", "OBS_KEY_MINUS");
	add_translation("Comma", "OBS_KEY_COMMA");
	add_translation("Plus", "OBS_KEY_PLUS");
	add_translation("Quote", "OBS_KEY_APOSTROPHE");
	add_translation("Semicolon", "OBS_KEY_SEMICOLON");
	add_translation("NumpadSubtract", "OBS_KEY_NUMMINUS");
	add_translation("NumpadAdd", "OBS_KEY_NUMPLUS");
	add_translation("NumpadDecimal", "OBS_KEY_NUMPERIOD");
	add_translation("NumpadDivide", "OBS_KEY_NUMSLASH");
	add_translation("NumpadMultiply", "OBS_KEY_NUMASTERISK");
	add_translation("Enter", "OBS_KEY_RETURN");
	add_translation("CapsLock", "OBS_KEY_CAPSLOCK");
	add_translation("NumLock", "OBS_KEY_NUMLOCK");
	add_translation("ScrollLock", "OBS_KEY_SCROLLLOCK");
	add_translation("Pause", "OBS_KEY_PAUSE");
	add_translation("Insert", "OBS_KEY_INSERT");
	add_translation("Home", "OBS_KEY_HOME");
	add_translation("End", "OBS_KEY_END");
	add_translation("Escape", "OBS_KEY_ESCAPE");
	add_translation("Delete", "OBS_KEY_DELETE");
	add_translation("ArrowUp", "OBS_KEY_UP");
	add_translation("ArrowDown", "OBS_KEY_DOWN");
	add_translation("ArrowLeft", "OBS_KEY_LEFT");
	add_translation("ArrowRight", "OBS_KEY_RIGHT");
	add_translation("PageUp", "OBS_KEY_PAGEUP");
	add_translation("PageDown", "OBS_KEY_PAGEDOWN");
	add_translation("BracketLeft", "OBS_KEY_BRACKETLEFT");
	add_translation("BracketRight", "OBS_KEY_BRACKETRIGHT");
#undef add_translation

	return "";
}

static string translate_hotkey(const Json &hotkey, const string &source)
{
	string name = hotkey["actionName"].string_value();

#define add_translation(in, str, out, source) \
	if (in == str) {                      \
		return out + source;          \
	}

	add_translation(name, "TOGGLE_SOURCE_VISIBILITY_SHOW",
			"libobs.show_scene_item.", source);
	add_translation(name, "TOGGLE_SOURCE_VISIBILITY_HIDE",
			"libobs.hide_scene_item.", source);

	string empty = "";

	add_translation(name, "SWITCH_TO_SCENE", "OBSBasic.SelectScene", empty);

	add_translation(name, "TOGGLE_MUTE", "libobs.mute", empty);
	add_translation(name, "TOGGLE_UNMUTE", "libobs.unmute", empty);
	add_translation(name, "PUSH_TO_MUTE", "libobs.push-to-mute", empty);
	add_translation(name, "PUSH_TO_TALK", "libobs.push-to-talk", empty);
	add_translation(name, "GAME_CAPTURE_HOTKEY_START", "hotkey_start",
			empty);
	add_translation(name, "GAME_CAPTURE_HOTKEY_STOP", "hotkey_stop", empty);

	return "";
#undef add_translation
}

static bool source_name_exists(const Json::array &sources, const string &name)
{
	for (size_t i = 0; i < sources.size(); i++) {
		Json item = sources[i];
		string source_name = item["name"].string_value();

		if (source_name == name)
			return true;
	}

	return false;
}

static string get_source_name_from_id(const Json &root,
				      const Json::array &sources,
				      const string &id)
{
	for (size_t i = 0; i < sources.size(); i++) {
		Json item = sources[i];
		string source_id = item["sl_id"].string_value();

		if (source_id == id)
			return item["name"].string_value();
	}

	Json::array scene_arr = root["scenes"]["items"].array_items();

	for (size_t i = 0; i < scene_arr.size(); i++) {
		Json item = scene_arr[i];
		string source_id = item["id"].string_value();

		if (source_id == id) {
			string name = item["name"].string_value();

			int copy = 1;
			string out_name = name;

			while (source_name_exists(sources, out_name))
				out_name = name + "(" + to_string(copy++) + ")";

			return out_name;
		}
	}

	return "";
}

static void get_hotkey_bindings(Json::object &out_hotkeys,
				const Json &in_hotkeys, const string &name)
{
	Json::array hot_arr = in_hotkeys.array_items();
	for (size_t i = 0; i < hot_arr.size(); i++) {
		Json hotkey = hot_arr[i];
		Json::array bindings = hotkey["bindings"].array_items();
		Json::array out_hotkey = Json::array{};

		string hotkey_name = translate_hotkey(hotkey, name);

		for (size_t x = 0; x < bindings.size(); x++) {
			Json binding = bindings[x];
			Json modifiers = binding["modifiers"];

			string key =
				translate_key(binding["key"].string_value());

			out_hotkey.push_back(
				Json::object{{"control", modifiers["ctrl"]},
					     {"shift", modifiers["shift"]},
					     {"command", modifiers["meta"]},
					     {"alt", modifiers["alt"]},
					     {"key", key}});
		}

		out_hotkeys[hotkey_name] = out_hotkey;
	}
}

static void get_scene_items(const Json &root, const Json::array &out_sources,
			    Json::object &scene, const Json::array &in)
{
	int length = 0;

	Json::object hotkeys = scene["hotkeys"].object_items();

	Json::array out_items = Json::array{};
	for (size_t i = 0; i < in.size(); i++) {
		Json item = in[i];

		Json in_crop = item["crop"];
		string id = item["sourceId"].string_value();
		string name = get_source_name_from_id(root, out_sources, id);

		Json::array hotkey_items =
			item["hotkeys"]["items"].array_items();

		get_hotkey_bindings(hotkeys, hotkey_items, name);

		out_items.push_back(Json::object{
			{"name", name},
			{"id", length++},
			{"pos",
			 Json::object{{"x", item["x"]}, {"y", item["y"]}}},
			{"scale", Json::object{{"x", item["scaleX"]},
					       {"y", item["scaleY"]}}},
			{"rot", item["rotation"]},
			{"visible", item["visible"]},
			{"crop_top", in_crop["top"]},
			{"crop_bottom", in_crop["bottom"]},
			{"crop_left", in_crop["left"]},
			{"crop_right", in_crop["right"]}});
	}

	scene["hotkeys"] = hotkeys;
	scene["settings"] =
		Json::object{{"items", out_items}, {"id_counter", length}};
}

static int attempt_import(const Json &root, const string &name, Json &res)
{
	Json::array source_arr = root["sources"]["items"].array_items();
	Json::array scenes_arr = root["scenes"]["items"].array_items();
	Json::array t_arr = root["transitions"]["transitions"].array_items();

	string t_id = root["transitions"]["defaultTransitionId"].string_value();

	Json::array out_sources = Json::array{};
	Json::array out_transitions = Json::array{};

	for (size_t i = 0; i < source_arr.size(); i++) {
		Json source = source_arr[i];

		Json in_hotkeys = source["hotkeys"];
		Json::array hotkey_items =
			source["hotkeys"]["items"].array_items();
		Json in_filters = source["filters"];
		Json::array filter_items = in_filters["items"].array_items();

		Json in_settings = source["settings"];
		Json in_sync = source["syncOffset"];

		int sync = (int)(in_sync["sec"].number_value() * 1000000000 +
				 in_sync["nsec"].number_value());

		double vol = source["volume"].number_value();
		bool muted = source["muted"].bool_value();
		string name = source["name"].string_value();
		int monitoring = (int)source["monitoringType"].int_value();

		Json::object out_hotkeys = Json::object{};
		get_hotkey_bindings(out_hotkeys, hotkey_items, "");

		Json::array out_filters = Json::array{};
		for (size_t x = 0; x < filter_items.size(); x++) {
			Json::object filter = filter_items[x].object_items();
			string type = filter["type"].string_value();
			filter["id"] = type;

			out_filters.push_back(filter);
		}

		int copy = 1;
		string out_name = name;
		while (source_name_exists(out_sources, out_name))
			out_name = name + "(" + to_string(copy++) + ")";

		string sl_id = source["id"].string_value();

		out_sources.push_back(
			Json::object{{"filters", out_filters},
				     {"hotkeys", out_hotkeys},
				     {"id", source["type"]},
				     {"sl_id", sl_id},
				     {"settings", in_settings},
				     {"sync", sync},
				     {"volume", vol},
				     {"muted", muted},
				     {"name", out_name},
				     {"monitoring_type", monitoring}});
	}

	string scene_name = "";

	for (size_t i = 0; i < scenes_arr.size(); i++) {
		Json scene = scenes_arr[i];

		Json in_hotkeys = scene["hotkeys"];
		Json::array hotkey_items = in_hotkeys["items"].array_items();
		Json in_filters = scene["filters"];
		Json::array filter_items = in_filters["items"].array_items();

		Json in_settings = scene["settings"];

		string name = scene["name"].string_value();

		Json::object out_hotkeys = Json::object{};
		get_hotkey_bindings(out_hotkeys, hotkey_items, "");

		Json::array out_filters = Json::array{};
		for (size_t x = 0; x < filter_items.size(); x++) {
			Json::object filter = filter_items[x].object_items();
			string type = filter["type"].string_value();
			filter["id"] = type;

			out_filters.push_back(filter);
		}

		int copy = 1;
		string out_name = name;
		while (source_name_exists(out_sources, out_name))
			out_name = name + "(" + to_string(copy++) + ")";

		if (scene_name.empty())
			scene_name = out_name;

		string sl_id = scene["id"].string_value();

		Json::object out =
			Json::object{{"filters", out_filters},
				     {"hotkeys", out_hotkeys},
				     {"id", "scene"},
				     {"sl_id", sl_id},
				     {"settings", in_settings},
				     {"volume", 1.0},
				     {"name", out_name},
				     {"private_settings", Json::object{}}};

		Json in_items = scene["sceneItems"];
		Json::array items_arr = in_items["items"].array_items();

		get_scene_items(root, out_sources, out, items_arr);

		out_sources.push_back(out);
	}

	string transition_name = "";

	for (size_t i = 0; i < t_arr.size(); i++) {
		Json transition = t_arr[i];

		Json in_settings = transition["settings"];

		int duration = transition["duration"].int_value();
		string name = transition["name"].string_value();
		string id = transition["id"].string_value();

		if (id == t_id)
			transition_name = name;

		out_transitions.push_back(
			Json::object{{"id", transition["type"]},
				     {"settings", in_settings},
				     {"name", name},
				     {"duration", duration}});
	}

	res = Json::object{{"sources", out_sources},
			   {"transitions", out_transitions},
			   {"current_scene", scene_name},
			   {"current_program_scene", scene_name},
			   {"current_transition", transition_name},
			   {"name", name == "" ? "Streamlabs Import" : name}};

	return IMPORTER_SUCCESS;
}

string SLImporter::Name(const string &path)
{
	string name;

	string folder = GetFolderFromPath(path);
	string manifest_file = GetFilenameFromPath(path);
	string manifest_path = folder + "manifest.json";

	if (os_file_exists(manifest_path.c_str())) {
		BPtr<char> file_data =
			os_quick_read_utf8_file(manifest_path.c_str());

		string err;
		Json data = Json::parse(file_data, err);

		if (err == "") {
			Json::array collections =
				data["collections"].array_items();

			bool name_set = false;

			for (size_t i = 0, l = collections.size(); i < l; i++) {
				Json c = collections[i];
				string c_id = c["id"].string_value();
				string c_name = c["name"].string_value();

				if (c_id == manifest_file) {
					name = std::move(c_name);
					name_set = true;
					break;
				}
			}

			if (!name_set) {
				name = "Unknown Streamlabs Import";
			}
		}
	} else {
		name = "Unknown Streamlabs Import";
	}

	return name;
}

int SLImporter::ImportScenes(const string &path, string &name, Json &res)
{
	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());

	std::string err;
	Json data = Json::parse(file_data, err);

	if (err != "")
		return IMPORTER_ERROR_DURING_CONVERSION;

	string node_type = data["nodeType"].string_value();

	int result = IMPORTER_ERROR_DURING_CONVERSION;

	if (node_type == "RootNode") {
		if (name == "") {
			string auto_name = Name(path);
			result = attempt_import(data, auto_name, res);
		} else {
			result = attempt_import(data, name, res);
		}
	}

	TranslateOSStudio(res);

	return result;
}

bool SLImporter::Check(const string &path)
{
	bool check = false;

	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());

	if (file_data) {
		string err;
		Json root = Json::parse(file_data, err);

		if (!root.is_null()) {
			string node_type = root["nodeType"].string_value();

			if (node_type == "RootNode")
				check = true;
		}
	}

	return check;
}

OBSImporterFiles SLImporter::FindFiles()
{
	OBSImporterFiles res;
#ifdef _WIN32
	char dst[512];

	int found = os_get_config_path(dst, 512,
				       "slobs-client\\SceneCollections\\");
	if (found == -1)
		return res;

	os_dir_t *dir = os_opendir(dst);
	struct os_dirent *ent;
	while ((ent = os_readdir(dir)) != NULL) {
		string name = ent->d_name;

		if (ent->directory || name[0] == '.' || name == "manifest.json")
			continue;

		size_t pos = name.find_last_of(".json");
		size_t end_pos = name.size() - 1;
		if (pos != -1 && pos == end_pos) {
			string str = dst + name;
			res.push_back(str);
		}
	}
	os_closedir(dir);
#endif
	return res;
}
