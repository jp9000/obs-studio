#include "obs-module.h"
#include "scripts.hpp"
#include "frontend-tools-config.h"
#include "../../properties-view.hpp"
#include "../../qt-wrappers.hpp"

#include <QFileDialog>
#include <QPlainTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QPushButton>
#include <QFontDatabase>
#include <QFont>
#include <QDialogButtonBox>
#include <QResizeEvent>
#include <QAction>
#include <QMessageBox>
#include <QMenu>
#include <QUrl>
#include <QDesktopServices>

#include <obs.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-scripting.h>

#include <util/config-file.h>
#include <util/platform.h>
#include <util/util.hpp>

#include <string>

#include "ui_scripts.h"

#if COMPILE_PYTHON && (defined(_WIN32) || defined(__APPLE__))
#define PYTHON_UI 1
#else
#define PYTHON_UI 0
#endif

#if ARCH_BITS == 64
#define ARCH_NAME "64bit"
#else
#define ARCH_NAME "32bit"
#endif

#define PYTHONPATH_LABEL_TEXT "PythonSettings.PythonInstallPath" ARCH_NAME

/* ----------------------------------------------------------------- */

using OBSScript = OBSObj<obs_script_t *, obs_script_destroy>;

struct ScriptData {
	std::vector<OBSScript> scripts;

	inline obs_script_t *FindScript(const char *path)
	{
		for (OBSScript &script : scripts) {
			const char *script_path = obs_script_get_path(script);
			if (strcmp(script_path, path) == 0) {
				return script;
			}
		}

		return nullptr;
	}

	bool ScriptOpened(const char *path)
	{
		for (OBSScript &script : scripts) {
			const char *script_path = obs_script_get_path(script);
			if (strcmp(script_path, path) == 0) {
				return true;
			}
		}

		return false;
	}
};

static ScriptData *scriptData = nullptr;
static ScriptsTool *scriptsWindow = nullptr;
static ScriptLogWindow *scriptLogWindow = nullptr;
static QPlainTextEdit *scriptLogWidget = nullptr;

/* ----------------------------------------------------------------- */

ScriptLogWindow::ScriptLogWindow() : QWidget(nullptr)
{
	const QFont fixedFont =
		QFontDatabase::systemFont(QFontDatabase::FixedFont);

	QPlainTextEdit *edit = new QPlainTextEdit();
	edit->setReadOnly(true);
	edit->setFont(fixedFont);
	edit->setWordWrapMode(QTextOption::NoWrap);

	QHBoxLayout *buttonLayout = new QHBoxLayout();
	QPushButton *clearButton = new QPushButton(tr("Clear"));
	connect(clearButton, &QPushButton::clicked, this,
		&ScriptLogWindow::ClearWindow);
	QPushButton *closeButton = new QPushButton(tr("Close"));
	connect(closeButton, &QPushButton::clicked, this, &QDialog::hide);

	buttonLayout->addStretch();
	buttonLayout->addWidget(clearButton);
	buttonLayout->addWidget(closeButton);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(edit);
	layout->addLayout(buttonLayout);

	setLayout(layout);
	scriptLogWidget = edit;

	resize(600, 400);

	config_t *global_config = obs_frontend_get_global_config();
	const char *geom =
		config_get_string(global_config, "ScriptLogWindow", "geometry");
	if (geom != nullptr) {
		QByteArray ba = QByteArray::fromBase64(QByteArray(geom));
		restoreGeometry(ba);
	}

	setWindowTitle(obs_module_text("ScriptLogWindow"));

	connect(edit->verticalScrollBar(), &QAbstractSlider::sliderMoved, this,
		&ScriptLogWindow::ScrollChanged);
}

ScriptLogWindow::~ScriptLogWindow()
{
	config_t *global_config = obs_frontend_get_global_config();
	config_set_string(global_config, "ScriptLogWindow", "geometry",
			  saveGeometry().toBase64().constData());
}

