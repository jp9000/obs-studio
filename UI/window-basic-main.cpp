/******************************************************************************
    Copyright (C) 2013-2015 by Hugh Bailey <obs.jim@gmail.com>
                               Zachary Lund <admin@computerquip.com>
                               Philippe Groarke <philippe.groarke@gmail.com>

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

#include <cstddef>
#include <ctime>
#include <functional>
#include <obs-data.h>
#include <obs.h>
#include <obs.hpp>
#include <QGuiApplication>
#include <QMessageBox>
#include <QShowEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QScreen>
#include <QColorDialog>
#include <QSizePolicy>
#include <QScrollBar>
#include <QTextStream>

#include <util/dstr.h>
#include <util/util.hpp>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <util/dstr.hpp>

#include "obs-app.hpp"
#include "platform.hpp"
#include "visibility-item-widget.hpp"
#include "item-widget-helpers.hpp"
#include "window-basic-settings.hpp"
#include "window-namedialog.hpp"
#include "window-basic-auto-config.hpp"
#include "window-basic-source-select.hpp"
#include "window-basic-main.hpp"
#include "window-basic-stats.hpp"
#include "window-basic-main-outputs.hpp"
#include "window-log-reply.hpp"
#include "window-projector.hpp"
#include "window-remux.hpp"
#include "qt-wrappers.hpp"
#include "context-bar-controls.hpp"
#include "obs-proxy-style.hpp"
#include "display-helpers.hpp"
#include "volume-control.hpp"
#include "remote-text.hpp"
#include "ui-validation.hpp"
#include "media-controls.hpp"
#include "undo-stack-obs.hpp"
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include "win-update/win-update.hpp"
#include "windows.h"
#endif

#include "ui_OBSBasic.h"
#include "ui_ColorSelect.h"

#include <QWindow>

#include <json11.hpp>

#ifdef ENABLE_WAYLAND
#include <obs-nix-platform.h>
#endif

using namespace json11;
using namespace std;

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif

#include "ui-config.h"

struct QCef;
struct QCefCookieManager;

QCef *cef = nullptr;
QCefCookieManager *panel_cookies = nullptr;

void DestroyPanelCookieManager();

namespace {

template<typename OBSRef> struct SignalContainer {
	OBSRef ref;
	vector<shared_ptr<OBSSignal>> handlers;
};
}

extern volatile long insideEventLoop;

Q_DECLARE_METATYPE(OBSScene);
Q_DECLARE_METATYPE(OBSSceneItem);
Q_DECLARE_METATYPE(OBSSource);
Q_DECLARE_METATYPE(obs_order_movement);
Q_DECLARE_METATYPE(SignalContainer<OBSScene>);

QDataStream &operator<<(QDataStream &out, const SignalContainer<OBSScene> &v)
{
	out << v.ref;
	return out;
}

QDataStream &operator>>(QDataStream &in, SignalContainer<OBSScene> &v)
{
	in >> v.ref;
	return in;
}

template<typename T> static T GetOBSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::OBSRef)).value<T>();
}

template<typename T> static void SetOBSRef(QListWidgetItem *item, T &&val)
{
	item->setData(static_cast<int>(QtDataRole::OBSRef),
		      QVariant::fromValue(val));
}

static void AddExtraModulePaths()
{
	char *plugins_path = getenv("OBS_PLUGINS_PATH");
	char *plugins_data_path = getenv("OBS_PLUGINS_DATA_PATH");
	if (plugins_path && plugins_data_path) {
		string data_path_with_module_suffix;
		data_path_with_module_suffix += plugins_data_path;
		data_path_with_module_suffix += "/%module%";
		obs_add_module_path(plugins_path,
				    data_path_with_module_suffix.c_str());
	}

	char base_module_dir[512];
#if defined(_WIN32) || defined(__APPLE__)
	int ret = GetProgramDataPath(base_module_dir, sizeof(base_module_dir),
				     "obs-studio/plugins/%module%");
#else
	int ret = GetConfigPath(base_module_dir, sizeof(base_module_dir),
				"obs-studio/plugins/%module%");
#endif

	if (ret <= 0)
		return;

	string path = base_module_dir;
#if defined(__APPLE__)
	obs_add_module_path((path + "/bin").c_str(), (path + "/data").c_str());

	BPtr<char> config_bin =
		os_get_config_path_ptr("obs-studio/plugins/%module%/bin");
	BPtr<char> config_data =
		os_get_config_path_ptr("obs-studio/plugins/%module%/data");
	obs_add_module_path(config_bin, config_data);

#elif ARCH_BITS == 64
	obs_add_module_path((path + "/bin/64bit").c_str(),
			    (path + "/data").c_str());
#else
	obs_add_module_path((path + "/bin/32bit").c_str(),
			    (path + "/data").c_str());
#endif
}

extern obs_frontend_callbacks *InitializeAPIInterface(OBSBasic *main);

void assignDockToggle(QDockWidget *dock, QAction *action)
{
	auto handleWindowToggle = [action](bool vis) {
		action->blockSignals(true);
		action->setChecked(vis);
		action->blockSignals(false);
	};
	auto handleMenuToggle = [dock](bool check) {
		dock->blockSignals(true);
		dock->setVisible(check);
		dock->blockSignals(false);
	};

	dock->connect(dock->toggleViewAction(), &QAction::toggled,
		      handleWindowToggle);
	dock->connect(action, &QAction::toggled, handleMenuToggle);
}

extern void RegisterTwitchAuth();
extern void RegisterRestreamAuth();

OBSBasic::OBSBasic(QWidget *parent)
	: OBSMainWindow(parent), undo_s(ui), ui(new Ui::OBSBasic)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	qRegisterMetaTypeStreamOperators<SignalContainer<OBSScene>>(
		"SignalContainer<OBSScene>");
#endif

	setAttribute(Qt::WA_NativeWindow);

#if TWITCH_ENABLED
	RegisterTwitchAuth();
#endif
#if RESTREAM_ENABLED
	RegisterRestreamAuth();
#endif

	setAcceptDrops(true);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this,
		SLOT(on_customContextMenuRequested(const QPoint &)));

	api = InitializeAPIInterface(this);

	ui->setupUi(this);
	ui->previewDisabledWidget->setVisible(false);
	ui->contextContainer->setStyle(new OBSProxyStyle);

	/* XXX: Disable drag/drop on Linux until Qt issues are fixed */
#if !defined(_WIN32) && !defined(__APPLE__)
	ui->scenes->setDragDropMode(QAbstractItemView::NoDragDrop);
#endif

	startingDockLayout = saveState();

	statsDock = new OBSDock();
	statsDock->setObjectName(QStringLiteral("statsDock"));
	statsDock->setFeatures(QDockWidget::DockWidgetClosable |
			       QDockWidget::DockWidgetMovable |
			       QDockWidget::DockWidgetFloatable);
	statsDock->setWindowTitle(QTStr("Basic.Stats"));
	addDockWidget(Qt::BottomDockWidgetArea, statsDock);
	statsDock->setVisible(false);
	statsDock->setFloating(true);
	statsDock->resize(700, 200);

	copyActionsDynamicProperties();

	char styleSheetPath[512];
	int ret = GetProfilePath(styleSheetPath, sizeof(styleSheetPath),
				 "stylesheet.qss");
	if (ret > 0) {
		if (QFile::exists(styleSheetPath)) {
			QString path =
				QString("file:///") + QT_UTF8(styleSheetPath);
			App()->setStyleSheet(path);
		}
	}

	qRegisterMetaType<int64_t>("int64_t");
	qRegisterMetaType<uint32_t>("uint32_t");
	qRegisterMetaType<OBSScene>("OBSScene");
	qRegisterMetaType<OBSSceneItem>("OBSSceneItem");
	qRegisterMetaType<OBSSource>("OBSSource");
	qRegisterMetaType<obs_hotkey_id>("obs_hotkey_id");
	qRegisterMetaType<SavedProjectorInfo *>("SavedProjectorInfo *");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	qRegisterMetaTypeStreamOperators<std::vector<std::shared_ptr<OBSSignal>>>(
		"std::vector<std::shared_ptr<OBSSignal>>");
	qRegisterMetaTypeStreamOperators<OBSScene>("OBSScene");
	qRegisterMetaTypeStreamOperators<OBSSceneItem>("OBSSceneItem");
#endif

	ui->scenes->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->sources->setAttribute(Qt::WA_MacShowFocusRect, false);

	bool sceneGrid = config_get_bool(App()->GlobalConfig(), "BasicWindow",
					 "gridMode");
	ui->scenes->SetGridMode(sceneGrid);

	ui->scenes->setItemDelegate(new SceneRenameDelegate(ui->scenes));

	auto displayResize = [this]() {
		struct obs_video_info ovi;

		if (obs_get_video_info(&ovi))
			ResizePreview(ovi.base_width, ovi.base_height);
	};

	connect(windowHandle(), &QWindow::screenChanged, displayResize);
	connect(ui->preview, &OBSQTDisplay::DisplayResized, displayResize);

	delete shortcutFilter;
	shortcutFilter = CreateShortcutFilter();
	installEventFilter(shortcutFilter);

	stringstream name;
	name << "OBS " << App()->GetVersionString();
	blog(LOG_INFO, "%s", name.str().c_str());
	blog(LOG_INFO, "---------------------------------");

	UpdateTitleBar();

	connect(ui->scenes->itemDelegate(),
		SIGNAL(closeEditor(QWidget *,
				   QAbstractItemDelegate::EndEditHint)),
		this,
		SLOT(SceneNameEdited(QWidget *,
				     QAbstractItemDelegate::EndEditHint)));

	cpuUsageInfo = os_cpu_usage_info_start();
	cpuUsageTimer = new QTimer(this);
	connect(cpuUsageTimer.data(), SIGNAL(timeout()), ui->statusbar,
		SLOT(UpdateCPUUsage()));
	cpuUsageTimer->start(3000);

	diskFullTimer = new QTimer(this);
	connect(diskFullTimer, SIGNAL(timeout()), this,
		SLOT(CheckDiskSpaceRemaining()));

	renameScene = new QAction(ui->scenesDock);
	renameScene->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameScene, SIGNAL(triggered()), this, SLOT(EditSceneName()));
	ui->scenesDock->addAction(renameScene);

	renameSource = new QAction(ui->sourcesDock);
	renameSource->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameSource, SIGNAL(triggered()), this,
		SLOT(EditSceneItemName()));
	ui->sourcesDock->addAction(renameSource);

#ifdef __APPLE__
	renameScene->setShortcut({Qt::Key_Return});
	renameSource->setShortcut({Qt::Key_Return});

	ui->actionRemoveSource->setShortcuts({Qt::Key_Backspace});
	ui->actionRemoveScene->setShortcuts({Qt::Key_Backspace});

	ui->action_Settings->setMenuRole(QAction::PreferencesRole);
	ui->actionE_xit->setMenuRole(QAction::QuitRole);
#else
	renameScene->setShortcut({Qt::Key_F2});
	renameSource->setShortcut({Qt::Key_F2});
#endif

#ifdef __linux__
	ui->actionE_xit->setShortcut(Qt::CTRL + Qt::Key_Q);
#endif

	auto addNudge = [this](const QKeySequence &seq, const char *s) {
		QAction *nudge = new QAction(ui->preview);
		nudge->setShortcut(seq);
		nudge->setShortcutContext(Qt::WidgetShortcut);
		ui->preview->addAction(nudge);
		connect(nudge, SIGNAL(triggered()), this, s);
	};

	addNudge(Qt::Key_Up, SLOT(NudgeUp()));
	addNudge(Qt::Key_Down, SLOT(NudgeDown()));
	addNudge(Qt::Key_Left, SLOT(NudgeLeft()));
	addNudge(Qt::Key_Right, SLOT(NudgeRight()));

	assignDockToggle(ui->scenesDock, ui->toggleScenes);
	assignDockToggle(ui->sourcesDock, ui->toggleSources);
	assignDockToggle(ui->mixerDock, ui->toggleMixer);
	assignDockToggle(ui->transitionsDock, ui->toggleTransitions);
	assignDockToggle(ui->controlsDock, ui->toggleControls);
	assignDockToggle(statsDock, ui->toggleStats);

	// Register shortcuts for Undo/Redo
	ui->actionMainUndo->setShortcut(Qt::CTRL + Qt::Key_Z);
	QList<QKeySequence> shrt;
	shrt << QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Z)
	     << QKeySequence(Qt::CTRL + Qt::Key_Y);
	ui->actionMainRedo->setShortcuts(shrt);

	ui->actionMainUndo->setShortcutContext(Qt::ApplicationShortcut);
	ui->actionMainRedo->setShortcutContext(Qt::ApplicationShortcut);

	//hide all docking panes
	ui->toggleScenes->setChecked(false);
	ui->toggleSources->setChecked(false);
	ui->toggleMixer->setChecked(false);
	ui->toggleTransitions->setChecked(false);
	ui->toggleControls->setChecked(false);
	ui->toggleStats->setChecked(false);

	QPoint curPos;

	//restore parent window geometry
	const char *geometry = config_get_string(App()->GlobalConfig(),
						 "BasicWindow", "geometry");
	if (geometry != NULL) {
		QByteArray byteArray =
			QByteArray::fromBase64(QByteArray(geometry));
		restoreGeometry(byteArray);

		QRect windowGeometry = normalGeometry();
		if (!WindowPositionValid(windowGeometry)) {
			QRect rect =
				QGuiApplication::primaryScreen()->geometry();
			setGeometry(QStyle::alignedRect(Qt::LeftToRight,
							Qt::AlignCenter, size(),
							rect));
		}

		curPos = pos();
	} else {
		QRect desktopRect =
			QGuiApplication::primaryScreen()->geometry();
		QSize adjSize = desktopRect.size() / 2 - size() / 2;
		curPos = QPoint(adjSize.width(), adjSize.height());
	}

	QPoint curSize(width(), height());

	QPoint statsDockSize(statsDock->width(), statsDock->height());
	QPoint statsDockPos = curSize / 2 - statsDockSize / 2;
	QPoint newPos = curPos + statsDockPos;
	statsDock->move(newPos);

	ui->previewLabel->setProperty("themeID", "previewProgramLabels");
	ui->previewLabel->style()->polish(ui->previewLabel);

	bool labels = config_get_bool(GetGlobalConfig(), "BasicWindow",
				      "StudioModeLabels");

	if (!previewProgramMode)
		ui->previewLabel->setHidden(true);
	else
		ui->previewLabel->setHidden(!labels);

	ui->previewDisabledWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->previewDisabledWidget,
		SIGNAL(customContextMenuRequested(const QPoint &)), this,
		SLOT(PreviewDisabledMenu(const QPoint &)));
	connect(ui->enablePreviewButton, SIGNAL(clicked()), this,
		SLOT(TogglePreview()));

	connect(ui->scenes, SIGNAL(scenesReordered()), this,
		SLOT(ScenesReordered()));
}

static void SaveAudioDevice(const char *name, int channel, obs_data_t *parent,
			    vector<OBSSource> &audioSources)
{
	obs_source_t *source = obs_get_output_source(channel);
	if (!source)
		return;

	audioSources.push_back(source);

	obs_data_t *data = obs_save_source(source);

	obs_data_set_obj(parent, name, data);

	obs_data_release(data);
	obs_source_release(source);
}

static obs_data_t *GenerateSaveData(obs_data_array_t *sceneOrder,
				    obs_data_array_t *quickTransitionData,
				    int transitionDuration,
				    obs_data_array_t *transitions,
				    OBSScene &scene, OBSSource &curProgramScene,
				    obs_data_array_t *savedProjectorList)
{
	obs_data_t *saveData = obs_data_create();

	vector<OBSSource> audioSources;
	audioSources.reserve(6);

	SaveAudioDevice(DESKTOP_AUDIO_1, 1, saveData, audioSources);
	SaveAudioDevice(DESKTOP_AUDIO_2, 2, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_1, 3, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_2, 4, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_3, 5, saveData, audioSources);
	SaveAudioDevice(AUX_AUDIO_4, 6, saveData, audioSources);

	/* -------------------------------- */
	/* save non-group sources           */

	auto FilterAudioSources = [&](obs_source_t *source) {
		if (obs_source_is_group(source))
			return false;

		return find(begin(audioSources), end(audioSources), source) ==
		       end(audioSources);
	};
	using FilterAudioSources_t = decltype(FilterAudioSources);

	obs_data_array_t *sourcesArray = obs_save_sources_filtered(
		[](void *data, obs_source_t *source) {
			return (*static_cast<FilterAudioSources_t *>(data))(
				source);
		},
		static_cast<void *>(&FilterAudioSources));

	/* -------------------------------- */
	/* save group sources separately    */

	/* saving separately ensures they won't be loaded in older versions */
	obs_data_array_t *groupsArray = obs_save_sources_filtered(
		[](void *, obs_source_t *source) {
			return obs_source_is_group(source);
		},
		nullptr);

	/* -------------------------------- */

	obs_source_t *transition = obs_get_output_source(0);
	obs_source_t *currentScene = obs_scene_get_source(scene);
	const char *sceneName = obs_source_get_name(currentScene);
	const char *programName = obs_source_get_name(curProgramScene);

	const char *sceneCollection = config_get_string(
		App()->GlobalConfig(), "Basic", "SceneCollection");

	obs_data_set_string(saveData, "current_scene", sceneName);
	obs_data_set_string(saveData, "current_program_scene", programName);
	obs_data_set_array(saveData, "scene_order", sceneOrder);
	obs_data_set_string(saveData, "name", sceneCollection);
	obs_data_set_array(saveData, "sources", sourcesArray);
	obs_data_set_array(saveData, "groups", groupsArray);
	obs_data_set_array(saveData, "quick_transitions", quickTransitionData);
	obs_data_set_array(saveData, "transitions", transitions);
	obs_data_set_array(saveData, "saved_projectors", savedProjectorList);
	obs_data_array_release(sourcesArray);
	obs_data_array_release(groupsArray);

	obs_data_set_string(saveData, "current_transition",
			    obs_source_get_name(transition));
	obs_data_set_int(saveData, "transition_duration", transitionDuration);
	obs_source_release(transition);

	return saveData;
}

void OBSBasic::copyActionsDynamicProperties()
{
	// Themes need the QAction dynamic properties
	for (QAction *x : ui->scenesToolbar->actions()) {
		QWidget *temp = ui->scenesToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}

	for (QAction *x : ui->sourcesToolbar->actions()) {
		QWidget *temp = ui->sourcesToolbar->widgetForAction(x);

		for (QByteArray &y : x->dynamicPropertyNames()) {
			temp->setProperty(y, x->property(y));
		}
	}
}

void OBSBasic::UpdateVolumeControlsDecayRate()
{
	double meterDecayRate =
		config_get_double(basicConfig, "Audio", "MeterDecayRate");

	for (size_t i = 0; i < volumes.size(); i++) {
		volumes[i]->SetMeterDecayRate(meterDecayRate);
	}
}

void OBSBasic::UpdateVolumeControlsPeakMeterType()
{
	uint32_t peakMeterTypeIdx =
		config_get_uint(basicConfig, "Audio", "PeakMeterType");

	enum obs_peak_meter_type peakMeterType;
	switch (peakMeterTypeIdx) {
	case 0:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	case 1:
		peakMeterType = TRUE_PEAK_METER;
		break;
	default:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	}

	for (size_t i = 0; i < volumes.size(); i++) {
		volumes[i]->setPeakMeterType(peakMeterType);
	}
}

void OBSBasic::ClearVolumeControls()
{
	for (VolControl *vol : volumes)
		delete vol;

	volumes.clear();
}

obs_data_array_t *OBSBasic::SaveSceneListOrder()
{
	obs_data_array_t *sceneOrder = obs_data_array_create();

	for (int i = 0; i < ui->scenes->count(); i++) {
		obs_data_t *data = obs_data_create();
		obs_data_set_string(data, "name",
				    QT_TO_UTF8(ui->scenes->item(i)->text()));
		obs_data_array_push_back(sceneOrder, data);
		obs_data_release(data);
	}

	return sceneOrder;
}

obs_data_array_t *OBSBasic::SaveProjectors()
{
	obs_data_array_t *savedProjectors = obs_data_array_create();

	auto saveProjector = [savedProjectors](OBSProjector *projector) {
		if (!projector)
			return;

		obs_data_t *data = obs_data_create();
		ProjectorType type = projector->GetProjectorType();

		switch (type) {
		case ProjectorType::Scene:
		case ProjectorType::Source: {
			obs_source_t *source = projector->GetSource();
			const char *name = obs_source_get_name(source);
			obs_data_set_string(data, "name", name);
			break;
		}
		default:
			break;
		}

		obs_data_set_int(data, "monitor", projector->GetMonitor());
		obs_data_set_int(data, "type", static_cast<int>(type));
		obs_data_set_string(
			data, "geometry",
			projector->saveGeometry().toBase64().constData());

		if (projector->IsAlwaysOnTopOverridden())
			obs_data_set_bool(data, "alwaysOnTop",
					  projector->IsAlwaysOnTop());

		obs_data_set_bool(data, "alwaysOnTopOverridden",
				  projector->IsAlwaysOnTopOverridden());

		obs_data_array_push_back(savedProjectors, data);
		obs_data_release(data);
	};

	for (size_t i = 0; i < projectors.size(); i++)
		saveProjector(static_cast<OBSProjector *>(projectors[i]));

	return savedProjectors;
}

void OBSBasic::Save(const char *file)
{
	OBSScene scene = GetCurrentScene();
	OBSSource curProgramScene = OBSGetStrongRef(programScene);
	if (!curProgramScene)
		curProgramScene = obs_scene_get_source(scene);

	obs_data_array_t *sceneOrder = SaveSceneListOrder();
	obs_data_array_t *transitions = SaveTransitions();
	obs_data_array_t *quickTrData = SaveQuickTransitions();
	obs_data_array_t *savedProjectorList = SaveProjectors();
	obs_data_t *saveData = GenerateSaveData(
		sceneOrder, quickTrData, ui->transitionDuration->value(),
		transitions, scene, curProgramScene, savedProjectorList);

	obs_data_set_bool(saveData, "preview_locked", ui->preview->Locked());
	obs_data_set_bool(saveData, "scaling_enabled",
			  ui->preview->IsFixedScaling());
	obs_data_set_int(saveData, "scaling_level",
			 ui->preview->GetScalingLevel());
	obs_data_set_double(saveData, "scaling_off_x",
			    ui->preview->GetScrollX());
	obs_data_set_double(saveData, "scaling_off_y",
			    ui->preview->GetScrollY());

	if (api) {
		obs_data_t *moduleObj = obs_data_create();
		api->on_save(moduleObj);
		obs_data_set_obj(saveData, "modules", moduleObj);
		obs_data_release(moduleObj);
	}

	if (!obs_data_save_json_safe(saveData, file, "tmp", "bak"))
		blog(LOG_ERROR, "Could not save scene data to %s", file);

	obs_data_release(saveData);
	obs_data_array_release(sceneOrder);
	obs_data_array_release(quickTrData);
	obs_data_array_release(transitions);
	obs_data_array_release(savedProjectorList);
}

void OBSBasic::DeferSaveBegin()
{
	os_atomic_inc_long(&disableSaving);
}

void OBSBasic::DeferSaveEnd()
{
	long result = os_atomic_dec_long(&disableSaving);
	if (result == 0) {
		SaveProject();
	}
}

static void LogFilter(obs_source_t *, obs_source_t *filter, void *v_val);

static void LoadAudioDevice(const char *name, int channel, obs_data_t *parent)
{
	obs_data_t *data = obs_data_get_obj(parent, name);
	if (!data)
		return;

	obs_source_t *source = obs_load_source(data);
	if (source) {
		obs_set_output_source(channel, source);

		const char *name = obs_source_get_name(source);
		blog(LOG_INFO, "[Loaded global audio device]: '%s'", name);
		obs_source_enum_filters(source, LogFilter, (void *)(intptr_t)1);
		obs_monitoring_type monitoring_type =
			obs_source_get_monitoring_type(source);
		if (monitoring_type != OBS_MONITORING_TYPE_NONE) {
			const char *type = (monitoring_type ==
					    OBS_MONITORING_TYPE_MONITOR_ONLY)
						   ? "monitor only"
						   : "monitor and output";

			blog(LOG_INFO, "    - monitoring: %s", type);
		}
		obs_source_release(source);
	}

	obs_data_release(data);
}

static inline bool HasAudioDevices(const char *source_id)
{
	const char *output_id = source_id;
	obs_properties_t *props = obs_get_source_properties(output_id);
	size_t count = 0;

	if (!props)
		return false;

	obs_property_t *devices = obs_properties_get(props, "device_id");
	if (devices)
		count = obs_property_list_item_count(devices);

	obs_properties_destroy(props);

	return count != 0;
}

void OBSBasic::CreateFirstRunSources()
{
	bool hasDesktopAudio = HasAudioDevices(App()->OutputAudioSource());
	bool hasInputAudio = HasAudioDevices(App()->InputAudioSource());

	if (hasDesktopAudio)
		ResetAudioDevice(App()->OutputAudioSource(), "default",
				 Str("Basic.DesktopDevice1"), 1);
	if (hasInputAudio)
		ResetAudioDevice(App()->InputAudioSource(), "default",
				 Str("Basic.AuxDevice1"), 3);
}

void OBSBasic::CreateDefaultScene(bool firstStart)
{
	disableSaving++;

	ClearSceneData();
	InitDefaultTransitions();
	CreateDefaultQuickTransitions();
	ui->transitionDuration->setValue(300);
	SetTransition(fadeTransition);

	obs_scene_t *scene = obs_scene_create(Str("Basic.Scene"));

	if (firstStart)
		CreateFirstRunSources();

	SetCurrentScene(scene, true);
	obs_scene_release(scene);

	disableSaving--;
}

static void ReorderItemByName(QListWidget *lw, const char *name, int newIndex)
{
	for (int i = 0; i < lw->count(); i++) {
		QListWidgetItem *item = lw->item(i);

		if (strcmp(name, QT_TO_UTF8(item->text())) == 0) {
			if (newIndex != i) {
				item = lw->takeItem(i);
				lw->insertItem(newIndex, item);
			}
			break;
		}
	}
}

void OBSBasic::LoadSceneListOrder(obs_data_array_t *array)
{
	size_t num = obs_data_array_count(array);

	for (size_t i = 0; i < num; i++) {
		obs_data_t *data = obs_data_array_item(array, i);
		const char *name = obs_data_get_string(data, "name");

		ReorderItemByName(ui->scenes, name, (int)i);

		obs_data_release(data);
	}
}

void OBSBasic::LoadSavedProjectors(obs_data_array_t *array)
{
	for (SavedProjectorInfo *info : savedProjectorsArray) {
		delete info;
	}
	savedProjectorsArray.clear();

	size_t num = obs_data_array_count(array);

	for (size_t i = 0; i < num; i++) {
		obs_data_t *data = obs_data_array_item(array, i);

		SavedProjectorInfo *info = new SavedProjectorInfo();
		info->monitor = obs_data_get_int(data, "monitor");
		info->type = static_cast<ProjectorType>(
			obs_data_get_int(data, "type"));
		info->geometry =
			std::string(obs_data_get_string(data, "geometry"));
		info->name = std::string(obs_data_get_string(data, "name"));
		info->alwaysOnTop = obs_data_get_bool(data, "alwaysOnTop");
		info->alwaysOnTopOverridden =
			obs_data_get_bool(data, "alwaysOnTopOverridden");

		savedProjectorsArray.emplace_back(info);

		obs_data_release(data);
	}
}

static void LogFilter(obs_source_t *, obs_source_t *filter, void *v_val)
{
	const char *name = obs_source_get_name(filter);
	const char *id = obs_source_get_id(filter);
	int val = (int)(intptr_t)v_val;
	string indent;

	for (int i = 0; i < val; i++)
		indent += "    ";

	blog(LOG_INFO, "%s- filter: '%s' (%s)", indent.c_str(), name, id);
}

static bool LogSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *v_val)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	int indent_count = (int)(intptr_t)v_val;
	string indent;

	for (int i = 0; i < indent_count; i++)
		indent += "    ";

	blog(LOG_INFO, "%s- source: '%s' (%s)", indent.c_str(), name, id);

	obs_monitoring_type monitoring_type =
		obs_source_get_monitoring_type(source);

	if (monitoring_type != OBS_MONITORING_TYPE_NONE) {
		const char *type =
			(monitoring_type == OBS_MONITORING_TYPE_MONITOR_ONLY)
				? "monitor only"
				: "monitor and output";

		blog(LOG_INFO, "    %s- monitoring: %s", indent.c_str(), type);
	}
	int child_indent = 1 + indent_count;
	obs_source_enum_filters(source, LogFilter,
				(void *)(intptr_t)child_indent);

	obs_source_t *show_tn = obs_sceneitem_get_show_transition(item);
	obs_source_t *hide_tn = obs_sceneitem_get_hide_transition(item);
	if (show_tn)
		blog(LOG_INFO, "    %s- show: '%s' (%s)", indent.c_str(),
		     obs_source_get_name(show_tn), obs_source_get_id(show_tn));
	if (hide_tn)
		blog(LOG_INFO, "    %s- hide: '%s' (%s)", indent.c_str(),
		     obs_source_get_name(hide_tn), obs_source_get_id(hide_tn));

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, LogSceneItem,
					       (void *)(intptr_t)child_indent);
	return true;
}

void OBSBasic::LogScenes()
{
	blog(LOG_INFO, "------------------------------------------------");
	blog(LOG_INFO, "Loaded scenes:");

	for (int i = 0; i < ui->scenes->count(); i++) {
		QListWidgetItem *item = ui->scenes->item(i);
		OBSScene scene = GetOBSRef<OBSScene>(item);

		obs_source_t *source = obs_scene_get_source(scene);
		const char *name = obs_source_get_name(source);

		blog(LOG_INFO, "- scene '%s':", name);
		obs_scene_enum_items(scene, LogSceneItem, (void *)(intptr_t)1);
		obs_source_enum_filters(source, LogFilter, (void *)(intptr_t)1);
	}

	blog(LOG_INFO, "------------------------------------------------");
}

void OBSBasic::Load(const char *file)
{
	disableSaving++;

	obs_data_t *data = obs_data_create_from_json_file_safe(file, "bak");
	if (!data) {
		disableSaving--;
		blog(LOG_INFO, "No scene file found, creating default scene");
		CreateDefaultScene(true);
		SaveProject();
		return;
	}

	LoadData(data, file);
}

void OBSBasic::LoadData(obs_data_t *data, const char *file)
{
	ClearSceneData();
	InitDefaultTransitions();
	ClearContextBar();

	if (devicePropertiesThread && devicePropertiesThread->isRunning()) {
		devicePropertiesThread->wait();
		devicePropertiesThread.reset();
	}

	QApplication::sendPostedEvents(this);

	obs_data_t *modulesObj = obs_data_get_obj(data, "modules");
	if (api)
		api->on_preload(modulesObj);

	obs_data_array_t *sceneOrder = obs_data_get_array(data, "scene_order");
	obs_data_array_t *sources = obs_data_get_array(data, "sources");
	obs_data_array_t *groups = obs_data_get_array(data, "groups");
	obs_data_array_t *transitions = obs_data_get_array(data, "transitions");
	const char *sceneName = obs_data_get_string(data, "current_scene");
	const char *programSceneName =
		obs_data_get_string(data, "current_program_scene");
	const char *transitionName =
		obs_data_get_string(data, "current_transition");

	if (!opt_starting_scene.empty()) {
		programSceneName = opt_starting_scene.c_str();
		if (!IsPreviewProgramMode())
			sceneName = opt_starting_scene.c_str();
	}

	int newDuration = obs_data_get_int(data, "transition_duration");
	if (!newDuration)
		newDuration = 300;

	if (!transitionName)
		transitionName = obs_source_get_name(fadeTransition);

	const char *curSceneCollection = config_get_string(
		App()->GlobalConfig(), "Basic", "SceneCollection");

	obs_data_set_default_string(data, "name", curSceneCollection);

	const char *name = obs_data_get_string(data, "name");
	obs_source_t *curScene;
	obs_source_t *curProgramScene;
	obs_source_t *curTransition;

	if (!name || !*name)
		name = curSceneCollection;

	LoadAudioDevice(DESKTOP_AUDIO_1, 1, data);
	LoadAudioDevice(DESKTOP_AUDIO_2, 2, data);
	LoadAudioDevice(AUX_AUDIO_1, 3, data);
	LoadAudioDevice(AUX_AUDIO_2, 4, data);
	LoadAudioDevice(AUX_AUDIO_3, 5, data);
	LoadAudioDevice(AUX_AUDIO_4, 6, data);

	if (!sources) {
		sources = groups;
		groups = nullptr;
	} else {
		obs_data_array_push_back_array(sources, groups);
	}

	obs_missing_files_t *files = obs_missing_files_create();

	auto cb = [](void *private_data, obs_source_t *source) {
		obs_missing_files_t *f = (obs_missing_files_t *)private_data;
		obs_missing_files_t *sf = obs_source_get_missing_files(source);

		obs_missing_files_append(f, sf);
		obs_missing_files_destroy(sf);

		UNUSED_PARAMETER(source);
	};

	obs_load_sources(sources, cb, files);

	if (transitions)
		LoadTransitions(transitions);
	if (sceneOrder)
		LoadSceneListOrder(sceneOrder);

	obs_data_array_release(transitions);

	curTransition = FindTransition(transitionName);
	if (!curTransition)
		curTransition = fadeTransition;

	ui->transitionDuration->setValue(newDuration);
	SetTransition(curTransition);

retryScene:
	curScene = obs_get_source_by_name(sceneName);
	curProgramScene = obs_get_source_by_name(programSceneName);

	/* if the starting scene command line parameter is bad at all,
	 * fall back to original settings */
	if (!opt_starting_scene.empty() && (!curScene || !curProgramScene)) {
		sceneName = obs_data_get_string(data, "current_scene");
		programSceneName =
			obs_data_get_string(data, "current_program_scene");
		obs_source_release(curScene);
		obs_source_release(curProgramScene);
		opt_starting_scene.clear();
		goto retryScene;
	}

	if (!curProgramScene) {
		curProgramScene = curScene;
		obs_source_addref(curScene);
	}

	SetCurrentScene(curScene, true);
	if (IsPreviewProgramMode())
		TransitionToScene(curProgramScene, true);
	obs_source_release(curScene);
	obs_source_release(curProgramScene);

	obs_data_array_release(sources);
	obs_data_array_release(groups);
	obs_data_array_release(sceneOrder);

	/* ------------------- */

	bool projectorSave = config_get_bool(GetGlobalConfig(), "BasicWindow",
					     "SaveProjectors");

	if (projectorSave) {
		obs_data_array_t *savedProjectors =
			obs_data_get_array(data, "saved_projectors");

		if (savedProjectors) {
			LoadSavedProjectors(savedProjectors);
			OpenSavedProjectors();
			activateWindow();
		}

		obs_data_array_release(savedProjectors);
	}

	/* ------------------- */

	std::string file_base = strrchr(file, '/') + 1;
	file_base.erase(file_base.size() - 5, 5);

	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollection",
			  name);
	config_set_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile",
			  file_base.c_str());

	obs_data_array_t *quickTransitionData =
		obs_data_get_array(data, "quick_transitions");
	LoadQuickTransitions(quickTransitionData);
	obs_data_array_release(quickTransitionData);

	RefreshQuickTransitions();

	bool previewLocked = obs_data_get_bool(data, "preview_locked");
	ui->preview->SetLocked(previewLocked);
	ui->actionLockPreview->setChecked(previewLocked);

	/* ---------------------- */

	bool fixedScaling = obs_data_get_bool(data, "scaling_enabled");
	int scalingLevel = (int)obs_data_get_int(data, "scaling_level");
	float scrollOffX = (float)obs_data_get_double(data, "scaling_off_x");
	float scrollOffY = (float)obs_data_get_double(data, "scaling_off_y");

	if (fixedScaling) {
		ui->preview->SetScalingLevel(scalingLevel);
		ui->preview->SetScrollingOffset(scrollOffX, scrollOffY);
	}
	ui->preview->SetFixedScaling(fixedScaling);
	emit ui->preview->DisplayResized();

	/* ---------------------- */

	if (api)
		api->on_load(modulesObj);

	obs_data_release(modulesObj);
	obs_data_release(data);

	if (!opt_starting_scene.empty())
		opt_starting_scene.clear();

	if (opt_start_streaming) {
		blog(LOG_INFO, "Starting stream due to command line parameter");
		QMetaObject::invokeMethod(this, "StartStreaming",
					  Qt::QueuedConnection);
		opt_start_streaming = false;
	}

	if (opt_start_recording) {
		blog(LOG_INFO,
		     "Starting recording due to command line parameter");
		QMetaObject::invokeMethod(this, "StartRecording",
					  Qt::QueuedConnection);
		opt_start_recording = false;
	}

	if (opt_start_replaybuffer) {
		QMetaObject::invokeMethod(this, "StartReplayBuffer",
					  Qt::QueuedConnection);
		opt_start_replaybuffer = false;
	}

	if (opt_start_virtualcam) {
		QMetaObject::invokeMethod(this, "StartVirtualCam",
					  Qt::QueuedConnection);
		opt_start_virtualcam = false;
	}

	LogScenes();

	if (obs_missing_files_count(files) > 0 &&
	    !App()->IsMissingFilesCheckDisabled()) {
		/* the window hasn't fully initialized by this point on macOS,
		 * so put this at the end of the current task queue. Fixes a
		 * bug where the window be behind OBS on startup */
		QTimer::singleShot(0, [this, files] {
			missDialog = new OBSMissingFiles(files, this);
			missDialog->setAttribute(Qt::WA_DeleteOnClose, true);
			missDialog->show();
			missDialog->raise();
		});
	} else {
		obs_missing_files_destroy(files);
	}

	disableSaving--;

	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_SCENE_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
	}
}

