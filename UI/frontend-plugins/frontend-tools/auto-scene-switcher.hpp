#pragma once

#include <QDialog>
#include <memory>
#include <vector>
#include <string>

#include "ui_auto-scene-switcher.h"

struct obs_weak_source;
typedef struct obs_weak_source obs_weak_source_t;

class QCloseEvent;

class SceneSwitcher : public QDialog {
	Q_OBJECT

public:
	std::unique_ptr<Ui_SceneSwitcher> ui;
	bool loading = true;

	SceneSwitcher(QWidget *parent);

	void closeEvent(QCloseEvent *event) override;

	void SetStarted();
	void SetStopped();

	int FindByData(const QString &window);

	void UpdateNonMatchingScene(const QString &name);

public slots:
	void on_switches_currentRowChanged(int idx);
	void on_close_clicked();
	void on_add_clicked();
	void on_remove_clicked();
	void on_noMatchDontSwitch_clicked();
	void on_noMatchSwitch_clicked();
	void on_startAtLaunch_toggled(bool value);
	void on_noMatchSwitchScene_currentTextChanged(const QString &text);
	void on_checkInterval_valueChanged(int value);
	void on_toggleStartButton_clicked();
};

void GetWindowList(std::vector<std::string> &windows);
void GetCurrentWindowTitle(std::string &title);
void CleanupSceneSwitcher();
