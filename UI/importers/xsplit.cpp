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
#include <ctype.h>

#include <QDomDocument>

using namespace std;
using namespace json11;

static int hex_string_to_int(string str)
{
	int res = 0;

	if (str[0] == '#')
		str = str.substr(1);

	for (size_t i = 0, l = str.size(); i < l; i++) {
		res *= 16;

		if (str[0] >= '0' && str[0] <= '9')
			res += str[0] - '0';
		else
			res += str[0] - 'A' + 10;

		str = str.substr(1);
	}

	return res;
}

static Json::object parse_text(QString &config)
{
	int start = config.indexOf("*{");
	config = config.mid(start + 1);
	config.replace("\\", "/");

	string err;
	Json data = Json::parse(config.toStdString(), err);

	if (err != "")
		return Json::object{};

	string outline = data["outline"].string_value();
	int out = 0;

	if (outline == "thick")
		out = 20;
	else if (outline == "thicker")
		out = 40;
	else if (outline == "thinner")
		out = 5;
	else if (outline == "thin")
		out = 10;

	string valign = data["vertAlign"].string_value();
	if (valign == "middle")
		valign = "center";

	Json font = Json::object{{"face", data["fontStyle"]}, {"size", 200}};

	return Json::object{
		{"text", data["text"]},
		{"font", font},
		{"outline", out > 0},
		{"outline_size", out},
		{"outline_color",
		 hex_string_to_int(data["outlineColor"].string_value())},
		{"color", hex_string_to_int(data["color"].string_value())},
		{"align", data["textAlign"]},
		{"valign", valign},
		{"alpha", data["opacity"]}};
}

static Json::array parse_playlist(QString &playlist)
{
	Json::array out = Json::array{};

	while (true) {
		int end = playlist.indexOf('*');
		QString path = playlist.left(end);

		out.push_back(Json::object{{"value", path.toStdString()}});

		int next = playlist.indexOf('|');
		if (next == -1)
			break;

		playlist = playlist.mid(next + 1);
	}

	return out;
}

static void parse_media_types(QDomNamedNodeMap &attr, Json::object &source,
			      Json::object &settings)
{
	QString playlist = attr.namedItem("FilePlaylist").nodeValue();

	if (playlist != "") {
		source["id"] = "vlc_source";
		settings["playlist"] = parse_playlist(playlist);

		QString end_op = attr.namedItem("OpWhenFinished").nodeValue();
		if (end_op == "2")
			settings["loop"] = true;
	} else {
		QString url = attr.namedItem("item").nodeValue();
		int sep = url.indexOf("://");

		if (sep != -1) {
			QString prot = url.left(sep);
			if (prot == "smlndi") {
				source["id"] = "ndi_source";
			} else {
				source["id"] = "ffmpeg_source";
				int info = url.indexOf("\\");
				QString input;

				if (info != -1) {
					input = url.left(info);
				} else {
					input = url;
				}

				settings["input"] = input.toStdString();
				settings["is_local_file"] = false;
			}
		} else {
			source["id"] = "ffmpeg_source";
			settings["local_file"] =
				url.replace("\\", "/").toStdString();
			settings["is_local_file"] = true;
		}
	}
}

static Json::object parse_slideshow(QString &config)
{
	int start = config.indexOf("images\":[");
	if (start == -1)
		return Json::object{};

	config = config.mid(start + 8);
	config.replace("\\\\", "/");

	int end = config.indexOf(']');
	if (end == -1)
		return Json::object{};

	string arr = config.left(end + 1).toStdString();
	string err;
	Json::array files = Json::parse(arr, err).array_items();

	if (err != "")
		return Json::object{};

	Json::array files_out = Json::array{};

	for (size_t i = 0; i < files.size(); i++) {
		string file = files[i].string_value();
		files_out.push_back(Json::object{{"value", file}});
	}

	QString options = config.mid(end + 1);
	options[0] = '{';

	Json opt = Json::parse(options.toStdString(), err);

	if (err != "")
		return Json::object{};

	return Json::object{{"randomize", opt["random"]},
			    {"slide_time",
			     opt["delay"].number_value() * 1000 + 700},
			    {"files", files_out}};
}

static bool source_name_exists(const string &name, const Json::array &sources)
{
	for (size_t i = 0; i < sources.size(); i++) {
		if (sources.at(i)["name"].string_value() == name)
			return true;
	}

	return false;
}

static Json get_source_with_id(const string &src_id, const Json::array &sources)
{
	for (size_t i = 0; i < sources.size(); i++) {
		if (sources.at(i)["src_id"].string_value() == src_id)
			return sources.at(i);
	}

	return nullptr;
}