#define SERVICE_PATH "service.json"

void OBSBasic::SaveService()
{
	if (!service)
		return;

	char serviceJsonPath[512];
	int ret = GetProfilePath(serviceJsonPath, sizeof(serviceJsonPath),
				 SERVICE_PATH);
	if (ret <= 0)
		return;

	obs_data_t *data = obs_data_create();
	obs_data_t *settings = obs_service_get_settings(service);

	obs_data_set_string(data, "type", obs_service_get_type(service));
	obs_data_set_obj(data, "settings", settings);

	if (!obs_data_save_json_safe(data, serviceJsonPath, "tmp", "bak"))
		blog(LOG_WARNING, "Failed to save service");

	obs_data_release(settings);
	obs_data_release(data);
}

bool OBSBasic::LoadService()
{
	const char *type;

	char serviceJsonPath[512];
	int ret = GetProfilePath(serviceJsonPath, sizeof(serviceJsonPath),
				 SERVICE_PATH);
	if (ret <= 0)
		return false;

	obs_data_t *data =
		obs_data_create_from_json_file_safe(serviceJsonPath, "bak");

	if (!data)
		return false;

	obs_data_set_default_string(data, "type", "rtmp_common");
	type = obs_data_get_string(data, "type");

	obs_data_t *settings = obs_data_get_obj(data, "settings");
	obs_data_t *hotkey_data = obs_data_get_obj(data, "hotkeys");

	service = obs_service_create(type, "default_service", settings,
				     hotkey_data);
	obs_service_release(service);

	obs_data_release(hotkey_data);
	obs_data_release(settings);
	obs_data_release(data);

	return !!service;
}

bool OBSBasic::InitService()
{
	ProfileScope("OBSBasic::InitService");

	if (LoadService())
		return true;

	service = obs_service_create("rtmp_common", "default_service", nullptr,
				     nullptr);
	if (!service)
		return false;
	obs_service_release(service);

	return true;
}

static const double scaled_vals[] = {1.0,         1.25, (1.0 / 0.75), 1.5,
				     (1.0 / 0.6), 1.75, 2.0,          2.25,
				     2.5,         2.75, 3.0,          0.0};

extern void CheckExistingCookieId();

bool OBSBasic::InitBasicConfigDefaults()
{
	QList<QScreen *> screens = QGuiApplication::screens();

	if (!screens.size()) {
		OBSErrorBox(NULL, "There appears to be no monitors.  Er, this "
				  "technically shouldn't be possible.");
		return false;
	}

	QScreen *primaryScreen = QGuiApplication::primaryScreen();

	uint32_t cx = primaryScreen->size().width();
	uint32_t cy = primaryScreen->size().height();

	cx *= devicePixelRatioF();
	cy *= devicePixelRatioF();

	bool oldResolutionDefaults = config_get_bool(
		App()->GlobalConfig(), "General", "Pre19Defaults");

	/* use 1920x1080 for new default base res if main monitor is above
	 * 1920x1080, but don't apply for people from older builds -- only to
	 * new users */
	if (!oldResolutionDefaults && (cx * cy) > (1920 * 1080)) {
		cx = 1920;
		cy = 1080;
	}

	bool changed = false;

	/* ----------------------------------------------------- */
	/* move over old FFmpeg track settings                   */
	if (config_has_user_value(basicConfig, "AdvOut", "FFAudioTrack") &&
	    !config_has_user_value(basicConfig, "AdvOut", "Pre22.1Settings")) {

		int track = (int)config_get_int(basicConfig, "AdvOut",
						"FFAudioTrack");
		config_set_int(basicConfig, "AdvOut", "FFAudioMixes",
			       1LL << (track - 1));
		config_set_bool(basicConfig, "AdvOut", "Pre22.1Settings", true);
		changed = true;
	}

	/* ----------------------------------------------------- */
	/* move over mixer values in advanced if older config */
	if (config_has_user_value(basicConfig, "AdvOut", "RecTrackIndex") &&
	    !config_has_user_value(basicConfig, "AdvOut", "RecTracks")) {

		uint64_t track =
			config_get_uint(basicConfig, "AdvOut", "RecTrackIndex");
		track = 1ULL << (track - 1);
		config_set_uint(basicConfig, "AdvOut", "RecTracks", track);
		config_remove_value(basicConfig, "AdvOut", "RecTrackIndex");
		changed = true;
	}

	/* ----------------------------------------------------- */
	/* set twitch chat extensions to "both" if prev version  */
	/* is under 24.1                                         */
	if (config_get_bool(GetGlobalConfig(), "General", "Pre24.1Defaults") &&
	    !config_has_user_value(basicConfig, "Twitch", "AddonChoice")) {
		config_set_int(basicConfig, "Twitch", "AddonChoice", 3);
		changed = true;
	}

	/* ----------------------------------------------------- */
	/* move bitrate enforcement setting to new value         */
	if (config_has_user_value(basicConfig, "SimpleOutput",
				  "EnforceBitrate") &&
	    !config_has_user_value(basicConfig, "Stream1",
				   "IgnoreRecommended") &&
	    !config_has_user_value(basicConfig, "Stream1", "MovedOldEnforce")) {
		bool enforce = config_get_bool(basicConfig, "SimpleOutput",
					       "EnforceBitrate");
		config_set_bool(basicConfig, "Stream1", "IgnoreRecommended",
				!enforce);
		config_set_bool(basicConfig, "Stream1", "MovedOldEnforce",
				true);
		changed = true;
	}

	/* ----------------------------------------------------- */

	if (changed)
		config_save_safe(basicConfig, "tmp", nullptr);

	/* ----------------------------------------------------- */

	config_set_default_string(basicConfig, "Output", "Mode", "Simple");

	config_set_default_bool(basicConfig, "Stream1", "IgnoreRecommended",
				false);

	config_set_default_string(basicConfig, "SimpleOutput", "FilePath",
				  GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "SimpleOutput", "RecFormat",
				  "mkv");
	config_set_default_uint(basicConfig, "SimpleOutput", "VBitrate", 2500);
	config_set_default_uint(basicConfig, "SimpleOutput", "ABitrate", 160);
	config_set_default_bool(basicConfig, "SimpleOutput", "UseAdvanced",
				false);
	config_set_default_string(basicConfig, "SimpleOutput", "Preset",
				  "veryfast");
	config_set_default_string(basicConfig, "SimpleOutput", "NVENCPreset",
				  "hq");
	config_set_default_string(basicConfig, "SimpleOutput", "RecQuality",
				  "Stream");
	config_set_default_bool(basicConfig, "SimpleOutput", "RecRB", false);
	config_set_default_int(basicConfig, "SimpleOutput", "RecRBTime", 20);
	config_set_default_int(basicConfig, "SimpleOutput", "RecRBSize", 512);
	config_set_default_string(basicConfig, "SimpleOutput", "RecRBPrefix",
				  "Replay");

	config_set_default_bool(basicConfig, "AdvOut", "ApplyServiceSettings",
				true);
	config_set_default_bool(basicConfig, "AdvOut", "UseRescale", false);
	config_set_default_uint(basicConfig, "AdvOut", "TrackIndex", 1);
	config_set_default_uint(basicConfig, "AdvOut", "VodTrackIndex", 2);
	config_set_default_string(basicConfig, "AdvOut", "Encoder", "obs_x264");

	config_set_default_string(basicConfig, "AdvOut", "RecType", "Standard");

	config_set_default_string(basicConfig, "AdvOut", "RecFilePath",
				  GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "AdvOut", "RecFormat", "mkv");
	config_set_default_bool(basicConfig, "AdvOut", "RecUseRescale", false);
	config_set_default_uint(basicConfig, "AdvOut", "RecTracks", (1 << 0));
	config_set_default_string(basicConfig, "AdvOut", "RecEncoder", "none");
	config_set_default_uint(basicConfig, "AdvOut", "FLVTrack", 1);

	config_set_default_bool(basicConfig, "AdvOut", "FFOutputToFile", true);
	config_set_default_string(basicConfig, "AdvOut", "FFFilePath",
				  GetDefaultVideoSavePath().c_str());
	config_set_default_string(basicConfig, "AdvOut", "FFExtension", "mp4");
	config_set_default_uint(basicConfig, "AdvOut", "FFVBitrate", 2500);
	config_set_default_uint(basicConfig, "AdvOut", "FFVGOPSize", 250);
	config_set_default_bool(basicConfig, "AdvOut", "FFUseRescale", false);
	config_set_default_bool(basicConfig, "AdvOut", "FFIgnoreCompat", false);
	config_set_default_uint(basicConfig, "AdvOut", "FFABitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "FFAudioMixes", 1);

	config_set_default_uint(basicConfig, "AdvOut", "Track1Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track2Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track3Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track4Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track5Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track6Bitrate", 160);

	config_set_default_bool(basicConfig, "AdvOut", "RecRB", false);
	config_set_default_uint(basicConfig, "AdvOut", "RecRBTime", 20);
	config_set_default_int(basicConfig, "AdvOut", "RecRBSize", 512);

	config_set_default_uint(basicConfig, "Video", "BaseCX", cx);
	config_set_default_uint(basicConfig, "Video", "BaseCY", cy);

	/* don't allow BaseCX/BaseCY to be susceptible to defaults changing */
	if (!config_has_user_value(basicConfig, "Video", "BaseCX") ||
	    !config_has_user_value(basicConfig, "Video", "BaseCY")) {
		config_set_uint(basicConfig, "Video", "BaseCX", cx);
		config_set_uint(basicConfig, "Video", "BaseCY", cy);
		config_save_safe(basicConfig, "tmp", nullptr);
	}

	config_set_default_string(basicConfig, "Output", "FilenameFormatting",
				  "%CCYY-%MM-%DD %hh-%mm-%ss");

	config_set_default_bool(basicConfig, "Output", "DelayEnable", false);
	config_set_default_uint(basicConfig, "Output", "DelaySec", 20);
	config_set_default_bool(basicConfig, "Output", "DelayPreserve", true);

	config_set_default_bool(basicConfig, "Output", "Reconnect", true);
	config_set_default_uint(basicConfig, "Output", "RetryDelay", 10);
	config_set_default_uint(basicConfig, "Output", "MaxRetries", 20);

	config_set_default_string(basicConfig, "Output", "BindIP", "default");
	config_set_default_bool(basicConfig, "Output", "NewSocketLoopEnable",
				false);
	config_set_default_bool(basicConfig, "Output", "LowLatencyEnable",
				false);

	int i = 0;
	uint32_t scale_cx = cx;
	uint32_t scale_cy = cy;

	/* use a default scaled resolution that has a pixel count no higher
	 * than 1280x720 */
	while (((scale_cx * scale_cy) > (1280 * 720)) && scaled_vals[i] > 0.0) {
		double scale = scaled_vals[i++];
		scale_cx = uint32_t(double(cx) / scale);
		scale_cy = uint32_t(double(cy) / scale);
	}

	config_set_default_uint(basicConfig, "Video", "OutputCX", scale_cx);
	config_set_default_uint(basicConfig, "Video", "OutputCY", scale_cy);

	/* don't allow OutputCX/OutputCY to be susceptible to defaults
	 * changing */
	if (!config_has_user_value(basicConfig, "Video", "OutputCX") ||
	    !config_has_user_value(basicConfig, "Video", "OutputCY")) {
		config_set_uint(basicConfig, "Video", "OutputCX", scale_cx);
		config_set_uint(basicConfig, "Video", "OutputCY", scale_cy);
		config_save_safe(basicConfig, "tmp", nullptr);
	}

	config_set_default_uint(basicConfig, "Video", "FPSType", 0);
	config_set_default_string(basicConfig, "Video", "FPSCommon", "30");
	config_set_default_uint(basicConfig, "Video", "FPSInt", 30);
	config_set_default_uint(basicConfig, "Video", "FPSNum", 30);
	config_set_default_uint(basicConfig, "Video", "FPSDen", 1);
	config_set_default_string(basicConfig, "Video", "ScaleType", "bicubic");
	config_set_default_string(basicConfig, "Video", "ColorFormat", "NV12");
	config_set_default_string(basicConfig, "Video", "ColorSpace", "709");
	config_set_default_string(basicConfig, "Video", "ColorRange",
				  "Partial");

	config_set_default_string(basicConfig, "Audio", "MonitoringDeviceId",
				  "default");
	config_set_default_string(
		basicConfig, "Audio", "MonitoringDeviceName",
		Str("Basic.Settings.Advanced.Audio.MonitoringDevice"
		    ".Default"));
	config_set_default_uint(basicConfig, "Audio", "SampleRate", 48000);
	config_set_default_string(basicConfig, "Audio", "ChannelSetup",
				  "Stereo");
	config_set_default_double(basicConfig, "Audio", "MeterDecayRate",
				  VOLUME_METER_DECAY_FAST);
	config_set_default_uint(basicConfig, "Audio", "PeakMeterType", 0);

	CheckExistingCookieId();

	return true;
}

extern bool EncoderAvailable(const char *encoder);

void OBSBasic::InitBasicConfigDefaults2()
{
	bool oldEncDefaults = config_get_bool(App()->GlobalConfig(), "General",
					      "Pre23Defaults");
	bool useNV = EncoderAvailable("ffmpeg_nvenc") && !oldEncDefaults;

	config_set_default_string(basicConfig, "SimpleOutput", "StreamEncoder",
				  useNV ? SIMPLE_ENCODER_NVENC
					: SIMPLE_ENCODER_X264);
	config_set_default_string(basicConfig, "SimpleOutput", "RecEncoder",
				  useNV ? SIMPLE_ENCODER_NVENC
					: SIMPLE_ENCODER_X264);
}

bool OBSBasic::InitBasicConfig()
{
	ProfileScope("OBSBasic::InitBasicConfig");

	char configPath[512];

	int ret = GetProfilePath(configPath, sizeof(configPath), "");
	if (ret <= 0) {
		OBSErrorBox(nullptr, "Failed to get profile path");
		return false;
	}

	if (os_mkdir(configPath) == MKDIR_ERROR) {
		OBSErrorBox(nullptr, "Failed to create profile path");
		return false;
	}

	ret = GetProfilePath(configPath, sizeof(configPath), "basic.ini");
	if (ret <= 0) {
		OBSErrorBox(nullptr, "Failed to get basic.ini path");
		return false;
	}

	int code = basicConfig.Open(configPath, CONFIG_OPEN_ALWAYS);
	if (code != CONFIG_SUCCESS) {
		OBSErrorBox(NULL, "Failed to open basic.ini: %d", code);
		return false;
	}

	if (config_get_string(basicConfig, "General", "Name") == nullptr) {
		const char *curName = config_get_string(App()->GlobalConfig(),
							"Basic", "Profile");

		config_set_string(basicConfig, "General", "Name", curName);
		basicConfig.SaveSafe("tmp");
	}

	return InitBasicConfigDefaults();
}

void OBSBasic::InitOBSCallbacks()
{
	ProfileScope("OBSBasic::InitOBSCallbacks");

	signalHandlers.reserve(signalHandlers.size() + 7);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_create",
				    OBSBasic::SourceCreated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_remove",
				    OBSBasic::SourceRemoved, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_activate",
				    OBSBasic::SourceActivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(),
				    "source_deactivate",
				    OBSBasic::SourceDeactivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(),
				    "source_audio_activate",
				    OBSBasic::SourceAudioActivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(),
				    "source_audio_deactivate",
				    OBSBasic::SourceAudioDeactivated, this);
	signalHandlers.emplace_back(obs_get_signal_handler(), "source_rename",
				    OBSBasic::SourceRenamed, this);
}

void OBSBasic::InitPrimitives()
{
	ProfileScope("OBSBasic::InitPrimitives");

	obs_enter_graphics();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f, 1.0f);
	box = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	boxLeft = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(1.0f, 0.0f);
	boxTop = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f, 1.0f);
	boxRight = gs_render_save();

	gs_render_start(true);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	boxBottom = gs_render_save();

	gs_render_start(true);
	for (int i = 0; i <= 360; i += (360 / 20)) {
		float pos = RAD(float(i));
		gs_vertex2f(cosf(pos), sinf(pos));
	}
	circle = gs_render_save();

	obs_leave_graphics();
}

void OBSBasic::ReplayBufferClicked()
{
	if (outputHandler->ReplayBufferActive())
		StopReplayBuffer();
	else
		StartReplayBuffer();
};

void OBSBasic::AddVCamButton()
{
	vcamButton = new ReplayBufferButton(QTStr("Basic.Main.StartVirtualCam"),
					    this);
	vcamButton->setCheckable(true);
	connect(vcamButton.data(), &QPushButton::clicked, this,
		&OBSBasic::VCamButtonClicked);

	vcamButton->setProperty("themeID", "vcamButton");
	ui->buttonsVLayout->insertWidget(2, vcamButton);
	setTabOrder(ui->recordButton, vcamButton);
	setTabOrder(vcamButton, ui->modeSwitch);
}

void OBSBasic::ResetOutputs()
{
	ProfileScope("OBSBasic::ResetOutputs");

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	bool advOut = astrcmpi(mode, "Advanced") == 0;

	if (!outputHandler || !outputHandler->Active()) {
		outputHandler.reset();
		outputHandler.reset(advOut ? CreateAdvancedOutputHandler(this)
					   : CreateSimpleOutputHandler(this));

		delete replayBufferButton;
		delete replayLayout;

		if (outputHandler->replayBuffer) {
			replayBufferButton = new ReplayBufferButton(
				QTStr("Basic.Main.StartReplayBuffer"), this);
			replayBufferButton->setCheckable(true);
			connect(replayBufferButton.data(),
				&QPushButton::clicked, this,
				&OBSBasic::ReplayBufferClicked);

			replayBufferButton->setSizePolicy(QSizePolicy::Ignored,
							  QSizePolicy::Fixed);

			replayLayout = new QHBoxLayout(this);
			replayLayout->addWidget(replayBufferButton);

			replayBufferButton->setProperty("themeID",
							"replayBufferButton");
			ui->buttonsVLayout->insertLayout(2, replayLayout);
			setTabOrder(ui->recordButton, replayBufferButton);
			setTabOrder(replayBufferButton,
				    ui->buttonsVLayout->itemAt(3)->widget());
		}

		if (sysTrayReplayBuffer)
			sysTrayReplayBuffer->setEnabled(
				!!outputHandler->replayBuffer);
	} else {
		outputHandler->Update();
	}
}

static void AddProjectorMenuMonitors(QMenu *parent, QObject *target,
				     const char *slot);

#define STARTUP_SEPARATOR \
	"==== Startup complete ==============================================="
#define SHUTDOWN_SEPARATOR \
	"==== Shutting down =================================================="

#define UNSUPPORTED_ERROR                                                     \
	"Failed to initialize video:\n\nRequired graphics API functionality " \
	"not found.  Your GPU may not be supported."

#define UNKNOWN_ERROR                                                  \
	"Failed to initialize video.  Your GPU may not be supported, " \
	"or your graphics drivers may need to be updated."

void OBSBasic::OBSInit()
{
	ProfileScope("OBSBasic::OBSInit");

	const char *sceneCollection = config_get_string(
		App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	char savePath[512];
	char fileName[512];
	int ret;

	if (!sceneCollection)
		throw "Failed to get scene collection name";

	ret = snprintf(fileName, 512, "obs-studio/basic/scenes/%s.json",
		       sceneCollection);
	if (ret <= 0)
		throw "Failed to create scene collection file name";

	ret = GetConfigPath(savePath, sizeof(savePath), fileName);
	if (ret <= 0)
		throw "Failed to get scene collection json file path";

	if (!InitBasicConfig())
		throw "Failed to load basic.ini";
	if (!ResetAudio())
		throw "Failed to initialize audio";

	ret = ResetVideo();

	switch (ret) {
	case OBS_VIDEO_MODULE_NOT_FOUND:
		throw "Failed to initialize video:  Graphics module not found";
	case OBS_VIDEO_NOT_SUPPORTED:
		throw UNSUPPORTED_ERROR;
	case OBS_VIDEO_INVALID_PARAM:
		throw "Failed to initialize video:  Invalid parameters";
	default:
		if (ret != OBS_VIDEO_SUCCESS)
			throw UNKNOWN_ERROR;
	}

	/* load audio monitoring */
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	const char *device_name =
		config_get_string(basicConfig, "Audio", "MonitoringDeviceName");
	const char *device_id =
		config_get_string(basicConfig, "Audio", "MonitoringDeviceId");

	obs_set_audio_monitoring_device(device_name, device_id);

	blog(LOG_INFO, "Audio monitoring device:\n\tname: %s\n\tid: %s",
	     device_name, device_id);
#endif

	InitOBSCallbacks();
	InitHotkeys();

	/* hack to prevent elgato from loading its own QtNetwork that it tries
	 * to ship with */
#if defined(_WIN32) && !defined(_DEBUG)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	LoadLibraryW(L"Qt5Network");
#else
	LoadLibraryW(L"Qt6Network");
#endif
#endif

	AddExtraModulePaths();
	blog(LOG_INFO, "---------------------------------");
	obs_load_all_modules();
	blog(LOG_INFO, "---------------------------------");
	obs_log_loaded_modules();
	blog(LOG_INFO, "---------------------------------");
	obs_post_load_modules();

#ifdef BROWSER_AVAILABLE
	cef = obs_browser_init_panel();
#endif

	obs_data_t *obsData = obs_get_private_data();
	vcamEnabled = obs_data_get_bool(obsData, "vcamEnabled");
	if (vcamEnabled) {
		AddVCamButton();
	}
	obs_data_release(obsData);

	InitBasicConfigDefaults2();

	CheckForSimpleModeX264Fallback();

	blog(LOG_INFO, STARTUP_SEPARATOR);

	ResetOutputs();
	CreateHotkeys();

	if (!InitService())
		throw "Failed to initialize service";

	InitPrimitives();

	sceneDuplicationMode = config_get_bool(
		App()->GlobalConfig(), "BasicWindow", "SceneDuplicationMode");
	swapScenesMode = config_get_bool(App()->GlobalConfig(), "BasicWindow",
					 "SwapScenesMode");
	editPropertiesMode = config_get_bool(
		App()->GlobalConfig(), "BasicWindow", "EditPropertiesMode");

	if (!opt_studio_mode) {
		SetPreviewProgramMode(config_get_bool(App()->GlobalConfig(),
						      "BasicWindow",
						      "PreviewProgramMode"));
	} else {
		SetPreviewProgramMode(true);
		opt_studio_mode = false;
	}

#define SET_VISIBILITY(name, control)                                         \
	do {                                                                  \
		if (config_has_user_value(App()->GlobalConfig(),              \
					  "BasicWindow", name)) {             \
			bool visible = config_get_bool(App()->GlobalConfig(), \
						       "BasicWindow", name);  \
			ui->control->setChecked(visible);                     \
		}                                                             \
	} while (false)

	SET_VISIBILITY("ShowListboxToolbars", toggleListboxToolbars);
	SET_VISIBILITY("ShowStatusBar", toggleStatusBar);
#undef SET_VISIBILITY

	bool sourceIconsVisible = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "ShowSourceIcons");
	ui->toggleSourceIcons->setChecked(sourceIconsVisible);

	bool contextVisible = config_get_bool(
		App()->GlobalConfig(), "BasicWindow", "ShowContextToolbars");
	ui->toggleContextBar->setChecked(contextVisible);
	ui->contextContainer->setVisible(contextVisible);
	if (contextVisible)
		UpdateContextBar(true);

	{
		ProfileScope("OBSBasic::Load");
		disableSaving--;
		Load(savePath);
		disableSaving++;
	}

	TimedCheckForUpdates();
	loaded = true;

	previewEnabled = config_get_bool(App()->GlobalConfig(), "BasicWindow",
					 "PreviewEnabled");

	if (!previewEnabled && !IsPreviewProgramMode())
		QMetaObject::invokeMethod(this, "EnablePreviewDisplay",
					  Qt::QueuedConnection,
					  Q_ARG(bool, previewEnabled));

#ifdef _WIN32
	uint32_t winVer = GetWindowsVersion();
	if (winVer > 0 && winVer < 0x602) {
		bool disableAero =
			config_get_bool(basicConfig, "Video", "DisableAero");
		SetAeroEnabled(!disableAero);
	}
#endif

	RefreshSceneCollections();
	RefreshProfiles();
	disableSaving--;

	auto addDisplay = [this](OBSQTDisplay *window) {
		obs_display_add_draw_callback(window->GetDisplay(),
					      OBSBasic::RenderMain, this);

		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi))
			ResizePreview(ovi.base_width, ovi.base_height);
	};

	connect(ui->preview, &OBSQTDisplay::DisplayCreated, addDisplay);

#ifdef _WIN32
	SetWin32DropStyle(this);
	show();
#endif

	bool alwaysOnTop = config_get_bool(App()->GlobalConfig(), "BasicWindow",
					   "AlwaysOnTop");

#ifdef ENABLE_WAYLAND
	bool isWayland = obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND;
#else
	bool isWayland = false;
#endif

	if (!isWayland && (alwaysOnTop || opt_always_on_top)) {
		SetAlwaysOnTop(this, true);
		ui->actionAlwaysOnTop->setChecked(true);
	} else if (isWayland) {
		if (opt_always_on_top)
			blog(LOG_INFO,
			     "Always On Top not available on Wayland, ignoring.");
		ui->actionAlwaysOnTop->setEnabled(false);
		ui->actionAlwaysOnTop->setVisible(false);
	}

#ifndef _WIN32
	show();
#endif

	/* setup stats dock */
	OBSBasicStats *statsDlg = new OBSBasicStats(statsDock, false);
	statsDock->setWidget(statsDlg);

	/* ----------------------------- */
	/* add custom browser docks      */

#ifdef BROWSER_AVAILABLE
	if (cef) {
		QAction *action = new QAction(QTStr("Basic.MainMenu."
						    "View.Docks."
						    "CustomBrowserDocks"),
					      this);
		ui->viewMenuDocks->insertAction(ui->toggleScenes, action);
		connect(action, &QAction::triggered, this,
			&OBSBasic::ManageExtraBrowserDocks);
		ui->viewMenuDocks->insertSeparator(ui->toggleScenes);

		LoadExtraBrowserDocks();
	}
#endif

	const char *dockStateStr = config_get_string(
		App()->GlobalConfig(), "BasicWindow", "DockState");

	if (!dockStateStr) {
		on_resetUI_triggered();
	} else {
		QByteArray dockState =
			QByteArray::fromBase64(QByteArray(dockStateStr));
		if (!restoreState(dockState))
			on_resetUI_triggered();
	}

	bool pre23Defaults = config_get_bool(App()->GlobalConfig(), "General",
					     "Pre23Defaults");
	if (pre23Defaults) {
		bool resetDockLock23 = config_get_bool(
			App()->GlobalConfig(), "General", "ResetDockLock23");
		if (!resetDockLock23) {
			config_set_bool(App()->GlobalConfig(), "General",
					"ResetDockLock23", true);
			config_remove_value(App()->GlobalConfig(),
					    "BasicWindow", "DocksLocked");
			config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		}
	}

	bool docksLocked = config_get_bool(App()->GlobalConfig(), "BasicWindow",
					   "DocksLocked");
	on_lockUI_toggled(docksLocked);
	ui->lockUI->blockSignals(true);
	ui->lockUI->setChecked(docksLocked);
	ui->lockUI->blockSignals(false);

#ifndef __APPLE__
	SystemTray(true);
#endif

#ifdef __APPLE__
	disableColorSpaceConversion(this);
#endif

	bool has_last_version = config_has_user_value(App()->GlobalConfig(),
						      "General", "LastVersion");
	bool first_run =
		config_get_bool(App()->GlobalConfig(), "General", "FirstRun");

	if (!first_run) {
		config_set_bool(App()->GlobalConfig(), "General", "FirstRun",
				true);
		config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	}

	if (!first_run && !has_last_version && !Active())
		QMetaObject::invokeMethod(this, "on_autoConfigure_triggered",
					  Qt::QueuedConnection);

	ToggleMixerLayout(config_get_bool(App()->GlobalConfig(), "BasicWindow",
					  "VerticalVolControl"));

	if (config_get_bool(basicConfig, "General", "OpenStatsOnStartup"))
		on_stats_triggered();

	OBSBasicStats::InitializeValues();

	/* ----------------------- */
	/* Add multiview menu      */

	ui->viewMenu->addSeparator();

	multiviewProjectorMenu = new QMenu(QTStr("MultiviewProjector"));
	ui->viewMenu->addMenu(multiviewProjectorMenu);
	AddProjectorMenuMonitors(multiviewProjectorMenu, this,
				 SLOT(OpenMultiviewProjector()));
	connect(ui->viewMenu->menuAction(), &QAction::hovered, this,
		&OBSBasic::UpdateMultiviewProjectorMenu);
	ui->viewMenu->addAction(QTStr("MultiviewWindowed"), this,
				SLOT(OpenMultiviewWindow()));

	ui->sources->UpdateIcons();

#if !defined(_WIN32)
	delete ui->actionShowCrashLogs;
	delete ui->actionUploadLastCrashLog;
	delete ui->menuCrashLogs;
	ui->actionShowCrashLogs = nullptr;
	ui->actionUploadLastCrashLog = nullptr;
	ui->menuCrashLogs = nullptr;
#if !defined(__APPLE__)
	delete ui->actionCheckForUpdates;
	ui->actionCheckForUpdates = nullptr;
#endif
#endif

#if defined(_WIN32) || defined(__APPLE__)
	if (App()->IsUpdaterDisabled())
		ui->actionCheckForUpdates->setEnabled(false);
#endif

	OnFirstLoad();

	activateWindow();

#ifdef __APPLE__
	QMetaObject::invokeMethod(this, "DeferredSysTrayLoad",
				  Qt::QueuedConnection, Q_ARG(int, 10));
#endif
}

void OBSBasic::OnFirstLoad()
{
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_FINISHED_LOADING);

#if defined(BROWSER_AVAILABLE) && defined(_WIN32)
	/* Attempt to load init screen if available */
	if (cef) {
		WhatsNewInfoThread *wnit = new WhatsNewInfoThread();
		connect(wnit, &WhatsNewInfoThread::Result, this,
			&OBSBasic::ReceivedIntroJson);

		introCheckThread.reset(wnit);
		introCheckThread->start();
	}
#endif

	Auth::Load();

	bool showLogViewerOnStartup = config_get_bool(
		App()->GlobalConfig(), "LogViewer", "ShowLogStartup");

	if (showLogViewerOnStartup) {
		if (!logView)
			logView = new OBSLogViewer();
		logView->show();
	}
}

void OBSBasic::DeferredSysTrayLoad(int requeueCount)
{
	if (--requeueCount > 0) {
		QMetaObject::invokeMethod(this, "DeferredSysTrayLoad",
					  Qt::QueuedConnection,
					  Q_ARG(int, requeueCount));
		return;
	}

	/* Minimizng to tray on initial startup does not work on mac
	 * unless it is done in the deferred load */
	SystemTray(true);
}

