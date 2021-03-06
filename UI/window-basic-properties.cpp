/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include "obs-app.hpp"
#include "window-basic-properties.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "display-helpers.hpp"
#include "properties-view.hpp"

#include <QCloseEvent>
#include <QScreen>
#include <QWindow>
#include <QMessageBox>

using namespace std;

static void CreateTransitionScene(OBSSource scene, const char *text,
				  uint32_t color);

OBSBasicProperties::OBSBasicProperties(QWidget *parent, OBSSource source_)
	: QDialog(parent),
	  preview(new OBSQTDisplay(this)),
	  main(qobject_cast<OBSBasic *>(parent)),
	  acceptClicked(false),
	  source(source_),
	  removedSignal(obs_source_get_signal_handler(source), "remove",
			OBSBasicProperties::SourceRemoved, this),
	  renamedSignal(obs_source_get_signal_handler(source), "rename",
			OBSBasicProperties::SourceRenamed, this),
	  oldSettings(obs_data_create()),
	  buttonBox(new QDialogButtonBox(this))
{
	int cx = (int)config_get_int(App()->GlobalConfig(), "PropertiesWindow",
				     "cx");
	int cy = (int)config_get_int(App()->GlobalConfig(), "PropertiesWindow",
				     "cy");

	enum obs_source_type type = obs_source_get_type(source);

	buttonBox->setObjectName(QStringLiteral("buttonBox"));
	buttonBox->setStandardButtons(QDialogButtonBox::Ok |
				      QDialogButtonBox::Cancel |
				      QDialogButtonBox::RestoreDefaults);

	buttonBox->button(QDialogButtonBox::Ok)->setText(QTStr("OK"));
	buttonBox->button(QDialogButtonBox::Cancel)->setText(QTStr("Cancel"));
	buttonBox->button(QDialogButtonBox::RestoreDefaults)
		->setText(QTStr("Defaults"));

	if (cx > 400 && cy > 400)
		resize(cx, cy);
	else
		resize(720, 580);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	QMetaObject::connectSlotsByName(this);

	/* The OBSData constructor increments the reference once */
	obs_data_release(oldSettings);

	OBSData settings = obs_source_get_settings(source);
	obs_data_apply(oldSettings, settings);
	obs_data_release(settings);

	view = new OBSPropertiesView(
		settings, source,
		(PropertiesReloadCallback)obs_source_properties,
		(PropertiesUpdateCallback)obs_source_update);
	view->setMinimumHeight(150);

	preview->setMinimumSize(20, 150);
	preview->setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	// Create a QSplitter to keep a unified workflow here.
	windowSplitter = new QSplitter(Qt::Orientation::Vertical, this);
	windowSplitter->addWidget(preview);
	windowSplitter->addWidget(view);
	windowSplitter->setChildrenCollapsible(false);
	//windowSplitter->setSizes(QList<int>({ 16777216, 150 }));
	windowSplitter->setStretchFactor(0, 3);
	windowSplitter->setStretchFactor(1, 1);

	setLayout(new QVBoxLayout(this));
	layout()->addWidget(windowSplitter);

	if (type == OBS_SOURCE_TYPE_TRANSITION) {
		AddPreviewButton();
		connect(view, SIGNAL(PropertiesRefreshed()), this,
			SLOT(AddPreviewButton()));
	}

	layout()->addWidget(buttonBox);
	layout()->setAlignment(buttonBox, Qt::AlignBottom);

	view->show();
	installEventFilter(CreateShortcutFilter());

	const char *name = obs_source_get_name(source);
	setWindowTitle(QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name)));

	obs_source_inc_showing(source);

	updatePropertiesSignal.Connect(obs_source_get_signal_handler(source),
				       "update_properties",
				       OBSBasicProperties::UpdateProperties,
				       this);

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(preview->GetDisplay(),
					      OBSBasicProperties::DrawPreview,
					      this);
	};
	auto addTransitionDrawCallback = [this]() {
		obs_display_add_draw_callback(
			preview->GetDisplay(),
			OBSBasicProperties::DrawTransitionPreview, this);
	};
	uint32_t caps = obs_source_get_output_flags(source);
	bool drawable_type = type == OBS_SOURCE_TYPE_INPUT ||
			     type == OBS_SOURCE_TYPE_SCENE;
	bool drawable_preview = (caps & OBS_SOURCE_VIDEO) != 0;

	if (drawable_preview && drawable_type) {
		preview->show();
		connect(preview.data(), &OBSQTDisplay::DisplayCreated,
			addDrawCallback);

	} else if (type == OBS_SOURCE_TYPE_TRANSITION) {
		sourceA =
			obs_source_create_private("scene", "sourceA", nullptr);
		sourceB =
			obs_source_create_private("scene", "sourceB", nullptr);

		obs_source_release(sourceA);
		obs_source_release(sourceB);

		uint32_t colorA = 0xFFB26F52;
		uint32_t colorB = 0xFF6FB252;

		CreateTransitionScene(sourceA, "A", colorA);
		CreateTransitionScene(sourceB, "B", colorB);

		/**
		 * The cloned source is made from scratch, rather than using
		 * obs_source_duplicate, as the stinger transition would not
		 * play correctly otherwise.
		 */

		obs_data_t *settings = obs_source_get_settings(source);

		sourceClone = obs_source_create_private(
			obs_source_get_id(source), "clone", settings);
		obs_source_release(sourceClone);

		obs_source_inc_active(sourceClone);
		obs_transition_set(sourceClone, sourceA);

		obs_data_release(settings);

		auto updateCallback = [=]() {
			obs_data_t *settings = obs_source_get_settings(source);
			obs_source_update(sourceClone, settings);

			obs_transition_clear(sourceClone);
			obs_transition_set(sourceClone, sourceA);
			obs_transition_force_stop(sourceClone);

			obs_data_release(settings);

			direction = true;
		};

		connect(view, &OBSPropertiesView::Changed, updateCallback);

		preview->show();
		connect(preview.data(), &OBSQTDisplay::DisplayCreated,
			addTransitionDrawCallback);

	} else {
		preview->hide();
	}
}

