#include "ui-validation.hpp"

#include <obs.hpp>
#include <QString>
#include <QMessageBox>
#include <QPushButton>

#include <obs-app.hpp>
#include <obs-service.h>

static int CountVideoSources()
{
	int count = 0;
	auto countSources = [](void *param, obs_source_t *source) {
		if (!source)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if ((flags & OBS_SOURCE_VIDEO) != 0)
			(*reinterpret_cast<int *>(param))++;

		return true;
	};

	obs_enum_sources(countSources, &count);
	return count;
}

bool UIValidation::NoSourcesConfirmation(QWidget *parent)
{
	// There are sources, don't need confirmation
	if (CountVideoSources() != 0)
		return true;

	// Ignore no video if no parent is visible to alert on
	if (!parent->isVisible())
		return true;

	QString msg = QTStr("NoSources.Text");
	msg += "\n\n";
	msg += QTStr("NoSources.Text.AddSource");

	QMessageBox messageBox(parent);
	messageBox.setWindowTitle(QTStr("NoSources.Title"));
	messageBox.setText(msg);

	QAbstractButton *yesButton =
		messageBox.addButton(QTStr("Yes"), QMessageBox::YesRole);
	messageBox.addButton(QTStr("No"), QMessageBox::NoRole);
	messageBox.setIcon(QMessageBox::Question);
	messageBox.exec();

	if (messageBox.clickedButton() != yesButton)
		return false;
	else
		return true;
}

StreamSettingsAction
UIValidation::StreamSettingsConfirmation(QWidget *parent, OBSService service)
{
	// Custom services can user API key in URL or user/pass combo.
	// So only check there is a URL
	char const *serviceType = obs_service_get_type(service);
	bool isCustomUrlService = (strcmp(serviceType, "rtmp_custom") == 0);

	char const *streamUrl = obs_service_get_url(service);
	char const *streamKey = obs_service_get_key(service);

	bool hasStreamUrl = (streamUrl != NULL && streamUrl[0] != '\0');
	bool hasStreamKey = ((streamKey != NULL && streamKey[0] != '\0') ||
			     isCustomUrlService);

	if (hasStreamUrl && hasStreamKey)
		return StreamSettingsAction::ContinueStream;

	QString msg;

	if (!hasStreamUrl && !hasStreamKey) {
		msg = QTStr("Basic.Settings.Stream.MissingUrlAndApiKey");
	} else if (!hasStreamKey) {
		msg = QTStr("Basic.Settings.Stream.MissingStreamKey");
	} else {
		msg = QTStr("Basic.Settings.Stream.MissingUrl");
	}

	QMessageBox messageBox(parent);
	messageBox.setWindowTitle(
		QTStr("Basic.Settings.Stream.MissingSettingAlert"));
	messageBox.setText(msg);

	QPushButton *cancel;
	QPushButton *settings;

#ifdef __APPLE__
#define ACCEPT_BUTTON QMessageBox::AcceptRole
#define REJECT_BUTTON QMessageBox::ResetRole
#else
#define ACCEPT_BUTTON QMessageBox::NoRole
#define REJECT_BUTTON QMessageBox::NoRole
#endif
	settings = messageBox.addButton(
		QTStr("Basic.Settings.Stream.StreamSettingsWarning"),
		ACCEPT_BUTTON);
	cancel = messageBox.addButton(QTStr("Cancel"), REJECT_BUTTON);

	messageBox.setDefaultButton(settings);
	messageBox.setEscapeButton(cancel);

	messageBox.setIcon(QMessageBox::Warning);
	messageBox.exec();

	if (messageBox.clickedButton() == settings)
		return StreamSettingsAction::OpenSettings;
	if (messageBox.clickedButton() == cancel)
		return StreamSettingsAction::Cancel;

	return StreamSettingsAction::ContinueStream;
}
