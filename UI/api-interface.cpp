#include <obs-frontend-internal.hpp>
#include "obs-app.hpp"
#include "qt-wrappers.hpp"
#include "window-basic-main.hpp"
#include "window-basic-main-outputs.hpp"

#include <functional>

using namespace std;

Q_DECLARE_METATYPE(OBSScene);
Q_DECLARE_METATYPE(OBSSource);

template<typename T> static T GetOBSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::OBSRef)).value<T>();
}

void EnumProfiles(function<bool(const char *, const char *)> &&cb);
void EnumSceneCollections(function<bool(const char *, const char *)> &&cb);

extern volatile bool streaming_active;
extern volatile bool recording_active;
extern volatile bool recording_paused;
extern volatile bool replaybuf_active;
extern volatile bool virtualcam_active;

/* ------------------------------------------------------------------------- */

template<typename T> struct OBSStudioCallback {
	T callback;
	void *private_data;

	inline OBSStudioCallback(T cb, void *p) : callback(cb), private_data(p)
	{
	}
};

template<typename T>
inline size_t GetCallbackIdx(vector<OBSStudioCallback<T>> &callbacks,
			     T callback, void *private_data)
{
	for (size_t i = 0; i < callbacks.size(); i++) {
		OBSStudioCallback<T> curCB = callbacks[i];
		if (curCB.callback == callback &&
		    curCB.private_data == private_data)
			return i;
	}

	return (size_t)-1;
}

struct OBSStudioAPI : obs_frontend_callbacks {
	OBSBasic *main;
	vector<OBSStudioCallback<obs_frontend_event_cb>> callbacks;
	vector<OBSStudioCallback<obs_frontend_save_cb>> saveCallbacks;
	vector<OBSStudioCallback<obs_frontend_save_cb>> preloadCallbacks;

	inline OBSStudioAPI(OBSBasic *main_) : main(main_) {}

	void *obs_frontend_get_main_window(void) override
	{
		return (void *)main;
	}

	void *obs_frontend_get_main_window_handle(void) override
	{
		return (void *)main->winId();
	}

	void *obs_frontend_get_system_tray(void) override
	{
		return (void *)main->trayIcon.data();
	}

	void obs_frontend_get_scenes(
		struct obs_frontend_source_list *sources) override
	{
		for (int i = 0; i < main->ui->scenes->count(); i++) {
			QListWidgetItem *item = main->ui->scenes->item(i);
			OBSScene scene = GetOBSRef<OBSScene>(item);
			obs_source_t *source = obs_scene_get_source(scene);

			obs_source_addref(source);
			da_push_back(sources->sources, &source);
		}
	}

	obs_source_t *obs_frontend_get_current_scene(void) override
	{
		OBSSource source;

		if (main->IsPreviewProgramMode()) {
			source = obs_weak_source_get_source(main->programScene);
		} else {
			source = main->GetCurrentSceneSource();
			obs_source_addref(source);
		}
		return source;
	}

	void obs_frontend_set_current_scene(obs_source_t *scene) override
	{
		if (main->IsPreviewProgramMode()) {
			QMetaObject::invokeMethod(
				main, "TransitionToScene", WaitConnection(),
				Q_ARG(OBSSource, OBSSource(scene)));
		} else {
			QMetaObject::invokeMethod(
				main, "SetCurrentScene", WaitConnection(),
				Q_ARG(OBSSource, OBSSource(scene)),
				Q_ARG(bool, false));
		}
	}

	void obs_frontend_get_transitions(
		struct obs_frontend_source_list *sources) override
	{
		for (int i = 0; i < main->ui->transitions->count(); i++) {
			OBSSource tr = main->ui->transitions->itemData(i)
					       .value<OBSSource>();

			if (!tr)
				continue;

			obs_source_addref(tr);
			da_push_back(sources->sources, &tr);
		}
	}

	obs_source_t *obs_frontend_get_current_transition(void) override
	{
		OBSSource tr = main->GetCurrentTransition();

		obs_source_addref(tr);
		return tr;
	}

	void
	obs_frontend_set_current_transition(obs_source_t *transition) override
	{
		QMetaObject::invokeMethod(main, "SetTransition",
					  Q_ARG(OBSSource,
						OBSSource(transition)));
	}

	int obs_frontend_get_transition_duration(void) override
	{
		return main->ui->transitionDuration->value();
	}