static void parse_items(QDomNode &item, Json::array &items,
			Json::array &sources)
{
	while (!item.isNull()) {
		QDomNamedNodeMap attr = item.attributes();
		QString srcid = attr.namedItem("srcid").nodeValue();
		double vol = attr.namedItem("volume").nodeValue().toDouble();
		int type = attr.namedItem("type").nodeValue().toInt();

		string name;
		Json::object settings;
		Json::object source;
		string temp_name;
		int x = 0;

		Json exists = get_source_with_id(srcid.toStdString(), sources);
		if (!exists.is_null()) {
			name = exists["name"].string_value();
			goto skip;
		}

		name = attr.namedItem("cname").nodeValue().toStdString();
		if (name.empty() || name[0] == '\0')
			name = attr.namedItem("name").nodeValue().toStdString();

		temp_name = name;
		while (source_name_exists(temp_name, sources)) {
			string new_name = name + " " + to_string(x++);
			temp_name = new_name;
		}

		name = temp_name;

		settings = Json::object{};
		source = Json::object{{"name", name},
				      {"src_id", srcid.toStdString()},
				      {"volume", vol}};

		/** type=1     means Media of some kind (Video Playlist, RTSP,
		               RTMP, NDI or Media File).
		    type=2     means either a DShow or WASAPI source.
		    type=4     means an Image source.
		    type=5     means either a Display or Window Capture.
		    type=7     means a Game Capture.
		    type=8     means rendered with a browser, which includes:
		                   Web Page, Image Slideshow, Text.
		    type=11    means another Scene. **/

		if (type == 1) {
			parse_media_types(attr, source, settings);
		} else if (type == 2) {
			QString audio = attr.namedItem("itemaudio").nodeValue();

			if (audio.isEmpty()) {
				source["id"] = "dshow_input";
			} else {
				source["id"] = "wasapi_input_capture";
				int dev = audio.indexOf("\\wave:") + 6;

				QString res =
					"{0.0.1.00000000}." + audio.mid(dev);
				res = res.toLower();

				settings["device_id"] = res.toStdString();
			}
		} else if (type == 4) {
			source["id"] = "image_source";

			QString path = attr.namedItem("item").nodeValue();
			path.replace("\\", "/");
			settings["file"] = path.toStdString();
		} else if (type == 5) {
			QString opt = attr.namedItem("item").nodeValue();

			QDomDocument options;
			options.setContent(opt);

			QDomNode el = options.documentElement();

			QDomNamedNodeMap o_attr = el.attributes();
			QString display =
				o_attr.namedItem("desktop").nodeValue();

			if (!display.isEmpty()) {
				source["id"] = "monitor_capture";
				int cursor = attr.namedItem("ScrCapShowMouse")
						     .nodeValue()
						     .toInt();
				settings["capture_cursor"] = cursor == 1;
			} else {
				source["id"] = "window_capture";

				QString exec =
					o_attr.namedItem("module").nodeValue();
				QString window =
					o_attr.namedItem("window").nodeValue();
				QString _class =
					o_attr.namedItem("class").nodeValue();

				int pos = exec.lastIndexOf('\\');

				if (_class.isEmpty()) {
					_class = "class";
				}

				QString res = window + ":" + _class + ":" +
					      exec.mid(pos + 1);

				settings["window"] = res.toStdString();
				settings["priority"] = 2;
			}
		} else if (type == 7) {
			QString opt = attr.namedItem("item").nodeValue();
			opt.replace("&lt;", "<");
			opt.replace("&gt;", ">");
			opt.replace("&quot;", "\"");

			QDomDocument doc;
			doc.setContent(opt);

			QDomNode el = doc.documentElement();
			QDomNamedNodeMap o_attr = el.attributes();

			QString name = o_attr.namedItem("wndname").nodeValue();
			QString exec =
				o_attr.namedItem("imagename").nodeValue();

			QString res = name = "::" + exec;

			source["id"] = "game_capture";
			settings["window"] = res.toStdString();
			settings["capture_mode"] = "window";
		} else if (type == 8) {
			QString plugin = attr.namedItem("item").nodeValue();

			if (plugin.startsWith(
				    "html:plugin:imageslideshowplg*")) {
				source["id"] = "slideshow";
				settings = parse_slideshow(plugin);
			} else if (plugin.startsWith("html:plugin:titleplg")) {
				source["id"] = "text_gdiplus";
				settings = parse_text(plugin);
			} else if (plugin.startsWith("http")) {
				source["id"] = "browser_source";
				int end = plugin.indexOf('*');
				settings["url"] =
					plugin.left(end).toStdString();
			}
		} else if (type == 11) {
			QString id = attr.namedItem("item").nodeValue();
			Json source =
				get_source_with_id(id.toStdString(), sources);
			name = source["name"].string_value();

			goto skip;
		}

		source["settings"] = settings;
		sources.push_back(source);

	skip:
		struct obs_video_info ovi;
		obs_get_video_info(&ovi);

		int width = ovi.base_width;
		int height = ovi.base_height;

		double pos_left =
			attr.namedItem("pos_left").nodeValue().toDouble();
		double pos_right =
			attr.namedItem("pos_right").nodeValue().toDouble();
		double pos_top =
			attr.namedItem("pos_top").nodeValue().toDouble();
		double pos_bottom =
			attr.namedItem("pos_bottom").nodeValue().toDouble();

		bool visible = attr.namedItem("visible").nodeValue() == "1";

		Json out_item = Json::object{
			{"bounds_type", 2},
			{"pos", Json::object{{"x", pos_left * width},
					     {"y", pos_top * height}}},
			{"bounds",
			 Json::object{{"x", (pos_right - pos_left) * width},
				      {"y", (pos_bottom - pos_top) * height}}},
			{"name", name},
			{"visible", visible}};

		items.push_back(out_item);

		item = item.nextSibling();
	}
}

