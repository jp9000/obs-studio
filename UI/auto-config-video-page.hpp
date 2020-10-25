#pragma once

#include <QWizard>
#include <QWizardPage>
#include <QSharedPointer>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>

#include "auto-config-enums.hpp"
#include "auto-config-model.hpp"

class AutoConfigWizard;

class AutoConfigVideoPage : public QWizardPage {
	Q_OBJECT

	QSharedPointer<AutoConfig::AutoConfigModel> wizardModel;
	QVBoxLayout *mainVLayout;
	QFormLayout *formLayout;
	QComboBox *resComboBox;
	QComboBox *fpsComboBox;

	void setupFormLayout();
	void populateFpsOptions();
	void populateResolutionOptions();

public:
	AutoConfigVideoPage(
		QWidget *parent,
		QSharedPointer<AutoConfig::AutoConfigModel> wizardModel);
	~AutoConfigVideoPage();

	virtual int nextId() const override;
	virtual bool validatePage() override;
};