OBSBasicProperties::~OBSBasicProperties()
{
	if (sourceClone) {
		obs_source_dec_active(sourceClone);
	}
	obs_source_dec_showing(source);
	main->SaveProject();
	main->UpdateContextBar();
}

void OBSBasicProperties::AddPreviewButton()
{
	QPushButton *playButton =
		new QPushButton(QTStr("PreviewTransition"), this);
	VScrollArea *area = view;
	area->widget()->layout()->addWidget(playButton);

	playButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	auto play = [=]() {
		OBSSource start;
		OBSSource end;

		if (direction) {
			start = sourceA;
			end = sourceB;
		} else {
			start = sourceB;
			end = sourceA;
		}

		obs_transition_set(sourceClone, start);
		obs_transition_start(sourceClone, OBS_TRANSITION_MODE_AUTO,
				     main->GetTransitionDuration(), end);
		direction = !direction;

		start = nullptr;
		end = nullptr;
	};

	connect(playButton, &QPushButton::clicked, play);
}

static obs_source_t *CreateLabel(const char *name, size_t h)
{
	obs_data_t *settings = obs_data_create();
	obs_data_t *font = obs_data_create();

	std::string text;
	text += " ";
	text += name;
	text += " ";

#if defined(_WIN32)
	obs_data_set_string(font, "face", "Arial");
#elif defined(__APPLE__)
	obs_data_set_string(font, "face", "Helvetica");
#else
	obs_data_set_string(font, "face", "Monospace");
#endif
	obs_data_set_int(font, "flags", 1); // Bold text
	obs_data_set_int(font, "size", min(int(h), 300));

	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);

#ifdef _WIN32
	const char *text_source_id = "text_gdiplus";
#else
	const char *text_source_id = "text_ft2_source";
#endif

	obs_source_t *txtSource =
		obs_source_create_private(text_source_id, name, settings);

	obs_data_release(font);
	obs_data_release(settings);

	return txtSource;
}

static void CreateTransitionScene(OBSSource scene, const char *text,
				  uint32_t color)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "width", obs_source_get_width(scene));
	obs_data_set_int(settings, "height", obs_source_get_height(scene));
	obs_data_set_int(settings, "color", color);

	obs_source_t *colorBG = obs_source_create_private(
		"color_source", "background", settings);

	obs_scene_add(obs_scene_from_source(scene), colorBG);

	obs_source_t *label = CreateLabel(text, obs_source_get_height(scene));
	obs_sceneitem_t *item =
		obs_scene_add(obs_scene_from_source(scene), label);

	vec2 size;
	vec2_set(&size, obs_source_get_width(scene),
#ifdef _WIN32
		 obs_source_get_height(scene));
