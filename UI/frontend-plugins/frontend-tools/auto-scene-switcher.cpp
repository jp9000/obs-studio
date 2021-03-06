#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>
#include <QMainWindow>
#include <QMessageBox>
#include <QAction>
#include "auto-scene-switcher.hpp"
#include "tool-helpers.hpp"

#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <mutex>

using namespace std;

#define DEFAULT_INTERVAL 300

struct SceneSwitch {
	OBSWeakSource scene;
	string window;
	regex re;

	inline SceneSwitch(OBSWeakSource scene_, const char *window_)
		: scene(scene_), window(window_), re(window_)
	{
	}
};

static inline bool WeakSourceValid(obs_weak_source_t *ws)
{
	obs_source_t *source = obs_weak_source_get_source(ws);
	if (source)
		obs_source_release(source);
	return !!source;
}

struct SwitcherData {
	thread th;
	condition_variable cv;
	mutex m;
	bool stop = false;

	vector<SceneSwitch> switches;
	OBSWeakSource nonMatchingScene;
	int interval = DEFAULT_INTERVAL;
	bool switchIfNotMatching = false;
	bool startAtLaunch = false;

	void Thread();
	void Start();
	void Stop();

	void Prune()
	{
		for (size_t i = 0; i < switches.size(); i++) {
			SceneSwitch &s = switches[i];
			if (!WeakSourceValid(s.scene))
				switches.erase(switches.begin() + i--);
		}

		if (nonMatchingScene && !WeakSourceValid(nonMatchingScene)) {
			switchIfNotMatching = false;
			nonMatchingScene = nullptr;
		}
	}

	inline ~SwitcherData() { Stop(); }
};

static SwitcherData *switcher = nullptr;

static inline QString MakeSwitchName(const QString &scene,
				     const QString &window)
{
	return QStringLiteral("[") + scene + QStringLiteral("]: ") + window;
}

SceneSwitcher::SceneSwitcher(QWidget *parent)
	: QDialog(parent), ui(new Ui_SceneSwitcher)
{
	ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	lock_guard<mutex> lock(switcher->m);

	switcher->Prune();

	BPtr<char *> scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		ui->scenes->addItem(name);
		ui->noMatchSwitchScene->addItem(name);
		temp++;
	}

	if (switcher->switchIfNotMatching)
		ui->noMatchSwitch->setChecked(true);
	else
		ui->noMatchDontSwitch->setChecked(true);

	ui->noMatchSwitchScene->setCurrentText(
		GetWeakSourceName(switcher->nonMatchingScene).c_str());
	ui->checkInterval->setValue(switcher->interval);

	vector<string> windows;
	GetWindowList(windows);

	for (string &window : windows)
		ui->windows->addItem(window.c_str());

	for (auto &s : switcher->switches) {
		string sceneName = GetWeakSourceName(s.scene);
		QString text =
			MakeSwitchName(sceneName.c_str(), s.window.c_str());

		QListWidgetItem *item = new QListWidgetItem(text, ui->switches);
		item->setData(Qt::UserRole, s.window.c_str());
	}

	if (switcher->th.joinable())
		SetStarted();
	else
		SetStopped();

	loading = false;
}

void SceneSwitcher::closeEvent(QCloseEvent *)
{
	obs_frontend_save();
}

int SceneSwitcher::FindByData(const QString &window)
{
	int count = ui->switches->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->switches->item(i);
		QString itemWindow = item->data(Qt::UserRole).toString();

		if (itemWindow == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SceneSwitcher::on_switches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->switches->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->switches) {
		if (window.compare(s.window.c_str()) == 0) {
			string name = GetWeakSourceName(s.scene);
			ui->scenes->setCurrentText(name.c_str());
			ui->windows->setCurrentText(window);
			break;
		}
	}
}

void SceneSwitcher::on_close_clicked()
{
	done(0);
}

void SceneSwitcher::on_add_clicked()
{
	QString sceneName = ui->scenes->currentText();
	QString windowName = ui->windows->currentText();

	if (windowName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	QVariant v = QVariant::fromValue(windowName);

	QString text = MakeSwitchName(sceneName, windowName);

	int idx = FindByData(windowName);

	if (idx == -1) {
		try {
			lock_guard<mutex> lock(switcher->m);
			switcher->switches.emplace_back(
				source, windowName.toUtf8().constData());

			QListWidgetItem *item =
				new QListWidgetItem(text, ui->switches);
			item->setData(Qt::UserRole, v);
		} catch (const regex_error &) {
			QMessageBox::warning(
				this, obs_module_text("InvalidRegex.Title"),
				obs_module_text("InvalidRegex.Text"));
		}
	} else {
		QListWidgetItem *item = ui->switches->item(idx);
		item->setText(text);

		string window = windowName.toUtf8().constData();

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto &s : switcher->switches) {
				if (s.window == window) {
					s.scene = source;
					break;
				}
			}
		}

		ui->switches->sortItems();
	}
}

void SceneSwitcher::on_remove_clicked()
{
	QListWidgetItem *item = ui->switches->currentItem();
	if (!item)
		return;

	string window =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->switches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.window == window) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_startAtLaunch_toggled(bool value)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->startAtLaunch = value;
}

void SceneSwitcher::UpdateNonMatchingScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->nonMatchingScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_noMatchDontSwitch_clicked()
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->switchIfNotMatching = false;
}

void SceneSwitcher::on_noMatchSwitch_clicked()
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->switchIfNotMatching = true;
	UpdateNonMatchingScene(ui->noMatchSwitchScene->currentText());
}

void SceneSwitcher::on_noMatchSwitchScene_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	UpdateNonMatchingScene(text);
}