void ScriptLogWindow::ScrollChanged(int val)
{
	QScrollBar *scroll = scriptLogWidget->verticalScrollBar();
	bottomScrolled = (val == scroll->maximum());
}

void ScriptLogWindow::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	if (bottomScrolled) {
		QScrollBar *scroll = scriptLogWidget->verticalScrollBar();
		scroll->setValue(scroll->maximum());
	}
}

void ScriptLogWindow::AddLogMsg(int log_level, QString msg)
{
	QScrollBar *scroll = scriptLogWidget->verticalScrollBar();
	bottomScrolled = scroll->value() == scroll->maximum();

	lines += QStringLiteral("\n");
	lines += msg;
	scriptLogWidget->setPlainText(lines);

	if (bottomScrolled)
		scroll->setValue(scroll->maximum());

	if (log_level <= LOG_WARNING) {
		show();
		raise();
	}
}

void ScriptLogWindow::ClearWindow()
{
	Clear();
	scriptLogWidget->setPlainText(QString());
}

void ScriptLogWindow::Clear()
{
	lines.clear();
}

/* ----------------------------------------------------------------- */

ScriptsTool::ScriptsTool() : QWidget(nullptr), ui(new Ui_ScriptsTool)
{
	ui->setupUi(this);
	RefreshLists();

#if PYTHON_UI
	config_t *config = obs_frontend_get_global_config();
	const char *path =
		config_get_string(config, "Python", "Path" ARCH_NAME);
	ui->pythonPath->setText(path);
	ui->pythonPathLabel->setText(obs_module_text(PYTHONPATH_LABEL_TEXT));
#else
	delete ui->pythonSettingsTab;
	ui->pythonSettingsTab = nullptr;
	ui->tabWidget->setStyleSheet("QTabWidget::pane {border: 0;}");
#endif

	delete propertiesView;
	propertiesView = new QWidget();
	propertiesView->setSizePolicy(QSizePolicy::Expanding,
				      QSizePolicy::Expanding);
	ui->propertiesLayout->addWidget(propertiesView);

	config_t *global_config = obs_frontend_get_global_config();
	int row =
		config_get_int(global_config, "scripts-tool", "prevScriptRow");
	ui->scripts->setCurrentRow(row);
}

ScriptsTool::~ScriptsTool()
{
	config_t *global_config = obs_frontend_get_global_config();
	config_set_int(global_config, "scripts-tool", "prevScriptRow",
		       ui->scripts->currentRow());

	delete ui;
}

void ScriptsTool::RemoveScript(const char *path)
{
	for (size_t i = 0; i < scriptData->scripts.size(); i++) {
		OBSScript &script = scriptData->scripts[i];

		const char *script_path = obs_script_get_path(script);
		if (strcmp(script_path, path) == 0) {
			scriptData->scripts.erase(scriptData->scripts.begin() +
						  i);
			break;
		}
	}
}

void ScriptsTool::ReloadScript(const char *path)
{
	for (OBSScript &script : scriptData->scripts) {
		const char *script_path = obs_script_get_path(script);
		if (strcmp(script_path, path) == 0) {
			obs_script_reload(script);

			OBSData settings = obs_data_create();
			obs_data_release(settings);

			obs_properties_t *prop =
				obs_script_get_properties(script);
			obs_properties_apply_settings(prop, settings);
			obs_properties_destroy(prop);

			break;
		}
	}
}

void ScriptsTool::RefreshLists()
{
	ui->scripts->clear();

	for (OBSScript &script : scriptData->scripts) {
		const char *script_file = obs_script_get_file(script);
		const char *script_path = obs_script_get_path(script);

		QListWidgetItem *item = new QListWidgetItem(script_file);
		item->setData(Qt::UserRole, QString(script_path));
		ui->scripts->addItem(item);
	}
}

