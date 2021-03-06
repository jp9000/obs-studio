#include "DecklinkOutputUI.h"
#include <obs-module.h>
#include <util/platform.h>
#include <util/util.hpp>
#include "decklink-ui-main.h"

DecklinkOutputUI::DecklinkOutputUI(QWidget *parent)
	: QDialog(parent), ui(new Ui_Output)
{
	ui->setupUi(this);

	setSizeGripEnabled(true);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	propertiesView = nullptr;
	previewPropertiesView = nullptr;
}

void DecklinkOutputUI::ShowHideDialog()
{
	SetupPropertiesView();
	SetupPreviewPropertiesView();

	setVisible(!isVisible());
}

void DecklinkOutputUI::SetupPropertiesView()
{
	if (propertiesView)
		delete propertiesView;

	obs_data_t *settings = obs_data_create();

	OBSData data = load_settings();
	if (data)
		obs_data_apply(settings, data);

	propertiesView = new OBSPropertiesView(
		settings, "decklink_output",
		(PropertiesReloadCallback)obs_get_output_properties, 170);

	ui->propertiesLayout->addWidget(propertiesView);
	obs_data_release(settings);

	connect(propertiesView, SIGNAL(Changed()), this,
		SLOT(PropertiesChanged()));
}

void DecklinkOutputUI::SaveSettings()
{
	BPtr<char> modulePath =
		obs_module_get_config_path(obs_current_module(), "");

	os_mkdirs(modulePath);

	BPtr<char> path = obs_module_get_config_path(
		obs_current_module(), "decklinkOutputProps.json");

	obs_data_t *settings = propertiesView->GetSettings();
	if (settings)
		obs_data_save_json_safe(settings, path, "tmp", "bak");
}

void DecklinkOutputUI::SetupPreviewPropertiesView()
{
	if (previewPropertiesView)
		delete previewPropertiesView;

	obs_data_t *settings = obs_data_create();

	OBSData data = load_preview_settings();
	if (data)
		obs_data_apply(settings, data);

	previewPropertiesView = new OBSPropertiesView(
		settings, "decklink_output",
		(PropertiesReloadCallback)obs_get_output_properties, 170);

	ui->previewPropertiesLayout->addWidget(previewPropertiesView);
	obs_data_release(settings);

	connect(previewPropertiesView, SIGNAL(Changed()), this,
		SLOT(PreviewPropertiesChanged()));
}

void DecklinkOutputUI::SavePreviewSettings()
{
	char *modulePath = obs_module_get_config_path(obs_current_module(), "");

	os_mkdirs(modulePath);

	char *path = obs_module_get_config_path(
		obs_current_module(), "decklinkPreviewOutputProps.json");

	obs_data_t *settings = previewPropertiesView->GetSettings();
	if (settings)
		obs_data_save_json_safe(settings, path, "tmp", "bak");
}

void DecklinkOutputUI::on_outputButton_clicked()
{
	SaveSettings();
	output_toggle();
}

void DecklinkOutputUI::PropertiesChanged()
{
	SaveSettings();
}

void DecklinkOutputUI::OutputStateChanged(bool active)
{
	QString text;
	if (active) {
		text = QString(obs_module_text("Stop"));
	} else {
		text = QString(obs_module_text("Start"));
	}

	ui->outputButton->setChecked(active);
	ui->outputButton->setText(text);
}

void DecklinkOutputUI::on_previewOutputButton_clicked()
{
	SavePreviewSettings();
	preview_output_toggle();
}

void DecklinkOutputUI::PreviewPropertiesChanged()
{
	SavePreviewSettings();
}

void DecklinkOutputUI::PreviewOutputStateChanged(bool active)
{
	QString text;
	if (active) {
		text = QString(obs_module_text("Stop"));
	} else {
		text = QString(obs_module_text("Start"));
	}

	ui->previewOutputButton->setChecked(active);
	ui->previewOutputButton->setText(text);
}