static Json::object parse_scenes(QDomElement &scenes)
{
	Json::array sources = Json::array{};

	QString first = "";

	QDomNode in_scene = scenes.firstChild();
	while (!in_scene.isNull()) {
		QString type = in_scene.nodeName();

		if (type == "placement") {
			QDomNamedNodeMap attr = in_scene.attributes();

			QString name = attr.namedItem("name").nodeValue();
			QString id = attr.namedItem("id").nodeValue();

			if (first.isEmpty())
				first = name;

			Json out = Json::object{
				{"id", "scene"},
				{"name", name.toStdString().c_str()},
				{"src_id", id.toStdString().c_str()}};

			sources.push_back(out);
		}
		in_scene = in_scene.nextSibling();
	}

	in_scene = scenes.firstChild();
	for (size_t i = 0, l = sources.size(); i < l; i++) {
		Json::object source = sources[i].object_items();
		Json::array items = Json::array{};
		QDomNode firstChild = in_scene.firstChild();

		parse_items(firstChild, items, sources);

		Json settings = Json::object{{"items", items},
					     {"id_counter", (int)items.size()}};

		source["settings"] = settings;
		sources[i] = source;

		in_scene = in_scene.nextSibling();
	}

	return Json::object{{"sources", sources},
			    {"current_scene", first.toStdString()},
			    {"current_program_scene", first.toStdString()}};
}

int XSplitImporter::ImportScenes(const string &path, string &name,
				 json11::Json &res)
{
	if (name == "")
		name = "XSplit Import";

	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());

	if (!file_data)
		return IMPORTER_FILE_WONT_OPEN;

	QDomDocument doc;
	doc.setContent(QString(file_data));

	QDomElement docElem = doc.documentElement();

	Json::object r = parse_scenes(docElem);
	r["name"] = name;

	res = r;

	return IMPORTER_SUCCESS;
}

bool XSplitImporter::Check(const string &path)
{
	bool check = false;

	BPtr<char> file_data = os_quick_read_utf8_file(path.c_str());

	if (!file_data)
		return false;

	string pos = file_data.Get();

	string line = ReadLine(pos);
	while (!line.empty()) {
		if (line.substr(0, 5) == "<?xml") {
			line = ReadLine(pos);
		} else {
			if (line.substr(0, 14) == "<configuration") {
				check = true;
			}
			break;
		}
	}

	return check;
}

OBSImporterFiles XSplitImporter::FindFiles()
{
	OBSImporterFiles res;
#ifdef _WIN32
	char dst[512];
	int found = os_get_program_data_path(
		dst, 512, "SplitMediaLabs\\XSplit\\Presentation2.0\\");

	if (found == -1)
		return res;

	os_dir_t *dir = os_opendir(dst);
	struct os_dirent *ent;

	while ((ent = os_readdir(dir)) != NULL) {
		string name = ent->d_name;

		if (ent->directory || name[0] == '.')
			continue;

		if (name == "Placements.bpres") {
			string str = dst + name;
			res.push_back(str);

			break;
		}
	}
	os_closedir(dir);
#endif
	return res;
}