void ScriptsTool::SetScriptDefaults(const char *path)
{
	for (OBSScript &script : scriptData->scripts) {
		const char *script_path = obs_script_get_path(script);
		if (strcmp(script_path, path) == 0) {
			obs_data_t *settings = obs_script_get_settings(script);
			obs_data_clear(settings);
			obs_data_release(settings);

			obs_script_update(script, nullptr);
			on_reloadScripts_clicked();
			break;
		}
	}
}

void ScriptsTool::on_close_clicked()
{
	close();
}

void ScriptsTool::on_addScripts_clicked()
{
	const char **formats = obs_scripting_supported_formats();
	const char **cur_format = formats;
	QString extensions;
	QString filter;

	while (*cur_format) {
		if (!extensions.isEmpty())
			extensions += QStringLiteral(" ");

		extensions += QStringLiteral("*.");
		extensions += *cur_format;

		cur_format++;
	}

	if (!extensions.isEmpty()) {
		filter += obs_module_text("FileFilter.ScriptFiles");
		filter += QStringLiteral(" (");
		filter += extensions;
		filter += QStringLiteral(")");
	}

	if (filter.isEmpty())
		return;

	static std::string lastBrowsedDir;

	if (lastBrowsedDir.empty()) {
		BPtr<char> baseScriptPath = obs_module_file("scripts");
		lastBrowsedDir = baseScriptPath;
	}

	QStringList files = OpenFiles(this,
				      QT_UTF8(obs_module_text("AddScripts")),
				      QT_UTF8(lastBrowsedDir.c_str()), filter);
	if (!files.count())
		return;

	for (const QString &file : files) {
		lastBrowsedDir =
			QFileInfo(file).absolutePath().toUtf8().constData();

		QByteArray pathBytes = file.toUtf8();
		const char *path = pathBytes.constData();

		if (scriptData->ScriptOpened(path)) {
			continue;
		}

		obs_script_t *script = obs_script_create(path, NULL);
		if (script) {
			const char *script_file = obs_script_get_file(script);

			scriptData->scripts.emplace_back(script);

			QListWidgetItem *item =
				new QListWidgetItem(script_file);
			item->setData(Qt::UserRole, QString(file));
			ui->scripts->addItem(item);

			OBSData settings = obs_data_create();
			obs_data_release(settings);

			obs_properties_t *prop =
				obs_script_get_properties(script);
			obs_properties_apply_settings(prop, settings);
			obs_properties_destroy(prop);

			ui->scripts->setCurrentItem(item);
		}
	}
}

void ScriptsTool::on_removeScripts_clicked()
{
	QList<QListWidgetItem *> items = ui->scripts->selectedItems();

	for (QListWidgetItem *item : items)
		RemoveScript(item->data(Qt::UserRole)
				     .toString()
				     .toUtf8()
				     .constData());
	RefreshLists();
}

void ScriptsTool::on_reloadScripts_clicked()
{
	QList<QListWidgetItem *> items = ui->scripts->selectedItems();
	for (QListWidgetItem *item : items)
		ReloadScript(item->data(Qt::UserRole)
				     .toString()
				     .toUtf8()
				     .constData());

	on_scripts_currentRowChanged(ui->scripts->currentRow());
}

void ScriptsTool::OpenScriptParentDirectory()
{
	QList<QListWidgetItem *> items = ui->scripts->selectedItems();
	for (QListWidgetItem *item : items) {
		QDir dir(item->data(Qt::UserRole).toString());
		dir.cdUp();
		QDesktopServices::openUrl(
			QUrl::fromLocalFile(dir.absolutePath()));
	}
}