	void obs_frontend_set_transition_duration(int duration) override
	{
		QMetaObject::invokeMethod(main->ui->transitionDuration,
					  "setValue", Q_ARG(int, duration));
	}

	void obs_frontend_release_tbar(void) override
	{
		QMetaObject::invokeMethod(main, "TBarReleased");
	}

	void obs_frontend_set_tbar_position(int position) override
	{
		QMetaObject::invokeMethod(main, "TBarChanged",
					  Q_ARG(int, position));
	}

	void obs_frontend_get_scene_collections(
		std::vector<std::string> &strings) override
	{
		auto addCollection = [&](const char *name, const char *) {
			strings.emplace_back(name);
			return true;
		};

		EnumSceneCollections(addCollection);
	}

	char *obs_frontend_get_current_scene_collection(void) override
	{
		const char *cur_name = config_get_string(
			App()->GlobalConfig(), "Basic", "SceneCollection");
		return bstrdup(cur_name);
	}

	void obs_frontend_set_current_scene_collection(
		const char *collection) override
	{
		QList<QAction *> menuActions =
			main->ui->sceneCollectionMenu->actions();
		QString qstrCollection = QT_UTF8(collection);

		for (int i = 0; i < menuActions.count(); i++) {
			QAction *action = menuActions[i];
			QVariant v = action->property("file_name");

			if (v.typeName() != nullptr) {
				if (action->text() == qstrCollection) {
					action->trigger();
					break;
				}
			}
		}
	}

	bool obs_frontend_add_scene_collection(const char *name) override
	{
		bool success = false;
		QMetaObject::invokeMethod(main, "AddSceneCollection",
					  WaitConnection(),
					  Q_RETURN_ARG(bool, success),
					  Q_ARG(bool, true),
					  Q_ARG(QString, QT_UTF8(name)));
		return success;
	}

	void
	obs_frontend_get_profiles(std::vector<std::string> &strings) override
	{
		auto addProfile = [&](const char *name, const char *) {
			strings.emplace_back(name);
			return true;
		};

		EnumProfiles(addProfile);
	}

	char *obs_frontend_get_current_profile(void) override
	{
		const char *name = config_get_string(App()->GlobalConfig(),
						     "Basic", "Profile");
		return bstrdup(name);
	}

	void obs_frontend_set_current_profile(const char *profile) override
	{
		QList<QAction *> menuActions = main->ui->profileMenu->actions();
		QString qstrProfile = QT_UTF8(profile);

		for (int i = 0; i < menuActions.count(); i++) {
			QAction *action = menuActions[i];
			QVariant v = action->property("file_name");

			if (v.typeName() != nullptr) {
				if (action->text() == qstrProfile) {
					action->trigger();
					break;
				}
			}
		}
	}

	void obs_frontend_streaming_start(void) override
	{
		QMetaObject::invokeMethod(main, "StartStreaming");
	}

	void obs_frontend_streaming_stop(void) override
	{
		QMetaObject::invokeMethod(main, "StopStreaming");
	}

	bool obs_frontend_streaming_active(void) override
	{
		return os_atomic_load_bool(&streaming_active);
	}

	void obs_frontend_recording_start(void) override
	{
		QMetaObject::invokeMethod(main, "StartRecording");
	}

	void obs_frontend_recording_stop(void) override
	{
		QMetaObject::invokeMethod(main, "StopRecording");
	}

	bool obs_frontend_recording_active(void) override
	{
		return os_atomic_load_bool(&recording_active);
	}

	void obs_frontend_recording_pause(bool pause) override
	{
		QMetaObject::invokeMethod(main, pause ? "PauseRecording"
						      : "UnpauseRecording");
	}

	bool obs_frontend_recording_paused(void) override
	{
		return os_atomic_load_bool(&recording_paused);
	}

	void obs_frontend_replay_buffer_start(void) override
	{
		QMetaObject::invokeMethod(main, "StartReplayBuffer");
	}

	void obs_frontend_replay_buffer_save(void) override
	{
		QMetaObject::invokeMethod(main, "ReplayBufferSave");
	}

	void obs_frontend_replay_buffer_stop(void) override
	{
		QMetaObject::invokeMethod(main, "StopReplayBuffer");
	}

	bool obs_frontend_replay_buffer_active(void) override
	{
		return os_atomic_load_bool(&replaybuf_active);
	}