void SceneSwitcher::on_checkInterval_valueChanged(int value)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->interval = value;
}

void SceneSwitcher::SetStarted()
{
	ui->toggleStartButton->setText(obs_module_text("Stop"));
	ui->pluginRunningText->setText(obs_module_text("Active"));
}

void SceneSwitcher::SetStopped()
{
	ui->toggleStartButton->setText(obs_module_text("Start"));
	ui->pluginRunningText->setText(obs_module_text("Inactive"));
}

void SceneSwitcher::on_toggleStartButton_clicked()
{
	if (switcher->th.joinable()) {
		switcher->Stop();
		SetStopped();
	} else {
		switcher->Start();
		SetStarted();
	}
}

static void SaveSceneSwitcher(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		lock_guard<mutex> lock(switcher->m);
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *array = obs_data_array_create();

		switcher->Prune();

		for (SceneSwitch &s : switcher->switches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.scene);
			if (source) {
				const char *n = obs_source_get_name(source);
				obs_data_set_string(array_obj, "scene", n);
				obs_data_set_string(array_obj, "window_title",
						    s.window.c_str());
				obs_data_array_push_back(array, array_obj);
				obs_source_release(source);
			}

			obs_data_release(array_obj);
		}

		string nonMatchingSceneName =
			GetWeakSourceName(switcher->nonMatchingScene);

		obs_data_set_int(obj, "interval", switcher->interval);
		obs_data_set_string(obj, "non_matching_scene",
				    nonMatchingSceneName.c_str());
		obs_data_set_bool(obj, "switch_if_not_matching",
				  switcher->switchIfNotMatching);
		obs_data_set_bool(obj, "active", switcher->th.joinable());
		obs_data_set_array(obj, "switches", array);

		obs_data_set_obj(save_data, "auto-scene-switcher", obj);

		obs_data_array_release(array);
		obs_data_release(obj);
	} else {
		switcher->m.lock();

		obs_data_t *obj =
			obs_data_get_obj(save_data, "auto-scene-switcher");
		obs_data_array_t *array = obs_data_get_array(obj, "switches");
		size_t count = obs_data_array_count(array);

		if (!obj)
			obj = obs_data_create();

		obs_data_set_default_int(obj, "interval", DEFAULT_INTERVAL);

		switcher->interval = obs_data_get_int(obj, "interval");
		switcher->switchIfNotMatching =
			obs_data_get_bool(obj, "switch_if_not_matching");
		string nonMatchingScene =
			obs_data_get_string(obj, "non_matching_scene");
		bool active = obs_data_get_bool(obj, "active");

		switcher->nonMatchingScene =
			GetWeakSourceByName(nonMatchingScene.c_str());

		switcher->switches.clear();

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(array, i);

			const char *scene =
				obs_data_get_string(array_obj, "scene");
			const char *window =
				obs_data_get_string(array_obj, "window_title");

			switcher->switches.emplace_back(
				GetWeakSourceByName(scene), window);

			obs_data_release(array_obj);
		}

		obs_data_array_release(array);
		obs_data_release(obj);

		switcher->m.unlock();

		if (active)
			switcher->Start();
		else
			switcher->Stop();
	}
}

void SwitcherData::Thread()
{
	chrono::duration<long long, milli> duration =
		chrono::milliseconds(interval);
	string lastTitle;
	string title;

	for (;;) {
		unique_lock<mutex> lock(m);
		OBSWeakSource scene;
		bool match = false;

		cv.wait_for(lock, duration);
		if (switcher->stop) {
			switcher->stop = false;
			break;
		}

		duration = chrono::milliseconds(interval);

		GetCurrentWindowTitle(title);

		if (lastTitle != title) {
			switcher->Prune();

			for (SceneSwitch &s : switches) {
				if (s.window == title) {
					match = true;
					scene = s.scene;
					break;
				}
			}

			/* try regex */
			if (!match) {
				for (SceneSwitch &s : switches) {
					try {
						bool matches = regex_match(
							title, s.re);
						if (matches) {
							match = true;
							scene = s.scene;
							break;
						}
					} catch (const regex_error &) {
					}
				}
			}

			if (!match && switchIfNotMatching && nonMatchingScene) {
				match = true;
				scene = nonMatchingScene;
			}

			if (match) {
				obs_source_t *source =
					obs_weak_source_get_source(scene);
				obs_source_t *currentSource =
					obs_frontend_get_current_scene();

				if (source && source != currentSource)
					obs_frontend_set_current_scene(source);

				obs_source_release(currentSource);
				obs_source_release(source);
			}
		}

		lastTitle = title;
	}
}

void SwitcherData::Start()
{
	if (!switcher->th.joinable())
		switcher->th = thread([]() { switcher->Thread(); });
}

void SwitcherData::Stop()
{
	if (th.joinable()) {
		{
			lock_guard<mutex> lock(m);
			stop = true;
		}
		cv.notify_one();
		th.join();
	}
}

extern "C" void FreeSceneSwitcher()
{
	CleanupSceneSwitcher();

	delete switcher;
	switcher = nullptr;
}

static void OBSEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT)
		FreeSceneSwitcher();
}

extern "C" void InitSceneSwitcher()
{
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("SceneSwitcher"));

	switcher = new SwitcherData;

	auto cb = []() {
		obs_frontend_push_ui_translation(obs_module_get_string);

		QMainWindow *window =
			(QMainWindow *)obs_frontend_get_main_window();

		SceneSwitcher ss(window);
		ss.exec();

		obs_frontend_pop_ui_translation();
	};

	obs_frontend_add_save_callback(SaveSceneSwitcher, nullptr);
	obs_frontend_add_event_callback(OBSEvent, nullptr);

	action->connect(action, &QAction::triggered, cb);
}
