#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QUrlQuery>
#include <string>

#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"

using namespace std;

static const char *textExtensions[] = {"txt", "log", nullptr};

static const char *imageExtensions[] = {"bmp",  "tga", "png",  "jpg",
					"jpeg", "gif", nullptr};

static const char *htmlExtensions[] = {"htm", "html", nullptr};

static const char *mediaExtensions[] = {
	"3ga",   "669",   "a52",   "aac",  "ac3",  "adt",  "adts", "aif",
	"aifc",  "aiff",  "amb",   "amr",  "aob",  "ape",  "au",   "awb",
	"caf",   "dts",   "flac",  "it",   "kar",  "m4a",  "m4b",  "m4p",
	"m5p",   "mid",   "mka",   "mlp",  "mod",  "mpa",  "mp1",  "mp2",
	"mp3",   "mpc",   "mpga",  "mus",  "oga",  "ogg",  "oma",  "opus",
	"qcp",   "ra",    "rmi",   "s3m",  "sid",  "spx",  "tak",  "thd",
	"tta",   "voc",   "vqf",   "w64",  "wav",  "wma",  "wv",   "xa",
	"xm",    "3g2",   "3gp",   "3gp2", "3gpp", "amv",  "asf",  "avi",
	"bik",   "crf",   "divx",  "drc",  "dv",   "evo",  "f4v",  "flv",
	"gvi",   "gxf",   "iso",   "m1v",  "m2v",  "m2t",  "m2ts", "m4v",
	"mkv",   "mov",   "mp2",   "mp2v", "mp4",  "mp4v", "mpe",  "mpeg",
	"mpeg1", "mpeg2", "mpeg4", "mpg",  "mpv2", "mts",  "mtv",  "mxf",
	"mxg",   "nsv",   "nuv",   "ogg",  "ogm",  "ogv",  "ogx",  "ps",
	"rec",   "rm",    "rmvb",  "rpl",  "thp",  "tod",  "ts",   "tts",
	"txd",   "vob",   "vro",   "webm", "wm",   "wmv",  "wtv",  nullptr};

static string GenerateSourceName(const char *base)
{
	string name;
	int inc = 0;

	for (;; inc++) {
		name = base;

		if (inc) {
			name += " (";
			name += to_string(inc + 1);
			name += ")";
		}

		obs_source_t *source = obs_get_source_by_name(name.c_str());

		if (!source)
			return name;
		else
			obs_source_release(source);
	}
}

void OBSBasic::AddDropURL(const char *url, QString &name, obs_data_t *settings,
			  const obs_video_info &ovi)
{
	QUrl path = QString::fromUtf8(url);
	QUrlQuery query = QUrlQuery(path.query(QUrl::FullyEncoded));

	int cx = (int)ovi.base_width;
	int cy = (int)ovi.base_height;

	if (query.hasQueryItem("layer-width"))
		cx = query.queryItemValue("layer-width").toInt();
	if (query.hasQueryItem("layer-height"))
		cy = query.queryItemValue("layer-height").toInt();

	obs_data_set_int(settings, "width", cx);
	obs_data_set_int(settings, "height", cy);

	name = query.hasQueryItem("layer-name")
		       ? query.queryItemValue("layer-name", QUrl::FullyDecoded)
		       : path.host();

	query.removeQueryItem("layer-width");
	query.removeQueryItem("layer-height");
	query.removeQueryItem("layer-name");
	path.setQuery(query);

	obs_data_set_string(settings, "url", QT_TO_UTF8(path.url()));
}

