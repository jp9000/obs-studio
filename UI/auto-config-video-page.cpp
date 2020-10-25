#include "auto-config-video-page.hpp"

#include <QSize>
#include <QScreen>
#include <QLabel>
#include <QString>

#include "obs-app.hpp"

AutoConfigVideoPage::AutoConfigVideoPage(
	QWidget *parent,
	QSharedPointer<AutoConfig::AutoConfigModel> wizardModel)
	: QWizardPage(parent)
{
	this->wizardModel = wizardModel;

	//Setup UI
	setTitle(QTStr("Basic.AutoConfig.VideoPage"));
	setSubTitle(QTStr("Basic.AutoConfig.VideoPage.SubTitle"));

	// Main vertical layout for widget
	mainVLayout = new QVBoxLayout(this);
	this->setLayout(mainVLayout);
	this->setMinimumWidth(AutoConfig::AutoConfigModel::wizardMinWidth());
	this->setMinimumHeight(300);

	// Create form section
	setupFormLayout();

	//Label is under, outside form
	QString canvasNote =
		QTStr("Basic.AutoConfig.VideoPage.CanvasExplanation");
	QLabel *canvasNoteLabel = new QLabel(canvasNote, this);
	canvasNoteLabel->setWordWrap(true);
	canvasNoteLabel->setObjectName("warningLabel");
	mainVLayout->addWidget(canvasNoteLabel);

	// Fill form out
	populateFpsOptions();
	populateResolutionOptions();
}

void AutoConfigVideoPage::populateResolutionOptions()
{
	obs_video_info videoInfo;
	obs_get_video_info(&videoInfo);

	QString cxStr = QString::number(videoInfo.base_width);
	QString cyStr = QString::number(videoInfo.base_height);

	int encoderRes = int(videoInfo.base_width << 16) |
			 int(videoInfo.base_height);
	QString currentStr =
		QTStr("Basic.AutoConfig.VideoPage.BaseResolution.UseCurrent");
	currentStr = currentStr.arg(cxStr, cyStr); // Use Current (%1x%2)
	resComboBox->addItem(currentStr, (int)encoderRes);

	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		const int screenWidth = screens[i]->size().width();
		QString screenWidthString = QString::number(screenWidth);
		const int screenHeight = screens[i]->size().height();
		QString screenHeightString = QString::number(screenHeight);

		encoderRes = (int(screenWidth << 16) | screenHeight);

		QString displayRes;
		displayRes = QTStr(
			"Basic.AutoConfig.VideoPage.BaseResolution.Display");
		displayRes = displayRes.arg(QString::number(i + 1),
					    screenWidthString,
					    screenHeightString);
		resComboBox->addItem(displayRes, encoderRes);
	}

	auto addRes = [&](int cx, int cy) {
		encoderRes = (cx << 16) | cy;
		QString str = QString("%1x%2").arg(QString::number(cx),
						   QString::number(cy));

		resComboBox->addItem(str, encoderRes);
	};

	addRes(1920, 1080);
	addRes(1280, 720);

	resComboBox->setCurrentIndex(0);
}

void AutoConfigVideoPage::setupFormLayout()
{
	// Create Form
	formLayout = new QFormLayout(this);
	formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
	formLayout->setLabelAlignment(Qt::AlignRight);
	formLayout->setFormAlignment(Qt::AlignRight | Qt::AlignVCenter);
	formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
	mainVLayout->addLayout(formLayout);

	// Fill Form
	resComboBox = new QComboBox(this);
	QString resLabel = QTStr("Basic.Settings.Video.BaseResolution");
	formLayout->addRow(resLabel, resComboBox);

	fpsComboBox = new QComboBox(this);
	QString fpsLabel = QTStr("Basic.Settings.Video.FPS");
	formLayout->addRow(fpsLabel, fpsComboBox);
}

void AutoConfigVideoPage::populateFpsOptions()
{
	obs_video_info videoInfo;
	obs_get_video_info(&videoInfo);

	long double fpsVal =
		(long double)videoInfo.fps_num / (long double)videoInfo.fps_den;

	QString fpsStr;
	if (videoInfo.fps_den > 1) {
		// Format FPS as float, two digits after decimal
		fpsStr = QString::number(fpsVal, 'f', 2);
	} else {
		// Format FPS as a whole number
		fpsStr = QString::number(fpsVal, 'g', 2);
	}

	QString highFps = QTStr("Basic.AutoConfig.VideoPage.FPS.PreferHighFPS");
	fpsComboBox->addItem(highFps, (int)AutoConfig::FPSType::PreferHighFPS);

	QString highRes = QTStr("Basic.AutoConfig.VideoPage.FPS.PreferHighRes");
	fpsComboBox->addItem(highRes, (int)AutoConfig::FPSType::PreferHighRes);

	QString current = QTStr("Basic.AutoConfig.VideoPage.FPS.UseCurrent");
	current = current.arg(fpsStr); // "Use Current (%1)"
	fpsComboBox->addItem(current, (int)AutoConfig::FPSType::UseCurrent);

	fpsComboBox->addItem(QStringLiteral("30"),
			     (int)AutoConfig::FPSType::fps30);
	fpsComboBox->addItem(QStringLiteral("60"),
			     (int)AutoConfig::FPSType::fps60);

	fpsComboBox->setCurrentIndex(0);
}

AutoConfigVideoPage::~AutoConfigVideoPage()
{
	//nop
}

int AutoConfigVideoPage::nextId() const
{
	return wizardModel->setupType == AutoConfig::SetupType::Recording
		       ? AutoConfig::WizardPage::TestPage
		       : AutoConfig::WizardPage::StreamPage;
}

bool AutoConfigVideoPage::validatePage()
{
	int encRes = resComboBox->currentData().toInt();
	wizardModel->baseResolutionCX = encRes >> 16;
	wizardModel->baseResolutionCY = encRes & 0xFFFF;
	wizardModel->fpsType =
		(AutoConfig::FPSType)fpsComboBox->currentData().toInt();

	obs_video_info videoInfo;
	obs_get_video_info(&videoInfo);

	switch (wizardModel->fpsType) {
	case AutoConfig::FPSType::PreferHighFPS:
		wizardModel->specificFPSNum = 0;
		wizardModel->specificFPSDen = 0;
		wizardModel->preferHighFPS = true;
		break;
	case AutoConfig::FPSType::PreferHighRes:
		wizardModel->specificFPSNum = 0;
		wizardModel->specificFPSDen = 0;
		wizardModel->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::UseCurrent:
		wizardModel->specificFPSNum = videoInfo.fps_num;
		wizardModel->specificFPSDen = videoInfo.fps_den;
		wizardModel->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::fps30:
		wizardModel->specificFPSNum = 30;
		wizardModel->specificFPSDen = 1;
		wizardModel->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::fps60:
		wizardModel->specificFPSNum = 60;
		wizardModel->specificFPSDen = 1;
		wizardModel->preferHighFPS = false;
		break;
	}

	return true;
}
