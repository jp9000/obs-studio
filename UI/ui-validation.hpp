#pragma once

#include <QObject>
#include <QWidget>

#include <obs.hpp>

enum class StreamSettingsAction {
	OpenSettings,
	Cancel,
	ContinueStream,
};

class UIValidation : public QObject {
	Q_OBJECT

public:
	/* Confirm video about to record or stream has sources.  Shows alert
	 * box notifying there are no video sources Returns true if user clicks
	 * "Yes" Returns false if user clicks "No" */
	static bool NoSourcesConfirmation(QWidget *parent);

	/* Check streaming requirements, shows warning with options to open
	 * settings, cancel stream, or attempt connection anyways.  If setup
	 * basics is missing in stream, explain missing fields and offer to
	 * open settings, cancel, or continue.  Returns Continue if all
	 * settings are valid. */
	static StreamSettingsAction
	StreamSettingsConfirmation(QWidget *parent, OBSService service);
};