	void *obs_frontend_add_tools_menu_qaction(const char *name) override
	{
		main->ui->menuTools->setEnabled(true);
		return (void *)main->ui->menuTools->addAction(QT_UTF8(name));
	}

	void obs_frontend_add_tools_menu_item(const char *name,
					      obs_frontend_cb callback,
					      void *private_data) override
	{
		main->ui->menuTools->setEnabled(true);

		auto func = [private_data, callback]() {
			callback(private_data);
		};

		QAction *action = main->ui->menuTools->addAction(QT_UTF8(name));
		QObject::connect(action, &QAction::triggered, func);
	}

	void *obs_frontend_add_dock(void *dock) override
	{
		return (void *)main->AddDockWidget((QDockWidget *)dock);
	}

	void obs_frontend_add_event_callback(obs_frontend_event_cb callback,
					     void *private_data) override
	{
		size_t idx = GetCallbackIdx(callbacks, callback, private_data);
		if (idx == (size_t)-1)
			callbacks.emplace_back(callback, private_data);
	}

	void obs_frontend_remove_event_callback(obs_frontend_event_cb callback,
						void *private_data) override
	{
		size_t idx = GetCallbackIdx(callbacks, callback, private_data);
		if (idx == (size_t)-1)
			return;

		callbacks.erase(callbacks.begin() + idx);
	}

	obs_output_t *obs_frontend_get_streaming_output(void) override
	{
		OBSOutput output = main->outputHandler->streamOutput;
		obs_output_addref(output);
		return output;
	}

	obs_output_t *obs_frontend_get_recording_output(void) override
	{
		OBSOutput out = main->outputHandler->fileOutput;
		obs_output_addref(out);
		return out;
	}

	obs_output_t *obs_frontend_get_replay_buffer_output(void) override
	{
		OBSOutput out = main->outputHandler->replayBuffer;
		obs_output_addref(out);
		return out;
	}

	config_t *obs_frontend_get_profile_config(void) override
	{
		return main->basicConfig;
	}

	config_t *obs_frontend_get_global_config(void) override
	{
		return App()->GlobalConfig();
	}

	void obs_frontend_open_projector(const char *type, int monitor,
					 const char *geometry,
					 const char *name) override
	{
		SavedProjectorInfo proj = {
			ProjectorType::Preview,
			monitor,
			geometry ? geometry : "",
			name ? name : "",
		};
		if (type) {
			if (astrcmpi(type, "Source") == 0)
				proj.type = ProjectorType::Source;
			else if (astrcmpi(type, "Scene") == 0)
				proj.type = ProjectorType::Scene;
			else if (astrcmpi(type, "StudioProgram") == 0)
				proj.type = ProjectorType::StudioProgram;
			else if (astrcmpi(type, "Multiview") == 0)
				proj.type = ProjectorType::Multiview;
		}
		QMetaObject::invokeMethod(main, "OpenSavedProjector",
					  WaitConnection(),
					  Q_ARG(SavedProjectorInfo *, &proj));
	}

	void obs_frontend_save(void) override { main->SaveProject(); }

	void obs_frontend_defer_save_begin(void) override
	{
		QMetaObject::invokeMethod(main, "DeferSaveBegin");
	}

	void obs_frontend_defer_save_end(void) override
	{
		QMetaObject::invokeMethod(main, "DeferSaveEnd");
	}

	void obs_frontend_add_save_callback(obs_frontend_save_cb callback,
					    void *private_data) override
	{
		size_t idx =
			GetCallbackIdx(saveCallbacks, callback, private_data);
		if (idx == (size_t)-1)
			saveCallbacks.emplace_back(callback, private_data);
	}

	void obs_frontend_remove_save_callback(obs_frontend_save_cb callback,
					       void *private_data) override
	{
		size_t idx =
			GetCallbackIdx(saveCallbacks, callback, private_data);
		if (idx == (size_t)-1)
			return;

		saveCallbacks.erase(saveCallbacks.begin() + idx);
	}

	void obs_frontend_add_preload_callback(obs_frontend_save_cb callback,
					       void *private_data) override
	{
		size_t idx = GetCallbackIdx(preloadCallbacks, callback,
					    private_data);
		if (idx == (size_t)-1)
			preloadCallbacks.emplace_back(callback, private_data);
	}

