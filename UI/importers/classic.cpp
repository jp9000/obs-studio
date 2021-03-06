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

#include <QByteArray>

using namespace std;
using namespace json11;

static bool source_name_exists(const Json::array &sources, const string &name)
{
	for (size_t i = 0; i < sources.size(); i++) {
		Json source = sources[i];
		if (name == source["name"].string_value())
			return true;
	}

	return false;
}

#define translate_int(in_key, in, out_key, out, off) \
	out[out_key] = in[in_key].int_value() + off;
#define translate_string(in_key, in, out_key, out) out[out_key] = in[in_key];
#define translate_double(in_key, in, out_key, out) \
	translate_string(in_key, in, out_key, out);
#define translate_bool(in_key, in, out_key, out) \
	out[out_key] = in[in_key].int_value() == 1;

static Json::object translate_scene_item(const Json &in, const Json &source)
{
	Json::object item = Json::object{};

	translate_string("name", source, "name", item);

	translate_int("crop.top", in, "crop_top", item, 0);
	translate_int("crop.bottom", in, "crop_bottom", item, 0);
	translate_int("crop.left", in, "crop_left", item, 0);
	translate_int("crop.right", in, "crop_right", item, 0);

	Json::object pos = Json::object{};
	translate_int("x", in, "x", pos, 0);
	translate_int("y", in, "y", pos, 0);

	Json::object bounds = Json::object{};
	translate_int("cx", in, "x", bounds, 0);
	translate_int("cy", in, "y", bounds, 0);

	item["pos"] = pos;
	item["bounds"] = bounds;
	item["bounds_type"] = 2;
	item["visible"] = true;

	return item;
}

static int red_blue_swap(int color)
{
	int r = color / 256 / 256;
	int b = color % 256;

	return color - (r * 65536) - b + (b * 65536) + r;
}

static void create_string_obj(const string &data, Json::array &arr);

static Json::object translate_source(const Json &in, const Json &sources)
{
	string id = in["class"].string_value();
	string name = in["name"].string_value();

	Json::array source_arr = sources.array_items();

	if (id == "GlobalSource") {
		for (size_t i = 0; i < source_arr.size(); i++) {
			Json source = source_arr[i];
			if (name == source["name"].string_value()) {
				Json::object obj = source.object_items();
				obj["preexist"] = true;
				return obj;
			}
		}
	}

	Json in_settings = in["data"];

	Json::object settings = Json::object{};
	Json::object out = Json::object{};

	int i = 0;
	string new_name = name;
	while (source_name_exists(source_arr, new_name)) {
		new_name = name + to_string(++i);
	}
	out["name"] = new_name;

	if (id == "TextSource") {
		out["id"] = "text_gdiplus";

		int color = in_settings["color"].int_value() + 16777216;
		color = red_blue_swap(color) + 4278190080;
		settings["color"] = color;

		color = in_settings["backgroundColor"].int_value();
		color = red_blue_swap(color + 16777216) + 4278190080;
		settings["bk_color"] = color;

		color = in_settings["outlineColor"].int_value();
		color = red_blue_swap(color + 16777216) + 4278190080;
		settings["outline_color"] = color;

		translate_string("text", in_settings, "text", settings);
		translate_int("backgroundOpacity", in_settings, "bk_opacity",
			      settings, 0);
		translate_bool("vertical", in_settings, "vertical", settings);
		translate_int("textOpacity", in_settings, "opacity", settings,
			      0);
		translate_bool("useOutline", in_settings, "outline", settings);
		translate_int("outlineOpacity", in_settings, "outline_opacity",
			      settings, 0);
		translate_int("outlineSize", in_settings, "outline_size",
			      settings, 0);
		translate_bool("useTextExtents", in_settings, "extents",
			       settings);
		translate_int("extentWidth", in_settings, "extents_cx",
			      settings, 0);
		translate_int("extentHeight", in_settings, "extents_cy",
			      settings, 0);
		translate_bool("mode", in_settings, "read_from_file", settings);
		translate_bool("wrap", in_settings, "extents_wrap", settings);

		string str = in_settings["file"].string_value();
		settings["file"] = StringReplace(str, "\\\\", "/");

		int in_align = in_settings["align"].int_value();
		string align = in_align == 0
				       ? "left"
				       : (in_align == 1 ? "center" : "right");

		settings["align"] = align;

		bool bold = in_settings["bold"].int_value() == 1;
		bool italic = in_settings["italic"].int_value() == 1;
		bool underline = in_settings["underline"].int_value() == 1;

		int flags = bold ? OBS_FONT_BOLD : 0;
		flags |= italic ? OBS_FONT_ITALIC : 0;
		flags |= underline ? OBS_FONT_UNDERLINE : 0;

		Json::object font = Json::object{};

		font["flags"] = flags;

		translate_int("fontSize", in_settings, "size", font, 0);
		translate_string("font", in_settings, "face", font);

		if (bold && italic) {
			font["style"] = "Bold Italic";
		} else if (bold) {
			font["style"] = "Bold";
		} else if (italic) {
			font["style"] = "Italic";
		} else {
			font["style"] = "Regular";
		}

		settings["font"] = font;
	} else if (id == "MonitorCaptureSource") {
		out["id"] = "monitor_capture";

		translate_int("monitor", in_settings, "monitor", settings, 0);
		translate_bool("captureMouse", in_settings, "capture_cursor",
			       settings);
	} else if (id == "BitmapImageSource") {
		out["id"] = "image_source";

		string str = in_settings["path"].string_value();
		settings["file"] = StringReplace(str, "\\\\", "/");
	} else if (id == "BitmapTransitionSource") {
		out["id"] = "slideshow";

		Json files = in_settings["bitmap"];

		if (!files.is_array()) {
			files = Json::array{in_settings["bitmap"]};
		}

		settings["files"] = files;
	} else if (id == "WindowCaptureSource") {
		out["id"] = "window_capture";

		string win = in_settings["window"].string_value();
		string winClass = in_settings["windowClass"].string_value();

		win = StringReplace(win, "/", "\\\\");
		win = StringReplace(win, ":", "#3A");
		winClass = StringReplace(winClass, ":", "#3A");

		settings["window"] = win + ":" + winClass + ":";
		settings["priority"] = 0;
	} else if (id == "CLRBrowserSource") {
		out["id"] = "browser_source";

		string browser_dec =
			QByteArray::fromBase64(in_settings["sourceSettings"]
						       .string_value()
						       .c_str())
				.toStdString();

		string err;

		Json browser = Json::parse(browser_dec, err);

		if (err != "")
			return Json::object{};

		Json::object obj = browser.object_items();

		translate_string("CSS", obj, "css", settings);
		translate_int("Height", obj, "height", settings, 0);
		translate_int("Width", obj, "width", settings, 0);
		translate_string("Url", obj, "url", settings);
	} else if (id == "DeviceCapture") {
		out["id"] = "dshow_input";

		string device_id = in_settings["deviceID"].string_value();
		string device_name = in_settings["deviceName"].string_value();

		settings["video_device_id"] = device_name + ":" + device_id;

		int w = in_settings["resolutionWidth"].int_value();
		int h = in_settings["resolutionHeight"].int_value();

		settings["resolution"] = to_string(w) + "x" + to_string(h);
	} else if (id == "GraphicsCapture") {
		bool hotkey = in_settings["useHotkey"].int_value() == 1;

		if (hotkey) {
			settings["capture_mode"] = "hotkey";
		} else {
			settings["capture_mode"] = "window";
		}

		string winClass = in_settings["windowClass"].string_value();
		string exec = in_settings["executable"].string_value();

		settings["window"] = ":" + winClass + ":" + exec;

		translate_bool("captureMouse", in_settings, "capture_cursor",
			       settings);
	}

	out["settings"] = settings;

	return out;
}