void ScriptsTool::on_scripts_customContextMenuRequested(const QPoint &pos)
{

	QListWidgetItem *item = ui->scripts->itemAt(pos);

	QMenu popup(this);

	obs_frontend_push_ui_translation(obs_module_get_string);

	popup.addAction(tr("Add"), this, SLOT(on_addScripts_clicked()));

	if (item) {
		popup.addSeparator();
		popup.addAction(obs_module_text("Reload"), this,
				SLOT(on_reloadScripts_clicked()));
		popup.addAction(obs_module_text("OpenFileLocation"), this,
				SLOT(OpenScriptParentDirectory()));
		popup.addSeparator();
		popup.addAction(tr("Remove"), this,
				SLOT(on_removeScripts_clicked()));
	}
	obs_frontend_pop_ui_translation();

	popup.exec(QCursor::pos());
}

void ScriptsTool::on_scriptLog_clicked()
{
	scriptLogWindow->show();
	scriptLogWindow->raise();
}

void ScriptsTool::on_pythonPathBrowse_clicked()
{
	QString curPath = ui->pythonPath->text();
	QString newPath =
		SelectDirectory(this, ui->pythonPathLabel->text(), curPath);

	if (newPath.isEmpty())
		return;

	QByteArray array = newPath.toUtf8();
	const char *path = array.constData();

	config_t *config = obs_frontend_get_global_config();
	config_set_string(config, "Python", "Path" ARCH_NAME, path);

	ui->pythonPath->setText(newPath);

	if (obs_scripting_python_loaded())
		return;
	if (!obs_scripting_load_python(path))
		return;

	for (OBSScript &script : scriptData->scripts) {
		enum obs_script_lang lang = obs_script_get_lang(script);
		if (lang == OBS_SCRIPT_LANG_PYTHON) {
			obs_script_reload(script);
		}
	}

	on_scripts_currentRowChanged(ui->scripts->currentRow());
}

void ScriptsTool::on_scripts_currentRowChanged(int row)
{
	ui->propertiesLayout->removeWidget(propertiesView);
	delete propertiesView;

	if (row == -1) {
		propertiesView = new QWidget();
		propertiesView->setSizePolicy(QSizePolicy::Expanding,
					      QSizePolicy::Expanding);
		ui->propertiesLayout->addWidget(propertiesView);
		ui->description->setText(QString());
		return;
	}

	QByteArray array =
		ui->scripts->item(row)->data(Qt::UserRole).toString().toUtf8();
	const char *path = array.constData();

	obs_script_t *script = scriptData->FindScript(path);
	if (!script) {
		propertiesView = nullptr;
		return;
	}

	OBSData settings = obs_script_get_settings(script);
	obs_data_release(settings);

	propertiesView = new OBSPropertiesView(
		settings, script,
		(PropertiesReloadCallback)obs_script_get_properties,
		(PropertiesUpdateCallback)obs_script_update);
	ui->propertiesLayout->addWidget(propertiesView);
	ui->description->setText(obs_script_get_description(script));
}

void ScriptsTool::on_defaults_clicked()
{
	QListWidgetItem *item = ui->scripts->currentItem();
	if (!item)
		return;

	SetScriptDefaults(
		item->data(Qt::UserRole).toString().toUtf8().constData());
}

void ScriptsTool::on_description_linkActivated(const QString &link)
{
	QUrl url(link, QUrl::StrictMode);
	if (url.isValid() && (url.scheme().compare("http") == 0 ||
			      url.scheme().compare("https") == 0)) {
		QString msg(obs_module_text("ScriptDescriptionLink.Text"));
		msg += "\n\n";
		msg += QString(obs_module_text(
				       "ScriptDescriptionLink.Text.Url"))
			       .arg(link);

		const char *open =
			obs_module_text("ScriptDescriptionLink.OpenURL");

		QMessageBox messageBox(this);
		messageBox.setWindowTitle(open);
		messageBox.setText(msg);

		obs_frontend_push_ui_translation(obs_module_get_string);
		QPushButton *yesButton =
			messageBox.addButton(open, QMessageBox::YesRole);
		QPushButton *noButton =
			messageBox.addButton(tr("Cancel"), QMessageBox::NoRole);
		obs_frontend_pop_ui_translation();
		messageBox.setDefaultButton(yesButton);
		messageBox.setEscapeButton(noButton);
		messageBox.setIcon(QMessageBox::Question);
		messageBox.exec();

		if (messageBox.clickedButton() == yesButton)
			QDesktopServices::openUrl(url);
	}
}