/* shows a "what's new" page on startup of new versions using CEF */
void OBSBasic::ReceivedIntroJson(const QString &text)
{
#ifdef BROWSER_AVAILABLE
#ifdef _WIN32
	if (closing)
		return;

	std::string err;
	Json json = Json::parse(QT_TO_UTF8(text), err);
	if (!err.empty())
		return;

	std::string info_url;
	int info_increment = -1;

	/* check to see if there's an info page for this version */
	const Json::array &items = json.array_items();
	for (const Json &item : items) {
		const std::string &version = item["version"].string_value();
		const std::string &url = item["url"].string_value();
		int increment = item["increment"].int_value();
		int rc = item["RC"].int_value();

		int major = 0;
		int minor = 0;

		sscanf(version.c_str(), "%d.%d", &major, &minor);
#if OBS_RELEASE_CANDIDATE > 0
		if (major == OBS_RELEASE_CANDIDATE_MAJOR &&
		    minor == OBS_RELEASE_CANDIDATE_MINOR &&
		    rc == OBS_RELEASE_CANDIDATE) {
#else
		if (major == LIBOBS_API_MAJOR_VER &&
		    minor == LIBOBS_API_MINOR_VER && rc == 0) {
#endif
			info_url = url;
			info_increment = increment;
		}
	}

	/* this version was not found, or no info for this version */
	if (info_increment == -1) {
		return;
	}

#if OBS_RELEASE_CANDIDATE > 0
	uint32_t lastVersion = config_get_int(App()->GlobalConfig(), "General",
					      "LastRCVersion");
#else
	uint32_t lastVersion =
		config_get_int(App()->GlobalConfig(), "General", "LastVersion");
#endif

	int current_version_increment = -1;

#if OBS_RELEASE_CANDIDATE > 0
	if (lastVersion < OBS_RELEASE_CANDIDATE_VER) {
#else
	if ((lastVersion & ~0xFFFF) < (LIBOBS_API_VER & ~0xFFFF)) {
#endif
		config_set_int(App()->GlobalConfig(), "General",
			       "InfoIncrement", -1);
	} else {
		current_version_increment = config_get_int(
			App()->GlobalConfig(), "General", "InfoIncrement");
	}

	if (info_increment <= current_version_increment) {
		return;
	}

	config_set_int(App()->GlobalConfig(), "General", "InfoIncrement",
		       info_increment);

	/* Don't show What's New dialog for new users */
#if !defined(OBS_RELEASE_CANDIDATE) || OBS_RELEASE_CANDIDATE == 0
	if (!lastVersion) {
		return;
	}
#endif
	cef->init_browser();

	WhatsNewBrowserInitThread *wnbit =
		new WhatsNewBrowserInitThread(QT_UTF8(info_url.c_str()));

	connect(wnbit, &WhatsNewBrowserInitThread::Result, this,
		&OBSBasic::ShowWhatsNew);

	whatsNewInitThread.reset(wnbit);
	whatsNewInitThread->start();

#else
	UNUSED_PARAMETER(text);
#endif
#else
	UNUSED_PARAMETER(text);
#endif
}

void OBSBasic::ShowWhatsNew(const QString &url)
{
#ifdef BROWSER_AVAILABLE
#ifdef _WIN32
	if (closing)
		return;

	std::string info_url = QT_TO_UTF8(url);

	QDialog *dlg = new QDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);
	dlg->setWindowTitle("What's New");
	dlg->resize(700, 600);

	Qt::WindowFlags flags = dlg->windowFlags();
	Qt::WindowFlags helpFlag = Qt::WindowContextHelpButtonHint;
	dlg->setWindowFlags(flags & (~helpFlag));

	QCefWidget *cefWidget = cef->create_widget(nullptr, info_url);
	if (!cefWidget) {
		return;
	}

	connect(cefWidget, SIGNAL(titleChanged(const QString &)), dlg,
		SLOT(setWindowTitle(const QString &)));

	QPushButton *close = new QPushButton(QTStr("Close"));
	connect(close, &QAbstractButton::clicked, dlg, &QDialog::accept);

	QHBoxLayout *bottomLayout = new QHBoxLayout();
	bottomLayout->addStretch();
	bottomLayout->addWidget(close);
	bottomLayout->addStretch();

	QVBoxLayout *topLayout = new QVBoxLayout(dlg);
	topLayout->addWidget(cefWidget);
	topLayout->addLayout(bottomLayout);

	dlg->show();
#else
	UNUSED_PARAMETER(url);
#endif
#else
	UNUSED_PARAMETER(url);
#endif
}

void OBSBasic::UpdateMultiviewProjectorMenu()
{
	multiviewProjectorMenu->clear();
	AddProjectorMenuMonitors(multiviewProjectorMenu, this,
				 SLOT(OpenMultiviewProjector()));
}

void OBSBasic::InitHotkeys()
{
	ProfileScope("OBSBasic::InitHotkeys");

	struct obs_hotkeys_translations t = {};
	t.insert = Str("Hotkeys.Insert");
	t.del = Str("Hotkeys.Delete");
	t.home = Str("Hotkeys.Home");
	t.end = Str("Hotkeys.End");
	t.page_up = Str("Hotkeys.PageUp");
	t.page_down = Str("Hotkeys.PageDown");
	t.num_lock = Str("Hotkeys.NumLock");
	t.scroll_lock = Str("Hotkeys.ScrollLock");
	t.caps_lock = Str("Hotkeys.CapsLock");
	t.backspace = Str("Hotkeys.Backspace");
	t.tab = Str("Hotkeys.Tab");
	t.print = Str("Hotkeys.Print");
	t.pause = Str("Hotkeys.Pause");
	t.left = Str("Hotkeys.Left");
	t.right = Str("Hotkeys.Right");
	t.up = Str("Hotkeys.Up");
	t.down = Str("Hotkeys.Down");
#ifdef _WIN32
	t.meta = Str("Hotkeys.Windows");
#else
	t.meta = Str("Hotkeys.Super");
#endif
	t.menu = Str("Hotkeys.Menu");
	t.space = Str("Hotkeys.Space");
	t.numpad_num = Str("Hotkeys.NumpadNum");
	t.numpad_multiply = Str("Hotkeys.NumpadMultiply");
	t.numpad_divide = Str("Hotkeys.NumpadDivide");
	t.numpad_plus = Str("Hotkeys.NumpadAdd");
	t.numpad_minus = Str("Hotkeys.NumpadSubtract");
	t.numpad_decimal = Str("Hotkeys.NumpadDecimal");
	t.apple_keypad_num = Str("Hotkeys.AppleKeypadNum");
	t.apple_keypad_multiply = Str("Hotkeys.AppleKeypadMultiply");
	t.apple_keypad_divide = Str("Hotkeys.AppleKeypadDivide");
	t.apple_keypad_plus = Str("Hotkeys.AppleKeypadAdd");
	t.apple_keypad_minus = Str("Hotkeys.AppleKeypadSubtract");
	t.apple_keypad_decimal = Str("Hotkeys.AppleKeypadDecimal");
	t.apple_keypad_equal = Str("Hotkeys.AppleKeypadEqual");
	t.mouse_num = Str("Hotkeys.MouseButton");
	t.escape = Str("Hotkeys.Escape");
	obs_hotkeys_set_translations(&t);

	obs_hotkeys_set_audio_hotkeys_translations(Str("Mute"), Str("Unmute"),
						   Str("Push-to-mute"),
						   Str("Push-to-talk"));

	obs_hotkeys_set_sceneitem_hotkeys_translations(Str("SceneItemShow"),
						       Str("SceneItemHide"));

	obs_hotkey_enable_callback_rerouting(true);
	obs_hotkey_set_callback_routing_func(OBSBasic::HotkeyTriggered, this);
}

void OBSBasic::ProcessHotkey(obs_hotkey_id id, bool pressed)
{
	obs_hotkey_trigger_routed_callback(id, pressed);
}

void OBSBasic::HotkeyTriggered(void *data, obs_hotkey_id id, bool pressed)
{
	OBSBasic &basic = *static_cast<OBSBasic *>(data);
	QMetaObject::invokeMethod(&basic, "ProcessHotkey",
				  Q_ARG(obs_hotkey_id, id),
				  Q_ARG(bool, pressed));
}

void OBSBasic::CreateHotkeys()
{
	ProfileScope("OBSBasic::CreateHotkeys");

	auto LoadHotkeyData = [&](const char *name) -> OBSData {
		const char *info =
			config_get_string(basicConfig, "Hotkeys", name);
		if (!info)
			return {};

		obs_data_t *data = obs_data_create_from_json(info);
		if (!data)
			return {};

		OBSData res = data;
		obs_data_release(data);
		return res;
	};

	auto LoadHotkey = [&](obs_hotkey_id id, const char *name) {
		obs_data_array_t *array =
			obs_data_get_array(LoadHotkeyData(name), "bindings");

		obs_hotkey_load(id, array);
		obs_data_array_release(array);
	};

	auto LoadHotkeyPair = [&](obs_hotkey_pair_id id, const char *name0,
				  const char *name1) {
		obs_data_array_t *array0 =
			obs_data_get_array(LoadHotkeyData(name0), "bindings");
		obs_data_array_t *array1 =
			obs_data_get_array(LoadHotkeyData(name1), "bindings");

		obs_hotkey_pair_load(id, array0, array1);
		obs_data_array_release(array0);
		obs_data_array_release(array1);
	};

#define MAKE_CALLBACK(pred, method, log_action)                            \
	[](void *data, obs_hotkey_pair_id, obs_hotkey_t *, bool pressed) { \
		OBSBasic &basic = *static_cast<OBSBasic *>(data);          \
		if ((pred) && pressed) {                                   \
			blog(LOG_INFO, log_action " due to hotkey");       \
			method();                                          \
			return true;                                       \
		}                                                          \
		return false;                                              \
	}

	streamingHotkeys = obs_hotkey_pair_register_frontend(
		"OBSBasic.StartStreaming", Str("Basic.Main.StartStreaming"),
		"OBSBasic.StopStreaming", Str("Basic.Main.StopStreaming"),
		MAKE_CALLBACK(!basic.outputHandler->StreamingActive() &&
				      basic.ui->streamButton->isEnabled(),
			      basic.StartStreaming, "Starting stream"),
		MAKE_CALLBACK(basic.outputHandler->StreamingActive() &&
				      basic.ui->streamButton->isEnabled(),
			      basic.StopStreaming, "Stopping stream"),
		this, this);
	LoadHotkeyPair(streamingHotkeys, "OBSBasic.StartStreaming",
		       "OBSBasic.StopStreaming");

	auto cb = [](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
		OBSBasic &basic = *static_cast<OBSBasic *>(data);
		if (basic.outputHandler->StreamingActive() && pressed) {
			basic.ForceStopStreaming();
		}
	};

	forceStreamingStopHotkey = obs_hotkey_register_frontend(
		"OBSBasic.ForceStopStreaming",
		Str("Basic.Main.ForceStopStreaming"), cb, this);
	LoadHotkey(forceStreamingStopHotkey, "OBSBasic.ForceStopStreaming");

	recordingHotkeys = obs_hotkey_pair_register_frontend(
		"OBSBasic.StartRecording", Str("Basic.Main.StartRecording"),
		"OBSBasic.StopRecording", Str("Basic.Main.StopRecording"),
		MAKE_CALLBACK(!basic.outputHandler->RecordingActive() &&
				      !basic.ui->recordButton->isChecked(),
			      basic.StartRecording, "Starting recording"),
		MAKE_CALLBACK(basic.outputHandler->RecordingActive() &&
				      basic.ui->recordButton->isChecked(),
			      basic.StopRecording, "Stopping recording"),
		this, this);
	LoadHotkeyPair(recordingHotkeys, "OBSBasic.StartRecording",
		       "OBSBasic.StopRecording");

	pauseHotkeys = obs_hotkey_pair_register_frontend(
		"OBSBasic.PauseRecording", Str("Basic.Main.PauseRecording"),
		"OBSBasic.UnpauseRecording", Str("Basic.Main.UnpauseRecording"),
		MAKE_CALLBACK(basic.pause && !basic.pause->isChecked(),
			      basic.PauseRecording, "Pausing recording"),
		MAKE_CALLBACK(basic.pause && basic.pause->isChecked(),
			      basic.UnpauseRecording, "Unpausing recording"),
		this, this);
	LoadHotkeyPair(pauseHotkeys, "OBSBasic.PauseRecording",
		       "OBSBasic.UnpauseRecording");

	replayBufHotkeys = obs_hotkey_pair_register_frontend(
		"OBSBasic.StartReplayBuffer",
		Str("Basic.Main.StartReplayBuffer"),
		"OBSBasic.StopReplayBuffer", Str("Basic.Main.StopReplayBuffer"),
		MAKE_CALLBACK(!basic.outputHandler->ReplayBufferActive(),
			      basic.StartReplayBuffer,
			      "Starting replay buffer"),
		MAKE_CALLBACK(basic.outputHandler->ReplayBufferActive(),
			      basic.StopReplayBuffer, "Stopping replay buffer"),
		this, this);
	LoadHotkeyPair(replayBufHotkeys, "OBSBasic.StartReplayBuffer",
		       "OBSBasic.StopReplayBuffer");

	if (vcamEnabled) {
		vcamHotkeys = obs_hotkey_pair_register_frontend(
			"OBSBasic.StartVirtualCam",
			Str("Basic.Main.StartVirtualCam"),
			"OBSBasic.StopVirtualCam",
			Str("Basic.Main.StopVirtualCam"),
			MAKE_CALLBACK(!basic.outputHandler->VirtualCamActive(),
				      basic.StartVirtualCam,
				      "Starting virtual camera"),
			MAKE_CALLBACK(basic.outputHandler->VirtualCamActive(),
				      basic.StopVirtualCam,
				      "Stopping virtual camera"),
			this, this);
		LoadHotkeyPair(vcamHotkeys, "OBSBasic.StartVirtualCam",
			       "OBSBasic.StopVirtualCam");
	}

	togglePreviewHotkeys = obs_hotkey_pair_register_frontend(
		"OBSBasic.EnablePreview",
		Str("Basic.Main.PreviewConextMenu.Enable"),
		"OBSBasic.DisablePreview", Str("Basic.Main.Preview.Disable"),
		MAKE_CALLBACK(!basic.previewEnabled, basic.EnablePreview,
			      "Enabling preview"),
		MAKE_CALLBACK(basic.previewEnabled, basic.DisablePreview,
			      "Disabling preview"),
		this, this);
	LoadHotkeyPair(togglePreviewHotkeys, "OBSBasic.EnablePreview",
		       "OBSBasic.DisablePreview");

	contextBarHotkeys = obs_hotkey_pair_register_frontend(
		"OBSBasic.ShowContextBar", Str("Basic.Main.ShowContextBar"),
		"OBSBasic.HideContextBar", Str("Basic.Main.HideContextBar"),
		MAKE_CALLBACK(!basic.ui->contextContainer->isVisible(),
			      basic.ShowContextBar, "Showing Context Bar"),
		MAKE_CALLBACK(basic.ui->contextContainer->isVisible(),
			      basic.HideContextBar, "Hiding Context Bar"),
		this, this);
	LoadHotkeyPair(contextBarHotkeys, "OBSBasic.ShowContextBar",
		       "OBSBasic.HideContextBar");
#undef MAKE_CALLBACK

	auto togglePreviewProgram = [](void *data, obs_hotkey_id,
				       obs_hotkey_t *, bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
						  "on_modeSwitch_clicked",
						  Qt::QueuedConnection);
	};

	togglePreviewProgramHotkey = obs_hotkey_register_frontend(
		"OBSBasic.TogglePreviewProgram",
		Str("Basic.TogglePreviewProgramMode"), togglePreviewProgram,
		this);
	LoadHotkey(togglePreviewProgramHotkey, "OBSBasic.TogglePreviewProgram");

	auto transition = [](void *data, obs_hotkey_id, obs_hotkey_t *,
			     bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
						  "TransitionClicked",
						  Qt::QueuedConnection);
	};

	transitionHotkey = obs_hotkey_register_frontend(
		"OBSBasic.Transition", Str("Transition"), transition, this);
	LoadHotkey(transitionHotkey, "OBSBasic.Transition");

	auto resetStats = [](void *data, obs_hotkey_id, obs_hotkey_t *,
			     bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
						  "ResetStatsHotkey",
						  Qt::QueuedConnection);
	};

	statsHotkey = obs_hotkey_register_frontend(
		"OBSBasic.ResetStats", Str("Basic.Stats.ResetStats"),
		resetStats, this);
	LoadHotkey(statsHotkey, "OBSBasic.ResetStats");

	auto screenshot = [](void *data, obs_hotkey_id, obs_hotkey_t *,
			     bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
						  "Screenshot",
						  Qt::QueuedConnection);
	};

	screenshotHotkey = obs_hotkey_register_frontend(
		"OBSBasic.Screenshot", Str("Screenshot"), screenshot, this);
	LoadHotkey(screenshotHotkey, "OBSBasic.Screenshot");

	auto screenshotSource = [](void *data, obs_hotkey_id, obs_hotkey_t *,
				   bool pressed) {
		if (pressed)
			QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
						  "ScreenshotSelectedSource",
						  Qt::QueuedConnection);
	};

	sourceScreenshotHotkey = obs_hotkey_register_frontend(
		"OBSBasic.SelectedSourceScreenshot",
		Str("Screenshot.SourceHotkey"), screenshotSource, this);
	LoadHotkey(sourceScreenshotHotkey, "OBSBasic.SelectedSourceScreenshot");
}

void OBSBasic::ClearHotkeys()
{
	obs_hotkey_pair_unregister(streamingHotkeys);
	obs_hotkey_pair_unregister(recordingHotkeys);
	obs_hotkey_pair_unregister(pauseHotkeys);
	obs_hotkey_pair_unregister(replayBufHotkeys);
	obs_hotkey_pair_unregister(vcamHotkeys);
	obs_hotkey_pair_unregister(togglePreviewHotkeys);
	obs_hotkey_pair_unregister(contextBarHotkeys);
	obs_hotkey_unregister(forceStreamingStopHotkey);
	obs_hotkey_unregister(togglePreviewProgramHotkey);
	obs_hotkey_unregister(transitionHotkey);
	obs_hotkey_unregister(statsHotkey);
	obs_hotkey_unregister(screenshotHotkey);
	obs_hotkey_unregister(sourceScreenshotHotkey);
}

OBSBasic::~OBSBasic()
{
	/* clear out UI event queue */
	QApplication::sendPostedEvents(App());

	if (updateCheckThread && updateCheckThread->isRunning())
		updateCheckThread->wait();

	delete screenshotData;
	delete logView;
	delete multiviewProjectorMenu;
	delete previewProjector;
	delete studioProgramProjector;
	delete previewProjectorSource;
	delete previewProjectorMain;
	delete sourceProjector;
	delete sceneProjectorMenu;
	delete scaleFilteringMenu;
	delete colorMenu;
	delete colorWidgetAction;
	delete colorSelect;
	delete deinterlaceMenu;
	delete perSceneTransitionMenu;
	delete shortcutFilter;
	delete trayMenu;
	delete programOptions;
	delete program;

	/* XXX: any obs data must be released before calling obs_shutdown.
	 * currently, we can't automate this with C++ RAII because of the
	 * delicate nature of obs_shutdown needing to be freed before the UI
	 * can be freed, and we have no control over the destruction order of
	 * the Qt UI stuff, so we have to manually clear any references to
	 * libobs. */
	delete cpuUsageTimer;
	os_cpu_usage_info_destroy(cpuUsageInfo);

	obs_hotkey_set_callback_routing_func(nullptr, nullptr);
	ClearHotkeys();

	service = nullptr;
	outputHandler.reset();

	if (interaction)
		delete interaction;

	if (properties)
		delete properties;

	if (filters)
		delete filters;

	if (transformWindow)
		delete transformWindow;

	if (advAudioWindow)
		delete advAudioWindow;

	if (about)
		delete about;

	obs_display_remove_draw_callback(ui->preview->GetDisplay(),
					 OBSBasic::RenderMain, this);

	obs_enter_graphics();
	gs_vertexbuffer_destroy(box);
	gs_vertexbuffer_destroy(boxLeft);
	gs_vertexbuffer_destroy(boxTop);
	gs_vertexbuffer_destroy(boxRight);
	gs_vertexbuffer_destroy(boxBottom);
	gs_vertexbuffer_destroy(circle);
	obs_leave_graphics();

	/* When shutting down, sometimes source references can get in to the
	 * event queue, and if we don't forcibly process those events they
	 * won't get processed until after obs_shutdown has been called.  I
	 * really wish there were a more elegant way to deal with this via C++,
	 * but Qt doesn't use C++ in a normal way, so you can't really rely on
	 * normal C++ behavior for your data to be freed in the order that you
	 * expect or want it to. */
	QApplication::sendPostedEvents(this);

	config_set_int(App()->GlobalConfig(), "General", "LastVersion",
		       LIBOBS_API_VER);
#if OBS_RELEASE_CANDIDATE > 0
	config_set_int(App()->GlobalConfig(), "General", "LastRCVersion",
		       OBS_RELEASE_CANDIDATE_VER);
#endif

	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);

#ifdef _WIN32
	uint32_t winVer = GetWindowsVersion();
	if (winVer > 0 && winVer < 0x602) {
		bool disableAero =
			config_get_bool(basicConfig, "Video", "DisableAero");
		if (disableAero) {
			SetAeroEnabled(true);
		}
	}
#endif

#ifdef BROWSER_AVAILABLE
	DestroyPanelCookieManager();
	delete cef;
	cef = nullptr;
#endif
}

void OBSBasic::SaveProjectNow()
{
	if (disableSaving)
		return;

	projectChanged = true;
	SaveProjectDeferred();
}

void OBSBasic::SaveProject()
{
	if (disableSaving)
		return;

	projectChanged = true;
	QMetaObject::invokeMethod(this, "SaveProjectDeferred",
				  Qt::QueuedConnection);
}

void OBSBasic::SaveProjectDeferred()
{
	if (disableSaving)
		return;

	if (!projectChanged)
		return;

	projectChanged = false;

	const char *sceneCollection = config_get_string(
		App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	char savePath[512];
	char fileName[512];
	int ret;

	if (!sceneCollection)
		return;

	ret = snprintf(fileName, 512, "obs-studio/basic/scenes/%s.json",
		       sceneCollection);
	if (ret <= 0)
		return;

	ret = GetConfigPath(savePath, sizeof(savePath), fileName);
	if (ret <= 0)
		return;

	Save(savePath);
}

OBSSource OBSBasic::GetProgramSource()
{
	return OBSGetStrongRef(programScene);
}

OBSScene OBSBasic::GetCurrentScene()
{
	QListWidgetItem *item = ui->scenes->currentItem();
	return item ? GetOBSRef<OBSScene>(item) : nullptr;
}

OBSSceneItem OBSBasic::GetSceneItem(QListWidgetItem *item)
{
	return item ? GetOBSRef<OBSSceneItem>(item) : nullptr;
}

OBSSceneItem OBSBasic::GetCurrentSceneItem()
{
	return ui->sources->Get(GetTopSelectedSourceItem());
}

void OBSBasic::UpdatePreviewScalingMenu()
{
	bool fixedScaling = ui->preview->IsFixedScaling();
	float scalingAmount = ui->preview->GetScalingAmount();
	if (!fixedScaling) {
		ui->actionScaleWindow->setChecked(true);
		ui->actionScaleCanvas->setChecked(false);
		ui->actionScaleOutput->setChecked(false);
		return;
	}

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	ui->actionScaleWindow->setChecked(false);
	ui->actionScaleCanvas->setChecked(scalingAmount == 1.0f);
	ui->actionScaleOutput->setChecked(scalingAmount ==
					  float(ovi.output_width) /
						  float(ovi.base_width));
}

void OBSBasic::CreateInteractionWindow(obs_source_t *source)
{
	bool closed = true;
	if (interaction)
		closed = interaction->close();

	if (!closed)
		return;

	interaction = new OBSBasicInteraction(this, source);
	interaction->Init();
	interaction->setAttribute(Qt::WA_DeleteOnClose, true);
}

void OBSBasic::CreatePropertiesWindow(obs_source_t *source)
{
	bool closed = true;
	if (properties)
		closed = properties->close();

	if (!closed)
		return;

	properties = new OBSBasicProperties(this, source);
	properties->Init();
	properties->setAttribute(Qt::WA_DeleteOnClose, true);
}

void OBSBasic::CreateFiltersWindow(obs_source_t *source)
{
	bool closed = true;
	if (filters)
		closed = filters->close();

	if (!closed)
		return;

	filters = new OBSBasicFilters(this, source);
	filters->Init();
	filters->setAttribute(Qt::WA_DeleteOnClose, true);
}

/* Qt callbacks for invokeMethod */

void OBSBasic::AddScene(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	obs_scene_t *scene = obs_scene_from_source(source);

	QListWidgetItem *item = new QListWidgetItem(QT_UTF8(name));
	SetOBSRef(item, OBSScene(scene));
	ui->scenes->addItem(item);

	obs_hotkey_register_source(
		source, "OBSBasic.SelectScene",
		Str("Basic.Hotkeys.SelectScene"),
		[](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
			OBSBasic *main = reinterpret_cast<OBSBasic *>(
				App()->GetMainWindow());

			auto potential_source =
				static_cast<obs_source_t *>(data);
			auto source = obs_source_get_ref(potential_source);
			if (source && pressed)
				main->SetCurrentScene(source);
			obs_source_release(source);
		},
		static_cast<obs_source_t *>(source));

	signal_handler_t *handler = obs_source_get_signal_handler(source);

	SignalContainer<OBSScene> container;
	container.ref = scene;
	container.handlers.assign({
		std::make_shared<OBSSignal>(handler, "item_add",
					    OBSBasic::SceneItemAdded, this),
		std::make_shared<OBSSignal>(handler, "reorder",
					    OBSBasic::SceneReordered, this),
		std::make_shared<OBSSignal>(handler, "refresh",
					    OBSBasic::SceneRefreshed, this),
	});

	item->setData(static_cast<int>(QtDataRole::OBSSignals),
		      QVariant::fromValue(container));

	/* if the scene already has items (a duplicated scene) add them */
	auto addSceneItem = [this](obs_sceneitem_t *item) {
		AddSceneItem(item);
	};

	using addSceneItem_t = decltype(addSceneItem);

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			addSceneItem_t *func;
			func = reinterpret_cast<addSceneItem_t *>(param);
			(*func)(item);
			return true;
		},
		&addSceneItem);

	SaveProject();

	if (!disableSaving) {
		obs_source_t *source = obs_scene_get_source(scene);
		blog(LOG_INFO, "User added scene '%s'",
		     obs_source_get_name(source));

		OBSProjector::UpdateMultiviewProjectors();
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
}

void OBSBasic::RemoveScene(OBSSource source)
{
	obs_scene_t *scene = obs_scene_from_source(source);

	QListWidgetItem *sel = nullptr;
	int count = ui->scenes->count();

	for (int i = 0; i < count; i++) {
		auto item = ui->scenes->item(i);
		auto cur_scene = GetOBSRef<OBSScene>(item);
		if (cur_scene != scene)
			continue;

		sel = item;
		break;
	}

	if (sel != nullptr) {
		if (sel == ui->scenes->currentItem())
			ui->sources->Clear();
		delete sel;
	}

	SaveProject();

	if (!disableSaving) {
		blog(LOG_INFO, "User Removed scene '%s'",
		     obs_source_get_name(source));

		OBSProjector::UpdateMultiviewProjectors();
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
}

static bool select_one(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_sceneitem_t *selectedItem =
		reinterpret_cast<obs_sceneitem_t *>(param);
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, select_one, param);

	obs_sceneitem_select(item, (selectedItem == item));

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasic::AddSceneItem(OBSSceneItem item)
{
	obs_scene_t *scene = obs_sceneitem_get_scene(item);

	if (GetCurrentScene() == scene)
		ui->sources->Add(item);

	SaveProject();

	if (!disableSaving) {
		obs_source_t *sceneSource = obs_scene_get_source(scene);
		obs_source_t *itemSource = obs_sceneitem_get_source(item);
		blog(LOG_INFO, "User added source '%s' (%s) to scene '%s'",
		     obs_source_get_name(itemSource),
		     obs_source_get_id(itemSource),
		     obs_source_get_name(sceneSource));

		obs_scene_enum_items(scene, select_one,
				     (obs_sceneitem_t *)item);
	}
}

static void RenameListValues(QListWidget *listWidget, const QString &newName,
			     const QString &prevName)
{
	QList<QListWidgetItem *> items =
		listWidget->findItems(prevName, Qt::MatchExactly);

	for (int i = 0; i < items.count(); i++)
		items[i]->setText(newName);
}

void OBSBasic::RenameSources(OBSSource source, QString newName,
			     QString prevName)
{
	RenameListValues(ui->scenes, newName, prevName);

	for (size_t i = 0; i < volumes.size(); i++) {
		if (volumes[i]->GetName().compare(prevName) == 0)
			volumes[i]->SetName(newName);
	}

	for (size_t i = 0; i < projectors.size(); i++) {
		if (projectors[i]->GetSource() == source)
			projectors[i]->RenameProjector(prevName, newName);
	}

	SaveProject();

	obs_scene_t *scene = obs_scene_from_source(source);
	if (scene)
		OBSProjector::UpdateMultiviewProjectors();

	UpdateContextBar();
}

void OBSBasic::ClearContextBar()
{
	QLayoutItem *la = ui->emptySpace->layout()->itemAt(0);
	if (la) {
		delete la->widget();
		ui->emptySpace->layout()->removeItem(la);
	}
}

static bool is_network_media_source(obs_source_t *source, const char *id)
{
	if (strcmp(id, "ffmpeg_source") != 0)
		return false;

	obs_data_t *s = obs_source_get_settings(source);
	bool is_local_file = obs_data_get_bool(s, "is_local_file");
	obs_data_release(s);

	return !is_local_file;
}

void OBSBasic::UpdateContextBarDeferred(bool force)
{
	QMetaObject::invokeMethod(this, "UpdateContextBar",
				  Qt::QueuedConnection, Q_ARG(bool, force));
}

void OBSBasic::UpdateContextBar(bool force)
{
	if (!ui->contextContainer->isVisible() && !force)
		return;

	OBSSceneItem item = GetCurrentSceneItem();

	ClearContextBar();

	if (item) {
		obs_source_t *source = obs_sceneitem_get_source(item);
		const char *id = obs_source_get_unversioned_id(source);
		uint32_t flags = obs_source_get_output_flags(source);

		ui->sourceInteractButton->setVisible(flags &
						     OBS_SOURCE_INTERACTION);

		if (flags & OBS_SOURCE_CONTROLLABLE_MEDIA) {
			if (!is_network_media_source(source, id)) {
				MediaControls *mediaControls =
					new MediaControls(ui->emptySpace);
				mediaControls->SetSource(source);

				ui->emptySpace->layout()->addWidget(
					mediaControls);
			}
		} else if (strcmp(id, "browser_source") == 0) {
			BrowserToolbar *c =
				new BrowserToolbar(ui->emptySpace, source);
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "wasapi_input_capture") == 0 ||
			   strcmp(id, "wasapi_output_capture") == 0 ||
			   strcmp(id, "coreaudio_input_capture") == 0 ||
			   strcmp(id, "coreaudio_output_capture") == 0 ||
			   strcmp(id, "pulse_input_capture") == 0 ||
			   strcmp(id, "pulse_output_capture") == 0 ||
			   strcmp(id, "alsa_input_capture") == 0) {
			AudioCaptureToolbar *c =
				new AudioCaptureToolbar(ui->emptySpace, source);
			c->Init();
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "window_capture") == 0 ||
			   strcmp(id, "xcomposite_input") == 0) {
			WindowCaptureToolbar *c = new WindowCaptureToolbar(
				ui->emptySpace, source);
			c->Init();
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "monitor_capture") == 0 ||
			   strcmp(id, "display_capture") == 0 ||
			   strcmp(id, "xshm_input") == 0) {
			DisplayCaptureToolbar *c = new DisplayCaptureToolbar(
				ui->emptySpace, source);
			c->Init();
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "dshow_input") == 0) {
			DeviceCaptureToolbar *c = new DeviceCaptureToolbar(
				ui->emptySpace, source);
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "game_capture") == 0) {
			GameCaptureToolbar *c =
				new GameCaptureToolbar(ui->emptySpace, source);
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "image_source") == 0) {
			ImageSourceToolbar *c =
				new ImageSourceToolbar(ui->emptySpace, source);
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "color_source") == 0) {
			ColorSourceToolbar *c =
				new ColorSourceToolbar(ui->emptySpace, source);
			ui->emptySpace->layout()->addWidget(c);

		} else if (strcmp(id, "text_ft2_source") == 0 ||
			   strcmp(id, "text_gdiplus") == 0) {
			TextSourceToolbar *c =
				new TextSourceToolbar(ui->emptySpace, source);
			ui->emptySpace->layout()->addWidget(c);
		}

		const char *name = obs_source_get_name(source);
		ui->contextSourceLabel->setText(name);

		ui->sourceFiltersButton->setEnabled(true);
		ui->sourcePropertiesButton->setEnabled(
			obs_source_configurable(source));
	} else {
		ui->contextSourceLabel->setText(
			QTStr("ContextBar.NoSelectedSource"));

		ui->sourceFiltersButton->setEnabled(false);
		ui->sourcePropertiesButton->setEnabled(false);
		ui->sourceInteractButton->setVisible(false);
	}
}

static inline bool SourceMixerHidden(obs_source_t *source)
{
	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	bool hidden = obs_data_get_bool(priv_settings, "mixer_hidden");
	obs_data_release(priv_settings);

	return hidden;
}

static inline void SetSourceMixerHidden(obs_source_t *source, bool hidden)
{
	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	obs_data_set_bool(priv_settings, "mixer_hidden", hidden);
	obs_data_release(priv_settings);
}

void OBSBasic::GetAudioSourceFilters()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	CreateFiltersWindow(source);
}

void OBSBasic::GetAudioSourceProperties()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	CreatePropertiesWindow(source);
}

void OBSBasic::HideAudioControl()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	if (!SourceMixerHidden(source)) {
		SetSourceMixerHidden(source, true);
		DeactivateAudioSource(source);
	}
}

void OBSBasic::UnhideAllAudioControls()
{
	auto UnhideAudioMixer = [this](obs_source_t *source) /* -- */
	{
		if (!obs_source_active(source))
			return true;
		if (!SourceMixerHidden(source))
			return true;

		SetSourceMixerHidden(source, false);
		ActivateAudioSource(source);
		return true;
	};

	using UnhideAudioMixer_t = decltype(UnhideAudioMixer);

	auto PreEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{ return (*reinterpret_cast<UnhideAudioMixer_t *>(data))(source); };

	obs_enum_sources(PreEnum, &UnhideAudioMixer);
}

void OBSBasic::ToggleHideMixer()
{
	OBSSceneItem item = GetCurrentSceneItem();
	OBSSource source = obs_sceneitem_get_source(item);

	if (!SourceMixerHidden(source)) {
		SetSourceMixerHidden(source, true);
		DeactivateAudioSource(source);
	} else {
		SetSourceMixerHidden(source, false);
		ActivateAudioSource(source);
	}
}

