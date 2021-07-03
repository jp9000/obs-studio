#include "window-basic-settings.hpp"
#include "window-basic-main.hpp"
#include "obs-frontend-api.h"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"
#include <QColorDialog>

#define I18nStr(str) QTStr(str).toStdString().c_str()

enum ColorPreset {
	COLOR_PRESET_DEFAULT,
	COLOR_PRESET_COLOR_BLIND_1,
	COLOR_PRESET_CUSTOM = 99,
};

static inline bool WidgetChanged(QWidget *widget)
{
	return widget->property("changed").toBool();
}

static inline QColor color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff,
		      (val >> 24) & 0xff);
}

static inline long long color_to_int(QColor color)
{
	auto shift = [&](unsigned val, int shift) {
		return ((val & 0xff) << shift);
	};

	return shift(color.red(), 0) | shift(color.green(), 8) |
	       shift(color.blue(), 16) | shift(color.alpha(), 24);
}

QColor OBSBasicSettings::GetColor(uint32_t colorVal, QString label)
{
	QColorDialog::ColorDialogOptions options;

#ifndef _WIN32
	options |= QColorDialog::DontUseNativeDialog;
#endif

	QColor color = color_from_int(colorVal);

	return QColorDialog::getColor(color, this, label, options);
}

void OBSBasicSettings::LoadA11ySettings(bool presetChange)
{
	config_t *config = GetGlobalConfig();

	if (!presetChange) {
		preset = config_get_int(config, "Accessibility", "ColorPreset");

		bool block = ui->colorPreset->blockSignals(true);
		ui->colorPreset->setCurrentIndex(std::min(
			preset, (uint32_t)ui->colorPreset->count() - 1));
		ui->colorPreset->blockSignals(block);

		bool checked = config_get_bool(config, "Accessibility",
					       "OverrideColors");

		ui->colorsGroupBox->setChecked(checked);
	}

	if (preset == COLOR_PRESET_DEFAULT) {
		ResetDefaultColors();
		SetDefaultColors();
	} else if (preset == COLOR_PRESET_COLOR_BLIND_1) {
		ResetDefaultColors();

		mixerGreenActive = 0x742E94;
		mixerGreen = 0x4A1A60;
		mixerYellowActive = 0x3349F9;
		mixerYellow = 0x1F2C97;
		mixerRedActive = 0xBEAC63;
		mixerRed = 0x675B28;

		selectRed = 0x3349F9;
		selectGreen = 0xFF56C9;
		selectBlue = 0xB09B44;

		SetDefaultColors();
	} else if (preset == COLOR_PRESET_CUSTOM) {
		SetDefaultColors();

		selectRed =
			config_get_int(config, "Accessibility", "SelectRed");
		selectGreen =
			config_get_int(config, "Accessibility", "SelectGreen");
		selectBlue =
			config_get_int(config, "Accessibility", "SelectBlue");

		mixerGreen =
			config_get_int(config, "Accessibility", "MixerGreen");
		mixerYellow =
			config_get_int(config, "Accessibility", "MixerYellow");
		mixerRed = config_get_int(config, "Accessibility", "MixerRed");

		mixerGreenActive = config_get_int(config, "Accessibility",
						  "MixerGreenActive");
		mixerYellowActive = config_get_int(config, "Accessibility",
						   "MixerYellowActive");
		mixerRedActive = config_get_int(config, "Accessibility",
						"MixerRedActive");
	}

	UpdateA11yColors();
}

void OBSBasicSettings::SaveA11ySettings()
{
	config_t *config = GetGlobalConfig();

	config_set_bool(config, "Accessibility", "OverrideColors",
			ui->colorsGroupBox->isChecked());
	config_set_int(config, "Accessibility", "ColorPreset", preset);

	config_set_int(config, "Accessibility", "SelectRed", selectRed);
	config_set_int(config, "Accessibility", "SelectGreen", selectGreen);
	config_set_int(config, "Accessibility", "SelectBlue", selectBlue);
	config_set_int(config, "Accessibility", "MixerGreen", mixerGreen);
	config_set_int(config, "Accessibility", "MixerYellow", mixerYellow);
	config_set_int(config, "Accessibility", "MixerRed", mixerRed);
	config_set_int(config, "Accessibility", "MixerGreenActive",
		       mixerGreenActive);
	config_set_int(config, "Accessibility", "MixerYellowActive",
		       mixerYellowActive);
	config_set_int(config, "Accessibility", "MixerRedActive",
		       mixerRedActive);

	main->RefreshVolumeColors();
}

