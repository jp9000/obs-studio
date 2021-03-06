#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <obs.hpp>
#include "decklink-captions.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("decklink-captons", "en-US")

struct obs_captions {
	std::string source_name;
	OBSWeakSource source;

	void start();
	void stop();

	obs_captions();
	inline ~obs_captions() { stop(); }
};

obs_captions::obs_captions() {}

static obs_captions *captions = nullptr;

DecklinkCaptionsUI::DecklinkCaptionsUI(QWidget *parent)
	: QDialog(parent), ui(new Ui_CaptionsDialog)
{
	ui->setupUi(this);

	setSizeGripEnabled(true);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	auto cb = [this](obs_source_t *source) {
		uint32_t caps = obs_source_get_output_flags(source);
		QString name = obs_source_get_name(source);

		if (caps & OBS_SOURCE_CEA_708)
			ui->source->addItem(name);

		OBSWeakSource weak = OBSGetWeakRef(source);
		if (weak == captions->source)
			ui->source->setCurrentText(name);
		return true;
	};

	using cb_t = decltype(cb);

	ui->source->blockSignals(true);
	ui->source->addItem(QStringLiteral(""));
	ui->source->setCurrentIndex(0);
	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			return (*static_cast<cb_t *>(data))(source);
		},
		&cb);
	ui->source->blockSignals(false);
}

void DecklinkCaptionsUI::on_source_currentIndexChanged(int)
{
	captions->stop();

	captions->source_name = ui->source->currentText().toUtf8().constData();
	captions->source = GetWeakSourceByName(captions->source_name.c_str());

	captions->start();
}

static void caption_callback(void *param, obs_source_t *source,
			     const struct obs_source_cea_708 *captions)
{
	UNUSED_PARAMETER(param);
	UNUSED_PARAMETER(source);
	obs_output *output = obs_frontend_get_streaming_output();
	if (output) {
		if (obs_frontend_streaming_active() &&
		    obs_output_active(output)) {
			obs_output_caption(output, captions);
		}
		obs_output_release(output);
	}
}

void obs_captions::start()
{
	OBSSource s = OBSGetStrongRef(source);
	if (!s) {
		//warn("Source invalid");
		return;
	}
	obs_source_add_caption_callback(s, caption_callback, nullptr);
}

void obs_captions::stop()
{
	OBSSource s = OBSGetStrongRef(source);
	if (s)
		obs_source_remove_caption_callback(s, caption_callback,
						   nullptr);
}

static void save_decklink_caption_data(obs_data_t *save_data, bool saving,
				       void *)
{
	if (saving) {
		obs_data_t *obj = obs_data_create();

		obs_data_set_string(obj, "source",
				    captions->source_name.c_str());

		obs_data_set_obj(save_data, "decklink_captions", obj);
		obs_data_release(obj);
	} else {
		captions->stop();

		obs_data_t *obj =
			obs_data_get_obj(save_data, "decklink_captions");
		if (!obj)
			obj = obs_data_create();

		captions->source_name = obs_data_get_string(obj, "source");
		captions->source =
			GetWeakSourceByName(captions->source_name.c_str());
		obs_data_release(obj);

		captions->start();
	}
}

void addOutputUI(void)
{
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("Decklink Captions"));

	captions = new obs_captions;

	auto cb = []() {
		obs_frontend_push_ui_translation(obs_module_get_string);

		QWidget *window = (QWidget *)obs_frontend_get_main_window();

		DecklinkCaptionsUI dialog(window);
		dialog.exec();

		obs_frontend_pop_ui_translation();
	};

	obs_frontend_add_save_callback(save_decklink_caption_data, nullptr);

	action->connect(action, &QAction::triggered, cb);
}

bool obs_module_load(void)
{
	addOutputUI();

	return true;
}