	void obs_frontend_remove_preload_callback(obs_frontend_save_cb callback,
						  void *private_data) override
	{
		size_t idx = GetCallbackIdx(preloadCallbacks, callback,
					    private_data);
		if (idx == (size_t)-1)
			return;

		preloadCallbacks.erase(preloadCallbacks.begin() + idx);
	}

	void obs_frontend_push_ui_translation(
		obs_frontend_translate_ui_cb translate) override
	{
		App()->PushUITranslation(translate);
	}

	void obs_frontend_pop_ui_translation(void) override
	{
		App()->PopUITranslation();
	}

	void obs_frontend_set_streaming_service(obs_service_t *service) override
	{
		main->SetService(service);
	}

	obs_service_t *obs_frontend_get_streaming_service(void) override
	{
		return main->GetService();
	}

	void obs_frontend_save_streaming_service(void) override
	{
		main->SaveService();
	}

	bool obs_frontend_preview_program_mode_active(void) override
	{
		return main->IsPreviewProgramMode();
	}

	void obs_frontend_set_preview_program_mode(bool enable) override
	{
		main->SetPreviewProgramMode(enable);
	}

	void obs_frontend_preview_program_trigger_transition(void) override
	{
		QMetaObject::invokeMethod(main, "TransitionClicked");
	}

	bool obs_frontend_preview_enabled(void) override
	{
		return main->previewEnabled;
	}

	void obs_frontend_set_preview_enabled(bool enable) override
	{
		if (main->previewEnabled != enable)
			main->EnablePreviewDisplay(enable);
	}

	obs_source_t *obs_frontend_get_current_preview_scene(void) override
	{
		OBSSource source = nullptr;

		if (main->IsPreviewProgramMode()) {
			source = main->GetCurrentSceneSource();
			obs_source_addref(source);
		}

		return source;
	}

	void
	obs_frontend_set_current_preview_scene(obs_source_t *scene) override
	{
		if (main->IsPreviewProgramMode()) {
			QMetaObject::invokeMethod(main, "SetCurrentScene",
						  Q_ARG(OBSSource,
							OBSSource(scene)),
						  Q_ARG(bool, false));
		}
	}

	void obs_frontend_take_screenshot(void) override
	{
		QMetaObject::invokeMethod(main, "Screenshot");
	}

	void obs_frontend_take_source_screenshot(obs_source_t *source) override
	{
		QMetaObject::invokeMethod(main, "Screenshot",
					  Q_ARG(OBSSource, OBSSource(source)));
	}

	obs_output_t *obs_frontend_get_virtualcam_output(void) override
	{
		OBSOutput output = main->outputHandler->virtualCam;
		obs_output_addref(output);
		return output;
	}

	void obs_frontend_start_virtualcam(void) override
	{
		QMetaObject::invokeMethod(main, "StartVirtualCam");
	}

	void obs_frontend_stop_virtualcam(void) override
	{
		QMetaObject::invokeMethod(main, "StopVirtualCam");
	}

	bool obs_frontend_virtualcam_active(void) override
	{
		return os_atomic_load_bool(&virtualcam_active);
	}

	void obs_frontend_reset_video(void) override { main->ResetVideo(); }

	void on_load(obs_data_t *settings) override
	{
		for (size_t i = saveCallbacks.size(); i > 0; i--) {
			auto cb = saveCallbacks[i - 1];
			cb.callback(settings, false, cb.private_data);
		}
	}

	void on_preload(obs_data_t *settings) override
	{
		for (size_t i = preloadCallbacks.size(); i > 0; i--) {
			auto cb = preloadCallbacks[i - 1];
			cb.callback(settings, false, cb.private_data);
		}
	}

	void on_save(obs_data_t *settings) override
	{
		for (size_t i = saveCallbacks.size(); i > 0; i--) {
			auto cb = saveCallbacks[i - 1];
			cb.callback(settings, true, cb.private_data);
		}
	}

	void on_event(enum obs_frontend_event event) override
	{
		if (main->disableSaving)
			return;

		for (size_t i = callbacks.size(); i > 0; i--) {
			auto cb = callbacks[i - 1];
			cb.callback(event, cb.private_data);
		}
	}
};

obs_frontend_callbacks *InitializeAPIInterface(OBSBasic *main)
{
	obs_frontend_callbacks *api = new OBSStudioAPI(main);
	obs_frontend_set_callbacks_internal(api);
	return api;
}
