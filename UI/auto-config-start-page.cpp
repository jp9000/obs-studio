#include "auto-config-start-page.hpp"

#include <QFormLayout>
#include <QRadioButton>
#include <obs.hpp>

#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "auto-config-model.hpp"
#include "qt-wrappers.hpp"

AutoConfigStartPage::AutoConfigStartPage(QWidget *parent) : QWizardPage(parent)
{
	selectedMode_ = PriorityMode::Streaming;
	setTitle(QTStr("Basic.AutoConfig.StartPage"));
	setSubTitle(QTStr("Basic.AutoConfig.StartPage.SubTitle"));

	// Setup UI
	QVBoxLayout *layout = new QVBoxLayout;
	this->setLayout(layout);
	this->setMinimumWidth(AutoConfig::AutoConfigModel::wizardMinWidth());

	// Setup Buttons
	QString streamLabel =
		QTStr("Basic.AutoConfig.StartPage.PrioritizeStreaming");
	QRadioButton *streamBtn = new QRadioButton(streamLabel, this);
	streamBtn->setChecked(true);
	layout->addWidget(streamBtn);
	connect(streamBtn, &QPushButton::clicked, [this] {
		selectedMode_ = PriorityMode::Streaming;
		emit priorityModeChanged(PriorityMode::Streaming);
	});

	QString recordLabel =
		QTStr("Basic.AutoConfig.StartPage.PrioritizeRecording");
	QRadioButton *recordBtn = new QRadioButton(recordLabel, this);
	layout->addWidget(recordBtn);
	connect(recordBtn, &QPushButton::clicked, [this] {
		selectedMode_ = PriorityMode::Recording;
		emit priorityModeChanged(PriorityMode::Recording);
	});

	OBSBasic *obsBasic = OBSBasic::Get();
	if (obsBasic->VCamEnabled()) {
		QString vCamLabel = QTStr(
			"Basic.AutoConfig.StartPage.PrioritizeVirtualCam");
		QRadioButton *vCamButton = new QRadioButton(vCamLabel, this);
		layout->addWidget(vCamButton);
		connect(vCamButton, &QPushButton::clicked, [this] {
			selectedMode_ = PriorityMode::VirtualCam;
			emit priorityModeChanged(PriorityMode::VirtualCam);
		});
	}

	// Add Spacer
	QSpacerItem *sp40 = new QSpacerItem(40, 40, QSizePolicy::Expanding,
					    QSizePolicy::Expanding);
	layout->addSpacerItem(sp40);

	// "...wizard will determine best settings based on..."
	QString info = QTStr("Basic.AutoConfig.Info");
	QLabel *infoLabel = new QLabel(info, this);
	infoLabel->setStyleSheet("font-weight: bold;");
	infoLabel->setWordWrap(true);
	layout->addWidget(infoLabel);

	// Add 5 spacer
	QSpacerItem *sp5 =
		new QSpacerItem(5, 5, QSizePolicy::Fixed, QSizePolicy::Fixed);
	layout->addSpacerItem(sp5);

	//"This can be run at any time..."
	QString runInfo = QTStr("Basic.AutoConfig.RunAnytime");
	QLabel *runLabel = new QLabel(runInfo, this);
	runLabel->setWordWrap(true);
	layout->addWidget(runLabel);

	//No spacer for the bottom/
}

AutoConfigStartPage::~AutoConfigStartPage()
{
	//nop
}

int AutoConfigStartPage::nextId() const
{
	if (selectedMode_ == PriorityMode::VirtualCam) {
		return AutoConfig::WizardPage::TestPage;
	}
	return AutoConfig::WizardPage::VideoPage;
}