/* ----------------------------------------------------------------- */

extern "C" void FreeScripts()
{
	obs_scripting_unload();
}

static void obs_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		delete scriptData;
		delete scriptsWindow;
		delete scriptLogWindow;

	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		scriptLogWindow->hide();
		scriptLogWindow->Clear();

		delete scriptData;
		scriptData = new ScriptData;
	}
}

static void load_script_data(obs_data_t *load_data, bool, void *)
{
	obs_data_array_t *array = obs_data_get_array(load_data, "scripts-tool");

	delete scriptData;
	scriptData = new ScriptData;

	size_t size = obs_data_array_count(array);
	for (size_t i = 0; i < size; i++) {
		obs_data_t *obj = obs_data_array_item(array, i);
		const char *path = obs_data_get_string(obj, "path");
		obs_data_t *settings = obs_data_get_obj(obj, "settings");

		obs_script_t *script = obs_script_create(path, settings);
		if (script) {
			scriptData->scripts.emplace_back(script);
		}

		obs_data_release(settings);
		obs_data_release(obj);
	}

	if (scriptsWindow)
		scriptsWindow->RefreshLists();

	obs_data_array_release(array);
}

static void save_script_data(obs_data_t *save_data, bool saving, void *)
{
	if (!saving)
		return;

	obs_data_array_t *array = obs_data_array_create();

	for (OBSScript &script : scriptData->scripts) {
		const char *script_path = obs_script_get_path(script);
		obs_data_t *settings = obs_script_save(script);

		obs_data_t *obj = obs_data_create();
		obs_data_set_string(obj, "path", script_path);
		obs_data_set_obj(obj, "settings", settings);
		obs_data_array_push_back(array, obj);
		obs_data_release(obj);

		obs_data_release(settings);
	}

	obs_data_set_array(save_data, "scripts-tool", array);
	obs_data_array_release(array);
}

static void script_log(void *, obs_script_t *script, int log_level,
		       const char *message)
{
	QString qmsg;

	if (script) {
		qmsg = QStringLiteral("[%1] %2").arg(
			obs_script_get_file(script), message);
	} else {
		qmsg = QStringLiteral("[Unknown Script] %1").arg(message);
	}

	QMetaObject::invokeMethod(scriptLogWindow, "AddLogMsg",
				  Q_ARG(int, log_level), Q_ARG(QString, qmsg));
}

extern "C" void InitScripts()
{
	scriptLogWindow = new ScriptLogWindow();

	obs_scripting_load();
	obs_scripting_set_log_callback(script_log, nullptr);

	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("Scripts"));

#if PYTHON_UI
	config_t *config = obs_frontend_get_global_config();
	const char *python_path =
		config_get_string(config, "Python", "Path" ARCH_NAME);

	if (!obs_scripting_python_loaded() && python_path && *python_path)
		obs_scripting_load_python(python_path);
#endif

	scriptData = new ScriptData;

	auto cb = []() {
		obs_frontend_push_ui_translation(obs_module_get_string);

		if (!scriptsWindow) {
			scriptsWindow = new ScriptsTool();
			scriptsWindow->show();
		} else {
			scriptsWindow->show();
			scriptsWindow->raise();
		}

		obs_frontend_pop_ui_translation();
	};

	obs_frontend_add_save_callback(save_script_data, nullptr);
	obs_frontend_add_preload_callback(load_script_data, nullptr);
	obs_frontend_add_event_callback(obs_event, nullptr);

	action->connect(action, &QAction::triggered, cb);
}