void OBSBasic::MixerRenameSource()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	OBSSource source = vol->GetSource();

	const char *prevName = obs_source_get_name(source);

	for (;;) {
		string name;
		bool accepted = NameDialog::AskForName(
			this, QTStr("Basic.Main.MixerRename.Title"),
			QTStr("Basic.Main.MixerRename.Text"), name,
			QT_UTF8(prevName));
		if (!accepted)
			return;

		if (name.empty()) {
			OBSMessageBox::warning(this,
					       QTStr("NoNameEntered.Title"),
					       QTStr("NoNameEntered.Text"));
			continue;
		}

		OBSSource sourceTest = obs_get_source_by_name(name.c_str());
		obs_source_release(sourceTest);

		if (sourceTest) {
			OBSMessageBox::warning(this, QTStr("NameExists.Title"),
					       QTStr("NameExists.Text"));
			continue;
		}

		obs_source_set_name(source, name.c_str());
		break;
	}
}

static inline bool SourceVolumeLocked(obs_source_t *source)
{
	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	bool lock = obs_data_get_bool(priv_settings, "volume_locked");
	obs_data_release(priv_settings);

	return lock;
}

void OBSBasic::LockVolumeControl(bool lock)
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	obs_data_t *priv_settings = obs_source_get_private_settings(source);
	obs_data_set_bool(priv_settings, "volume_locked", lock);
	obs_data_release(priv_settings);

	vol->EnableSlider(!lock);
}

void OBSBasic::VolControlContextMenu()
{
	VolControl *vol = reinterpret_cast<VolControl *>(sender());

	/* ------------------- */

	QAction lockAction(QTStr("LockVolume"), this);
	lockAction.setCheckable(true);
	lockAction.setChecked(SourceVolumeLocked(vol->GetSource()));

	QAction hideAction(QTStr("Hide"), this);
	QAction unhideAllAction(QTStr("UnhideAll"), this);
	QAction mixerRenameAction(QTStr("Rename"), this);

	QAction copyFiltersAction(QTStr("Copy.Filters"), this);
	QAction pasteFiltersAction(QTStr("Paste.Filters"), this);

	QAction filtersAction(QTStr("Filters"), this);
	QAction propertiesAction(QTStr("Properties"), this);
	QAction advPropAction(QTStr("Basic.MainMenu.Edit.AdvAudio"), this);

	QAction toggleControlLayoutAction(QTStr("VerticalLayout"), this);
	toggleControlLayoutAction.setCheckable(true);
	toggleControlLayoutAction.setChecked(config_get_bool(
		GetGlobalConfig(), "BasicWindow", "VerticalVolControl"));

	/* ------------------- */

	connect(&hideAction, &QAction::triggered, this,
		&OBSBasic::HideAudioControl, Qt::DirectConnection);
	connect(&unhideAllAction, &QAction::triggered, this,
		&OBSBasic::UnhideAllAudioControls, Qt::DirectConnection);
	connect(&lockAction, &QAction::toggled, this,
		&OBSBasic::LockVolumeControl, Qt::DirectConnection);
	connect(&mixerRenameAction, &QAction::triggered, this,
		&OBSBasic::MixerRenameSource, Qt::DirectConnection);

	connect(&copyFiltersAction, &QAction::triggered, this,
		&OBSBasic::AudioMixerCopyFilters, Qt::DirectConnection);
	connect(&pasteFiltersAction, &QAction::triggered, this,
		&OBSBasic::AudioMixerPasteFilters, Qt::DirectConnection);

	connect(&filtersAction, &QAction::triggered, this,
		&OBSBasic::GetAudioSourceFilters, Qt::DirectConnection);
	connect(&propertiesAction, &QAction::triggered, this,
		&OBSBasic::GetAudioSourceProperties, Qt::DirectConnection);
	connect(&advPropAction, &QAction::triggered, this,
		&OBSBasic::on_actionAdvAudioProperties_triggered,
		Qt::DirectConnection);

	/* ------------------- */

	connect(&toggleControlLayoutAction, &QAction::changed, this,
		&OBSBasic::ToggleVolControlLayout, Qt::DirectConnection);

	/* ------------------- */

	hideAction.setProperty("volControl",
			       QVariant::fromValue<VolControl *>(vol));
	lockAction.setProperty("volControl",
			       QVariant::fromValue<VolControl *>(vol));
	mixerRenameAction.setProperty("volControl",
				      QVariant::fromValue<VolControl *>(vol));

	copyFiltersAction.setProperty("volControl",
				      QVariant::fromValue<VolControl *>(vol));
	pasteFiltersAction.setProperty("volControl",
				       QVariant::fromValue<VolControl *>(vol));

	filtersAction.setProperty("volControl",
				  QVariant::fromValue<VolControl *>(vol));
	propertiesAction.setProperty("volControl",
				     QVariant::fromValue<VolControl *>(vol));

	/* ------------------- */

	copyFiltersAction.setEnabled(obs_source_filter_count(vol->GetSource()) >
				     0);

	if (copyFiltersString == nullptr)
		pasteFiltersAction.setEnabled(false);
	else
		pasteFiltersAction.setEnabled(true);

	QMenu popup;
	vol->SetContextMenu(&popup);
	popup.addAction(&lockAction);
	popup.addSeparator();
	popup.addAction(&unhideAllAction);
	popup.addAction(&hideAction);
	popup.addAction(&mixerRenameAction);
	popup.addSeparator();
	popup.addAction(&copyFiltersAction);
	popup.addAction(&pasteFiltersAction);
	popup.addSeparator();
	popup.addAction(&toggleControlLayoutAction);
	popup.addSeparator();
	popup.addAction(&filtersAction);
	popup.addAction(&propertiesAction);
	popup.addAction(&advPropAction);
	popup.exec(QCursor::pos());
	vol->SetContextMenu(nullptr);
}

void OBSBasic::on_hMixerScrollArea_customContextMenuRequested()
{
	StackedMixerAreaContextMenuRequested();
}

void OBSBasic::on_vMixerScrollArea_customContextMenuRequested()
{
	StackedMixerAreaContextMenuRequested();
}

void OBSBasic::StackedMixerAreaContextMenuRequested()
{
	QAction unhideAllAction(QTStr("UnhideAll"), this);

	QAction advPropAction(QTStr("Basic.MainMenu.Edit.AdvAudio"), this);

	QAction toggleControlLayoutAction(QTStr("VerticalLayout"), this);
	toggleControlLayoutAction.setCheckable(true);
	toggleControlLayoutAction.setChecked(config_get_bool(
		GetGlobalConfig(), "BasicWindow", "VerticalVolControl"));

	/* ------------------- */

	connect(&unhideAllAction, &QAction::triggered, this,
		&OBSBasic::UnhideAllAudioControls, Qt::DirectConnection);

	connect(&advPropAction, &QAction::triggered, this,
		&OBSBasic::on_actionAdvAudioProperties_triggered,
		Qt::DirectConnection);

	/* ------------------- */

	connect(&toggleControlLayoutAction, &QAction::changed, this,
		&OBSBasic::ToggleVolControlLayout, Qt::DirectConnection);

	/* ------------------- */

	QMenu popup;
	popup.addAction(&unhideAllAction);
	popup.addSeparator();
	popup.addAction(&toggleControlLayoutAction);
	popup.addSeparator();
	popup.addAction(&advPropAction);
	popup.exec(QCursor::pos());
}

void OBSBasic::ToggleMixerLayout(bool vertical)
{
	if (vertical) {
		ui->stackedMixerArea->setMinimumSize(180, 220);
		ui->stackedMixerArea->setCurrentIndex(1);
	} else {
		ui->stackedMixerArea->setMinimumSize(220, 0);
		ui->stackedMixerArea->setCurrentIndex(0);
	}
}

void OBSBasic::ToggleVolControlLayout()
{
	bool vertical = !config_get_bool(GetGlobalConfig(), "BasicWindow",
					 "VerticalVolControl");
	config_set_bool(GetGlobalConfig(), "BasicWindow", "VerticalVolControl",
			vertical);
	ToggleMixerLayout(vertical);

	// We need to store it so we can delete current and then add
	// at the right order
	vector<OBSSource> sources;
	for (size_t i = 0; i != volumes.size(); i++)
		sources.emplace_back(volumes[i]->GetSource());

	ClearVolumeControls();

	for (const auto &source : sources)
		ActivateAudioSource(source);
}

void OBSBasic::ActivateAudioSource(OBSSource source)
{
	if (SourceMixerHidden(source))
		return;
	if (!obs_source_audio_active(source))
		return;

	bool vertical = config_get_bool(GetGlobalConfig(), "BasicWindow",
					"VerticalVolControl");
	VolControl *vol = new VolControl(source, true, vertical);

	vol->EnableSlider(!SourceVolumeLocked(source));

	double meterDecayRate =
		config_get_double(basicConfig, "Audio", "MeterDecayRate");
	vol->SetMeterDecayRate(meterDecayRate);

	uint32_t peakMeterTypeIdx =
		config_get_uint(basicConfig, "Audio", "PeakMeterType");

	enum obs_peak_meter_type peakMeterType;
	switch (peakMeterTypeIdx) {
	case 0:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	case 1:
		peakMeterType = TRUE_PEAK_METER;
		break;
	default:
		peakMeterType = SAMPLE_PEAK_METER;
		break;
	}

	vol->setPeakMeterType(peakMeterType);

	vol->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(vol, &QWidget::customContextMenuRequested, this,
		&OBSBasic::VolControlContextMenu);
	connect(vol, &VolControl::ConfigClicked, this,
		&OBSBasic::VolControlContextMenu);

	InsertQObjectByName(volumes, vol);

	for (auto volume : volumes) {
		if (vertical)
			ui->vVolControlLayout->addWidget(volume);
		else
			ui->hVolControlLayout->addWidget(volume);
	}
}

void OBSBasic::DeactivateAudioSource(OBSSource source)
{
	for (size_t i = 0; i < volumes.size(); i++) {
		if (volumes[i]->GetSource() == source) {
			delete volumes[i];
			volumes.erase(volumes.begin() + i);
			break;
		}
	}
}

bool OBSBasic::QueryRemoveSource(obs_source_t *source)
{
	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE &&
	    !obs_source_is_group(source)) {
		int count = ui->scenes->count();

		if (count == 1) {
			OBSMessageBox::information(this,
						   QTStr("FinalScene.Title"),
						   QTStr("FinalScene.Text"));
			return false;
		}
	}

	const char *name = obs_source_get_name(source);

	QString text = QTStr("ConfirmRemove.Text");
	text.replace("$1", QT_UTF8(name));

	QMessageBox remove_source(this);
	remove_source.setText(text);
	QPushButton *Yes =
		remove_source.addButton(QTStr("Yes"), QMessageBox::YesRole);
	remove_source.setDefaultButton(Yes);
	remove_source.addButton(QTStr("No"), QMessageBox::NoRole);
	remove_source.setIcon(QMessageBox::Question);
	remove_source.setWindowTitle(QTStr("ConfirmRemove.Title"));
	remove_source.exec();

	return Yes == remove_source.clickedButton();
}

#define UPDATE_CHECK_INTERVAL (60 * 60 * 24 * 4) /* 4 days */

#ifdef UPDATE_SPARKLE
void init_sparkle_updater(bool update_to_undeployed);
void trigger_sparkle_update();
#endif

void OBSBasic::TimedCheckForUpdates()
{
	if (App()->IsUpdaterDisabled())
		return;
	if (!config_get_bool(App()->GlobalConfig(), "General",
			     "EnableAutoUpdates"))
		return;

#ifdef UPDATE_SPARKLE
	init_sparkle_updater(config_get_bool(App()->GlobalConfig(), "General",
					     "UpdateToUndeployed"));
#elif _WIN32
	long long lastUpdate = config_get_int(App()->GlobalConfig(), "General",
					      "LastUpdateCheck");
	uint32_t lastVersion =
		config_get_int(App()->GlobalConfig(), "General", "LastVersion");

	if (lastVersion < LIBOBS_API_VER) {
		lastUpdate = 0;
		config_set_int(App()->GlobalConfig(), "General",
			       "LastUpdateCheck", 0);
	}

	long long t = (long long)time(nullptr);
	long long secs = t - lastUpdate;

	if (secs > UPDATE_CHECK_INTERVAL)
		CheckForUpdates(false);
#endif
}

void OBSBasic::CheckForUpdates(bool manualUpdate)
{
#ifdef UPDATE_SPARKLE
	trigger_sparkle_update();
#elif _WIN32
	ui->actionCheckForUpdates->setEnabled(false);

	if (updateCheckThread && updateCheckThread->isRunning())
		return;

	updateCheckThread.reset(new AutoUpdateThread(manualUpdate));
	updateCheckThread->start();
#endif

	UNUSED_PARAMETER(manualUpdate);
}

void OBSBasic::updateCheckFinished()
{
	ui->actionCheckForUpdates->setEnabled(true);
}

void OBSBasic::DuplicateSelectedScene()
{
	OBSScene curScene = GetCurrentScene();

	if (!curScene)
		return;

	OBSSource curSceneSource = obs_scene_get_source(curScene);
	QString format{obs_source_get_name(curSceneSource)};
	format += " %1";

	int i = 2;
	QString placeHolderText = format.arg(i);
	obs_source_t *source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(placeHolderText)))) {
		obs_source_release(source);
		placeHolderText = format.arg(++i);
	}

	for (;;) {
		string name;
		bool accepted = NameDialog::AskForName(
			this, QTStr("Basic.Main.AddSceneDlg.Title"),
			QTStr("Basic.Main.AddSceneDlg.Text"), name,
			placeHolderText);
		if (!accepted)
			return;

		if (name.empty()) {
			OBSMessageBox::warning(this,
					       QTStr("NoNameEntered.Title"),
					       QTStr("NoNameEntered.Text"));
			continue;
		}

		obs_source_t *source = obs_get_source_by_name(name.c_str());
		if (source) {
			OBSMessageBox::warning(this, QTStr("NameExists.Title"),
					       QTStr("NameExists.Text"));

			obs_source_release(source);
			continue;
		}

		obs_scene_t *scene = obs_scene_duplicate(curScene, name.c_str(),
							 OBS_SCENE_DUP_REFS);
		source = obs_scene_get_source(scene);
		SetCurrentScene(source, true);

		auto undo = [](const std::string &data) {
			obs_source_t *source =
				obs_get_source_by_name(data.c_str());
			obs_source_remove(source);
			obs_source_release(source);
		};

		auto redo = [this, name](const std::string &data) {
			obs_source_t *source =
				obs_get_source_by_name(data.c_str());
			obs_scene_t *scene = obs_scene_from_source(source);
			obs_source_release(source);
			scene = obs_scene_duplicate(scene, name.c_str(),
						    OBS_SCENE_DUP_REFS);
			source = obs_scene_get_source(scene);
			SetCurrentScene(source, true);
			obs_scene_release(scene);
		};

		undo_s.add_action(
			QTStr("Undo.Scene.Duplicate")
				.arg(obs_source_get_name(source)),
			undo, redo, obs_source_get_name(source),
			obs_source_get_name(obs_scene_get_source(curScene)));

		obs_scene_release(scene);

		break;
	}
}

static bool save_undo_source_enum(obs_scene_t *scene, obs_sceneitem_t *item,
				  void *p)
{
	UNUSED_PARAMETER(scene);

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (obs_obj_is_private(source) && !obs_source_removed(source))
		return true;

	obs_data_array_t *array = (obs_data_array_t *)p;

	/* check if the source is already stored in the array */
	const char *name = obs_source_get_name(source);
	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *sourceData = obs_data_array_item(array, i);
		if (strcmp(name, obs_data_get_string(sourceData, "name")) ==
		    0) {
			obs_data_release(sourceData);
			return true;
		}
		obs_data_release(sourceData);
	}

	if (obs_source_is_group(source))
		obs_scene_enum_items(obs_group_from_source(source),
				     save_undo_source_enum, p);

	obs_data_t *source_data = obs_save_source(source);
	obs_data_array_push_back(array, source_data);
	obs_data_release(source_data);
	return true;
}

void OBSBasic::RemoveSelectedScene()
{
	OBSScene scene = GetCurrentScene();
	obs_source_t *source = obs_scene_get_source(scene);

	OBSSource curProgramScene = OBSGetStrongRef(programScene);

	if (!source || !QueryRemoveSource(source)) {
		return;
	}

	/* ------------------------------ */
	/* save all sources in scene      */

	obs_data_array_t *array = obs_data_array_create();

	obs_scene_enum_items(scene, save_undo_source_enum, array);

	obs_data_t *scene_data = obs_save_source(source);
	obs_data_array_push_back(array, scene_data);
	obs_data_release(scene_data);

	/* --------------------------- */
	/* undo/redo                   */

	auto undo = [this](const std::string &json) {
		obs_data_t *base = obs_data_create_from_json(json.c_str());
		obs_data_array_t *array = obs_data_get_array(base, "array");
		int savedIndex = (int)obs_data_get_int(base, "index");
		std::vector<obs_source_t *> sources;

		/* create missing sources */
		size_t count = obs_data_array_count(array);
		sources.reserve(count);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *data = obs_data_array_item(array, i);
			const char *name = obs_data_get_string(data, "name");

			obs_source_t *source = obs_get_source_by_name(name);
			if (!source)
				source = obs_load_source(data);
			sources.push_back(source);

			obs_data_release(data);
		}

		/* actually load sources now */
		for (obs_source_t *source : sources)
			obs_source_load2(source);

		obs_source_t *scene_source = sources.back();
		OBSScene scene = obs_scene_from_source(scene_source);
		SetCurrentScene(scene, true);

		/* set original index in list box */
		ui->scenes->blockSignals(true);
		int curIndex = ui->scenes->currentRow();
		QListWidgetItem *item = ui->scenes->takeItem(curIndex);
		ui->scenes->insertItem(savedIndex, item);
		ui->scenes->setCurrentRow(savedIndex);
		ui->scenes->blockSignals(false);

		/* release sources */
		for (obs_source_t *source : sources)
			obs_source_release(source);

		obs_data_array_release(array);
		obs_data_release(base);
	};

	auto redo = [](const std::string &name) {
		obs_source_t *source = obs_get_source_by_name(name.c_str());
		obs_source_remove(source);
		obs_source_release(source);
	};

	obs_data_t *data = obs_data_create();
	obs_data_set_array(data, "array", array);
	obs_data_set_int(data, "index", ui->scenes->currentRow());

	const char *scene_name = obs_source_get_name(source);
	undo_s.add_action(QTStr("Undo.Delete").arg(scene_name), undo, redo,
			  obs_data_get_json(data), scene_name);

	obs_data_array_release(array);
	obs_data_release(data);

	/* --------------------------- */
	/* remove                      */

	obs_source_remove(source);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
}

void OBSBasic::RemoveSelectedSceneItem()
{
	OBSSceneItem item = GetCurrentSceneItem();
	if (item) {
		obs_source_t *source = obs_sceneitem_get_source(item);
		if (QueryRemoveSource(source))
			obs_sceneitem_remove(item);
	}
}

void OBSBasic::ReorderSources(OBSScene scene)
{
	if (scene != GetCurrentScene() || ui->sources->IgnoreReorder())
		return;

	ui->sources->ReorderItems();
	SaveProject();
}

void OBSBasic::RefreshSources(OBSScene scene)
{
	if (scene != GetCurrentScene() || ui->sources->IgnoreReorder())
		return;

	ui->sources->RefreshItems();
	SaveProject();
}

/* OBS Callbacks */

void OBSBasic::SceneReordered(void *data, calldata_t *params)
{
	OBSBasic *window = static_cast<OBSBasic *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "ReorderSources",
				  Q_ARG(OBSScene, OBSScene(scene)));
}

void OBSBasic::SceneRefreshed(void *data, calldata_t *params)
{
	OBSBasic *window = static_cast<OBSBasic *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "RefreshSources",
				  Q_ARG(OBSScene, OBSScene(scene)));
}

void OBSBasic::SceneItemAdded(void *data, calldata_t *params)
{
	OBSBasic *window = static_cast<OBSBasic *>(data);

	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "AddSceneItem",
				  Q_ARG(OBSSceneItem, OBSSceneItem(item)));
}

void OBSBasic::SourceCreated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (obs_scene_from_source(source) != NULL)
		QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
					  "AddScene", WaitConnection(),
					  Q_ARG(OBSSource, OBSSource(source)));
}

void OBSBasic::SourceRemoved(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (obs_scene_from_source(source) != NULL)
		QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
					  "RemoveScene",
					  Q_ARG(OBSSource, OBSSource(source)));
}

void OBSBasic::SourceActivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	uint32_t flags = obs_source_get_output_flags(source);

	if (flags & OBS_SOURCE_AUDIO)
		QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
					  "ActivateAudioSource",
					  Q_ARG(OBSSource, OBSSource(source)));
}

void OBSBasic::SourceDeactivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	uint32_t flags = obs_source_get_output_flags(source);

	if (flags & OBS_SOURCE_AUDIO)
		QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
					  "DeactivateAudioSource",
					  Q_ARG(OBSSource, OBSSource(source)));
}

void OBSBasic::SourceAudioActivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");

	if (obs_source_active(source))
		QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
					  "ActivateAudioSource",
					  Q_ARG(OBSSource, OBSSource(source)));
}

void OBSBasic::SourceAudioDeactivated(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
				  "DeactivateAudioSource",
				  Q_ARG(OBSSource, OBSSource(source)));
}

void OBSBasic::SourceRenamed(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	const char *newName = calldata_string(params, "new_name");
	const char *prevName = calldata_string(params, "prev_name");

	QMetaObject::invokeMethod(static_cast<OBSBasic *>(data),
				  "RenameSources", Q_ARG(OBSSource, source),
				  Q_ARG(QString, QT_UTF8(newName)),
				  Q_ARG(QString, QT_UTF8(prevName)));

	blog(LOG_INFO, "Source '%s' renamed to '%s'", prevName, newName);
}

void OBSBasic::DrawBackdrop(float cx, float cy)
{
	if (!box)
		return;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawBackdrop");

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	vec4 colorVal;
	vec4_set(&colorVal, 0.0f, 0.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(color, &colorVal);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_scale3f(float(cx), float(cy), 1.0f);

	gs_load_vertexbuffer(box);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_pop();
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_load_vertexbuffer(nullptr);

	GS_DEBUG_MARKER_END();
}

void OBSBasic::RenderMain(void *data, uint32_t cx, uint32_t cy)
{
	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "RenderMain");

	OBSBasic *window = static_cast<OBSBasic *>(data);
	obs_video_info ovi;

	obs_get_video_info(&ovi);

	window->previewCX = int(window->previewScale * float(ovi.base_width));
	window->previewCY = int(window->previewScale * float(ovi.base_height));

	gs_viewport_push();
	gs_projection_push();

	obs_display_t *display = window->ui->preview->GetDisplay();
	uint32_t width, height;
	obs_display_size(display, &width, &height);
	float right = float(width) - window->previewX;
	float bottom = float(height) - window->previewY;

	gs_ortho(-window->previewX, right, -window->previewY, bottom, -100.0f,
		 100.0f);

	window->ui->preview->DrawOverflow();

	/* --------------------------------------- */

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height),
		 -100.0f, 100.0f);
	gs_set_viewport(window->previewX, window->previewY, window->previewCX,
			window->previewCY);

	if (window->IsPreviewProgramMode()) {
		window->DrawBackdrop(float(ovi.base_width),
				     float(ovi.base_height));

		OBSScene scene = window->GetCurrentScene();
		obs_source_t *source = obs_scene_get_source(scene);
		if (source)
			obs_source_video_render(source);
	} else {
		obs_render_main_texture_src_color_only();
	}
	gs_load_vertexbuffer(nullptr);

	/* --------------------------------------- */

	gs_ortho(-window->previewX, right, -window->previewY, bottom, -100.0f,
		 100.0f);
	gs_reset_viewport();

	window->ui->preview->DrawSceneEditing();

	/* --------------------------------------- */

	gs_projection_pop();
	gs_viewport_pop();

	GS_DEBUG_MARKER_END();

	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
}

/* Main class functions */

obs_service_t *OBSBasic::GetService()
{
	if (!service) {
		service =
			obs_service_create("rtmp_common", NULL, NULL, nullptr);
		obs_service_release(service);
	}
	return service;
}

void OBSBasic::SetService(obs_service_t *newService)
{
	if (newService)
		service = newService;
}

int OBSBasic::GetTransitionDuration()
{
	return ui->transitionDuration->value();
}

bool OBSBasic::StreamingActive() const
{
	if (!outputHandler)
		return false;
	return outputHandler->StreamingActive();
}

bool OBSBasic::Active() const
{
	if (!outputHandler)
		return false;
	return outputHandler->Active();
}

#ifdef _WIN32
#define IS_WIN32 1
#else
#define IS_WIN32 0
#endif

static inline int AttemptToResetVideo(struct obs_video_info *ovi)
{
	return obs_reset_video(ovi);
}

static inline enum obs_scale_type GetScaleType(ConfigFile &basicConfig)
{
	const char *scaleTypeStr =
		config_get_string(basicConfig, "Video", "ScaleType");

	if (astrcmpi(scaleTypeStr, "bilinear") == 0)
		return OBS_SCALE_BILINEAR;
	else if (astrcmpi(scaleTypeStr, "lanczos") == 0)
		return OBS_SCALE_LANCZOS;
	else if (astrcmpi(scaleTypeStr, "area") == 0)
		return OBS_SCALE_AREA;
	else
		return OBS_SCALE_BICUBIC;
}

static inline enum video_format GetVideoFormatFromName(const char *name)
{
	if (astrcmpi(name, "I420") == 0)
		return VIDEO_FORMAT_I420;
	else if (astrcmpi(name, "NV12") == 0)
		return VIDEO_FORMAT_NV12;
	else if (astrcmpi(name, "I444") == 0)
		return VIDEO_FORMAT_I444;
#if 0 //currently unsupported
	else if (astrcmpi(name, "YVYU") == 0)
		return VIDEO_FORMAT_YVYU;
	else if (astrcmpi(name, "YUY2") == 0)
		return VIDEO_FORMAT_YUY2;
	else if (astrcmpi(name, "UYVY") == 0)
		return VIDEO_FORMAT_UYVY;
#endif
	else
		return VIDEO_FORMAT_RGBA;
}

void OBSBasic::ResetUI()
{
	bool studioPortraitLayout = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "StudioPortraitLayout");

	bool labels = config_get_bool(GetGlobalConfig(), "BasicWindow",
				      "StudioModeLabels");

	if (studioPortraitLayout)
		ui->previewLayout->setDirection(QBoxLayout::TopToBottom);
	else
		ui->previewLayout->setDirection(QBoxLayout::LeftToRight);

	if (previewProgramMode)
		ui->previewLabel->setHidden(!labels);

	if (programLabel)
		programLabel->setHidden(!labels);
}

int OBSBasic::ResetVideo()
{
	if (outputHandler && outputHandler->Active())
		return OBS_VIDEO_CURRENTLY_ACTIVE;

	ProfileScope("OBSBasic::ResetVideo");

	struct obs_video_info ovi;
	int ret;

	GetConfigFPS(ovi.fps_num, ovi.fps_den);

	const char *colorFormat =
		config_get_string(basicConfig, "Video", "ColorFormat");
	const char *colorSpace =
		config_get_string(basicConfig, "Video", "ColorSpace");
	const char *colorRange =
		config_get_string(basicConfig, "Video", "ColorRange");

	ovi.graphics_module = App()->GetRenderModule();
	ovi.base_width =
		(uint32_t)config_get_uint(basicConfig, "Video", "BaseCX");
	ovi.base_height =
		(uint32_t)config_get_uint(basicConfig, "Video", "BaseCY");
	ovi.output_width =
		(uint32_t)config_get_uint(basicConfig, "Video", "OutputCX");
	ovi.output_height =
		(uint32_t)config_get_uint(basicConfig, "Video", "OutputCY");
	ovi.output_format = GetVideoFormatFromName(colorFormat);
	ovi.colorspace = astrcmpi(colorSpace, "601") == 0
				 ? VIDEO_CS_601
				 : (astrcmpi(colorSpace, "709") == 0
					    ? VIDEO_CS_709
					    : VIDEO_CS_SRGB);
	ovi.range = astrcmpi(colorRange, "Full") == 0 ? VIDEO_RANGE_FULL
						      : VIDEO_RANGE_PARTIAL;
	ovi.adapter =
		config_get_uint(App()->GlobalConfig(), "Video", "AdapterIdx");
	ovi.gpu_conversion = true;
	ovi.scale_type = GetScaleType(basicConfig);

	if (ovi.base_width < 8 || ovi.base_height < 8) {
		ovi.base_width = 1920;
		ovi.base_height = 1080;
		config_set_uint(basicConfig, "Video", "BaseCX", 1920);
		config_set_uint(basicConfig, "Video", "BaseCY", 1080);
	}

	if (ovi.output_width < 8 || ovi.output_height < 8) {
		ovi.output_width = ovi.base_width;
		ovi.output_height = ovi.base_height;
		config_set_uint(basicConfig, "Video", "OutputCX",
				ovi.base_width);
		config_set_uint(basicConfig, "Video", "OutputCY",
				ovi.base_height);
	}

	ret = AttemptToResetVideo(&ovi);
	if (IS_WIN32 && ret != OBS_VIDEO_SUCCESS) {
		if (ret == OBS_VIDEO_CURRENTLY_ACTIVE) {
			blog(LOG_WARNING, "Tried to reset when "
					  "already active");
			return ret;
		}

		/* Try OpenGL if DirectX fails on windows */
		if (astrcmpi(ovi.graphics_module, DL_OPENGL) != 0) {
			blog(LOG_WARNING,
			     "Failed to initialize obs video (%d) "
			     "with graphics_module='%s', retrying "
			     "with graphics_module='%s'",
			     ret, ovi.graphics_module, DL_OPENGL);
			ovi.graphics_module = DL_OPENGL;
			ret = AttemptToResetVideo(&ovi);
		}
	} else if (ret == OBS_VIDEO_SUCCESS) {
		ResizePreview(ovi.base_width, ovi.base_height);
		if (program)
			ResizeProgram(ovi.base_width, ovi.base_height);
	}

	if (ret == OBS_VIDEO_SUCCESS) {
		OBSBasicStats::InitializeValues();
		OBSProjector::UpdateMultiviewProjectors();
	}

	return ret;
}

bool OBSBasic::ResetAudio()
{
	ProfileScope("OBSBasic::ResetAudio");

	struct obs_audio_info ai;
	ai.samples_per_sec =
		config_get_uint(basicConfig, "Audio", "SampleRate");

	const char *channelSetupStr =
		config_get_string(basicConfig, "Audio", "ChannelSetup");

	if (strcmp(channelSetupStr, "Mono") == 0)
		ai.speakers = SPEAKERS_MONO;
	else if (strcmp(channelSetupStr, "2.1") == 0)
		ai.speakers = SPEAKERS_2POINT1;
	else if (strcmp(channelSetupStr, "4.0") == 0)
		ai.speakers = SPEAKERS_4POINT0;
	else if (strcmp(channelSetupStr, "4.1") == 0)
		ai.speakers = SPEAKERS_4POINT1;
	else if (strcmp(channelSetupStr, "5.1") == 0)
		ai.speakers = SPEAKERS_5POINT1;
	else if (strcmp(channelSetupStr, "7.1") == 0)
		ai.speakers = SPEAKERS_7POINT1;
	else
		ai.speakers = SPEAKERS_STEREO;

	return obs_reset_audio(&ai);
}

void OBSBasic::ResetAudioDevice(const char *sourceId, const char *deviceId,
				const char *deviceDesc, int channel)
{
	bool disable = deviceId && strcmp(deviceId, "disabled") == 0;
	obs_source_t *source;
	obs_data_t *settings;

	source = obs_get_output_source(channel);
	if (source) {
		if (disable) {
			obs_set_output_source(channel, nullptr);
		} else {
			settings = obs_source_get_settings(source);
			const char *oldId =
				obs_data_get_string(settings, "device_id");
			if (strcmp(oldId, deviceId) != 0) {
				obs_data_set_string(settings, "device_id",
						    deviceId);
				obs_source_update(source, settings);
			}
			obs_data_release(settings);
		}

		obs_source_release(source);

	} else if (!disable) {
		settings = obs_data_create();
		obs_data_set_string(settings, "device_id", deviceId);
		source = obs_source_create(sourceId, deviceDesc, settings,
					   nullptr);
		obs_data_release(settings);

		obs_set_output_source(channel, source);
		obs_source_release(source);
	}
}

void OBSBasic::ResizePreview(uint32_t cx, uint32_t cy)
{
	QSize targetSize;
	bool isFixedScaling;
	obs_video_info ovi;

	/* resize preview panel to fix to the top section of the window */
	targetSize = GetPixelSize(ui->preview);

	isFixedScaling = ui->preview->IsFixedScaling();
	obs_get_video_info(&ovi);

	if (isFixedScaling) {
		previewScale = ui->preview->GetScalingAmount();
		GetCenterPosFromFixedScale(
			int(cx), int(cy),
			targetSize.width() - PREVIEW_EDGE_SIZE * 2,
			targetSize.height() - PREVIEW_EDGE_SIZE * 2, previewX,
			previewY, previewScale);
		previewX += ui->preview->GetScrollX();
		previewY += ui->preview->GetScrollY();

	} else {
		GetScaleAndCenterPos(int(cx), int(cy),
				     targetSize.width() - PREVIEW_EDGE_SIZE * 2,
				     targetSize.height() -
					     PREVIEW_EDGE_SIZE * 2,
				     previewX, previewY, previewScale);
	}

	previewX += float(PREVIEW_EDGE_SIZE);
	previewY += float(PREVIEW_EDGE_SIZE);
}

void OBSBasic::CloseDialogs()
{
	QList<QDialog *> childDialogs = this->findChildren<QDialog *>();
	if (!childDialogs.isEmpty()) {
		for (int i = 0; i < childDialogs.size(); ++i) {
			childDialogs.at(i)->close();
		}
	}

	if (!stats.isNull())
		stats->close(); //call close to save Stats geometry
	if (!remux.isNull())
		remux->close();
}

void OBSBasic::EnumDialogs()
{
	visDialogs.clear();
	modalDialogs.clear();
	visMsgBoxes.clear();

	/* fill list of Visible dialogs and Modal dialogs */
	QList<QDialog *> dialogs = findChildren<QDialog *>();
	for (QDialog *dialog : dialogs) {
		if (dialog->isVisible())
			visDialogs.append(dialog);
		if (dialog->isModal())
			modalDialogs.append(dialog);
	}

	/* fill list of Visible message boxes */
	QList<QMessageBox *> msgBoxes = findChildren<QMessageBox *>();
	for (QMessageBox *msgbox : msgBoxes) {
		if (msgbox->isVisible())
			visMsgBoxes.append(msgbox);
	}
}

void OBSBasic::ClearProjectors()
{
	for (size_t i = 0; i < projectors.size(); i++) {
		if (projectors[i])
			delete projectors[i];
	}

	projectors.clear();
}