#define SetStyle(label, colorVal)                                             \
	color = color_from_int(colorVal);                                     \
	color.setAlpha(255);                                                  \
	palette = QPalette(color);                                            \
	label->setFrameStyle(QFrame::Sunken | QFrame::Panel);                 \
	label->setText(color.name(QColor::HexRgb));                           \
	label->setPalette(palette);                                           \
	label->setStyleSheet(QString("background-color: %1; color: %2;")      \
				     .arg(palette.color(QPalette::Window)     \
						  .name(QColor::HexRgb))      \
				     .arg(palette.color(QPalette::WindowText) \
						  .name(QColor::HexRgb)));    \
	label->setAutoFillBackground(true);                                   \
	label->setAlignment(Qt::AlignCenter);

void OBSBasicSettings::UpdateA11yColors()
{
	QPalette palette;
	QColor color;

	SetStyle(ui->color1, selectRed);
	SetStyle(ui->color2, selectGreen);
	SetStyle(ui->color3, selectBlue);
	SetStyle(ui->color4, mixerGreen);
	SetStyle(ui->color5, mixerYellow);
	SetStyle(ui->color6, mixerRed);
	SetStyle(ui->color7, mixerGreenActive);
	SetStyle(ui->color8, mixerYellowActive);
	SetStyle(ui->color9, mixerRedActive);
}

void OBSBasicSettings::SetDefaultColors()
{
	config_t *config = GetGlobalConfig();
	config_set_default_int(config, "Accessibility", "SelectRed", selectRed);
	config_set_default_int(config, "Accessibility", "SelectGreen",
			       selectGreen);
	config_set_default_int(config, "Accessibility", "SelectBlue",
			       selectBlue);

	config_set_default_int(config, "Accessibility", "MixerGreen",
			       mixerGreen);
	config_set_default_int(config, "Accessibility", "MixerYellow",
			       mixerYellow);
	config_set_default_int(config, "Accessibility", "MixerRed", mixerRed);

	config_set_default_int(config, "Accessibility", "MixerGreenActive",
			       mixerGreenActive);
	config_set_default_int(config, "Accessibility", "MixerYellowActive",
			       mixerYellowActive);
	config_set_default_int(config, "Accessibility", "MixerRedActive",
			       mixerRedActive);
}

void OBSBasicSettings::ResetDefaultColors()
{
	selectRed = 0x0000FF;
	selectGreen = 0x00FF00;
	selectBlue = 0xFF7F00;
	mixerGreen = 0x267f26;
	mixerYellow = 0x267f7f;
	mixerRed = 0x26267f;
	mixerGreenActive = 0x4cff4c;
	mixerYellowActive = 0x4cffff;
	mixerRedActive = 0x4c4cff;
}

void OBSBasicSettings::on_colorPreset_currentIndexChanged(int idx)
{
	preset = idx == ui->colorPreset->count() - 1 ? COLOR_PRESET_CUSTOM
						     : idx;
	LoadA11ySettings(true);
}

void OBSBasicSettings::on_choose1_clicked()
{
	QColor color = GetColor(
		selectRed,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.SelectRed"));

	if (!color.isValid())
		return;

	selectRed = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose2_clicked()
{
	QColor color = GetColor(
		selectGreen,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.SelectGreen"));

	if (!color.isValid())
		return;

	selectGreen = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose3_clicked()
{
	QColor color = GetColor(
		selectBlue,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.SelectBlue"));

	if (!color.isValid())
		return;

	selectBlue = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose4_clicked()
{
	QColor color = GetColor(
		mixerGreen,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.MixerGreen"));

	if (!color.isValid())
		return;

	mixerGreen = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose5_clicked()
{
	QColor color = GetColor(
		mixerYellow,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.MixerYellow"));

	if (!color.isValid())
		return;

	mixerYellow = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose6_clicked()
{
	QColor color = GetColor(
		mixerRed,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.MixerRed"));

	if (!color.isValid())
		return;

	mixerRed = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose7_clicked()
{
	QColor color = GetColor(
		mixerGreenActive,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.MixerGreenActive"));

	if (!color.isValid())
		return;

	mixerGreenActive = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose8_clicked()
{
	QColor color = GetColor(
		mixerYellowActive,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.MixerYellowActive"));

	if (!color.isValid())
		return;

	mixerYellowActive = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}

void OBSBasicSettings::on_choose9_clicked()
{
	QColor color = GetColor(
		mixerRedActive,
		QTStr("Basic.Settings.Accessibility.ColorOverrides.MixerRedActive"));

	if (!color.isValid())
		return;

	mixerRedActive = color_to_int(color);

	preset = COLOR_PRESET_CUSTOM;
	bool block = ui->colorPreset->blockSignals(true);
	ui->colorPreset->setCurrentIndex(ui->colorPreset->count() - 1);
	ui->colorPreset->blockSignals(block);

	A11yChanged();

	UpdateA11yColors();
}
