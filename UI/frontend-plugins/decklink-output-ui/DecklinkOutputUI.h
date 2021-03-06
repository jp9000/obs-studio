#pragma once

#include <QDialog>

#include "ui_output.h"
#include "../../UI/properties-view.hpp"

class DecklinkOutputUI : public QDialog {
	Q_OBJECT
private:
	OBSPropertiesView *propertiesView;
	OBSPropertiesView *previewPropertiesView;

public slots:
	void on_outputButton_clicked();
	void PropertiesChanged();
	void OutputStateChanged(bool);

	void on_previewOutputButton_clicked();
	void PreviewPropertiesChanged();
	void PreviewOutputStateChanged(bool);

public:
	std::unique_ptr<Ui_Output> ui;
	DecklinkOutputUI(QWidget *parent);

	void ShowHideDialog();

	void SetupPropertiesView();
	void SaveSettings();

	void SetupPreviewPropertiesView();
	void SavePreviewSettings();
};