void OBSBasic::ClearSceneData()
{
	disableSaving++;

	CloseDialogs();

	ClearVolumeControls();
	ClearListItems(ui->scenes);
	ui->sources->Clear();
	ClearQuickTransitions();
	ui->transitions->clear();

	ClearProjectors();

	for (int i = 0; i < MAX_CHANNELS; i++)
		obs_set_output_source(i, nullptr);

	lastScene = nullptr;
	swapScene = nullptr;
	programScene = nullptr;
	prevFTBSource = nullptr;

	copyStrings.clear();
	copyFiltersString = nullptr;
	copyFilter = nullptr;

	auto cb = [](void *unused, obs_source_t *source) {
		obs_source_remove(source);
		UNUSED_PARAMETER(unused);
		return true;
	};

	obs_enum_scenes(cb, nullptr);
	obs_enum_sources(cb, nullptr);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP);

	undo_s.clear();

	disableSaving--;

	blog(LOG_INFO, "All scene data cleared");
	blog(LOG_INFO, "------------------------------------------------");
}

void OBSBasic::closeEvent(QCloseEvent *event)
{
	/* Do not close window if inside of a temporary event loop because we
	 * could be inside of an Auth::LoadUI call.  Keep trying once per
	 * second until we've exit any known sub-loops. */
	if (os_atomic_load_long(&insideEventLoop) != 0) {
		QTimer::singleShot(1000, this, SLOT(close()));
		event->ignore();
		return;
	}

	if (isVisible())
		config_set_string(App()->GlobalConfig(), "BasicWindow",
				  "geometry",
				  saveGeometry().toBase64().constData());

	if (outputHandler && outputHandler->Active()) {
		SetShowing(true);

		QMessageBox::StandardButton button = OBSMessageBox::question(
			this, QTStr("ConfirmExit.Title"),
			QTStr("ConfirmExit.Text"));

		if (button == QMessageBox::No) {
			event->ignore();
			restart = false;
			return;
		}
	}

	QWidget::closeEvent(event);
	if (!event->isAccepted())
		return;

	blog(LOG_INFO, SHUTDOWN_SEPARATOR);

	closing = true;

	if (introCheckThread)
		introCheckThread->wait();
	if (whatsNewInitThread)
		whatsNewInitThread->wait();
	if (updateCheckThread)
		updateCheckThread->wait();
	if (logUploadThread)
		logUploadThread->wait();
	if (devicePropertiesThread && devicePropertiesThread->isRunning()) {
		devicePropertiesThread->wait();
		devicePropertiesThread.reset();
	}

	QApplication::sendPostedEvents(this);

	signalHandlers.clear();

	Auth::Save();
	SaveProjectNow();
	auth.reset();

	delete extraBrowsers;

	config_set_string(App()->GlobalConfig(), "BasicWindow", "DockState",
			  saveState().toBase64().constData());

#ifdef BROWSER_AVAILABLE
	SaveExtraBrowserDocks();
	ClearExtraBrowserDocks();
#endif

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_EXIT);

	disableSaving++;

	/* Clear all scene data (dialogs, widgets, widget sub-items, scenes,
	 * sources, etc) so that all references are released before shutdown */
	ClearSceneData();

	App()->quit();
}

void OBSBasic::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::WindowStateChange) {
		QWindowStateChangeEvent *stateEvent =
			(QWindowStateChangeEvent *)event;

		if (isMinimized()) {
			if (trayIcon && trayIcon->isVisible() &&
			    sysTrayMinimizeToTray()) {
				ToggleShowHide();
			}

			if (previewEnabled)
				EnablePreviewDisplay(false);
		} else if (stateEvent->oldState() & Qt::WindowMinimized &&
			   isVisible()) {
			if (previewEnabled)
				EnablePreviewDisplay(true);
		}
	}
}

void OBSBasic::on_actionShow_Recordings_triggered()
{
	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	const char *type = config_get_string(basicConfig, "AdvOut", "RecType");
	const char *adv_path =
		strcmp(type, "Standard")
			? config_get_string(basicConfig, "AdvOut", "FFFilePath")
			: config_get_string(basicConfig, "AdvOut",
					    "RecFilePath");
	const char *path = strcmp(mode, "Advanced")
				   ? config_get_string(basicConfig,
						       "SimpleOutput",
						       "FilePath")
				   : adv_path;
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void OBSBasic::on_actionRemux_triggered()
{
	if (!remux.isNull()) {
		remux->show();
		remux->raise();
		return;
	}

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	const char *path = strcmp(mode, "Advanced")
				   ? config_get_string(basicConfig,
						       "SimpleOutput",
						       "FilePath")
				   : config_get_string(basicConfig, "AdvOut",
						       "RecFilePath");

	OBSRemux *remuxDlg;
	remuxDlg = new OBSRemux(path, this);
	remuxDlg->show();
	remux = remuxDlg;
}

void OBSBasic::on_action_Settings_triggered()
{
	static bool settings_already_executing = false;

	/* Do not load settings window if inside of a temporary event loop
	 * because we could be inside of an Auth::LoadUI call.  Keep trying
	 * once per second until we've exit any known sub-loops. */
	if (os_atomic_load_long(&insideEventLoop) != 0) {
		QTimer::singleShot(1000, this,
				   SLOT(on_action_Settings_triggered()));
		return;
	}

	if (settings_already_executing) {
		return;
	}

	settings_already_executing = true;

	{
		OBSBasicSettings settings(this);
		settings.exec();
	}

	SystemTray(false);

	settings_already_executing = false;

	if (restart) {
		QMessageBox::StandardButton button = OBSMessageBox::question(
			this, QTStr("Restart"), QTStr("NeedsRestart"));

		if (button == QMessageBox::Yes)
			close();
		else
			restart = false;
	}
}

void save_audio_source(int channel, obs_data_t *save)
{
	obs_source_t *source = obs_get_output_source(channel);
	if (!source)
		return;

	obs_data_t *obj = obs_data_create();

	obs_data_set_double(obj, "vol", obs_source_get_volume(source));
	obs_data_set_double(obj, "balance",
			    obs_source_get_balance_value(source));
	obs_data_set_double(obj, "mixers", obs_source_get_audio_mixers(source));
	obs_data_set_double(obj, "sync", obs_source_get_sync_offset(source));
	obs_data_set_double(obj, "flags", obs_source_get_flags(source));

	obs_data_set_obj(save, std::to_string(channel).c_str(), obj);
	obs_data_release(obj);
	obs_source_release(source);
}

void load_audio_source(int channel, obs_data_t *data)
{
	obs_source_t *source = obs_get_output_source(channel);
	if (!source)
		return;

	obs_data_t *save =
		obs_data_get_obj(data, std::to_string(channel).c_str());

	obs_source_set_volume(source, obs_data_get_double(save, "vol"));
	obs_source_set_balance_value(source,
				     obs_data_get_double(save, "balance"));
	obs_source_set_audio_mixers(source,
				    obs_data_get_double(save, "mixers"));
	obs_source_set_sync_offset(source, obs_data_get_double(save, "sync"));
	obs_source_set_flags(source, obs_data_get_double(save, "flags"));

	obs_data_release(save);
	obs_source_release(source);
}

void OBSBasic::on_actionAdvAudioProperties_triggered()
{
	if (advAudioWindow != nullptr) {
		advAudioWindow->raise();
		return;
	}

	bool iconsVisible = config_get_bool(App()->GlobalConfig(),
					    "BasicWindow", "ShowSourceIcons");

	advAudioWindow = new OBSBasicAdvAudio(this);
	advAudioWindow->show();
	advAudioWindow->setAttribute(Qt::WA_DeleteOnClose, true);
	advAudioWindow->SetIconsVisible(iconsVisible);

	connect(advAudioWindow, SIGNAL(destroyed()), this,
		SLOT(AdvAudioPropsDestroyed()));
}

void OBSBasic::AdvAudioPropsClicked()
{
	on_actionAdvAudioProperties_triggered();
}

void OBSBasic::AdvAudioPropsDestroyed()
{
	advAudioWindow = nullptr;
}

void OBSBasic::on_scenes_currentItemChanged(QListWidgetItem *current,
					    QListWidgetItem *prev)
{
	obs_source_t *source = NULL;

	if (current) {
		obs_scene_t *scene;

		scene = GetOBSRef<OBSScene>(current);
		source = obs_scene_get_source(scene);
	}

	SetCurrentScene(source);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);

	UpdateContextBar();

	UNUSED_PARAMETER(prev);
}

void OBSBasic::EditSceneName()
{
	ui->scenesDock->removeAction(renameScene);
	QListWidgetItem *item = ui->scenes->currentItem();
	Qt::ItemFlags flags = item->flags();

	item->setFlags(flags | Qt::ItemIsEditable);
	ui->scenes->editItem(item);
	item->setFlags(flags);
}

void OBSBasic::AddProjectorMenuMonitors(QMenu *parent, QObject *target,
					const char *slot)
{
	QAction *action;
	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QRect screenGeometry = screen->geometry();
		qreal ratio = screen->devicePixelRatio();
		QString name = "";
#ifdef _WIN32
		QTextStream fullname(&name);
		fullname << GetMonitorName(screen->name());
		fullname << " (";
		fullname << (i + 1);
		fullname << ")";
#elif defined(__APPLE__)
		name = screen->name();
#else
		name = screen->model().simplified();

		if (name.length() > 1 && name.endsWith("-"))
			name.chop(1);
#endif
		name = name.simplified();

		if (name.length() == 0) {
			name = QString("%1 %2")
				       .arg(QTStr("Display"))
				       .arg(QString::number(i + 1));
		}
		QString str =
			QString("%1: %2x%3 @ %4,%5")
				.arg(name,
				     QString::number(screenGeometry.width() *
						     ratio),
				     QString::number(screenGeometry.height() *
						     ratio),
				     QString::number(screenGeometry.x()),
				     QString::number(screenGeometry.y()));

		action = parent->addAction(str, target, slot);
		action->setProperty("monitor", i);
	}
}

void OBSBasic::on_scenes_customContextMenuRequested(const QPoint &pos)
{
	QListWidgetItem *item = ui->scenes->itemAt(pos);

	QMenu popup(this);
	QMenu order(QTStr("Basic.MainMenu.Edit.Order"), this);

	popup.addAction(QTStr("Add"), this,
			SLOT(on_actionAddScene_triggered()));

	if (item) {
		QAction *copyFilters = new QAction(QTStr("Copy.Filters"), this);
		copyFilters->setEnabled(false);
		connect(copyFilters, SIGNAL(triggered()), this,
			SLOT(SceneCopyFilters()));
		QAction *pasteFilters =
			new QAction(QTStr("Paste.Filters"), this);
		pasteFilters->setEnabled(copyFiltersString);
		connect(pasteFilters, SIGNAL(triggered()), this,
			SLOT(ScenePasteFilters()));

		popup.addSeparator();
		popup.addAction(QTStr("Duplicate"), this,
				SLOT(DuplicateSelectedScene()));
		popup.addAction(copyFilters);
		popup.addAction(pasteFilters);
		popup.addSeparator();
		popup.addAction(QTStr("Rename"), this, SLOT(EditSceneName()));
		popup.addAction(QTStr("Remove"), this,
				SLOT(RemoveSelectedScene()));
		popup.addSeparator();

		order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveUp"), this,
				SLOT(on_actionSceneUp_triggered()));
		order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveDown"),
				this, SLOT(on_actionSceneDown_triggered()));
		order.addSeparator();
		order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveToTop"),
				this, SLOT(MoveSceneToTop()));
		order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveToBottom"),
				this, SLOT(MoveSceneToBottom()));
		popup.addMenu(&order);

		popup.addSeparator();

		delete sceneProjectorMenu;
		sceneProjectorMenu = new QMenu(QTStr("SceneProjector"));
		AddProjectorMenuMonitors(sceneProjectorMenu, this,
					 SLOT(OpenSceneProjector()));
		popup.addMenu(sceneProjectorMenu);

		QAction *sceneWindow = popup.addAction(
			QTStr("SceneWindow"), this, SLOT(OpenSceneWindow()));

		popup.addAction(sceneWindow);
		popup.addAction(QTStr("Screenshot.Scene"), this,
				SLOT(ScreenshotScene()));
		popup.addSeparator();
		popup.addAction(QTStr("Filters"), this,
				SLOT(OpenSceneFilters()));

		popup.addSeparator();

		delete perSceneTransitionMenu;
		perSceneTransitionMenu = CreatePerSceneTransitionMenu();
		popup.addMenu(perSceneTransitionMenu);

		/* ---------------------- */

		QAction *multiviewAction =
			popup.addAction(QTStr("ShowInMultiview"));

		OBSSource source = GetCurrentSceneSource();
		OBSData data = obs_source_get_private_settings(source);
		obs_data_release(data);

		obs_data_set_default_bool(data, "show_in_multiview", true);
		bool show = obs_data_get_bool(data, "show_in_multiview");

		multiviewAction->setCheckable(true);
		multiviewAction->setChecked(show);

		auto showInMultiview = [](OBSData data) {
			bool show =
				obs_data_get_bool(data, "show_in_multiview");
			obs_data_set_bool(data, "show_in_multiview", !show);
			OBSProjector::UpdateMultiviewProjectors();
		};

		connect(multiviewAction, &QAction::triggered,
			std::bind(showInMultiview, data));

		copyFilters->setEnabled(obs_source_filter_count(source) > 0);
	}

	popup.addSeparator();

	bool grid = ui->scenes->GetGridMode();

	QAction *gridAction = new QAction(grid ? QTStr("Basic.Main.ListMode")
					       : QTStr("Basic.Main.GridMode"),
					  this);
	connect(gridAction, SIGNAL(triggered()), this,
		SLOT(GridActionClicked()));
	popup.addAction(gridAction);

	popup.exec(QCursor::pos());
}

void OBSBasic::GridActionClicked()
{
	bool gridMode = !ui->scenes->GetGridMode();
	ui->scenes->SetGridMode(gridMode);
}

void OBSBasic::on_actionAddScene_triggered()
{
	string name;
	QString format{QTStr("Basic.Main.DefaultSceneName.Text")};

	int i = 2;
	QString placeHolderText = format.arg(i);
	obs_source_t *source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(placeHolderText)))) {
		obs_source_release(source);
		placeHolderText = format.arg(++i);
	}

	bool accepted = NameDialog::AskForName(
		this, QTStr("Basic.Main.AddSceneDlg.Title"),
		QTStr("Basic.Main.AddSceneDlg.Text"), name, placeHolderText);

	if (accepted) {
		if (name.empty()) {
			OBSMessageBox::warning(this,
					       QTStr("NoNameEntered.Title"),
					       QTStr("NoNameEntered.Text"));
			on_actionAddScene_triggered();
			return;
		}

		obs_source_t *source = obs_get_source_by_name(name.c_str());
		if (source) {
			OBSMessageBox::warning(this, QTStr("NameExists.Title"),
					       QTStr("NameExists.Text"));

			obs_source_release(source);
			on_actionAddScene_triggered();
			return;
		}

		auto undo_fn = [](const std::string &data) {
			obs_source_t *t = obs_get_source_by_name(data.c_str());
			if (t) {
				obs_source_release(t);
				obs_source_remove(t);
			}
		};

		auto redo_fn = [this](const std::string &data) {
			obs_scene_t *scene = obs_scene_create(data.c_str());
			obs_source_t *source = obs_scene_get_source(scene);
			SetCurrentScene(source, true);
			obs_scene_release(scene);
		};
		undo_s.add_action(QTStr("Undo.Add").arg(QString(name.c_str())),
				  undo_fn, redo_fn, name, name);

		obs_scene_t *scene = obs_scene_create(name.c_str());
		source = obs_scene_get_source(scene);
		SetCurrentScene(source);
		RefreshSources(scene);
		obs_scene_release(scene);
	}
}

void OBSBasic::on_actionRemoveScene_triggered()
{
	RemoveSelectedScene();
}

void OBSBasic::ChangeSceneIndex(bool relative, int offset, int invalidIdx)
{
	int idx = ui->scenes->currentRow();
	if (idx == -1 || idx == invalidIdx)
		return;

	ui->scenes->blockSignals(true);
	QListWidgetItem *item = ui->scenes->takeItem(idx);

	if (!relative)
		idx = 0;

	ui->scenes->insertItem(idx + offset, item);
	ui->scenes->setCurrentRow(idx + offset);
	item->setSelected(true);
	ui->scenes->blockSignals(false);

	OBSProjector::UpdateMultiviewProjectors();
}

void OBSBasic::on_actionSceneUp_triggered()
{
	ChangeSceneIndex(true, -1, 0);
}

void OBSBasic::on_actionSceneDown_triggered()
{
	ChangeSceneIndex(true, 1, ui->scenes->count() - 1);
}

void OBSBasic::MoveSceneToTop()
{
	ChangeSceneIndex(false, 0, 0);
}

void OBSBasic::MoveSceneToBottom()
{
	ChangeSceneIndex(false, ui->scenes->count() - 1,
			 ui->scenes->count() - 1);
}

void OBSBasic::EditSceneItemName()
{
	int idx = GetTopSelectedSourceItem();
	ui->sources->Edit(idx);
}

void OBSBasic::SetDeinterlacingMode()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	obs_deinterlace_mode mode =
		(obs_deinterlace_mode)action->property("mode").toInt();
	OBSSceneItem sceneItem = GetCurrentSceneItem();
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);

	obs_source_set_deinterlace_mode(source, mode);
}

void OBSBasic::SetDeinterlacingOrder()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	obs_deinterlace_field_order order =
		(obs_deinterlace_field_order)action->property("order").toInt();
	OBSSceneItem sceneItem = GetCurrentSceneItem();
	obs_source_t *source = obs_sceneitem_get_source(sceneItem);

	obs_source_set_deinterlace_field_order(source, order);
}

QMenu *OBSBasic::AddDeinterlacingMenu(QMenu *menu, obs_source_t *source)
{
	obs_deinterlace_mode deinterlaceMode =
		obs_source_get_deinterlace_mode(source);
	obs_deinterlace_field_order deinterlaceOrder =
		obs_source_get_deinterlace_field_order(source);
	QAction *action;

#define ADD_MODE(name, mode)                                    \
	action = menu->addAction(QTStr("" name), this,          \
				 SLOT(SetDeinterlacingMode())); \
	action->setProperty("mode", (int)mode);                 \
	action->setCheckable(true);                             \
	action->setChecked(deinterlaceMode == mode);

	ADD_MODE("Disable", OBS_DEINTERLACE_MODE_DISABLE);
	ADD_MODE("Deinterlacing.Discard", OBS_DEINTERLACE_MODE_DISCARD);
	ADD_MODE("Deinterlacing.Retro", OBS_DEINTERLACE_MODE_RETRO);
	ADD_MODE("Deinterlacing.Blend", OBS_DEINTERLACE_MODE_BLEND);
	ADD_MODE("Deinterlacing.Blend2x", OBS_DEINTERLACE_MODE_BLEND_2X);
	ADD_MODE("Deinterlacing.Linear", OBS_DEINTERLACE_MODE_LINEAR);
	ADD_MODE("Deinterlacing.Linear2x", OBS_DEINTERLACE_MODE_LINEAR_2X);
	ADD_MODE("Deinterlacing.Yadif", OBS_DEINTERLACE_MODE_YADIF);
	ADD_MODE("Deinterlacing.Yadif2x", OBS_DEINTERLACE_MODE_YADIF_2X);
#undef ADD_MODE

	menu->addSeparator();

#define ADD_ORDER(name, order)                                       \
	action = menu->addAction(QTStr("Deinterlacing." name), this, \
				 SLOT(SetDeinterlacingOrder()));     \
	action->setProperty("order", (int)order);                    \
	action->setCheckable(true);                                  \
	action->setChecked(deinterlaceOrder == order);

	ADD_ORDER("TopFieldFirst", OBS_DEINTERLACE_FIELD_ORDER_TOP);
	ADD_ORDER("BottomFieldFirst", OBS_DEINTERLACE_FIELD_ORDER_BOTTOM);
#undef ADD_ORDER

	return menu;
}

void OBSBasic::SetScaleFilter()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	obs_scale_type mode = (obs_scale_type)action->property("mode").toInt();
	OBSSceneItem sceneItem = GetCurrentSceneItem();

	obs_sceneitem_set_scale_filter(sceneItem, mode);
}

QMenu *OBSBasic::AddScaleFilteringMenu(QMenu *menu, obs_sceneitem_t *item)
{
	obs_scale_type scaleFilter = obs_sceneitem_get_scale_filter(item);
	QAction *action;

#define ADD_MODE(name, mode)                                                   \
	action =                                                               \
		menu->addAction(QTStr("" name), this, SLOT(SetScaleFilter())); \
	action->setProperty("mode", (int)mode);                                \
	action->setCheckable(true);                                            \
	action->setChecked(scaleFilter == mode);

	ADD_MODE("Disable", OBS_SCALE_DISABLE);
	ADD_MODE("ScaleFiltering.Point", OBS_SCALE_POINT);
	ADD_MODE("ScaleFiltering.Bilinear", OBS_SCALE_BILINEAR);
	ADD_MODE("ScaleFiltering.Bicubic", OBS_SCALE_BICUBIC);
	ADD_MODE("ScaleFiltering.Lanczos", OBS_SCALE_LANCZOS);
	ADD_MODE("ScaleFiltering.Area", OBS_SCALE_AREA);
#undef ADD_MODE

	return menu;
}

QMenu *OBSBasic::AddBackgroundColorMenu(QMenu *menu,
					QWidgetAction *widgetAction,
					ColorSelect *select,
					obs_sceneitem_t *item)
{
	QAction *action;

	menu->setStyleSheet(QString(
		"*[bgColor=\"1\"]{background-color:rgba(255,68,68,33%);}"
		"*[bgColor=\"2\"]{background-color:rgba(255,255,68,33%);}"
		"*[bgColor=\"3\"]{background-color:rgba(68,255,68,33%);}"
		"*[bgColor=\"4\"]{background-color:rgba(68,255,255,33%);}"
		"*[bgColor=\"5\"]{background-color:rgba(68,68,255,33%);}"
		"*[bgColor=\"6\"]{background-color:rgba(255,68,255,33%);}"
		"*[bgColor=\"7\"]{background-color:rgba(68,68,68,33%);}"
		"*[bgColor=\"8\"]{background-color:rgba(255,255,255,33%);}"));

	obs_data_t *privData = obs_sceneitem_get_private_settings(item);
	obs_data_release(privData);

	obs_data_set_default_int(privData, "color-preset", 0);
	int preset = obs_data_get_int(privData, "color-preset");

	action = menu->addAction(QTStr("Clear"), this, +SLOT(ColorChange()));
	action->setCheckable(true);
	action->setProperty("bgColor", 0);
	action->setChecked(preset == 0);

	action = menu->addAction(QTStr("CustomColor"), this,
				 +SLOT(ColorChange()));
	action->setCheckable(true);
	action->setProperty("bgColor", 1);
	action->setChecked(preset == 1);

	menu->addSeparator();

	widgetAction->setDefaultWidget(select);

	for (int i = 1; i < 9; i++) {
		stringstream button;
		button << "preset" << i;
		QPushButton *colorButton =
			select->findChild<QPushButton *>(button.str().c_str());
		if (preset == i + 1)
			colorButton->setStyleSheet("border: 2px solid black");

		colorButton->setProperty("bgColor", i);
		select->connect(colorButton, SIGNAL(released()), this,
				SLOT(ColorChange()));
	}

	menu->addAction(widgetAction);

	return menu;
}

ColorSelect::ColorSelect(QWidget *parent)
	: QWidget(parent), ui(new Ui::ColorSelect)
{
	ui->setupUi(this);
}

void OBSBasic::CreateSourcePopupMenu(int idx, bool preview)
{
	QMenu popup(this);
	delete previewProjectorSource;
	delete sourceProjector;
	delete scaleFilteringMenu;
	delete colorMenu;
	delete colorWidgetAction;
	delete colorSelect;
	delete deinterlaceMenu;

	if (preview) {
		QAction *action = popup.addAction(
			QTStr("Basic.Main.PreviewConextMenu.Enable"), this,
			SLOT(TogglePreview()));
		action->setCheckable(true);
		action->setChecked(
			obs_display_enabled(ui->preview->GetDisplay()));
		if (IsPreviewProgramMode())
			action->setEnabled(false);

		popup.addAction(ui->actionLockPreview);
		popup.addMenu(ui->scalingMenu);

		previewProjectorSource = new QMenu(QTStr("PreviewProjector"));
		AddProjectorMenuMonitors(previewProjectorSource, this,
					 SLOT(OpenPreviewProjector()));

		popup.addMenu(previewProjectorSource);

		QAction *previewWindow =
			popup.addAction(QTStr("PreviewWindow"), this,
					SLOT(OpenPreviewWindow()));

		popup.addAction(previewWindow);

		popup.addAction(QTStr("Screenshot.Preview"), this,
				SLOT(ScreenshotScene()));

		popup.addSeparator();
	}

	QPointer<QMenu> addSourceMenu = CreateAddSourcePopupMenu();
	if (addSourceMenu)
		popup.addMenu(addSourceMenu);

	ui->actionCopyFilters->setEnabled(false);
	ui->actionCopySource->setEnabled(false);

	if (ui->sources->MultipleBaseSelected()) {
		popup.addSeparator();
		popup.addAction(QTStr("Basic.Main.GroupItems"), ui->sources,
				SLOT(GroupSelectedItems()));

	} else if (ui->sources->GroupsSelected()) {
		popup.addSeparator();
		popup.addAction(QTStr("Basic.Main.Ungroup"), ui->sources,
				SLOT(UngroupSelectedGroups()));
	}

	popup.addSeparator();
	popup.addAction(ui->actionCopySource);
	popup.addAction(ui->actionPasteRef);
	popup.addAction(ui->actionPasteDup);
	popup.addSeparator();

	popup.addSeparator();
	popup.addAction(ui->actionCopyFilters);
	popup.addAction(ui->actionPasteFilters);
	popup.addSeparator();

	if (idx != -1) {
		if (addSourceMenu)
			popup.addSeparator();

		OBSSceneItem sceneItem = ui->sources->Get(idx);
		bool lock = obs_sceneitem_locked(sceneItem);
		obs_source_t *source = obs_sceneitem_get_source(sceneItem);
		uint32_t flags = obs_source_get_output_flags(source);
		bool isAsyncVideo = (flags & OBS_SOURCE_ASYNC_VIDEO) ==
				    OBS_SOURCE_ASYNC_VIDEO;
		bool hasAudio = (flags & OBS_SOURCE_AUDIO) == OBS_SOURCE_AUDIO;
		QAction *action;

		colorMenu = new QMenu(QTStr("ChangeBG"));
		colorWidgetAction = new QWidgetAction(colorMenu);
		colorSelect = new ColorSelect(colorMenu);
		popup.addMenu(AddBackgroundColorMenu(
			colorMenu, colorWidgetAction, colorSelect, sceneItem));
		popup.addAction(QTStr("Rename"), this,
				SLOT(EditSceneItemName()));
		popup.addAction(QTStr("Remove"), this,
				SLOT(on_actionRemoveSource_triggered()));
		popup.addSeparator();
		popup.addMenu(ui->orderMenu);
		popup.addMenu(ui->transformMenu);

		ui->actionResetTransform->setEnabled(!lock);
		ui->actionRotate90CW->setEnabled(!lock);
		ui->actionRotate90CCW->setEnabled(!lock);
		ui->actionRotate180->setEnabled(!lock);
		ui->actionFlipHorizontal->setEnabled(!lock);
		ui->actionFlipVertical->setEnabled(!lock);
		ui->actionFitToScreen->setEnabled(!lock);
		ui->actionStretchToScreen->setEnabled(!lock);
		ui->actionCenterToScreen->setEnabled(!lock);
		ui->actionVerticalCenter->setEnabled(!lock);
		ui->actionHorizontalCenter->setEnabled(!lock);

		sourceProjector = new QMenu(QTStr("SourceProjector"));
		AddProjectorMenuMonitors(sourceProjector, this,
					 SLOT(OpenSourceProjector()));

		QAction *sourceWindow = popup.addAction(
			QTStr("SourceWindow"), this, SLOT(OpenSourceWindow()));

		popup.addAction(sourceWindow);

		popup.addSeparator();

		if (hasAudio) {
			QAction *actionHideMixer =
				popup.addAction(QTStr("HideMixer"), this,
						SLOT(ToggleHideMixer()));
			actionHideMixer->setCheckable(true);
			actionHideMixer->setChecked(SourceMixerHidden(source));
		}

		if (isAsyncVideo) {
			deinterlaceMenu = new QMenu(QTStr("Deinterlacing"));
			popup.addMenu(
				AddDeinterlacingMenu(deinterlaceMenu, source));
			popup.addSeparator();
		}

		QAction *resizeOutput =
			popup.addAction(QTStr("ResizeOutputSizeOfSource"), this,
					SLOT(ResizeOutputSizeOfSource()));

		int width = obs_source_get_width(source);
		int height = obs_source_get_height(source);

		resizeOutput->setEnabled(!obs_video_active());

		if (width < 8 || height < 8)
			resizeOutput->setEnabled(false);

		scaleFilteringMenu = new QMenu(QTStr("ScaleFiltering"));
		popup.addMenu(
			AddScaleFilteringMenu(scaleFilteringMenu, sceneItem));
		popup.addSeparator();

		popup.addMenu(sourceProjector);
		popup.addAction(sourceWindow);
		popup.addAction(QTStr("Screenshot.Source"), this,
				SLOT(ScreenshotSelectedSource()));
		popup.addSeparator();

		popup.addMenu(CreateVisibilityTransitionMenu(true));
		popup.addMenu(CreateVisibilityTransitionMenu(false));
		popup.addSeparator();

		action = popup.addAction(QTStr("Interact"), this,
					 SLOT(on_actionInteract_triggered()));

		action->setEnabled(obs_source_get_output_flags(source) &
				   OBS_SOURCE_INTERACTION);

		popup.addAction(QTStr("Filters"), this, SLOT(OpenFilters()));
		popup.addAction(QTStr("Properties"), this,
				SLOT(on_actionSourceProperties_triggered()));

		ui->actionCopyFilters->setEnabled(
			obs_source_filter_count(source) > 0);
		ui->actionCopySource->setEnabled(true);
	}
	ui->actionPasteFilters->setEnabled(copyFiltersString && idx != -1);

	popup.exec(QCursor::pos());
}

void OBSBasic::on_sources_customContextMenuRequested(const QPoint &pos)
{
	if (ui->scenes->count()) {
		QModelIndex idx = ui->sources->indexAt(pos);
		CreateSourcePopupMenu(idx.row(), false);
	}
}

void OBSBasic::on_scenes_itemDoubleClicked(QListWidgetItem *witem)
{
	if (!witem)
		return;

	if (IsPreviewProgramMode()) {
		bool doubleClickSwitch =
			config_get_bool(App()->GlobalConfig(), "BasicWindow",
					"TransitionOnDoubleClick");

		if (doubleClickSwitch)
			TransitionClicked();
	}
}

void OBSBasic::AddSource(const char *id)
{
	if (id && *id) {
		OBSBasicSourceSelect sourceSelect(this, id, undo_s);
		sourceSelect.exec();
		if (sourceSelect.newSource && strcmp(id, "group") != 0) {
			CreatePropertiesWindow(sourceSelect.newSource);
		}
	}
}

QMenu *OBSBasic::CreateAddSourcePopupMenu()
{
	const char *unversioned_type;
	const char *type;
	bool foundValues = false;
	bool foundDeprecated = false;
	size_t idx = 0;

	QMenu *popup = new QMenu(QTStr("Add"), this);
	QMenu *deprecated = new QMenu(QTStr("Deprecated"), popup);

	auto getActionAfter = [](QMenu *menu, const QString &name) {
		QList<QAction *> actions = menu->actions();

		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(name) >= 0)
				return menuAction;
		}

		return (QAction *)nullptr;
	};

	auto addSource = [this, getActionAfter](QMenu *popup, const char *type,
						const char *name) {
		QString qname = QT_UTF8(name);
		QAction *popupItem = new QAction(qname, this);
		popupItem->setData(QT_UTF8(type));
		connect(popupItem, SIGNAL(triggered(bool)), this,
			SLOT(AddSourceFromAction()));

		QIcon icon;

		if (strcmp(type, "scene") == 0)
			icon = GetSceneIcon();
		else
			icon = GetSourceIcon(type);

		popupItem->setIcon(icon);

		QAction *after = getActionAfter(popup, qname);
		popup->insertAction(after, popupItem);
	};

	while (obs_enum_input_types2(idx++, &type, &unversioned_type)) {
		const char *name = obs_source_get_display_name(type);
		uint32_t caps = obs_get_source_output_flags(type);

		if ((caps & OBS_SOURCE_CAP_DISABLED) != 0)
			continue;

		if ((caps & OBS_SOURCE_DEPRECATED) == 0) {
			addSource(popup, unversioned_type, name);
		} else {
			addSource(deprecated, unversioned_type, name);
			foundDeprecated = true;
		}
		foundValues = true;
	}

	addSource(popup, "scene", Str("Basic.Scene"));

	popup->addSeparator();
	QAction *addGroup = new QAction(QTStr("Group"), this);
	addGroup->setData(QT_UTF8("group"));
	addGroup->setIcon(GetGroupIcon());
	connect(addGroup, SIGNAL(triggered(bool)), this,
		SLOT(AddSourceFromAction()));
	popup->addAction(addGroup);

	if (!foundDeprecated) {
		delete deprecated;
		deprecated = nullptr;
	}

	if (!foundValues) {
		delete popup;
		popup = nullptr;

	} else if (foundDeprecated) {
		popup->addSeparator();
		popup->addMenu(deprecated);
	}

	return popup;
}

void OBSBasic::AddSourceFromAction()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action)
		return;

	AddSource(QT_TO_UTF8(action->data().toString()));
}

void OBSBasic::AddSourcePopupMenu(const QPoint &pos)
{
	if (!GetCurrentScene()) {
		// Tell the user he needs a scene first (help beginners).
		OBSMessageBox::information(
			this, QTStr("Basic.Main.AddSourceHelp.Title"),
			QTStr("Basic.Main.AddSourceHelp.Text"));
		return;
	}

	QScopedPointer<QMenu> popup(CreateAddSourcePopupMenu());
	if (popup)
		popup->exec(pos);
}

void OBSBasic::on_actionAddSource_triggered()
{
	AddSourcePopupMenu(QCursor::pos());
}

static bool remove_items(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	vector<OBSSceneItem> &items =
		*reinterpret_cast<vector<OBSSceneItem> *>(param);

	if (obs_sceneitem_selected(item)) {
		items.emplace_back(item);
	} else if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, remove_items, &items);
	}
	return true;
};