#undef translate_int
#undef translate_string
#undef translate_double
#undef translate_bool

static void translate_sc(const Json &in, Json &out)
{
	Json::object res = Json::object{};

	Json::array out_sources = Json::array{};
	Json::array global = in["globals"].array_items();

	if (!in["globals"].is_null()) {
		for (size_t i = 0; i < global.size(); i++) {
			Json source = global[i];

			Json out_source = translate_source(source, out_sources);
			out_sources.push_back(out_source);
		}
	}

	Json::array scenes = in["scenes"].array_items();
	string first_name = "";

	for (size_t i = 0; i < scenes.size(); i++) {
		Json in_scene = scenes[i];

		if (first_name.empty())
			first_name = in_scene["name"].string_value();

		Json::array items = Json::array{};

		Json::array sources = in_scene["sources"].array_items();

		for (size_t x = sources.size(); x > 0; x--) {
			Json source = sources[x - 1];

			Json::object out_source =
				translate_source(source, out_sources);
			Json::object out_item =
				translate_scene_item(source, out_source);

			out_item["id"] = (int)x - 1;

			items.push_back(out_item);

			if (out_source.find("preexist") == out_source.end())
				out_sources.push_back(out_source);
		}

		out_sources.push_back(Json::object{
			{"id", "scene"},
			{"name", in_scene["name"]},
			{"settings",
			 Json::object{{"items", items},
				      {"id_counter", (int)items.size()}}}});
	}

	res["current_scene"] = first_name;
	res["current_program_scene"] = first_name;
	res["sources"] = out_sources;
	res["name"] = in["name"];

	out = res;
}

static void create_string(const string &name, Json::object &out,
			  const string &data)
{
	string str = StringReplace(data, "\\\\", "/");
	out[name] = str;
}

static void create_string_obj(const string &data, Json::array &arr)
{
	Json::object obj = Json::object{};
	create_string("value", obj, data);
	arr.push_back(obj);
}

static void create_double(const string &name, Json::object &out,
			  const string &data)
{
	double d = atof(data.c_str());
	out[name] = d;
}

static void create_int(const string &name, Json::object &out,
		       const string &data)
{
	int i = atoi(data.c_str());
	out[name] = i;
}

