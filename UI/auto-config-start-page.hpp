#pragma once

#include <QWizard>
#include <QWizardPage>

#include "auto-config-enums.hpp"

enum class PriorityMode {
	Streaming,
	Recording,
	VirtualCam,
};

class AutoConfigStartPage : public QWizardPage {
	Q_OBJECT

public:
	AutoConfigStartPage(QWidget *parent = nullptr);
	~AutoConfigStartPage();

	virtual int nextId() const override;

private:
	PriorityMode selectedMode_;

signals:
	void priorityModeChanged(PriorityMode mode);
};