OBSData OBSBasic::BackupScene(obs_scene_t *scene,
			      std::vector<obs_source_t *> *sources)
{
	obs_data_array_t *undo_array = obs_data_array_create();

	if (!sources) {
		obs_scene_enum_items(scene, save_undo_source_enum, undo_array);
	} else {
		for (obs_source_t *source : *sources) {
			obs_data_t *source_data = obs_save_source(source);
			obs_data_array_push_back(undo_array, source_data);
			obs_data_release(source_data);
		}
	}

	obs_data_t *scene_data = obs_save_source(obs_scene_get_source(scene));
	obs_data_array_push_back(undo_array, scene_data);
	obs_data_release(scene_data);

	OBSData data = obs_data_create();
	obs_data_release(data);

	obs_data_set_array(data, "array", undo_array);
	obs_data_get_json(data);
	obs_data_array_release(undo_array);
	return data;
}

static bool add_source_enum(obs_scene_t *, obs_sceneitem_t *item, void *p)
{
	auto sources = static_cast<std::vector<obs_source_t *> *>(p);
	sources->push_back(obs_sceneitem_get_source(item));
	return true;
}

void OBSBasic::CreateSceneUndoRedoAction(const QString &action_name,
					 OBSData undo_data, OBSData redo_data)
{
	auto undo_redo = [this](const std::string &json) {
		obs_data_t *base = obs_data_create_from_json(json.c_str());
		obs_data_array_t *array = obs_data_get_array(base, "array");
		std::vector<obs_source_t *> sources;
		std::vector<obs_source_t *> old_sources;

		/* create missing sources */
		const size_t count = obs_data_array_count(array);
		sources.reserve(count);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *data = obs_data_array_item(array, i);
			const char *name = obs_data_get_string(data, "name");

			obs_source_t *source = obs_get_source_by_name(name);
			if (!source)
				source = obs_load_source(data);

			sources.push_back(source);

			/* update scene/group settings to restore their
			* contents to their saved settings */
			obs_scene_t *scene =
				obs_group_or_scene_from_source(source);
			if (scene) {
				obs_scene_enum_items(scene, add_source_enum,
						     &old_sources);
				obs_data_t *scene_settings =
					obs_data_get_obj(data, "settings");
				obs_source_update(source, scene_settings);
				obs_data_release(scene_settings);
			}

			obs_data_release(data);
		}
		for (obs_source_t *source : old_sources)
			obs_source_addref(source);

		/* actually load sources now */
		for (obs_source_t *source : sources)
			obs_source_load2(source);

		/* release sources */
		for (obs_source_t *source : sources)
			obs_source_release(source);
		for (obs_source_t *source : old_sources)
			obs_source_release(source);

		obs_data_array_release(array);
		obs_data_release(base);

		ui->sources->RefreshItems();
	};

	const char *undo_json = obs_data_get_last_json(undo_data);
	const char *redo_json = obs_data_get_last_json(redo_data);

	undo_s.add_action(action_name, undo_redo, undo_redo, undo_json,
			  redo_json);
}

void OBSBasic::on_actionRemoveSource_triggered()
{
	vector<OBSSceneItem> items;
	OBSScene scene = GetCurrentScene();
	obs_source_t *scene_source = obs_scene_get_source(scene);

	obs_scene_enum_items(scene, remove_items, &items);

	if (!items.size())
		return;

	/* ------------------------------------- */
	/* confirm action with user              */

	bool confirmed = false;

	if (items.size() > 1) {
		QString text = QTStr("ConfirmRemove.TextMultiple")
				       .arg(QString::number(items.size()));

		QMessageBox remove_items(this);
		remove_items.setText(text);
		QPushButton *Yes = remove_items.addButton(QTStr("Yes"),
							  QMessageBox::YesRole);
		remove_items.setDefaultButton(Yes);
		remove_items.addButton(QTStr("No"), QMessageBox::NoRole);
		remove_items.setIcon(QMessageBox::Question);
		remove_items.setWindowTitle(QTStr("ConfirmRemove.Title"));
		remove_items.exec();

		confirmed = Yes == remove_items.clickedButton();
	} else {
		OBSSceneItem &item = items[0];
		obs_source_t *source = obs_sceneitem_get_source(item);
		if (source && QueryRemoveSource(source))
			confirmed = true;
	}
	if (!confirmed)
		return;

	/* ----------------------------------------------- */
	/* save undo data                                  */

	OBSData undo_data = BackupScene(scene_source);

	/* ----------------------------------------------- */
	/* remove items                                    */

	for (auto &item : items)
		obs_sceneitem_remove(item);

	/* ----------------------------------------------- */
	/* save redo data                                  */

	OBSData redo_data = BackupScene(scene_source);

	/* ----------------------------------------------- */
	/* add undo/redo action                            */

	QString action_name;
	if (items.size() > 1) {
		action_name = QTStr("Undo.Sources.Multi")
				      .arg(QString::number(items.size()));
	} else {
		QString str = QTStr("Undo.Delete");
		action_name = str.arg(obs_source_get_name(
			obs_sceneitem_get_source(items[0])));
	}

	CreateSceneUndoRedoAction(action_name, undo_data, redo_data);
}

void OBSBasic::on_actionInteract_triggered()
{
	OBSSceneItem item = GetCurrentSceneItem();
	OBSSource source = obs_sceneitem_get_source(item);

	if (source)
		CreateInteractionWindow(source);
}

void OBSBasic::on_actionSourceProperties_triggered()
{
	OBSSceneItem item = GetCurrentSceneItem();
	OBSSource source = obs_sceneitem_get_source(item);

	if (source)
		CreatePropertiesWindow(source);
}

void OBSBasic::MoveSceneItem(enum obs_order_movement movement,
			     const QString &action_name)
{
	OBSSceneItem item = GetCurrentSceneItem();
	obs_source_t *source = obs_sceneitem_get_source(item);

	if (!source)
		return;

	OBSScene scene = GetCurrentScene();
	std::vector<obs_source_t *> sources;
	if (scene != obs_sceneitem_get_scene(item))
		sources.push_back(
			obs_scene_get_source(obs_sceneitem_get_scene(item)));

	OBSData undo_data = BackupScene(scene, &sources);

	obs_sceneitem_set_order(item, movement);

	const char *source_name = obs_source_get_name(source);
	const char *scene_name =
		obs_source_get_name(obs_scene_get_source(scene));

	OBSData redo_data = BackupScene(scene, &sources);
	CreateSceneUndoRedoAction(action_name.arg(source_name, scene_name),
				  undo_data, redo_data);
}

void OBSBasic::on_actionSourceUp_triggered()
{
	MoveSceneItem(OBS_ORDER_MOVE_UP, QTStr("Undo.MoveUp"));
}

void OBSBasic::on_actionSourceDown_triggered()
{
	MoveSceneItem(OBS_ORDER_MOVE_DOWN, QTStr("Undo.MoveDown"));
}

void OBSBasic::on_actionMoveUp_triggered()
{
	MoveSceneItem(OBS_ORDER_MOVE_UP, QTStr("Undo.MoveUp"));
}

void OBSBasic::on_actionMoveDown_triggered()
{
	MoveSceneItem(OBS_ORDER_MOVE_DOWN, QTStr("Undo.MoveDown"));
}

void OBSBasic::on_actionMoveToTop_triggered()
{
	MoveSceneItem(OBS_ORDER_MOVE_TOP, QTStr("Undo.MoveToTop"));
}

void OBSBasic::on_actionMoveToBottom_triggered()
{
	MoveSceneItem(OBS_ORDER_MOVE_BOTTOM, QTStr("Undo.MoveToBottom"));
}

static BPtr<char> ReadLogFile(const char *subdir, const char *log)
{
	char logDir[512];
	if (GetConfigPath(logDir, sizeof(logDir), subdir) <= 0)
		return nullptr;

	string path = logDir;
	path += "/";
	path += log;

	BPtr<char> file = os_quick_read_utf8_file(path.c_str());
	if (!file)
		blog(LOG_WARNING, "Failed to read log file %s", path.c_str());

	return file;
}

void OBSBasic::UploadLog(const char *subdir, const char *file, const bool crash)
{
	BPtr<char> fileString{ReadLogFile(subdir, file)};

	if (!fileString)
		return;

	if (!*fileString)
		return;

	ui->menuLogFiles->setEnabled(false);
#if defined(_WIN32)
	ui->menuCrashLogs->setEnabled(false);
#endif

	stringstream ss;
	ss << "OBS " << App()->GetVersionString() << " log file uploaded at "
	   << CurrentDateTimeString() << "\n\n"
	   << fileString;

	if (logUploadThread) {
		logUploadThread->wait();
	}

	RemoteTextThread *thread =
		new RemoteTextThread("https://obsproject.com/logs/upload",
				     "text/plain", ss.str().c_str());

	logUploadThread.reset(thread);
	if (crash) {
		connect(thread, &RemoteTextThread::Result, this,
			&OBSBasic::crashUploadFinished);
	} else {
		connect(thread, &RemoteTextThread::Result, this,
			&OBSBasic::logUploadFinished);
	}
	logUploadThread->start();
}

void OBSBasic::on_actionShowLogs_triggered()
{
	char logDir[512];
	if (GetConfigPath(logDir, sizeof(logDir), "obs-studio/logs") <= 0)
		return;

	QUrl url = QUrl::fromLocalFile(QT_UTF8(logDir));
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionUploadCurrentLog_triggered()
{
	UploadLog("obs-studio/logs", App()->GetCurrentLog(), false);
}

void OBSBasic::on_actionUploadLastLog_triggered()
{
	UploadLog("obs-studio/logs", App()->GetLastLog(), false);
}

void OBSBasic::on_actionViewCurrentLog_triggered()
{
	if (!logView)
		logView = new OBSLogViewer();

	if (!logView->isVisible()) {
		logView->setVisible(true);
	} else {
		logView->setWindowState(
			(logView->windowState() & ~Qt::WindowMinimized) |
			Qt::WindowActive);
		logView->activateWindow();
		logView->raise();
	}
}

void OBSBasic::on_actionShowCrashLogs_triggered()
{
	char logDir[512];
	if (GetConfigPath(logDir, sizeof(logDir), "obs-studio/crashes") <= 0)
		return;

	QUrl url = QUrl::fromLocalFile(QT_UTF8(logDir));
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionUploadLastCrashLog_triggered()
{
	UploadLog("obs-studio/crashes", App()->GetLastCrashLog(), true);
}

void OBSBasic::on_actionCheckForUpdates_triggered()
{
	CheckForUpdates(true);
}

void OBSBasic::logUploadFinished(const QString &text, const QString &error)
{
	ui->menuLogFiles->setEnabled(true);
#if defined(_WIN32)
	ui->menuCrashLogs->setEnabled(true);
#endif

	if (text.isEmpty()) {
		OBSMessageBox::critical(
			this, QTStr("LogReturnDialog.ErrorUploadingLog"),
			error);
		return;
	}
	openLogDialog(text, false);
}

void OBSBasic::crashUploadFinished(const QString &text, const QString &error)
{
	ui->menuLogFiles->setEnabled(true);
#if defined(_WIN32)
	ui->menuCrashLogs->setEnabled(true);
#endif

	if (text.isEmpty()) {
		OBSMessageBox::critical(
			this, QTStr("LogReturnDialog.ErrorUploadingLog"),
			error);
		return;
	}
	openLogDialog(text, true);
}

void OBSBasic::openLogDialog(const QString &text, const bool crash)
{

	obs_data_t *returnData = obs_data_create_from_json(QT_TO_UTF8(text));
	string resURL = obs_data_get_string(returnData, "url");
	QString logURL = resURL.c_str();
	obs_data_release(returnData);

	OBSLogReply logDialog(this, logURL, crash);
	logDialog.exec();
}

static void RenameListItem(OBSBasic *parent, QListWidget *listWidget,
			   obs_source_t *source, const string &name)
{
	const char *prevName = obs_source_get_name(source);
	if (name == prevName)
		return;

	obs_source_t *foundSource = obs_get_source_by_name(name.c_str());
	QListWidgetItem *listItem = listWidget->currentItem();

	if (foundSource || name.empty()) {
		listItem->setText(QT_UTF8(prevName));

		if (foundSource) {
			OBSMessageBox::warning(parent,
					       QTStr("NameExists.Title"),
					       QTStr("NameExists.Text"));
		} else if (name.empty()) {
			OBSMessageBox::warning(parent,
					       QTStr("NoNameEntered.Title"),
					       QTStr("NoNameEntered.Text"));
		}

		obs_source_release(foundSource);
	} else {
		auto undo = [prev = std::string(prevName)](
				    const std::string &data) {
			obs_source_t *source =
				obs_get_source_by_name(data.c_str());
			obs_source_set_name(source, prev.c_str());
			obs_source_release(source);
		};

		auto redo = [name](const std::string &data) {
			obs_source_t *source =
				obs_get_source_by_name(data.c_str());
			obs_source_set_name(source, name.c_str());
			obs_source_release(source);
		};

		std::string undo_data(name);
		std::string redo_data(prevName);
		parent->undo_s.add_action(
			QTStr("Undo.Rename").arg(name.c_str()), undo, redo,
			undo_data, redo_data);

		listItem->setText(QT_UTF8(name.c_str()));
		obs_source_set_name(source, name.c_str());
	}
}

void OBSBasic::SceneNameEdited(QWidget *editor,
			       QAbstractItemDelegate::EndEditHint endHint)
{
	OBSScene scene = GetCurrentScene();
	QLineEdit *edit = qobject_cast<QLineEdit *>(editor);
	string text = QT_TO_UTF8(edit->text().trimmed());

	if (!scene)
		return;

	obs_source_t *source = obs_scene_get_source(scene);
	RenameListItem(this, ui->scenes, source, text);

	ui->scenesDock->addAction(renameScene);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);

	UNUSED_PARAMETER(endHint);
}

void OBSBasic::OpenFilters()
{
	OBSSceneItem item = GetCurrentSceneItem();
	OBSSource source = obs_sceneitem_get_source(item);

	CreateFiltersWindow(source);
}

void OBSBasic::OpenSceneFilters()
{
	OBSScene scene = GetCurrentScene();
	OBSSource source = obs_scene_get_source(scene);

	CreateFiltersWindow(source);
}

#define RECORDING_START \
	"==== Recording Start ==============================================="
#define RECORDING_STOP \
	"==== Recording Stop ================================================"
#define REPLAY_BUFFER_START \
	"==== Replay Buffer Start ==========================================="
#define REPLAY_BUFFER_STOP \
	"==== Replay Buffer Stop ============================================"
#define STREAMING_START \
	"==== Streaming Start ==============================================="
#define STREAMING_STOP \
	"==== Streaming Stop ================================================"
#define VIRTUAL_CAM_START \
	"==== Virtual Camera Start =========================================="
#define VIRTUAL_CAM_STOP \
	"==== Virtual Camera Stop ==========================================="

void OBSBasic::DisplayStreamStartError()
{
	QString message = !outputHandler->lastError.empty()
				  ? QTStr(outputHandler->lastError.c_str())
				  : QTStr("Output.StartFailedGeneric");
	ui->streamButton->setText(QTStr("Basic.Main.StartStreaming"));
	ui->streamButton->setEnabled(true);
	ui->streamButton->setChecked(false);

	if (sysTrayStream) {
		sysTrayStream->setText(ui->streamButton->text());
		sysTrayStream->setEnabled(true);
	}

	QMessageBox::critical(this, QTStr("Output.StartStreamFailed"), message);
}

void OBSBasic::StartStreaming()
{
	if (outputHandler->StreamingActive())
		return;
	if (disableOutputsRef)
		return;

	if (!outputHandler->SetupStreaming(service)) {
		DisplayStreamStartError();
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STARTING);

	SaveProject();

	ui->streamButton->setEnabled(false);
	ui->streamButton->setChecked(false);
	ui->streamButton->setText(QTStr("Basic.Main.Connecting"));

	if (sysTrayStream) {
		sysTrayStream->setEnabled(false);
		sysTrayStream->setText(ui->streamButton->text());
	}

	if (!outputHandler->StartStreaming(service)) {
		DisplayStreamStartError();
		return;
	}

	bool recordWhenStreaming = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	if (recordWhenStreaming)
		StartRecording();

	bool replayBufferWhileStreaming = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	if (replayBufferWhileStreaming)
		StartReplayBuffer();
}

#ifdef _WIN32
static inline void UpdateProcessPriority()
{
	const char *priority = config_get_string(App()->GlobalConfig(),
						 "General", "ProcessPriority");
	if (priority && strcmp(priority, "Normal") != 0)
		SetProcessPriority(priority);
}

static inline void ClearProcessPriority()
{
	const char *priority = config_get_string(App()->GlobalConfig(),
						 "General", "ProcessPriority");
	if (priority && strcmp(priority, "Normal") != 0)
		SetProcessPriority("Normal");
}
#else
#define UpdateProcessPriority() \
	do {                    \
	} while (false)
#define ClearProcessPriority() \
	do {                   \
	} while (false)
#endif

inline void OBSBasic::OnActivate()
{
	if (ui->profileMenu->isEnabled()) {
		ui->profileMenu->setEnabled(false);
		ui->autoConfigure->setEnabled(false);
		App()->IncrementSleepInhibition();
		UpdateProcessPriority();

		if (trayIcon && trayIcon->isVisible()) {
#ifdef __APPLE__
			QIcon trayMask =
				QIcon(":/res/images/tray_active_macos.png");
			trayMask.setIsMask(true);
			trayIcon->setIcon(
				QIcon::fromTheme("obs-tray", trayMask));
#else
			trayIcon->setIcon(QIcon::fromTheme(
				"obs-tray-active",
				QIcon(":/res/images/tray_active.png")));
#endif
		}
	}
}

extern volatile bool recording_paused;
extern volatile bool replaybuf_active;

inline void OBSBasic::OnDeactivate()
{
	if (!outputHandler->Active() && !ui->profileMenu->isEnabled()) {
		ui->profileMenu->setEnabled(true);
		ui->autoConfigure->setEnabled(true);
		App()->DecrementSleepInhibition();
		ClearProcessPriority();

		if (trayIcon && trayIcon->isVisible()) {
#ifdef __APPLE__
			QIcon trayIconFile =
				QIcon(":/res/images/obs_macos.png");
			trayIconFile.setIsMask(true);
#else
			QIcon trayIconFile = QIcon(":/res/images/obs.png");
#endif
			trayIcon->setIcon(
				QIcon::fromTheme("obs-tray", trayIconFile));
		}
	} else if (outputHandler->Active() && trayIcon &&
		   trayIcon->isVisible()) {
		if (os_atomic_load_bool(&recording_paused)) {
#ifdef __APPLE__
			QIcon trayIconFile =
				QIcon(":/res/images/obs_paused_macos.png");
			trayIconFile.setIsMask(true);
#else
			QIcon trayIconFile =
				QIcon(":/res/images/obs_paused.png");
#endif
			trayIcon->setIcon(QIcon::fromTheme("obs-tray-paused",
							   trayIconFile));
		} else {
#ifdef __APPLE__
			QIcon trayIconFile =
				QIcon(":/res/images/tray_active_macos.png");
			trayIconFile.setIsMask(true);
#else
			QIcon trayIconFile =
				QIcon(":/res/images/tray_active.png");
#endif
			trayIcon->setIcon(QIcon::fromTheme("obs-tray-active",
							   trayIconFile));
		}
	}
}

void OBSBasic::StopStreaming()
{
	SaveProject();

	if (outputHandler->StreamingActive())
		outputHandler->StopStreaming(streamingStopping);

	OnDeactivate();

	bool recordWhenStreaming = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool keepRecordingWhenStreamStops =
		config_get_bool(GetGlobalConfig(), "BasicWindow",
				"KeepRecordingWhenStreamStops");
	if (recordWhenStreaming && !keepRecordingWhenStreamStops)
		StopRecording();

	bool replayBufferWhileStreaming = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	bool keepReplayBufferStreamStops =
		config_get_bool(GetGlobalConfig(), "BasicWindow",
				"KeepReplayBufferStreamStops");
	if (replayBufferWhileStreaming && !keepReplayBufferStreamStops)
		StopReplayBuffer();
}

void OBSBasic::ForceStopStreaming()
{
	SaveProject();

	if (outputHandler->StreamingActive())
		outputHandler->StopStreaming(true);

	OnDeactivate();

	bool recordWhenStreaming = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool keepRecordingWhenStreamStops =
		config_get_bool(GetGlobalConfig(), "BasicWindow",
				"KeepRecordingWhenStreamStops");
	if (recordWhenStreaming && !keepRecordingWhenStreamStops)
		StopRecording();

	bool replayBufferWhileStreaming = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	bool keepReplayBufferStreamStops =
		config_get_bool(GetGlobalConfig(), "BasicWindow",
				"KeepReplayBufferStreamStops");
	if (replayBufferWhileStreaming && !keepReplayBufferStreamStops)
		StopReplayBuffer();
}

void OBSBasic::StreamDelayStarting(int sec)
{
	ui->streamButton->setText(QTStr("Basic.Main.StopStreaming"));
	ui->streamButton->setEnabled(true);
	ui->streamButton->setChecked(true);

	if (sysTrayStream) {
		sysTrayStream->setText(ui->streamButton->text());
		sysTrayStream->setEnabled(true);
	}

	if (!startStreamMenu.isNull())
		startStreamMenu->deleteLater();

	startStreamMenu = new QMenu();
	startStreamMenu->addAction(QTStr("Basic.Main.StopStreaming"), this,
				   SLOT(StopStreaming()));
	startStreamMenu->addAction(QTStr("Basic.Main.ForceStopStreaming"), this,
				   SLOT(ForceStopStreaming()));
	ui->streamButton->setMenu(startStreamMenu);

	ui->statusbar->StreamDelayStarting(sec);

	OnActivate();
}

void OBSBasic::StreamDelayStopping(int sec)
{
	ui->streamButton->setText(QTStr("Basic.Main.StartStreaming"));
	ui->streamButton->setEnabled(true);
	ui->streamButton->setChecked(false);

	if (sysTrayStream) {
		sysTrayStream->setText(ui->streamButton->text());
		sysTrayStream->setEnabled(true);
	}

	if (!startStreamMenu.isNull())
		startStreamMenu->deleteLater();

	startStreamMenu = new QMenu();
	startStreamMenu->addAction(QTStr("Basic.Main.StartStreaming"), this,
				   SLOT(StartStreaming()));
	startStreamMenu->addAction(QTStr("Basic.Main.ForceStopStreaming"), this,
				   SLOT(ForceStopStreaming()));
	ui->streamButton->setMenu(startStreamMenu);

	ui->statusbar->StreamDelayStopping(sec);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STOPPING);
}

void OBSBasic::StreamingStart()
{
	ui->streamButton->setText(QTStr("Basic.Main.StopStreaming"));
	ui->streamButton->setEnabled(true);
	ui->streamButton->setChecked(true);
	ui->statusbar->StreamStarted(outputHandler->streamOutput);

	if (sysTrayStream) {
		sysTrayStream->setText(ui->streamButton->text());
		sysTrayStream->setEnabled(true);
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STARTED);

	OnActivate();

	blog(LOG_INFO, STREAMING_START);
}

void OBSBasic::StreamStopping()
{
	ui->streamButton->setText(QTStr("Basic.Main.StoppingStreaming"));

	if (sysTrayStream)
		sysTrayStream->setText(ui->streamButton->text());

	streamingStopping = true;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STOPPING);
}

void OBSBasic::StreamingStop(int code, QString last_error)
{
	const char *errorDescription = "";
	DStr errorMessage;
	bool use_last_error = false;
	bool encode_error = false;

	switch (code) {
	case OBS_OUTPUT_BAD_PATH:
		errorDescription = Str("Output.ConnectFail.BadPath");
		break;

	case OBS_OUTPUT_CONNECT_FAILED:
		use_last_error = true;
		errorDescription = Str("Output.ConnectFail.ConnectFailed");
		break;

	case OBS_OUTPUT_INVALID_STREAM:
		errorDescription = Str("Output.ConnectFail.InvalidStream");
		break;

	case OBS_OUTPUT_ENCODE_ERROR:
		encode_error = true;
		break;

	default:
	case OBS_OUTPUT_ERROR:
		use_last_error = true;
		errorDescription = Str("Output.ConnectFail.Error");
		break;

	case OBS_OUTPUT_DISCONNECTED:
		/* doesn't happen if output is set to reconnect.  note that
		 * reconnects are handled in the output, not in the UI */
		use_last_error = true;
		errorDescription = Str("Output.ConnectFail.Disconnected");
	}

	if (use_last_error && !last_error.isEmpty())
		dstr_printf(errorMessage, "%s\n\n%s", errorDescription,
			    QT_TO_UTF8(last_error));
	else
		dstr_copy(errorMessage, errorDescription);

	ui->statusbar->StreamStopped();

	ui->streamButton->setText(QTStr("Basic.Main.StartStreaming"));
	ui->streamButton->setEnabled(true);
	ui->streamButton->setChecked(false);

	if (sysTrayStream) {
		sysTrayStream->setText(ui->streamButton->text());
		sysTrayStream->setEnabled(true);
	}

	streamingStopping = false;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_STREAMING_STOPPED);

	OnDeactivate();

	blog(LOG_INFO, STREAMING_STOP);

	if (encode_error) {
		OBSMessageBox::information(
			this, QTStr("Output.StreamEncodeError.Title"),
			QTStr("Output.StreamEncodeError.Msg"));

	} else if (code != OBS_OUTPUT_SUCCESS && isVisible()) {
		OBSMessageBox::information(this,
					   QTStr("Output.ConnectFail.Title"),
					   QT_UTF8(errorMessage));

	} else if (code != OBS_OUTPUT_SUCCESS && !isVisible()) {
		SysTrayNotify(QT_UTF8(errorDescription),
			      QSystemTrayIcon::Warning);
	}

	if (!startStreamMenu.isNull()) {
		ui->streamButton->setMenu(nullptr);
		startStreamMenu->deleteLater();
		startStreamMenu = nullptr;
	}
}

void OBSBasic::AutoRemux()
{
	bool autoRemux = config_get_bool(Config(), "Video", "AutoRemux");

	if (!autoRemux)
		return;

	const char *recType = config_get_string(Config(), "AdvOut", "RecType");

	bool ffmpegOutput = astrcmpi(recType, "FFmpeg") == 0;

	if (ffmpegOutput)
		return;

	QString input = outputHandler->lastRecordingPath.c_str();
	if (input.isEmpty())
		return;

	QFileInfo fi(input);
	QString suffix = fi.suffix();

	/* do not remux if lossless */
	if (suffix.compare("avi", Qt::CaseInsensitive) == 0) {
		return;
	}

	QString path = fi.path();

	QString output = input;
	output.resize(output.size() - suffix.size());
	output += "mp4";

	OBSRemux *remux = new OBSRemux(QT_TO_UTF8(path), this, true);
	remux->show();
	remux->AutoRemux(input, output);
}

void OBSBasic::StartRecording()
{
	if (outputHandler->RecordingActive())
		return;
	if (disableOutputsRef)
		return;

	if (!OutputPathValid()) {
		OutputPathInvalidMessage();
		ui->recordButton->setChecked(false);
		return;
	}

	if (LowDiskSpace()) {
		DiskSpaceMessage();
		ui->recordButton->setChecked(false);
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STARTING);

	SaveProject();

	if (!outputHandler->StartRecording())
		ui->recordButton->setChecked(false);
}

void OBSBasic::RecordStopping()
{
	ui->recordButton->setText(QTStr("Basic.Main.StoppingRecording"));

	if (sysTrayRecord)
		sysTrayRecord->setText(ui->recordButton->text());

	recordingStopping = true;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STOPPING);
}

void OBSBasic::StopRecording()
{
	SaveProject();

	if (outputHandler->RecordingActive())
		outputHandler->StopRecording(recordingStopping);

	OnDeactivate();
}

void OBSBasic::RecordingStart()
{
	ui->statusbar->RecordingStarted(outputHandler->fileOutput);
	ui->recordButton->setText(QTStr("Basic.Main.StopRecording"));
	ui->recordButton->setChecked(true);

	if (sysTrayRecord)
		sysTrayRecord->setText(ui->recordButton->text());

	recordingStopping = false;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STARTED);

	if (!diskFullTimer->isActive())
		diskFullTimer->start(1000);

	OnActivate();
	UpdatePause();

	blog(LOG_INFO, RECORDING_START);
}

void OBSBasic::RecordingStop(int code, QString last_error)
{
	ui->statusbar->RecordingStopped();
	ui->recordButton->setText(QTStr("Basic.Main.StartRecording"));
	ui->recordButton->setChecked(false);

	if (sysTrayRecord)
		sysTrayRecord->setText(ui->recordButton->text());

	blog(LOG_INFO, RECORDING_STOP);

	if (code == OBS_OUTPUT_UNSUPPORTED && isVisible()) {
		OBSMessageBox::critical(this, QTStr("Output.RecordFail.Title"),
					QTStr("Output.RecordFail.Unsupported"));

	} else if (code == OBS_OUTPUT_ENCODE_ERROR && isVisible()) {
		OBSMessageBox::warning(
			this, QTStr("Output.RecordError.Title"),
			QTStr("Output.RecordError.EncodeErrorMsg"));

	} else if (code == OBS_OUTPUT_NO_SPACE && isVisible()) {
		OBSMessageBox::warning(this,
				       QTStr("Output.RecordNoSpace.Title"),
				       QTStr("Output.RecordNoSpace.Msg"));

	} else if (code != OBS_OUTPUT_SUCCESS && isVisible()) {

		const char *errorDescription;
		DStr errorMessage;
		bool use_last_error = true;

		errorDescription = Str("Output.RecordError.Msg");

		if (use_last_error && !last_error.isEmpty())
			dstr_printf(errorMessage, "%s\n\n%s", errorDescription,
				    QT_TO_UTF8(last_error));
		else
			dstr_copy(errorMessage, errorDescription);

		OBSMessageBox::critical(this, QTStr("Output.RecordError.Title"),
					QT_UTF8(errorMessage));

	} else if (code == OBS_OUTPUT_UNSUPPORTED && !isVisible()) {
		SysTrayNotify(QTStr("Output.RecordFail.Unsupported"),
			      QSystemTrayIcon::Warning);

	} else if (code == OBS_OUTPUT_NO_SPACE && !isVisible()) {
		SysTrayNotify(QTStr("Output.RecordNoSpace.Msg"),
			      QSystemTrayIcon::Warning);

	} else if (code != OBS_OUTPUT_SUCCESS && !isVisible()) {
		SysTrayNotify(QTStr("Output.RecordError.Msg"),
			      QSystemTrayIcon::Warning);
	} else if (code == OBS_OUTPUT_SUCCESS) {
		if (outputHandler) {
			std::string path = outputHandler->lastRecordingPath;
			QString str = QTStr("Basic.StatusBar.RecordingSavedTo");
			ShowStatusBarMessage(str.arg(QT_UTF8(path.c_str())));
		}
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_RECORDING_STOPPED);

	if (diskFullTimer->isActive())
		diskFullTimer->stop();

	AutoRemux();

	OnDeactivate();
	UpdatePause(false);
}

void OBSBasic::ShowReplayBufferPauseWarning()
{
	auto msgBox = []() {
		QMessageBox msgbox(App()->GetMainWindow());
		msgbox.setWindowTitle(QTStr("Output.ReplayBuffer."
					    "PauseWarning.Title"));
		msgbox.setText(QTStr("Output.ReplayBuffer."
				     "PauseWarning.Text"));
		msgbox.setIcon(QMessageBox::Icon::Information);
		msgbox.addButton(QMessageBox::Ok);

		QCheckBox *cb = new QCheckBox(QTStr("DoNotShowAgain"));
		msgbox.setCheckBox(cb);

		msgbox.exec();

		if (cb->isChecked()) {
			config_set_bool(App()->GlobalConfig(), "General",
					"WarnedAboutReplayBufferPausing", true);
			config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		}
	};

	bool warned = config_get_bool(App()->GlobalConfig(), "General",
				      "WarnedAboutReplayBufferPausing");
	if (!warned) {
		QMetaObject::invokeMethod(App(), "Exec", Qt::QueuedConnection,
					  Q_ARG(VoidFunc, msgBox));
	}
}

void OBSBasic::StartReplayBuffer()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;
	if (outputHandler->ReplayBufferActive())
		return;
	if (disableOutputsRef)
		return;

	if (!UIValidation::NoSourcesConfirmation(this)) {
		replayBufferButton->setChecked(false);
		return;
	}

	if (LowDiskSpace()) {
		DiskSpaceMessage();
		replayBufferButton->setChecked(false);
		return;
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING);

	SaveProject();

	if (!outputHandler->StartReplayBuffer()) {
		replayBufferButton->setChecked(false);
	} else if (os_atomic_load_bool(&recording_paused)) {
		ShowReplayBufferPauseWarning();
	}
}

void OBSBasic::ReplayBufferStopping()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	replayBufferButton->setText(QTStr("Basic.Main.StoppingReplayBuffer"));

	if (sysTrayReplayBuffer)
		sysTrayReplayBuffer->setText(replayBufferButton->text());

	replayBufferStopping = true;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING);
}

void OBSBasic::StopReplayBuffer()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	SaveProject();

	if (outputHandler->ReplayBufferActive())
		outputHandler->StopReplayBuffer(replayBufferStopping);

	OnDeactivate();
}

void OBSBasic::ReplayBufferStart()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	replayBufferButton->setText(QTStr("Basic.Main.StopReplayBuffer"));
	replayBufferButton->setChecked(true);

	if (sysTrayReplayBuffer)
		sysTrayReplayBuffer->setText(replayBufferButton->text());

	replayBufferStopping = false;
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED);

	OnActivate();
	UpdateReplayBuffer();

	blog(LOG_INFO, REPLAY_BUFFER_START);
}

void OBSBasic::ReplayBufferSave()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;
	if (!outputHandler->ReplayBufferActive())
		return;

	calldata_t cd = {0};
	proc_handler_t *ph =
		obs_output_get_proc_handler(outputHandler->replayBuffer);
	proc_handler_call(ph, "save", &cd);
	calldata_free(&cd);
}

void OBSBasic::ReplayBufferSaved()
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;
	if (!outputHandler->ReplayBufferActive())
		return;

	calldata_t cd = {0};
	proc_handler_t *ph =
		obs_output_get_proc_handler(outputHandler->replayBuffer);
	proc_handler_call(ph, "get_last_replay", &cd);
	QString path = QT_UTF8(calldata_string(&cd, "path"));
	QString msg = QTStr("Basic.StatusBar.ReplayBufferSavedTo").arg(path);
	ShowStatusBarMessage(msg);
	calldata_free(&cd);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED);
}