#else
		 obs_source_get_height(scene) * 0.8);
#endif

	obs_sceneitem_set_bounds(item, &size);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);

	obs_data_release(settings);
	obs_source_release(colorBG);
	obs_source_release(label);
}

void OBSBasicProperties::SourceRemoved(void *data, calldata_t *params)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data),
				  "close");

	UNUSED_PARAMETER(params);
}

void OBSBasicProperties::SourceRenamed(void *data, calldata_t *params)
{
	const char *name = calldata_string(params, "new_name");
	QString title = QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name));

	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data),
				  "setWindowTitle", Q_ARG(QString, title));
}

void OBSBasicProperties::UpdateProperties(void *data, calldata_t *)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data)->view,
				  "ReloadProperties");
}

void OBSBasicProperties::on_buttonBox_clicked(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole val = buttonBox->buttonRole(button);

	if (val == QDialogButtonBox::AcceptRole) {
		acceptClicked = true;
		close();

		if (view->DeferUpdate())
			view->UpdateSettings();

	} else if (val == QDialogButtonBox::RejectRole) {
		obs_data_t *settings = obs_source_get_settings(source);
		obs_data_clear(settings);
		obs_data_release(settings);

		if (view->DeferUpdate())
			obs_data_apply(settings, oldSettings);
		else
			obs_source_update(source, oldSettings);

		close();

	} else if (val == QDialogButtonBox::ResetRole) {
		obs_data_t *settings = obs_source_get_settings(source);
		obs_data_clear(settings);
		obs_data_release(settings);

		if (!view->DeferUpdate())
			obs_source_update(source, nullptr);

		view->ReloadProperties();
	}
}

void OBSBasicProperties::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	OBSBasicProperties *window = static_cast<OBSBasicProperties *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->source), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void OBSBasicProperties::DrawTransitionPreview(void *data, uint32_t cx,
					       uint32_t cy)
{
	OBSBasicProperties *window = static_cast<OBSBasicProperties *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->source), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(window->sourceClone);

	gs_projection_pop();
	gs_viewport_pop();
}

void OBSBasicProperties::Cleanup()
{
	config_set_int(App()->GlobalConfig(), "PropertiesWindow", "cx",
		       width());
	config_set_int(App()->GlobalConfig(), "PropertiesWindow", "cy",
		       height());

	obs_display_remove_draw_callback(preview->GetDisplay(),
					 OBSBasicProperties::DrawPreview, this);
	obs_display_remove_draw_callback(
		preview->GetDisplay(),
		OBSBasicProperties::DrawTransitionPreview, this);
}

void OBSBasicProperties::reject()
{
	if (!acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			return;
		}
	}

	Cleanup();
	done(0);
}

void OBSBasicProperties::closeEvent(QCloseEvent *event)
{
	if (!acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			event->ignore();
			return;
		}
	}

	QDialog::closeEvent(event);
	if (!event->isAccepted())
		return;

	Cleanup();
}

void OBSBasicProperties::Init()
{
	show();
}

int OBSBasicProperties::CheckSettings()
{
	OBSData currentSettings = obs_source_get_settings(source);
	const char *oldSettingsJson = obs_data_get_json(oldSettings);
	const char *currentSettingsJson = obs_data_get_json(currentSettings);

	int ret = strcmp(currentSettingsJson, oldSettingsJson);

	obs_data_release(currentSettings);
	return ret;
}

bool OBSBasicProperties::ConfirmQuit()
{
	QMessageBox::StandardButton button;

	button = OBSMessageBox::question(
		this, QTStr("Basic.PropertiesWindow.ConfirmTitle"),
		QTStr("Basic.PropertiesWindow.Confirm"),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

	switch (button) {
	case QMessageBox::Save:
		acceptClicked = true;
		if (view->DeferUpdate())
			view->UpdateSettings();
		// Do nothing because the settings are already updated
		break;
	case QMessageBox::Discard:
		obs_source_update(source, oldSettings);
		break;
	case QMessageBox::Cancel:
		return false;
		break;
	default:
		/* If somehow the dialog fails to show, just default to
		 * saving the settings. */
		break;
	}
	return true;
}