static void create_data_item(Json::object &out, const string &line)
{
	size_t end_pos = line.find(':') - 1;

	if (end_pos == string::npos)
		return;

	size_t start_pos = 0;
	while (line[start_pos] == ' ')
		start_pos++;

	string name = line.substr(start_pos, end_pos - start_pos);
	const char *c_name = name.c_str();

	string first = line.substr(end_pos + 3);

	if ((first[0] >= 'A' && first[0] <= 'Z') ||
	    (first[0] >= 'a' && first[0] <= 'z') || first[0] == '\\' ||
	    first[0] == '/') {
		if (out.find(c_name) != out.end()) {
			Json::array arr = out[c_name].array_items();
			if (out[c_name].is_string()) {
				Json::array new_arr = Json::array{};
				string str = out[c_name].string_value();
				create_string_obj(str, new_arr);
				arr = std::move(new_arr);
			}

			create_string_obj(first, arr);
			out[c_name] = arr;
		} else {
			create_string(c_name, out, first);
		}
	} else if (first[0] == '"') {
		string str = first.substr(1, first.size() - 2);

		if (out.find(c_name) != out.end()) {
			Json::array arr = out[c_name].array_items();
			if (out[c_name].is_string()) {
				Json::array new_arr = Json::array{};
				string str1 = out[c_name].string_value();
				create_string_obj(str1, new_arr);
				arr = std::move(new_arr);
			}

			create_string_obj(str, arr);
			out[c_name] = arr;
		} else {
			create_string(c_name, out, str);
		}
	} else if (first.find('.') != string::npos) {
		create_double(c_name, out, first);
	} else {
		create_int(c_name, out, first);
	}
}

static Json::object create_object(Json::object &out, string &line, string &src);

static Json::array create_sources(Json::object &out, string &line, string &src)
{
	Json::array res = Json::array{};

	line = ReadLine(src);
	size_t l_len = line.size();
	while (!line.empty() && line[l_len - 1] != '}') {
		size_t end_pos = line.find(':');

		if (end_pos == string::npos)
			return Json::array{};

		size_t start_pos = 0;
		while (line[start_pos] == ' ')
			start_pos++;

		string name = line.substr(start_pos, end_pos - start_pos - 1);

		Json::object nul = Json::object();

		Json::object source = create_object(nul, line, src);
		source["name"] = name;
		res.push_back(source);

		line = ReadLine(src);
		l_len = line.size();
	}

	if (!out.empty())
		out["sources"] = res;

	return res;
}

static Json::object create_object(Json::object &out, string &line, string &src)
{
	size_t end_pos = line.find(':');

	if (end_pos == string::npos)
		return Json::object{};

	size_t start_pos = 0;
	while (line[start_pos] == ' ')
		start_pos++;

	string name = line.substr(start_pos, end_pos - start_pos - 1);

	Json::object res = Json::object{};

	line = ReadLine(src);

	size_t l_len = line.size() - 1;

	while (!line.empty() && line[l_len] != '}') {
		start_pos = 0;
		while (line[start_pos] == ' ')
			start_pos++;

		if (line.substr(start_pos, 7) == "sources")
			create_sources(res, line, src);
		else if (line[l_len] == '{')
			create_object(res, line, src);
		else
			create_data_item(res, line);

		line = ReadLine(src);
		l_len = line.size() - 1;
	}

	if (!out.empty())
		out[name] = res;

	return res;
}

string ClassicImporter::Name(const string &path)
{
	return GetFilenameFromPath(path);
}

int ClassicImporter::ImportScenes(const string &path, string &name, Json &res)
{
	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());
	if (!file_data)
		return IMPORTER_FILE_WONT_OPEN;

	if (name.empty())
		name = GetFilenameFromPath(path);

	Json::object data = Json::object{};
	data["name"] = name;

	string file = file_data.Get();
	string line = ReadLine(file);

	while (!line.empty() && line[0] != '\0') {
		string key = line != "global sources : {" ? "scenes"
							  : "globals";

		Json::array arr = create_sources(data, line, file);
		data[key] = arr;

		line = ReadLine(file);
	}

	Json sc = data;
	translate_sc(sc, res);

	return IMPORTER_SUCCESS;
}

bool ClassicImporter::Check(const string &path)
{
	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());

	if (!file_data)
		return false;

	bool check = false;

	if (strncmp(file_data, "scenes : {\r\n", 12) == 0)
		check = true;

	return check;
}

OBSImporterFiles ClassicImporter::FindFiles()
{
	OBSImporterFiles res;

#ifdef _WIN32
	char dst[512];
	int found = os_get_config_path(dst, 512, "OBS\\sceneCollection\\");
	if (found == -1)
		return res;

	os_dir_t *dir = os_opendir(dst);
	struct os_dirent *ent;
	while ((ent = os_readdir(dir)) != NULL) {
		if (ent->directory || *ent->d_name == '.')
			continue;

		string name = ent->d_name;
		size_t pos = name.find(".xconfig");
		if (pos != -1 && pos == name.length() - 8) {
			string path = dst + name;
			res.push_back(path);
		}
	}

	os_closedir(dir);
#endif

	return res;
}