void OBSBasic::ReplayBufferStop(int code)
{
	if (!outputHandler || !outputHandler->replayBuffer)
		return;

	replayBufferButton->setText(QTStr("Basic.Main.StartReplayBuffer"));
	replayBufferButton->setChecked(false);

	if (sysTrayReplayBuffer)
		sysTrayReplayBuffer->setText(replayBufferButton->text());

	blog(LOG_INFO, REPLAY_BUFFER_STOP);

	if (code == OBS_OUTPUT_UNSUPPORTED && isVisible()) {
		OBSMessageBox::critical(this, QTStr("Output.RecordFail.Title"),
					QTStr("Output.RecordFail.Unsupported"));

	} else if (code == OBS_OUTPUT_NO_SPACE && isVisible()) {
		OBSMessageBox::warning(this,
				       QTStr("Output.RecordNoSpace.Title"),
				       QTStr("Output.RecordNoSpace.Msg"));

	} else if (code != OBS_OUTPUT_SUCCESS && isVisible()) {
		OBSMessageBox::critical(this, QTStr("Output.RecordError.Title"),
					QTStr("Output.RecordError.Msg"));

	} else if (code == OBS_OUTPUT_UNSUPPORTED && !isVisible()) {
		SysTrayNotify(QTStr("Output.RecordFail.Unsupported"),
			      QSystemTrayIcon::Warning);

	} else if (code == OBS_OUTPUT_NO_SPACE && !isVisible()) {
		SysTrayNotify(QTStr("Output.RecordNoSpace.Msg"),
			      QSystemTrayIcon::Warning);

	} else if (code != OBS_OUTPUT_SUCCESS && !isVisible()) {
		SysTrayNotify(QTStr("Output.RecordError.Msg"),
			      QSystemTrayIcon::Warning);
	}

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);

	OnDeactivate();
	UpdateReplayBuffer(false);
}

void OBSBasic::StartVirtualCam()
{
	if (!outputHandler || !outputHandler->virtualCam)
		return;
	if (outputHandler->VirtualCamActive())
		return;
	if (disableOutputsRef)
		return;

	SaveProject();

	if (!outputHandler->StartVirtualCam()) {
		vcamButton->setChecked(false);
	}
}

void OBSBasic::StopVirtualCam()
{
	if (!outputHandler || !outputHandler->virtualCam)
		return;

	SaveProject();

	if (outputHandler->VirtualCamActive())
		outputHandler->StopVirtualCam();

	OnDeactivate();
}

void OBSBasic::OnVirtualCamStart()
{
	if (!outputHandler || !outputHandler->virtualCam)
		return;

	vcamButton->setText(QTStr("Basic.Main.StopVirtualCam"));
	if (sysTrayVirtualCam)
		sysTrayVirtualCam->setText(QTStr("Basic.Main.StopVirtualCam"));
	vcamButton->setChecked(true);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED);

	OnActivate();

	blog(LOG_INFO, VIRTUAL_CAM_START);
}

void OBSBasic::OnVirtualCamStop(int)
{
	if (!outputHandler || !outputHandler->virtualCam)
		return;

	vcamButton->setText(QTStr("Basic.Main.StartVirtualCam"));
	if (sysTrayVirtualCam)
		sysTrayVirtualCam->setText(QTStr("Basic.Main.StartVirtualCam"));
	vcamButton->setChecked(false);

	if (api)
		api->on_event(OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED);

	blog(LOG_INFO, VIRTUAL_CAM_STOP);

	OnDeactivate();
}

void OBSBasic::on_streamButton_clicked()
{
	if (outputHandler->StreamingActive()) {
		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow",
					       "WarnBeforeStoppingStream");

		if (confirm && isVisible()) {
			QMessageBox::StandardButton button =
				OBSMessageBox::question(
					this, QTStr("ConfirmStop.Title"),
					QTStr("ConfirmStop.Text"),
					QMessageBox::Yes | QMessageBox::No,
					QMessageBox::No);

			if (button == QMessageBox::No) {
				ui->streamButton->setChecked(true);
				return;
			}
		}

		StopStreaming();
	} else {
		if (!UIValidation::NoSourcesConfirmation(this)) {
			ui->streamButton->setChecked(false);
			return;
		}

		auto action =
			UIValidation::StreamSettingsConfirmation(this, service);
		switch (action) {
		case StreamSettingsAction::ContinueStream:
			break;
		case StreamSettingsAction::OpenSettings:
			on_action_Settings_triggered();
			ui->streamButton->setChecked(false);
			return;
		case StreamSettingsAction::Cancel:
			ui->streamButton->setChecked(false);
			return;
		}

		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow",
					       "WarnBeforeStartingStream");

		bool bwtest = false;

		if (this->auth) {
			obs_data_t *settings =
				obs_service_get_settings(service);
			bwtest = obs_data_get_bool(settings, "bwtest");
			obs_data_release(settings);
		}

		if (bwtest && isVisible()) {
			QMessageBox::StandardButton button =
				OBSMessageBox::question(
					this, QTStr("ConfirmBWTest.Title"),
					QTStr("ConfirmBWTest.Text"));

			if (button == QMessageBox::No) {
				ui->streamButton->setChecked(false);
				return;
			}
		} else if (confirm && isVisible()) {
			QMessageBox::StandardButton button =
				OBSMessageBox::question(
					this, QTStr("ConfirmStart.Title"),
					QTStr("ConfirmStart.Text"),
					QMessageBox::Yes | QMessageBox::No,
					QMessageBox::No);

			if (button == QMessageBox::No) {
				ui->streamButton->setChecked(false);
				return;
			}
		}

		StartStreaming();
	}
}

void OBSBasic::on_recordButton_clicked()
{
	if (outputHandler->RecordingActive()) {
		bool confirm = config_get_bool(GetGlobalConfig(), "BasicWindow",
					       "WarnBeforeStoppingRecord");

		if (confirm && isVisible()) {
			QMessageBox::StandardButton button =
				OBSMessageBox::question(
					this, QTStr("ConfirmStopRecord.Title"),
					QTStr("ConfirmStopRecord.Text"),
					QMessageBox::Yes | QMessageBox::No,
					QMessageBox::No);

			if (button == QMessageBox::No) {
				ui->recordButton->setChecked(true);
				return;
			}
		}
		StopRecording();
	} else {
		if (!UIValidation::NoSourcesConfirmation(this)) {
			ui->recordButton->setChecked(false);
			return;
		}

		StartRecording();
	}
}

void OBSBasic::VCamButtonClicked()
{
	if (outputHandler->VirtualCamActive()) {
		StopVirtualCam();
	} else {
		if (!UIValidation::NoSourcesConfirmation(this)) {
			vcamButton->setChecked(false);
			return;
		}

		StartVirtualCam();
	}
}

void OBSBasic::on_settingsButton_clicked()
{
	on_action_Settings_triggered();
}

void OBSBasic::on_actionHelpPortal_triggered()
{
	QUrl url = QUrl("https://obsproject.com/help", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionWebsite_triggered()
{
	QUrl url = QUrl("https://obsproject.com", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionDiscord_triggered()
{
	QUrl url = QUrl("https://obsproject.com/discord", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}

void OBSBasic::on_actionShowSettingsFolder_triggered()
{
	char path[512];
	int ret = GetConfigPath(path, 512, "obs-studio");
	if (ret <= 0)
		return;

	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void OBSBasic::on_actionShowProfileFolder_triggered()
{
	char path[512];
	int ret = GetProfilePath(path, 512, "");
	if (ret <= 0)
		return;

	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

int OBSBasic::GetTopSelectedSourceItem()
{
	QModelIndexList selectedItems =
		ui->sources->selectionModel()->selectedIndexes();
	return selectedItems.count() ? selectedItems[0].row() : -1;
}

QModelIndexList OBSBasic::GetAllSelectedSourceItems()
{
	return ui->sources->selectionModel()->selectedIndexes();
}

void OBSBasic::on_preview_customContextMenuRequested(const QPoint &pos)
{
	CreateSourcePopupMenu(GetTopSelectedSourceItem(), true);

	UNUSED_PARAMETER(pos);
}

void OBSBasic::ProgramViewContextMenuRequested(const QPoint &)
{
	QMenu popup(this);
	QPointer<QMenu> studioProgramProjector;

	studioProgramProjector = new QMenu(QTStr("StudioProgramProjector"));
	AddProjectorMenuMonitors(studioProgramProjector, this,
				 SLOT(OpenStudioProgramProjector()));

	popup.addMenu(studioProgramProjector);

	QAction *studioProgramWindow =
		popup.addAction(QTStr("StudioProgramWindow"), this,
				SLOT(OpenStudioProgramWindow()));

	popup.addAction(studioProgramWindow);

	popup.addAction(QTStr("Screenshot.StudioProgram"), this,
			SLOT(ScreenshotProgram()));

	popup.exec(QCursor::pos());
}

void OBSBasic::PreviewDisabledMenu(const QPoint &pos)
{
	QMenu popup(this);
	delete previewProjectorMain;

	QAction *action =
		popup.addAction(QTStr("Basic.Main.PreviewConextMenu.Enable"),
				this, SLOT(TogglePreview()));
	action->setCheckable(true);
	action->setChecked(obs_display_enabled(ui->preview->GetDisplay()));

	previewProjectorMain = new QMenu(QTStr("PreviewProjector"));
	AddProjectorMenuMonitors(previewProjectorMain, this,
				 SLOT(OpenPreviewProjector()));

	QAction *previewWindow = popup.addAction(QTStr("PreviewWindow"), this,
						 SLOT(OpenPreviewWindow()));

	popup.addMenu(previewProjectorMain);
	popup.addAction(previewWindow);
	popup.exec(QCursor::pos());

	UNUSED_PARAMETER(pos);
}

void OBSBasic::on_actionAlwaysOnTop_triggered()
{
#ifndef _WIN32
	/* Make sure all dialogs are safely and successfully closed before
	 * switching the always on top mode due to the fact that windows all
	 * have to be recreated, so queue the actual toggle to happen after
	 * all events related to closing the dialogs have finished */
	CloseDialogs();
#endif

	QMetaObject::invokeMethod(this, "ToggleAlwaysOnTop",
				  Qt::QueuedConnection);
}

void OBSBasic::ToggleAlwaysOnTop()
{
	bool isAlwaysOnTop = IsAlwaysOnTop(this);

	ui->actionAlwaysOnTop->setChecked(!isAlwaysOnTop);
	SetAlwaysOnTop(this, !isAlwaysOnTop);

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "AlwaysOnTop",
			isAlwaysOnTop);

	show();
}

void OBSBasic::GetFPSCommon(uint32_t &num, uint32_t &den) const
{
	const char *val = config_get_string(basicConfig, "Video", "FPSCommon");

	if (strcmp(val, "10") == 0) {
		num = 10;
		den = 1;
	} else if (strcmp(val, "20") == 0) {
		num = 20;
		den = 1;
	} else if (strcmp(val, "24 NTSC") == 0) {
		num = 24000;
		den = 1001;
	} else if (strcmp(val, "25 PAL") == 0) {
		num = 25;
		den = 1;
	} else if (strcmp(val, "29.97") == 0) {
		num = 30000;
		den = 1001;
	} else if (strcmp(val, "48") == 0) {
		num = 48;
		den = 1;
	} else if (strcmp(val, "50 PAL") == 0) {
		num = 50;
		den = 1;
	} else if (strcmp(val, "59.94") == 0) {
		num = 60000;
		den = 1001;
	} else if (strcmp(val, "60") == 0) {
		num = 60;
		den = 1;
	} else {
		num = 30;
		den = 1;
	}
}

void OBSBasic::GetFPSInteger(uint32_t &num, uint32_t &den) const
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSInt");
	den = 1;
}

void OBSBasic::GetFPSFraction(uint32_t &num, uint32_t &den) const
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNum");
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSDen");
}

void OBSBasic::GetFPSNanoseconds(uint32_t &num, uint32_t &den) const
{
	num = 1000000000;
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNS");
}

void OBSBasic::GetConfigFPS(uint32_t &num, uint32_t &den) const
{
	uint32_t type = config_get_uint(basicConfig, "Video", "FPSType");

	if (type == 1) //"Integer"
		GetFPSInteger(num, den);
	else if (type == 2) //"Fraction"
		GetFPSFraction(num, den);
	else if (false) //"Nanoseconds", currently not implemented
		GetFPSNanoseconds(num, den);
	else
		GetFPSCommon(num, den);
}

config_t *OBSBasic::Config() const
{
	return basicConfig;
}

void OBSBasic::on_actionEditTransform_triggered()
{
	if (transformWindow)
		transformWindow->close();

	if (!GetCurrentSceneItem())
		return;

	transformWindow = new OBSBasicTransform(this);
	transformWindow->show();
	transformWindow->setAttribute(Qt::WA_DeleteOnClose, true);
}

static obs_transform_info copiedTransformInfo;
static obs_sceneitem_crop copiedCropInfo;

void OBSBasic::on_actionCopyTransform_triggered()
{
	auto func = [](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
		if (!obs_sceneitem_selected(item))
			return true;

		obs_sceneitem_defer_update_begin(item);
		obs_sceneitem_get_info(item, &copiedTransformInfo);
		obs_sceneitem_get_crop(item, &copiedCropInfo);
		obs_sceneitem_defer_update_end(item);

		UNUSED_PARAMETER(scene);
		UNUSED_PARAMETER(param);
		return true;
	};

	obs_scene_enum_items(GetCurrentScene(), func, nullptr);
	ui->actionPasteTransform->setEnabled(true);
}

void undo_redo(const std::string &data)
{
	obs_data_t *dat = obs_data_create_from_json(data.c_str());
	obs_source_t *source =
		obs_get_source_by_name(obs_data_get_string(dat, "scene_name"));
	reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
		->SetCurrentScene(source, true);
	obs_source_release(source);
	obs_data_release(dat);

	obs_scene_load_transform_states(data.c_str());
}

void OBSBasic::on_actionPasteTransform_triggered()
{
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	auto func = [](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
		if (!obs_sceneitem_selected(item))
			return true;

		obs_sceneitem_defer_update_begin(item);
		obs_sceneitem_set_info(item, &copiedTransformInfo);
		obs_sceneitem_set_crop(item, &copiedCropInfo);
		obs_sceneitem_defer_update_end(item);

		UNUSED_PARAMETER(scene);
		UNUSED_PARAMETER(param);
		return true;
	};

	obs_scene_enum_items(GetCurrentScene(), func, nullptr);

	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(
		QTStr("Undo.Transform.Paste")
			.arg(obs_source_get_name(GetCurrentSceneSource())),
		undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

static bool reset_tr(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, reset_tr, nullptr);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	obs_sceneitem_defer_update_begin(item);

	obs_transform_info info;
	vec2_set(&info.pos, 0.0f, 0.0f);
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.rot = 0.0f;
	info.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
	info.bounds_type = OBS_BOUNDS_NONE;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, 0.0f, 0.0f);
	obs_sceneitem_set_info(item, &info);

	obs_sceneitem_crop crop = {};
	obs_sceneitem_set_crop(item, &crop);

	obs_sceneitem_defer_update_end(item);

	UNUSED_PARAMETER(scene);
	UNUSED_PARAMETER(param);
	return true;
}

void OBSBasic::on_actionResetTransform_triggered()
{
	obs_scene_t *scene = GetCurrentScene();

	obs_data_t *wrapper = obs_scene_save_transform_states(scene, false);
	obs_scene_enum_items(scene, reset_tr, nullptr);
	obs_data_t *rwrapper = obs_scene_save_transform_states(scene, false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(
		QTStr("Undo.Transform.Reset")
			.arg(obs_source_get_name(obs_scene_get_source(scene))),
		undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);

	obs_scene_enum_items(GetCurrentScene(), reset_tr, nullptr);
}

static void GetItemBox(obs_sceneitem_t *item, vec3 &tl, vec3 &br)
{
	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
	vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

	auto GetMinPos = [&](float x, float y) {
		vec3 pos;
		vec3_set(&pos, x, y, 0.0f);
		vec3_transform(&pos, &pos, &boxTransform);
		vec3_min(&tl, &tl, &pos);
		vec3_max(&br, &br, &pos);
	};

	GetMinPos(0.0f, 0.0f);
	GetMinPos(1.0f, 0.0f);
	GetMinPos(0.0f, 1.0f);
	GetMinPos(1.0f, 1.0f);
}

static vec3 GetItemTL(obs_sceneitem_t *item)
{
	vec3 tl, br;
	GetItemBox(item, tl, br);
	return tl;
}

static void SetItemTL(obs_sceneitem_t *item, const vec3 &tl)
{
	vec3 newTL;
	vec2 pos;

	obs_sceneitem_get_pos(item, &pos);
	newTL = GetItemTL(item);
	pos.x += tl.x - newTL.x;
	pos.y += tl.y - newTL.y;
	obs_sceneitem_set_pos(item, &pos);
}

static bool RotateSelectedSources(obs_scene_t *scene, obs_sceneitem_t *item,
				  void *param)
{
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, RotateSelectedSources,
					       param);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	float rot = *reinterpret_cast<float *>(param);

	vec3 tl = GetItemTL(item);

	rot += obs_sceneitem_get_rot(item);
	if (rot >= 360.0f)
		rot -= 360.0f;
	else if (rot <= -360.0f)
		rot += 360.0f;
	obs_sceneitem_set_rot(item, rot);

	obs_sceneitem_force_update_transform(item);

	SetItemTL(item, tl);

	UNUSED_PARAMETER(scene);
	return true;
};

void OBSBasic::on_actionRotate90CW_triggered()
{
	float f90CW = 90.0f;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), RotateSelectedSources, &f90CW);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.Rotate")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

void OBSBasic::on_actionRotate90CCW_triggered()
{
	float f90CCW = -90.0f;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), RotateSelectedSources, &f90CCW);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.Rotate")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

void OBSBasic::on_actionRotate180_triggered()
{
	float f180 = 180.0f;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), RotateSelectedSources, &f180);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.Rotate")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

static bool MultiplySelectedItemScale(obs_scene_t *scene, obs_sceneitem_t *item,
				      void *param)
{
	vec2 &mul = *reinterpret_cast<vec2 *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, MultiplySelectedItemScale,
					       param);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	vec3 tl = GetItemTL(item);

	vec2 scale;
	obs_sceneitem_get_scale(item, &scale);
	vec2_mul(&scale, &scale, &mul);
	obs_sceneitem_set_scale(item, &scale);

	obs_sceneitem_force_update_transform(item);

	SetItemTL(item, tl);

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasic::on_actionFlipHorizontal_triggered()
{
	vec2 scale;
	vec2_set(&scale, -1.0f, 1.0f);
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), MultiplySelectedItemScale,
			     &scale);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.HFlip")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

void OBSBasic::on_actionFlipVertical_triggered()
{
	vec2 scale;
	vec2_set(&scale, 1.0f, -1.0f);
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), MultiplySelectedItemScale,
			     &scale);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.VFlip")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

static bool CenterAlignSelectedItems(obs_scene_t *scene, obs_sceneitem_t *item,
				     void *param)
{
	obs_bounds_type boundsType =
		*reinterpret_cast<obs_bounds_type *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, CenterAlignSelectedItems,
					       param);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	obs_transform_info itemInfo;
	vec2_set(&itemInfo.pos, 0.0f, 0.0f);
	vec2_set(&itemInfo.scale, 1.0f, 1.0f);
	itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	itemInfo.rot = 0.0f;

	vec2_set(&itemInfo.bounds, float(ovi.base_width),
		 float(ovi.base_height));
	itemInfo.bounds_type = boundsType;
	itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

	obs_sceneitem_set_info(item, &itemInfo);

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasic::on_actionFitToScreen_triggered()
{
	obs_bounds_type boundsType = OBS_BOUNDS_SCALE_INNER;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), CenterAlignSelectedItems,
			     &boundsType);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.FitToScreen")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

void OBSBasic::on_actionStretchToScreen_triggered()
{
	obs_bounds_type boundsType = OBS_BOUNDS_STRETCH;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), CenterAlignSelectedItems,
			     &boundsType);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.StretchToScreen")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

enum class CenterType {
	Scene,
	Vertical,
	Horizontal,
};

static bool center_to_scene(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	CenterType centerType = *reinterpret_cast<CenterType *>(param);

	vec3 tl, br, itemCenter, screenCenter, offset;
	obs_video_info ovi;
	obs_transform_info oti;

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, center_to_scene,
					       &centerType);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	obs_get_video_info(&ovi);
	obs_sceneitem_get_info(item, &oti);

	if (centerType == CenterType::Scene)
		vec3_set(&screenCenter, float(ovi.base_width),
			 float(ovi.base_height), 0.0f);
	else if (centerType == CenterType::Vertical)
		vec3_set(&screenCenter, float(oti.bounds.x),
			 float(ovi.base_height), 0.0f);
	else if (centerType == CenterType::Horizontal)
		vec3_set(&screenCenter, float(ovi.base_width),
			 float(oti.bounds.y), 0.0f);

	vec3_mulf(&screenCenter, &screenCenter, 0.5f);

	GetItemBox(item, tl, br);

	vec3_sub(&itemCenter, &br, &tl);
	vec3_mulf(&itemCenter, &itemCenter, 0.5f);
	vec3_add(&itemCenter, &itemCenter, &tl);

	vec3_sub(&offset, &screenCenter, &itemCenter);
	vec3_add(&tl, &tl, &offset);

	if (centerType == CenterType::Vertical)
		tl.x = oti.pos.x;
	else if (centerType == CenterType::Horizontal)
		tl.y = oti.pos.y;

	SetItemTL(item, tl);
	return true;
};

void OBSBasic::on_actionCenterToScreen_triggered()
{
	CenterType centerType = CenterType::Scene;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), center_to_scene, &centerType);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.Center")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

void OBSBasic::on_actionVerticalCenter_triggered()
{
	CenterType centerType = CenterType::Vertical;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), center_to_scene, &centerType);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.VCenter")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

void OBSBasic::on_actionHorizontalCenter_triggered()
{
	CenterType centerType = CenterType::Horizontal;
	obs_data_t *wrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);
	obs_scene_enum_items(GetCurrentScene(), center_to_scene, &centerType);
	obs_data_t *rwrapper =
		obs_scene_save_transform_states(GetCurrentScene(), false);

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	undo_s.add_action(QTStr("Undo.Transform.HCenter")
				  .arg(obs_source_get_name(obs_scene_get_source(
					  GetCurrentScene()))),
			  undo_redo, undo_redo, undo_data, redo_data);

	obs_data_release(wrapper);
	obs_data_release(rwrapper);
}

void OBSBasic::EnablePreviewDisplay(bool enable)
{
	obs_display_set_enabled(ui->preview->GetDisplay(), enable);
	ui->preview->setVisible(enable);
	ui->previewDisabledWidget->setVisible(!enable);

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "PreviewEnabled",
			enable);
}

void OBSBasic::TogglePreview()
{
	previewEnabled = !previewEnabled;
	EnablePreviewDisplay(previewEnabled);
}

void OBSBasic::EnablePreview()
{
	if (previewProgramMode)
		return;

	previewEnabled = true;
	EnablePreviewDisplay(true);
}

void OBSBasic::DisablePreview()
{
	if (previewProgramMode)
		return;

	previewEnabled = false;
	EnablePreviewDisplay(false);
}

static bool nudge_callback(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	struct vec2 &offset = *reinterpret_cast<struct vec2 *>(param);
	struct vec2 pos;

	if (!obs_sceneitem_selected(item)) {
		if (obs_sceneitem_is_group(item)) {
			struct vec3 offset3;
			vec3_set(&offset3, offset.x, offset.y, 0.0f);

			struct matrix4 matrix;
			obs_sceneitem_get_draw_transform(item, &matrix);
			vec4_set(&matrix.t, 0.0f, 0.0f, 0.0f, 1.0f);
			matrix4_inv(&matrix, &matrix);
			vec3_transform(&offset3, &offset3, &matrix);

			struct vec2 new_offset;
			vec2_set(&new_offset, offset3.x, offset3.y);
			obs_sceneitem_group_enum_items(item, nudge_callback,
						       &new_offset);
		}

		return true;
	}

	obs_sceneitem_get_pos(item, &pos);
	vec2_add(&pos, &pos, &offset);
	obs_sceneitem_set_pos(item, &pos);
	return true;
}

void OBSBasic::Nudge(int dist, MoveDir dir)
{
	if (ui->preview->Locked())
		return;

	struct vec2 offset;
	vec2_set(&offset, 0.0f, 0.0f);

	switch (dir) {
	case MoveDir::Up:
		offset.y = (float)-dist;
		break;
	case MoveDir::Down:
		offset.y = (float)dist;
		break;
	case MoveDir::Left:
		offset.x = (float)-dist;
		break;
	case MoveDir::Right:
		offset.x = (float)dist;
		break;
	}

	if (!recent_nudge) {
		recent_nudge = true;
		obs_data_t *wrapper = obs_scene_save_transform_states(
			GetCurrentScene(), true);
		std::string undo_data(obs_data_get_json(wrapper));

		nudge_timer = new QTimer;
		QObject::connect(
			nudge_timer, &QTimer::timeout,
			[this, &recent_nudge = recent_nudge, undo_data]() {
				obs_data_t *rwrapper =
					obs_scene_save_transform_states(
						GetCurrentScene(), true);
				std::string redo_data(
					obs_data_get_json(rwrapper));

				undo_s.add_action(
					QTStr("Undo.Transform")
						.arg(obs_source_get_name(
							GetCurrentSceneSource())),
					undo_redo, undo_redo, undo_data,
					redo_data);

				recent_nudge = false;
				obs_data_release(rwrapper);
			});
		connect(nudge_timer, &QTimer::timeout, nudge_timer,
			&QTimer::deleteLater);
		nudge_timer->setSingleShot(true);

		obs_data_release(wrapper);
	}

	if (nudge_timer) {
		nudge_timer->stop();
		nudge_timer->start(1000);
	} else {
		blog(LOG_ERROR, "No nudge timer!");
	}

	obs_scene_enum_items(GetCurrentScene(), nudge_callback, &offset);
}

void OBSBasic::NudgeUp()
{
	Nudge(1, MoveDir::Up);
}
void OBSBasic::NudgeDown()
{
	Nudge(1, MoveDir::Down);
}
void OBSBasic::NudgeLeft()
{
	Nudge(1, MoveDir::Left);
}
void OBSBasic::NudgeRight()
{
	Nudge(1, MoveDir::Right);
}

void OBSBasic::DeleteProjector(OBSProjector *projector)
{
	for (size_t i = 0; i < projectors.size(); i++) {
		if (projectors[i] == projector) {
			projectors[i]->deleteLater();
			projectors.erase(projectors.begin() + i);
			break;
		}
	}
}

OBSProjector *OBSBasic::OpenProjector(obs_source_t *source, int monitor,
				      ProjectorType type)
{
	/* seriously?  10 monitors? */
	if (monitor > 9 || monitor > QGuiApplication::screens().size() - 1)
		return nullptr;

	OBSProjector *projector =
		new OBSProjector(nullptr, source, monitor, type);

	projectors.emplace_back(projector);

	return projector;
}

void OBSBasic::OpenStudioProgramProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OpenProjector(nullptr, monitor, ProjectorType::StudioProgram);
}

void OBSBasic::OpenPreviewProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OpenProjector(nullptr, monitor, ProjectorType::Preview);
}

void OBSBasic::OpenSourceProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OBSSceneItem item = GetCurrentSceneItem();
	if (!item)
		return;

	OpenProjector(obs_sceneitem_get_source(item), monitor,
		      ProjectorType::Source);
}

void OBSBasic::OpenMultiviewProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OpenProjector(nullptr, monitor, ProjectorType::Multiview);
}

void OBSBasic::OpenSceneProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;

	OpenProjector(obs_scene_get_source(scene), monitor,
		      ProjectorType::Scene);
}

void OBSBasic::OpenStudioProgramWindow()
{
	OpenProjector(nullptr, -1, ProjectorType::StudioProgram);
}

void OBSBasic::OpenPreviewWindow()
{
	OpenProjector(nullptr, -1, ProjectorType::Preview);
}

void OBSBasic::OpenSourceWindow()
{
	OBSSceneItem item = GetCurrentSceneItem();
	if (!item)
		return;

	OBSSource source = obs_sceneitem_get_source(item);

	OpenProjector(obs_sceneitem_get_source(item), -1,
		      ProjectorType::Source);
}

void OBSBasic::OpenMultiviewWindow()
{
	OpenProjector(nullptr, -1, ProjectorType::Multiview);
}

void OBSBasic::OpenSceneWindow()
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;

	OBSSource source = obs_scene_get_source(scene);

	OpenProjector(obs_scene_get_source(scene), -1, ProjectorType::Scene);
}

void OBSBasic::OpenSavedProjectors()
{
	for (SavedProjectorInfo *info : savedProjectorsArray) {
		OpenSavedProjector(info);
	}
}

void OBSBasic::OpenSavedProjector(SavedProjectorInfo *info)
{
	if (info) {
		OBSProjector *projector = nullptr;
		switch (info->type) {
		case ProjectorType::Source:
		case ProjectorType::Scene: {
			OBSSource source =
				obs_get_source_by_name(info->name.c_str());
			if (!source)
				return;

			projector = OpenProjector(source, info->monitor,
						  info->type);

			obs_source_release(source);
			break;
		}
		default: {
			projector = OpenProjector(nullptr, info->monitor,
						  info->type);
			break;
		}
		}

		if (projector && !info->geometry.empty() && info->monitor < 0) {
			QByteArray byteArray = QByteArray::fromBase64(
				QByteArray(info->geometry.c_str()));
			projector->restoreGeometry(byteArray);

			if (!WindowPositionValid(projector->normalGeometry())) {
				QRect rect = QGuiApplication::primaryScreen()
						     ->geometry();
				projector->setGeometry(QStyle::alignedRect(
					Qt::LeftToRight, Qt::AlignCenter,
					size(), rect));
			}

			if (info->alwaysOnTopOverridden)
				projector->SetIsAlwaysOnTop(info->alwaysOnTop,
							    true);
		}
	}
}

void OBSBasic::on_actionFullscreenInterface_triggered()
{
	if (!isFullScreen())
		showFullScreen();
	else
		showNormal();
}

void OBSBasic::UpdateTitleBar()
{
	stringstream name;

	const char *profile =
		config_get_string(App()->GlobalConfig(), "Basic", "Profile");
	const char *sceneCollection = config_get_string(
		App()->GlobalConfig(), "Basic", "SceneCollection");

	name << "OBS ";
	if (previewProgramMode)
		name << "Studio ";

	name << App()->GetVersionString();
	if (App()->IsPortableMode())
		name << " - Portable Mode";

	name << " - " << Str("TitleBar.Profile") << ": " << profile;
	name << " - " << Str("TitleBar.Scenes") << ": " << sceneCollection;

	setWindowTitle(QT_UTF8(name.str().c_str()));
}

int OBSBasic::GetProfilePath(char *path, size_t size, const char *file) const
{
	char profiles_path[512];
	const char *profile =
		config_get_string(App()->GlobalConfig(), "Basic", "ProfileDir");
	int ret;

	if (!profile)
		return -1;
	if (!path)
		return -1;
	if (!file)
		file = "";

	ret = GetConfigPath(profiles_path, 512, "obs-studio/basic/profiles");
	if (ret <= 0)
		return ret;

	if (!*file)
		return snprintf(path, size, "%s/%s", profiles_path, profile);

	return snprintf(path, size, "%s/%s/%s", profiles_path, profile, file);
}

void OBSBasic::on_resetUI_triggered()
{
	/* prune deleted extra docks */
	for (int i = extraDocks.size() - 1; i >= 0; i--) {
		if (!extraDocks[i]) {
			extraDocks.removeAt(i);
		}
	}

	if (extraDocks.size()) {
		QMessageBox::StandardButton button = QMessageBox::question(
			this, QTStr("ResetUIWarning.Title"),
			QTStr("ResetUIWarning.Text"));

		if (button == QMessageBox::No)
			return;
	}

	/* undock/hide/center extra docks */
	for (int i = extraDocks.size() - 1; i >= 0; i--) {
		if (extraDocks[i]) {
			extraDocks[i]->setVisible(true);
			extraDocks[i]->setFloating(true);
			extraDocks[i]->move(frameGeometry().topLeft() +
					    rect().center() -
					    extraDocks[i]->rect().center());
			extraDocks[i]->setVisible(false);
		}
	}

	restoreState(startingDockLayout);

	int cx = width();
	int cy = height();

	int cx22_5 = cx * 225 / 1000;
	int cx5 = cx * 5 / 100;

	cy = cy * 225 / 1000;

	int mixerSize = cx - (cx22_5 * 2 + cx5 * 2);

	QList<QDockWidget *> docks{ui->scenesDock, ui->sourcesDock,
				   ui->mixerDock, ui->transitionsDock,
				   ui->controlsDock};

	QList<int> sizes{cx22_5, cx22_5, mixerSize, cx5, cx5};

	ui->scenesDock->setVisible(true);
	ui->sourcesDock->setVisible(true);
	ui->mixerDock->setVisible(true);
	ui->transitionsDock->setVisible(true);
	ui->controlsDock->setVisible(true);
	statsDock->setVisible(false);
	statsDock->setFloating(true);

	resizeDocks(docks, {cy, cy, cy, cy, cy}, Qt::Vertical);
	resizeDocks(docks, sizes, Qt::Horizontal);

	activateWindow();
}

void OBSBasic::on_lockUI_toggled(bool lock)
{
	QDockWidget::DockWidgetFeatures features =
		lock ? QDockWidget::NoDockWidgetFeatures
		     : (QDockWidget::DockWidgetClosable |
			QDockWidget::DockWidgetMovable |
			QDockWidget::DockWidgetFloatable);

	QDockWidget::DockWidgetFeatures mainFeatures = features;
	mainFeatures &= ~QDockWidget::QDockWidget::DockWidgetClosable;

	ui->scenesDock->setFeatures(mainFeatures);
	ui->sourcesDock->setFeatures(mainFeatures);
	ui->mixerDock->setFeatures(mainFeatures);
	ui->transitionsDock->setFeatures(mainFeatures);
	ui->controlsDock->setFeatures(mainFeatures);
	statsDock->setFeatures(features);

	for (int i = extraDocks.size() - 1; i >= 0; i--) {
		if (!extraDocks[i]) {
			extraDocks.removeAt(i);
		} else {
			extraDocks[i]->setFeatures(features);
		}
	}

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "DocksLocked",
			lock);
}

void OBSBasic::on_toggleListboxToolbars_toggled(bool visible)
{
	ui->sourcesToolbar->setVisible(visible);
	ui->scenesToolbar->setVisible(visible);

	config_set_bool(App()->GlobalConfig(), "BasicWindow",
			"ShowListboxToolbars", visible);
}

void OBSBasic::ShowContextBar()
{
	on_toggleContextBar_toggled(true);
}

void OBSBasic::HideContextBar()
{
	on_toggleContextBar_toggled(false);
}

void OBSBasic::on_toggleContextBar_toggled(bool visible)
{
	config_set_bool(App()->GlobalConfig(), "BasicWindow",
			"ShowContextToolbars", visible);
	this->ui->contextContainer->setVisible(visible);
	UpdateContextBar(true);
}