void OBSBasic::AddDropSource(const char *data, DropType image)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	obs_data_t *settings = obs_data_create();
	obs_source_t *source = nullptr;
	const char *type = nullptr;
	std::vector<const char *> types;
	QString name;

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	switch (image) {
	case DropType_RawText:
		obs_data_set_string(settings, "text", data);
#ifdef _WIN32
		type = "text_gdiplus";
#else
		type = "text_ft2_source";
#endif
		break;
	case DropType_Text:
#ifdef _WIN32
		obs_data_set_bool(settings, "read_from_file", true);
		obs_data_set_string(settings, "file", data);
		name = QUrl::fromLocalFile(QString(data)).fileName();
		type = "text_gdiplus";
#else
		obs_data_set_bool(settings, "from_file", true);
		obs_data_set_string(settings, "text_file", data);
		type = "text_ft2_source";
#endif
		break;
	case DropType_Image:
		obs_data_set_string(settings, "file", data);
		name = QUrl::fromLocalFile(QString(data)).fileName();
		type = "image_source";
		break;
	case DropType_Media:
		obs_data_set_string(settings, "local_file", data);
		name = QUrl::fromLocalFile(QString(data)).fileName();
		type = "ffmpeg_source";
		break;
	case DropType_Html:
		obs_data_set_bool(settings, "is_local_file", true);
		obs_data_set_string(settings, "local_file", data);
		obs_data_set_int(settings, "width", ovi.base_width);
		obs_data_set_int(settings, "height", ovi.base_height);
		name = QUrl::fromLocalFile(QString(data)).fileName();
		types = {"browser_source", "linuxbrowser-source"};
		break;
	case DropType_Url:
		AddDropURL(data, name, settings, ovi);
		types = {"browser_source", "linuxbrowser-source"};
		break;
	}

	for (char const *t : types) {
		if (obs_source_get_display_name(t)) {
			type = t;
			break;
		}
	}
	if (type == nullptr || !obs_source_get_display_name(type)) {
		obs_data_release(settings);
		return;
	}

	if (name.isEmpty())
		name = obs_source_get_display_name(type);
	source = obs_source_create(type,
				   GenerateSourceName(QT_TO_UTF8(name)).c_str(),
				   settings, nullptr);
	if (source) {
		OBSScene scene = main->GetCurrentScene();
		obs_scene_add(scene, source);
		obs_source_release(source);
	}

	obs_data_release(settings);
}

void OBSBasic::dragEnterEvent(QDragEnterEvent *event)
{
	// refuse drops of our own widgets
	if (event->source() != nullptr) {
		event->setDropAction(Qt::IgnoreAction);
		return;
	}

	event->acceptProposedAction();
}

void OBSBasic::dragLeaveEvent(QDragLeaveEvent *event)
{
	event->accept();
}

void OBSBasic::dragMoveEvent(QDragMoveEvent *event)
{
	event->acceptProposedAction();
}

void OBSBasic::ConfirmDropUrl(const QString &url)
{
	if (url.left(7).compare("http://", Qt::CaseInsensitive) == 0 ||
	    url.left(8).compare("https://", Qt::CaseInsensitive) == 0) {

		activateWindow();

		QString msg = QTStr("AddUrl.Text");
		msg += "\n\n";
		msg += QTStr("AddUrl.Text.Url").arg(url);

		QMessageBox messageBox(this);
		messageBox.setWindowTitle(QTStr("AddUrl.Title"));
		messageBox.setText(msg);

		QPushButton *yesButton = messageBox.addButton(
			QTStr("Yes"), QMessageBox::YesRole);
		QPushButton *noButton =
			messageBox.addButton(QTStr("No"), QMessageBox::NoRole);
		messageBox.setDefaultButton(yesButton);
		messageBox.setEscapeButton(noButton);
		messageBox.setIcon(QMessageBox::Question);
		messageBox.exec();

		if (messageBox.clickedButton() == yesButton)
			AddDropSource(QT_TO_UTF8(url), DropType_Url);
	}
}

void OBSBasic::dropEvent(QDropEvent *event)
{
	const QMimeData *mimeData = event->mimeData();

	if (mimeData->hasUrls()) {
		QList<QUrl> urls = mimeData->urls();

		for (int i = 0; i < urls.size(); i++) {
			QUrl url = urls[i];
			QString file = url.toLocalFile();
			QFileInfo fileInfo(file);

			if (!fileInfo.exists()) {
				ConfirmDropUrl(url.url());
				continue;
			}

			QString suffixQStr = fileInfo.suffix();
			QByteArray suffixArray = suffixQStr.toUtf8();
			const char *suffix = suffixArray.constData();
			bool found = false;

			const char **cmp;

#define CHECK_SUFFIX(extensions, type)                         \
	cmp = extensions;                                      \
	while (*cmp) {                                         \
		if (strcmp(*cmp, suffix) == 0) {               \
			AddDropSource(QT_TO_UTF8(file), type); \
			found = true;                          \
			break;                                 \
		}                                              \
                                                               \
		cmp++;                                         \
	}                                                      \
                                                               \
	if (found)                                             \
		continue;

			CHECK_SUFFIX(textExtensions, DropType_Text);
			CHECK_SUFFIX(htmlExtensions, DropType_Html);
			CHECK_SUFFIX(imageExtensions, DropType_Image);
			CHECK_SUFFIX(mediaExtensions, DropType_Media);

#undef CHECK_SUFFIX
		}
	} else if (mimeData->hasText()) {
		AddDropSource(QT_TO_UTF8(mimeData->text()), DropType_RawText);
	}
}