void OBSBasic::on_toggleStatusBar_toggled(bool visible)
{
	ui->statusbar->setVisible(visible);

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "ShowStatusBar",
			visible);
}

void OBSBasic::on_toggleSourceIcons_toggled(bool visible)
{
	ui->sources->SetIconsVisible(visible);
	if (advAudioWindow != nullptr)
		advAudioWindow->SetIconsVisible(visible);

	config_set_bool(App()->GlobalConfig(), "BasicWindow", "ShowSourceIcons",
			visible);
}

void OBSBasic::on_actionLockPreview_triggered()
{
	ui->preview->ToggleLocked();
	ui->actionLockPreview->setChecked(ui->preview->Locked());
}

void OBSBasic::on_scalingMenu_aboutToShow()
{
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	QAction *action = ui->actionScaleCanvas;
	QString text = QTStr("Basic.MainMenu.Edit.Scale.Canvas");
	text = text.arg(QString::number(ovi.base_width),
			QString::number(ovi.base_height));
	action->setText(text);

	action = ui->actionScaleOutput;
	text = QTStr("Basic.MainMenu.Edit.Scale.Output");
	text = text.arg(QString::number(ovi.output_width),
			QString::number(ovi.output_height));
	action->setText(text);
	action->setVisible(!(ovi.output_width == ovi.base_width &&
			     ovi.output_height == ovi.base_height));

	UpdatePreviewScalingMenu();
}

void OBSBasic::on_actionScaleWindow_triggered()
{
	ui->preview->SetFixedScaling(false);
	ui->preview->ResetScrollingOffset();
	emit ui->preview->DisplayResized();
}

void OBSBasic::on_actionScaleCanvas_triggered()
{
	ui->preview->SetFixedScaling(true);
	ui->preview->SetScalingLevel(0);
	emit ui->preview->DisplayResized();
}

void OBSBasic::on_actionScaleOutput_triggered()
{
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	ui->preview->SetFixedScaling(true);
	float scalingAmount = float(ovi.output_width) / float(ovi.base_width);
	// log base ZOOM_SENSITIVITY of x = log(x) / log(ZOOM_SENSITIVITY)
	int32_t approxScalingLevel =
		int32_t(round(log(scalingAmount) / log(ZOOM_SENSITIVITY)));
	ui->preview->SetScalingLevel(approxScalingLevel);
	ui->preview->SetScalingAmount(scalingAmount);
	emit ui->preview->DisplayResized();
}

void OBSBasic::SetShowing(bool showing)
{
	if (!showing && isVisible()) {
		config_set_string(App()->GlobalConfig(), "BasicWindow",
				  "geometry",
				  saveGeometry().toBase64().constData());

		/* hide all visible child dialogs */
		visDlgPositions.clear();
		if (!visDialogs.isEmpty()) {
			for (QDialog *dlg : visDialogs) {
				visDlgPositions.append(dlg->pos());
				dlg->hide();
			}
		}

		if (showHide)
			showHide->setText(QTStr("Basic.SystemTray.Show"));
		QTimer::singleShot(250, this, SLOT(hide()));

		setVisible(false);

#ifdef __APPLE__
		EnableOSXDockIcon(false);
#endif

	} else if (showing && !isVisible()) {
		if (showHide)
			showHide->setText(QTStr("Basic.SystemTray.Hide"));
		QTimer::singleShot(250, this, SLOT(show()));

		setVisible(true);

#ifdef __APPLE__
		EnableOSXDockIcon(true);
#endif

		/* raise and activate window to ensure it is on top */
		raise();
		activateWindow();

		/* show all child dialogs that was visible earlier */
		if (!visDialogs.isEmpty()) {
			for (int i = 0; i < visDialogs.size(); ++i) {
				QDialog *dlg = visDialogs[i];
				dlg->move(visDlgPositions[i]);
				dlg->show();
			}
		}

		/* Unminimize window if it was hidden to tray instead of task
		 * bar. */
		if (sysTrayMinimizeToTray()) {
			Qt::WindowStates state;
			state = windowState() & ~Qt::WindowMinimized;
			state |= Qt::WindowActive;
			setWindowState(state);
		}
	}
}

void OBSBasic::ToggleShowHide()
{
	bool showing = isVisible();
	if (showing) {
		/* check for modal dialogs */
		EnumDialogs();
		if (!modalDialogs.isEmpty() || !visMsgBoxes.isEmpty())
			return;
	}
	SetShowing(!showing);
}

void OBSBasic::SystemTrayInit()
{
#ifdef __APPLE__
	QIcon trayIconFile = QIcon(":/res/images/obs_macos.png");
	trayIconFile.setIsMask(true);
#else
	QIcon trayIconFile = QIcon(":/res/images/obs.png");
#endif
	trayIcon.reset(new QSystemTrayIcon(
		QIcon::fromTheme("obs-tray", trayIconFile), this));
	trayIcon->setToolTip("OBS Studio");

	showHide = new QAction(QTStr("Basic.SystemTray.Show"), trayIcon.data());
	sysTrayStream = new QAction(QTStr("Basic.Main.StartStreaming"),
				    trayIcon.data());
	sysTrayRecord = new QAction(QTStr("Basic.Main.StartRecording"),
				    trayIcon.data());
	sysTrayReplayBuffer = new QAction(QTStr("Basic.Main.StartReplayBuffer"),
					  trayIcon.data());
	sysTrayVirtualCam = new QAction(QTStr("Basic.Main.StartVirtualCam"),
					trayIcon.data());
	exit = new QAction(QTStr("Exit"), trayIcon.data());

	trayMenu = new QMenu;
	previewProjector = new QMenu(QTStr("PreviewProjector"));
	studioProgramProjector = new QMenu(QTStr("StudioProgramProjector"));
	AddProjectorMenuMonitors(previewProjector, this,
				 SLOT(OpenPreviewProjector()));
	AddProjectorMenuMonitors(studioProgramProjector, this,
				 SLOT(OpenStudioProgramProjector()));
	trayMenu->addAction(showHide);
	trayMenu->addMenu(previewProjector);
	trayMenu->addMenu(studioProgramProjector);
	trayMenu->addAction(sysTrayStream);
	trayMenu->addAction(sysTrayRecord);
	trayMenu->addAction(sysTrayReplayBuffer);
	trayMenu->addAction(sysTrayVirtualCam);
	trayMenu->addAction(exit);
	trayIcon->setContextMenu(trayMenu);
	trayIcon->show();

	if (outputHandler && !outputHandler->replayBuffer)
		sysTrayReplayBuffer->setEnabled(false);

	sysTrayVirtualCam->setEnabled(vcamEnabled);

	connect(trayIcon.data(),
		SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
		SLOT(IconActivated(QSystemTrayIcon::ActivationReason)));
	connect(showHide, SIGNAL(triggered()), this, SLOT(ToggleShowHide()));
	connect(sysTrayStream, SIGNAL(triggered()), this,
		SLOT(on_streamButton_clicked()));
	connect(sysTrayRecord, SIGNAL(triggered()), this,
		SLOT(on_recordButton_clicked()));
	connect(sysTrayReplayBuffer.data(), &QAction::triggered, this,
		&OBSBasic::ReplayBufferClicked);
	connect(sysTrayVirtualCam.data(), &QAction::triggered, this,
		&OBSBasic::VCamButtonClicked);
	connect(exit, SIGNAL(triggered()), this, SLOT(close()));
}

void OBSBasic::IconActivated(QSystemTrayIcon::ActivationReason reason)
{
	// Refresh projector list
	previewProjector->clear();
	studioProgramProjector->clear();
	AddProjectorMenuMonitors(previewProjector, this,
				 SLOT(OpenPreviewProjector()));
	AddProjectorMenuMonitors(studioProgramProjector, this,
				 SLOT(OpenStudioProgramProjector()));

#ifdef __APPLE__
	UNUSED_PARAMETER(reason);
#else
	if (reason == QSystemTrayIcon::Trigger) {
		EnablePreviewDisplay(previewEnabled && !isVisible());
		ToggleShowHide();
	}
#endif
}

void OBSBasic::SysTrayNotify(const QString &text,
			     QSystemTrayIcon::MessageIcon n)
{
	if (trayIcon && trayIcon->isVisible() &&
	    QSystemTrayIcon::supportsMessages()) {
		QSystemTrayIcon::MessageIcon icon =
			QSystemTrayIcon::MessageIcon(n);
		trayIcon->showMessage("OBS Studio", text, icon, 10000);
	}
}

void OBSBasic::SystemTray(bool firstStarted)
{
	if (!QSystemTrayIcon::isSystemTrayAvailable())
		return;
	if (!trayIcon && !firstStarted)
		return;

	bool sysTrayWhenStarted = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "SysTrayWhenStarted");
	bool sysTrayEnabled = config_get_bool(GetGlobalConfig(), "BasicWindow",
					      "SysTrayEnabled");

	if (firstStarted)
		SystemTrayInit();

	if (!sysTrayEnabled) {
		trayIcon->hide();
	} else {
		trayIcon->show();
		if (firstStarted && (sysTrayWhenStarted || opt_minimize_tray)) {
			QTimer::singleShot(50, this, SLOT(hide()));
			EnablePreviewDisplay(false);
			setVisible(false);
#ifdef __APPLE__
			EnableOSXDockIcon(false);
#endif
			opt_minimize_tray = false;
		}
	}

	if (isVisible())
		showHide->setText(QTStr("Basic.SystemTray.Hide"));
	else
		showHide->setText(QTStr("Basic.SystemTray.Show"));
}

bool OBSBasic::sysTrayMinimizeToTray()
{
	return config_get_bool(GetGlobalConfig(), "BasicWindow",
			       "SysTrayMinimizeToTray");
}

void OBSBasic::on_actionMainUndo_triggered()
{
	undo_s.undo();
}

void OBSBasic::on_actionMainRedo_triggered()
{
	undo_s.redo();
}

void OBSBasic::on_actionCopySource_triggered()
{
	copyStrings.clear();
	bool allowPastingDuplicate = true;

	for (auto &selectedSource : GetAllSelectedSourceItems()) {
		OBSSceneItem item = ui->sources->Get(selectedSource.row());
		if (!item)
			continue;

		on_actionCopyTransform_triggered();

		OBSSource source = obs_sceneitem_get_source(item);

		copyStrings.push_front(obs_source_get_name(source));

		copyVisible = obs_sceneitem_visible(item);

		uint32_t output_flags = obs_source_get_output_flags(source);
		if (output_flags & OBS_SOURCE_DO_NOT_DUPLICATE)
			allowPastingDuplicate = false;
	}

	ui->actionPasteRef->setEnabled(true);
	ui->actionPasteDup->setEnabled(allowPastingDuplicate);
}

void OBSBasic::on_actionPasteRef_triggered()
{
	OBSSource scene_source = GetCurrentSceneSource();
	OBSData undo_data = BackupScene(scene_source);

	undo_s.push_disabled();

	for (auto &copyString : copyStrings) {
		/* do not allow duplicate refs of the same group in the same scene */
		OBSScene scene = GetCurrentScene();
		if (!!obs_scene_get_group(scene, copyString))
			continue;

		OBSBasicSourceSelect::SourcePaste(copyString, copyVisible,
						  false);
		on_actionPasteTransform_triggered();
	}

	undo_s.pop_disabled();

	QString action_name = QTStr("Undo.PasteSourceRef");
	const char *scene_name = obs_source_get_name(scene_source);

	OBSData redo_data = BackupScene(scene_source);
	CreateSceneUndoRedoAction(action_name.arg(scene_name), undo_data,
				  redo_data);
}

void OBSBasic::on_actionPasteDup_triggered()
{
	OBSSource scene_source = GetCurrentSceneSource();
	OBSData undo_data = BackupScene(scene_source);

	undo_s.push_disabled();

	for (auto &copyString : copyStrings) {
		OBSBasicSourceSelect::SourcePaste(copyString, copyVisible,
						  true);
		on_actionPasteTransform_triggered();
	}

	undo_s.pop_disabled();

	QString action_name = QTStr("Undo.PasteSource");
	const char *scene_name = obs_source_get_name(scene_source);

	OBSData redo_data = BackupScene(scene_source);
	CreateSceneUndoRedoAction(action_name.arg(scene_name), undo_data,
				  redo_data);
}

void OBSBasic::AudioMixerCopyFilters()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *source = vol->GetSource();

	copyFiltersString = obs_source_get_name(source);
}

void OBSBasic::AudioMixerPasteFilters()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	VolControl *vol = action->property("volControl").value<VolControl *>();
	obs_source_t *dstSource = vol->GetSource();

	OBSSource source = obs_get_source_by_name(copyFiltersString);
	obs_source_release(source);

	if (source == dstSource)
		return;

	obs_source_copy_filters(dstSource, source);
}

void OBSBasic::SceneCopyFilters()
{
	copyFiltersString = obs_source_get_name(GetCurrentSceneSource());
}

void OBSBasic::ScenePasteFilters()
{
	OBSSource source = obs_get_source_by_name(copyFiltersString);
	obs_source_release(source);

	OBSSource dstSource = GetCurrentSceneSource();

	if (source == dstSource)
		return;

	obs_source_copy_filters(dstSource, source);
}

void OBSBasic::on_actionCopyFilters_triggered()
{
	OBSSceneItem item = GetCurrentSceneItem();

	if (!item)
		return;

	OBSSource source = obs_sceneitem_get_source(item);

	copyFiltersString = obs_source_get_name(source);

	ui->actionPasteFilters->setEnabled(true);
}

void OBSBasic::CreateFilterPasteUndoRedoAction(const QString &text,
					       obs_source_t *source,
					       obs_data_array_t *undo_array,
					       obs_data_array_t *redo_array)
{
	auto undo_redo = [this](const std::string &json) {
		obs_data_t *data = obs_data_create_from_json(json.c_str());
		obs_data_array_t *array = obs_data_get_array(data, "array");
		const char *name = obs_data_get_string(data, "name");
		obs_source_t *source = obs_get_source_by_name(name);

		obs_source_restore_filters(source, array);
		obs_source_release(source);

		obs_data_array_release(array);
		obs_data_release(data);

		if (filters)
			filters->UpdateSource(source);
	};

	const char *name = obs_source_get_name(source);

	obs_data_t *undo_data = obs_data_create();
	obs_data_t *redo_data = obs_data_create();
	obs_data_set_array(undo_data, "array", undo_array);
	obs_data_set_array(redo_data, "array", redo_array);
	obs_data_set_string(undo_data, "name", name);
	obs_data_set_string(redo_data, "name", name);

	undo_s.add_action(text, undo_redo, undo_redo,
			  obs_data_get_json(undo_data),
			  obs_data_get_json(redo_data));

	obs_data_release(undo_data);
	obs_data_release(redo_data);
}

void OBSBasic::on_actionPasteFilters_triggered()
{
	OBSSource source = obs_get_source_by_name(copyFiltersString);
	obs_source_release(source);

	OBSSceneItem sceneItem = GetCurrentSceneItem();
	OBSSource dstSource = obs_sceneitem_get_source(sceneItem);

	if (source == dstSource)
		return;

	obs_data_array_t *undo_array = obs_source_backup_filters(dstSource);
	obs_source_copy_filters(dstSource, source);
	obs_data_array_t *redo_array = obs_source_backup_filters(dstSource);

	const char *srcName = obs_source_get_name(source);
	const char *dstName = obs_source_get_name(dstSource);
	QString text =
		QTStr("Undo.Filters.Paste.Multiple").arg(srcName, dstName);

	CreateFilterPasteUndoRedoAction(text, dstSource, undo_array,
					redo_array);

	obs_data_array_release(undo_array);
	obs_data_array_release(redo_array);
}

static void ConfirmColor(SourceTree *sources, const QColor &color,
			 QModelIndexList selectedItems)
{
	for (int x = 0; x < selectedItems.count(); x++) {
		SourceTreeItem *treeItem =
			sources->GetItemWidget(selectedItems[x].row());
		treeItem->setStyleSheet("background: " +
					color.name(QColor::HexArgb));
		treeItem->style()->unpolish(treeItem);
		treeItem->style()->polish(treeItem);

		OBSSceneItem sceneItem = sources->Get(selectedItems[x].row());
		obs_data_t *privData =
			obs_sceneitem_get_private_settings(sceneItem);
		obs_data_set_int(privData, "color-preset", 1);
		obs_data_set_string(privData, "color",
				    QT_TO_UTF8(color.name(QColor::HexArgb)));
		obs_data_release(privData);
	}
}

void OBSBasic::ColorChange()
{
	QModelIndexList selectedItems =
		ui->sources->selectionModel()->selectedIndexes();
	QAction *action = qobject_cast<QAction *>(sender());
	QPushButton *colorButton = qobject_cast<QPushButton *>(sender());

	if (selectedItems.count() == 0)
		return;

	if (colorButton) {
		int preset = colorButton->property("bgColor").value<int>();

		for (int x = 0; x < selectedItems.count(); x++) {
			SourceTreeItem *treeItem = ui->sources->GetItemWidget(
				selectedItems[x].row());
			treeItem->setStyleSheet("");
			treeItem->setProperty("bgColor", preset);
			treeItem->style()->unpolish(treeItem);
			treeItem->style()->polish(treeItem);

			OBSSceneItem sceneItem =
				ui->sources->Get(selectedItems[x].row());
			obs_data_t *privData =
				obs_sceneitem_get_private_settings(sceneItem);
			obs_data_set_int(privData, "color-preset", preset + 1);
			obs_data_set_string(privData, "color", "");
			obs_data_release(privData);
		}

		for (int i = 1; i < 9; i++) {
			stringstream button;
			button << "preset" << i;
			QPushButton *cButton =
				colorButton->parentWidget()
					->findChild<QPushButton *>(
						button.str().c_str());
			cButton->setStyleSheet("border: 1px solid black");
		}

		colorButton->setStyleSheet("border: 2px solid black");
	} else if (action) {
		int preset = action->property("bgColor").value<int>();

		if (preset == 1) {
			OBSSceneItem curSceneItem = GetCurrentSceneItem();
			SourceTreeItem *curTreeItem =
				GetItemWidgetFromSceneItem(curSceneItem);
			obs_data_t *curPrivData =
				obs_sceneitem_get_private_settings(
					curSceneItem);

			int oldPreset =
				obs_data_get_int(curPrivData, "color-preset");
			const QString oldSheet = curTreeItem->styleSheet();

			auto liveChangeColor = [=](const QColor &color) {
				if (color.isValid()) {
					curTreeItem->setStyleSheet(
						"background: " +
						color.name(QColor::HexArgb));
				}
			};

			auto changedColor = [=](const QColor &color) {
				if (color.isValid()) {
					ConfirmColor(ui->sources, color,
						     selectedItems);
				}
			};

			auto rejected = [=]() {
				if (oldPreset == 1) {
					curTreeItem->setStyleSheet(oldSheet);
					curTreeItem->setProperty("bgColor", 0);
				} else if (oldPreset == 0) {
					curTreeItem->setStyleSheet(
						"background: none");
					curTreeItem->setProperty("bgColor", 0);
				} else {
					curTreeItem->setStyleSheet("");
					curTreeItem->setProperty("bgColor",
								 oldPreset - 1);
				}

				curTreeItem->style()->unpolish(curTreeItem);
				curTreeItem->style()->polish(curTreeItem);
			};

			QColorDialog::ColorDialogOptions options =
				QColorDialog::ShowAlphaChannel;

			const char *oldColor =
				obs_data_get_string(curPrivData, "color");
			const char *customColor = *oldColor != 0 ? oldColor
								 : "#55FF0000";
#ifndef _WIN32
			options |= QColorDialog::DontUseNativeDialog;
#endif

			QColorDialog *colorDialog = new QColorDialog(this);
			colorDialog->setOptions(options);
			colorDialog->setCurrentColor(QColor(customColor));
			connect(colorDialog, &QColorDialog::currentColorChanged,
				liveChangeColor);
			connect(colorDialog, &QColorDialog::colorSelected,
				changedColor);
			connect(colorDialog, &QColorDialog::rejected, rejected);
			colorDialog->open();

			obs_data_release(curPrivData);
		} else {
			for (int x = 0; x < selectedItems.count(); x++) {
				SourceTreeItem *treeItem =
					ui->sources->GetItemWidget(
						selectedItems[x].row());
				treeItem->setStyleSheet("background: none");
				treeItem->setProperty("bgColor", preset);
				treeItem->style()->unpolish(treeItem);
				treeItem->style()->polish(treeItem);

				OBSSceneItem sceneItem = ui->sources->Get(
					selectedItems[x].row());
				obs_data_t *privData =
					obs_sceneitem_get_private_settings(
						sceneItem);
				obs_data_set_int(privData, "color-preset",
						 preset);
				obs_data_set_string(privData, "color", "");
				obs_data_release(privData);
			}
		}
	}
}

SourceTreeItem *OBSBasic::GetItemWidgetFromSceneItem(obs_sceneitem_t *sceneItem)
{
	int i = 0;
	SourceTreeItem *treeItem = ui->sources->GetItemWidget(i);
	OBSSceneItem item = ui->sources->Get(i);
	int64_t id = obs_sceneitem_get_id(sceneItem);
	while (treeItem && obs_sceneitem_get_id(item) != id) {
		i++;
		treeItem = ui->sources->GetItemWidget(i);
		item = ui->sources->Get(i);
	}
	if (treeItem)
		return treeItem;

	return nullptr;
}

void OBSBasic::on_autoConfigure_triggered()
{
	AutoConfig test(this);
	test.setModal(true);
	test.show();
	test.exec();
}

void OBSBasic::on_stats_triggered()
{
	if (!stats.isNull()) {
		stats->show();
		stats->raise();
		return;
	}

	OBSBasicStats *statsDlg;
	statsDlg = new OBSBasicStats(nullptr);
	statsDlg->show();
	stats = statsDlg;
}

void OBSBasic::on_actionShowAbout_triggered()
{
	if (about)
		about->close();

	about = new OBSAbout(this);
	about->show();

	about->setAttribute(Qt::WA_DeleteOnClose, true);
}

void OBSBasic::ResizeOutputSizeOfSource()
{
	if (obs_video_active())
		return;

	QMessageBox resize_output(this);
	resize_output.setText(QTStr("ResizeOutputSizeOfSource.Text") + "\n\n" +
			      QTStr("ResizeOutputSizeOfSource.Continue"));
	QAbstractButton *Yes =
		resize_output.addButton(QTStr("Yes"), QMessageBox::YesRole);
	resize_output.addButton(QTStr("No"), QMessageBox::NoRole);
	resize_output.setIcon(QMessageBox::Warning);
	resize_output.setWindowTitle(QTStr("ResizeOutputSizeOfSource"));
	resize_output.exec();

	if (resize_output.clickedButton() != Yes)
		return;

	OBSSource source = obs_sceneitem_get_source(GetCurrentSceneItem());

	int width = obs_source_get_width(source);
	int height = obs_source_get_height(source);

	config_set_uint(basicConfig, "Video", "BaseCX", width);
	config_set_uint(basicConfig, "Video", "BaseCY", height);
	config_set_uint(basicConfig, "Video", "OutputCX", width);
	config_set_uint(basicConfig, "Video", "OutputCY", height);

	ResetVideo();
	ResetOutputs();
	config_save_safe(basicConfig, "tmp", nullptr);
	on_actionFitToScreen_triggered();
}

QAction *OBSBasic::AddDockWidget(QDockWidget *dock)
{
	QAction *action = ui->viewMenuDocks->addAction(dock->windowTitle());
	action->setCheckable(true);
	assignDockToggle(dock, action);
	extraDocks.push_back(dock);

	bool lock = ui->lockUI->isChecked();
	QDockWidget::DockWidgetFeatures features =
		lock ? QDockWidget::NoDockWidgetFeatures
		     : (QDockWidget::DockWidgetClosable |
			QDockWidget::DockWidgetMovable |
			QDockWidget::DockWidgetFloatable);

	dock->setFeatures(features);

	/* prune deleted docks */
	for (int i = extraDocks.size() - 1; i >= 0; i--) {
		if (!extraDocks[i]) {
			extraDocks.removeAt(i);
		}
	}

	return action;
}

OBSBasic *OBSBasic::Get()
{
	return reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
}

bool OBSBasic::StreamingActive()
{
	if (!outputHandler)
		return false;
	return outputHandler->StreamingActive();
}

bool OBSBasic::RecordingActive()
{
	if (!outputHandler)
		return false;
	return outputHandler->RecordingActive();
}

bool OBSBasic::ReplayBufferActive()
{
	if (!outputHandler)
		return false;
	return outputHandler->ReplayBufferActive();
}

SceneRenameDelegate::SceneRenameDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{
}

void SceneRenameDelegate::setEditorData(QWidget *editor,
					const QModelIndex &index) const
{
	QStyledItemDelegate::setEditorData(editor, index);
	QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
	if (lineEdit)
		lineEdit->selectAll();
}

bool SceneRenameDelegate::eventFilter(QObject *editor, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Escape) {
			QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
			if (lineEdit)
				lineEdit->undo();
		}
	}

	return QStyledItemDelegate::eventFilter(editor, event);
}

void OBSBasic::UpdatePatronJson(const QString &text, const QString &error)
{
	if (!error.isEmpty())
		return;

	patronJson = QT_TO_UTF8(text);
}

void OBSBasic::PauseRecording()
{
	if (!pause || !outputHandler || !outputHandler->fileOutput ||
	    os_atomic_load_bool(&recording_paused))
		return;

	obs_output_t *output = outputHandler->fileOutput;

	if (obs_output_pause(output, true)) {
		pause->setAccessibleName(QTStr("Basic.Main.UnpauseRecording"));
		pause->setToolTip(QTStr("Basic.Main.UnpauseRecording"));
		pause->blockSignals(true);
		pause->setChecked(true);
		pause->blockSignals(false);

		ui->statusbar->RecordingPaused();

		if (trayIcon && trayIcon->isVisible()) {
#ifdef __APPLE__
			QIcon trayIconFile =
				QIcon(":/res/images/obs_paused_macos.png");
			trayIconFile.setIsMask(true);
#else
			QIcon trayIconFile =
				QIcon(":/res/images/obs_paused.png");
#endif
			trayIcon->setIcon(QIcon::fromTheme("obs-tray-paused",
							   trayIconFile));
		}

		os_atomic_set_bool(&recording_paused, true);

		if (api)
			api->on_event(OBS_FRONTEND_EVENT_RECORDING_PAUSED);

		if (os_atomic_load_bool(&replaybuf_active))
			ShowReplayBufferPauseWarning();
	}
}

void OBSBasic::UnpauseRecording()
{
	if (!pause || !outputHandler || !outputHandler->fileOutput ||
	    !os_atomic_load_bool(&recording_paused))
		return;

	obs_output_t *output = outputHandler->fileOutput;

	if (obs_output_pause(output, false)) {
		pause->setAccessibleName(QTStr("Basic.Main.PauseRecording"));
		pause->setToolTip(QTStr("Basic.Main.PauseRecording"));
		pause->blockSignals(true);
		pause->setChecked(false);
		pause->blockSignals(false);

		ui->statusbar->RecordingUnpaused();

		if (trayIcon && trayIcon->isVisible()) {
#ifdef __APPLE__
			QIcon trayIconFile =
				QIcon(":/res/images/tray_active_macos.png");
			trayIconFile.setIsMask(true);
#else
			QIcon trayIconFile =
				QIcon(":/res/images/tray_active.png");
#endif
			trayIcon->setIcon(QIcon::fromTheme("obs-tray-active",
							   trayIconFile));
		}

		os_atomic_set_bool(&recording_paused, false);

		if (api)
			api->on_event(OBS_FRONTEND_EVENT_RECORDING_UNPAUSED);
	}
}

void OBSBasic::PauseToggled()
{
	if (!pause || !outputHandler || !outputHandler->fileOutput)
		return;

	obs_output_t *output = outputHandler->fileOutput;
	bool enable = !obs_output_paused(output);

	if (enable)
		PauseRecording();
	else
		UnpauseRecording();
}

void OBSBasic::UpdatePause(bool activate)
{
	if (!activate || !outputHandler || !outputHandler->RecordingActive()) {
		pause.reset();
		return;
	}

	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	bool adv = astrcmpi(mode, "Advanced") == 0;
	bool shared;

	if (adv) {
		const char *recType =
			config_get_string(basicConfig, "AdvOut", "RecType");

		if (astrcmpi(recType, "FFmpeg") == 0) {
			shared = config_get_bool(basicConfig, "AdvOut",
						 "FFOutputToFile");
		} else {
			const char *recordEncoder = config_get_string(
				basicConfig, "AdvOut", "RecEncoder");
			shared = astrcmpi(recordEncoder, "none") == 0;
		}
	} else {
		const char *quality = config_get_string(
			basicConfig, "SimpleOutput", "RecQuality");
		shared = strcmp(quality, "Stream") == 0;
	}

	if (!shared) {
		pause.reset(new QPushButton());
		pause->setAccessibleName(QTStr("Basic.Main.PauseRecording"));
		pause->setToolTip(QTStr("Basic.Main.PauseRecording"));
		pause->setCheckable(true);
		pause->setChecked(false);
		pause->setProperty("themeID",
				   QVariant(QStringLiteral("pauseIconSmall")));

		QSizePolicy sp;
		sp.setHeightForWidth(true);
		pause->setSizePolicy(sp);

		connect(pause.data(), &QAbstractButton::clicked, this,
			&OBSBasic::PauseToggled);
		ui->recordingLayout->addWidget(pause.data());
	} else {
		pause.reset();
	}
}

void OBSBasic::UpdateReplayBuffer(bool activate)
{
	if (!activate || !outputHandler ||
	    !outputHandler->ReplayBufferActive()) {
		replay.reset();
		return;
	}

	replay.reset(new QPushButton());
	replay->setAccessibleName(QTStr("Basic.Main.SaveReplay"));
	replay->setToolTip(QTStr("Basic.Main.SaveReplay"));
	replay->setChecked(false);
	replay->setProperty("themeID",
			    QVariant(QStringLiteral("replayIconSmall")));

	QSizePolicy sp;
	sp.setHeightForWidth(true);
	replay->setSizePolicy(sp);

	connect(replay.data(), &QAbstractButton::clicked, this,
		&OBSBasic::ReplayBufferSave);
	replayLayout->addWidget(replay.data());
	setTabOrder(replayLayout->itemAt(0)->widget(),
		    replayLayout->itemAt(1)->widget());
	setTabOrder(replayLayout->itemAt(1)->widget(),
		    ui->buttonsVLayout->itemAt(3)->widget());
}

#define MBYTE (1024ULL * 1024ULL)
#define MBYTES_LEFT_STOP_REC 50ULL
#define MAX_BYTES_LEFT (MBYTES_LEFT_STOP_REC * MBYTE)

const char *OBSBasic::GetCurrentOutputPath()
{
	const char *path = nullptr;
	const char *mode = config_get_string(Config(), "Output", "Mode");

	if (strcmp(mode, "Advanced") == 0) {
		const char *advanced_mode =
			config_get_string(Config(), "AdvOut", "RecType");

		if (strcmp(advanced_mode, "FFmpeg") == 0) {
			path = config_get_string(Config(), "AdvOut",
						 "FFFilePath");
		} else {
			path = config_get_string(Config(), "AdvOut",
						 "RecFilePath");
		}
	} else {
		path = config_get_string(Config(), "SimpleOutput", "FilePath");
	}

	return path;
}

void OBSBasic::OutputPathInvalidMessage()
{
	blog(LOG_ERROR, "Recording stopped because of bad output path");

	OBSMessageBox::critical(this, QTStr("Output.BadPath.Title"),
				QTStr("Output.BadPath.Text"));
}

bool OBSBasic::OutputPathValid()
{
	const char *mode = config_get_string(Config(), "Output", "Mode");
	if (strcmp(mode, "Advanced") == 0) {
		const char *advanced_mode =
			config_get_string(Config(), "AdvOut", "RecType");
		if (strcmp(advanced_mode, "FFmpeg") == 0) {
			bool is_local = config_get_bool(Config(), "AdvOut",
							"FFOutputToFile");
			if (!is_local)
				return true;
		}
	}

	const char *path = GetCurrentOutputPath();
	return path && *path && QDir(path).exists();
}

void OBSBasic::DiskSpaceMessage()
{
	blog(LOG_ERROR, "Recording stopped because of low disk space");

	OBSMessageBox::critical(this, QTStr("Output.RecordNoSpace.Title"),
				QTStr("Output.RecordNoSpace.Msg"));
}

bool OBSBasic::LowDiskSpace()
{
	const char *path;

	path = GetCurrentOutputPath();
	if (!path)
		return false;

	uint64_t num_bytes = os_get_free_disk_space(path);

	if (num_bytes < (MAX_BYTES_LEFT))
		return true;
	else
		return false;
}

void OBSBasic::CheckDiskSpaceRemaining()
{
	if (LowDiskSpace()) {
		StopRecording();
		StopReplayBuffer();

		DiskSpaceMessage();
	}
}

void OBSBasic::ScenesReordered()
{
	OBSProjector::UpdateMultiviewProjectors();
}

void OBSBasic::ResetStatsHotkey()
{
	QList<OBSBasicStats *> list = findChildren<OBSBasicStats *>();

	foreach(OBSBasicStats * s, list) s->Reset();
}

void OBSBasic::on_customContextMenuRequested(const QPoint &pos)
{
	QWidget *widget = childAt(pos);
	const char *className = nullptr;
	if (widget != nullptr)
		className = widget->metaObject()->className();

	if (!className || strstr(className, "Dock") != nullptr)
		ui->viewMenuDocks->exec(mapToGlobal(pos));
}

void OBSBasic::UpdateProjectorHideCursor()
{
	for (size_t i = 0; i < projectors.size(); i++)
		projectors[i]->SetHideCursor();
}

void OBSBasic::UpdateProjectorAlwaysOnTop(bool top)
{
	for (size_t i = 0; i < projectors.size(); i++)
		SetAlwaysOnTop(projectors[i], top);
}

void OBSBasic::ResetProjectors()
{
	obs_data_array_t *savedProjectorList = SaveProjectors();
	ClearProjectors();
	LoadSavedProjectors(savedProjectorList);
	OpenSavedProjectors();
	obs_data_array_release(savedProjectorList);
}

void OBSBasic::on_sourcePropertiesButton_clicked()
{
	on_actionSourceProperties_triggered();
}

void OBSBasic::on_sourceFiltersButton_clicked()
{
	OpenFilters();
}

void OBSBasic::on_sourceInteractButton_clicked()
{
	on_actionInteract_triggered();
}

void OBSBasic::ShowStatusBarMessage(const QString &message)
{
	ui->statusbar->clearMessage();
	ui->statusbar->showMessage(message, 10000);
}
