#include <QMessageBox>
#include <QScreen>

#include <obs.hpp>

#include "window-basic-auto-config.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "url-push-button.hpp"

#include "ui_AutoConfigStartPage.h"
#include "ui_AutoConfigVideoPage.h"
#include "ui_AutoConfigStreamPage.h"

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#include "auth-oauth.hpp"
#endif

struct QCef;
struct QCefCookieManager;

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

#define wiz reinterpret_cast<AutoConfig *>(wizard())

/* ------------------------------------------------------------------------- */

#define SERVICE_PATH "service.json"

static OBSData OpenServiceSettings(std::string &type)
{
	char serviceJsonPath[512];
	int ret = GetProfilePath(serviceJsonPath, sizeof(serviceJsonPath),
				 SERVICE_PATH);
	if (ret <= 0)
		return OBSData();

	OBSData data =
		obs_data_create_from_json_file_safe(serviceJsonPath, "bak");
	obs_data_release(data);

	obs_data_set_default_string(data, "type", "rtmp_common");
	type = obs_data_get_string(data, "type");

	OBSData settings = obs_data_get_obj(data, "settings");
	obs_data_release(settings);

	return settings;
}

static void GetServiceInfo(std::string &type, std::string &service,
			   std::string &server, std::string &key)
{
	OBSData settings = OpenServiceSettings(type);

	service = obs_data_get_string(settings, "service");
	server = obs_data_get_string(settings, "server");
	key = obs_data_get_string(settings, "key");
}

/* ------------------------------------------------------------------------- */

AutoConfigStartPage::AutoConfigStartPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigStartPage)
{
	ui->setupUi(this);
	setTitle(QTStr("Basic.AutoConfig.StartPage"));
	setSubTitle(QTStr("Basic.AutoConfig.StartPage.SubTitle"));

	OBSBasic *main = OBSBasic::Get();
	if (main->VCamEnabled()) {
		QRadioButton *prioritizeVCam = new QRadioButton(
			QTStr("Basic.AutoConfig.StartPage.PrioritizeVirtualCam"),
			this);
		QBoxLayout *box = reinterpret_cast<QBoxLayout *>(layout());
		box->insertWidget(2, prioritizeVCam);

		connect(prioritizeVCam, &QPushButton::clicked, this,
			&AutoConfigStartPage::PrioritizeVCam);
	}
}

AutoConfigStartPage::~AutoConfigStartPage()
{
	delete ui;
}

int AutoConfigStartPage::nextId() const
{
	return wiz->type == AutoConfig::Type::VirtualCam
		       ? AutoConfig::TestPage
		       : AutoConfig::VideoPage;
}

void AutoConfigStartPage::on_prioritizeStreaming_clicked()
{
	wiz->type = AutoConfig::Type::Streaming;
}

void AutoConfigStartPage::on_prioritizeRecording_clicked()
{
	wiz->type = AutoConfig::Type::Recording;
}

void AutoConfigStartPage::PrioritizeVCam()
{
	wiz->type = AutoConfig::Type::VirtualCam;
}

/* ------------------------------------------------------------------------- */

#define RES_TEXT(x) "Basic.AutoConfig.VideoPage." x
#define RES_USE_CURRENT RES_TEXT("BaseResolution.UseCurrent")
#define RES_USE_DISPLAY RES_TEXT("BaseResolution.Display")
#define FPS_USE_CURRENT RES_TEXT("FPS.UseCurrent")
#define FPS_PREFER_HIGH_FPS RES_TEXT("FPS.PreferHighFPS")
#define FPS_PREFER_HIGH_RES RES_TEXT("FPS.PreferHighRes")

AutoConfigVideoPage::AutoConfigVideoPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigVideoPage)
{
	ui->setupUi(this);

	setTitle(QTStr("Basic.AutoConfig.VideoPage"));
	setSubTitle(QTStr("Basic.AutoConfig.VideoPage.SubTitle"));

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	long double fpsVal =
		(long double)ovi.fps_num / (long double)ovi.fps_den;

	QString fpsStr = (ovi.fps_den > 1) ? QString::number(fpsVal, 'f', 2)
					   : QString::number(fpsVal, 'g', 2);

	ui->fps->addItem(QTStr(FPS_PREFER_HIGH_FPS),
			 (int)AutoConfig::FPSType::PreferHighFPS);
	ui->fps->addItem(QTStr(FPS_PREFER_HIGH_RES),
			 (int)AutoConfig::FPSType::PreferHighRes);
	ui->fps->addItem(QTStr(FPS_USE_CURRENT).arg(fpsStr),
			 (int)AutoConfig::FPSType::UseCurrent);
	ui->fps->addItem(QStringLiteral("30"), (int)AutoConfig::FPSType::fps30);
	ui->fps->addItem(QStringLiteral("60"), (int)AutoConfig::FPSType::fps60);
	ui->fps->setCurrentIndex(0);

	QString cxStr = QString::number(ovi.base_width);
	QString cyStr = QString::number(ovi.base_height);

	int encRes = int(ovi.base_width << 16) | int(ovi.base_height);
	ui->canvasRes->addItem(QTStr(RES_USE_CURRENT).arg(cxStr, cyStr),
			       (int)encRes);

	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QSize as = screen->size();

		encRes = int(as.width() << 16) | int(as.height());

		QString str = QTStr(RES_USE_DISPLAY)
				      .arg(QString::number(i + 1),
					   QString::number(as.width()),
					   QString::number(as.height()));

		ui->canvasRes->addItem(str, encRes);
	}

	auto addRes = [&](int cx, int cy) {
		encRes = (cx << 16) | cy;
		QString str = QString("%1x%2").arg(QString::number(cx),
						   QString::number(cy));
		ui->canvasRes->addItem(str, encRes);
	};

	addRes(1920, 1080);
	addRes(1280, 720);

	ui->canvasRes->setCurrentIndex(0);
}

AutoConfigVideoPage::~AutoConfigVideoPage()
{
	delete ui;
}

int AutoConfigVideoPage::nextId() const
{
	return wiz->type == AutoConfig::Type::Recording
		       ? AutoConfig::TestPage
		       : AutoConfig::StreamPage;
}

bool AutoConfigVideoPage::validatePage()
{
	int encRes = ui->canvasRes->currentData().toInt();
	wiz->baseResolutionCX = encRes >> 16;
	wiz->baseResolutionCY = encRes & 0xFFFF;
	wiz->fpsType = (AutoConfig::FPSType)ui->fps->currentData().toInt();

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	switch (wiz->fpsType) {
	case AutoConfig::FPSType::PreferHighFPS:
		wiz->specificFPSNum = 0;
		wiz->specificFPSDen = 0;
		wiz->preferHighFPS = true;
		break;
	case AutoConfig::FPSType::PreferHighRes:
		wiz->specificFPSNum = 0;
		wiz->specificFPSDen = 0;
		wiz->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::UseCurrent:
		wiz->specificFPSNum = ovi.fps_num;
		wiz->specificFPSDen = ovi.fps_den;
		wiz->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::fps30:
		wiz->specificFPSNum = 30;
		wiz->specificFPSDen = 1;
		wiz->preferHighFPS = false;
		break;
	case AutoConfig::FPSType::fps60:
		wiz->specificFPSNum = 60;
		wiz->specificFPSDen = 1;
		wiz->preferHighFPS = false;
		break;
	}

	return true;
}

/* ------------------------------------------------------------------------- */

enum class ListOpt : int {
	ShowAll = 1,
	Custom,
};

AutoConfigStreamPage::AutoConfigStreamPage(QWidget *parent)
	: QWizardPage(parent), ui(new Ui_AutoConfigStreamPage)
{
	ui->setupUi(this);
	ui->bitrateLabel->setVisible(false);
	ui->bitrate->setVisible(false);
	ui->connectAccount2->setVisible(false);
	ui->disconnectAccount->setVisible(false);

	int vertSpacing = ui->topLayout->verticalSpacing();

	QMargins m = ui->topLayout->contentsMargins();
	m.setBottom(vertSpacing / 2);
	ui->topLayout->setContentsMargins(m);

	m = ui->loginPageLayout->contentsMargins();
	m.setTop(vertSpacing / 2);
	ui->loginPageLayout->setContentsMargins(m);

	m = ui->streamkeyPageLayout->contentsMargins();
	m.setTop(vertSpacing / 2);
	ui->streamkeyPageLayout->setContentsMargins(m);

	setTitle(QTStr("Basic.AutoConfig.StreamPage"));
	setSubTitle(QTStr("Basic.AutoConfig.StreamPage.SubTitle"));

	LoadServices(false);

	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(ServiceChanged()));
	connect(ui->customServer, SIGNAL(textChanged(const QString &)), this,
		SLOT(ServiceChanged()));
	connect(ui->customServer, SIGNAL(textChanged(const QString &)), this,
		SLOT(UpdateKeyLink()));
	connect(ui->customServer, SIGNAL(editingFinished()), this,
		SLOT(UpdateKeyLink()));
	connect(ui->doBandwidthTest, SIGNAL(toggled(bool)), this,
		SLOT(ServiceChanged()));

	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateServerList()));

	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateKeyLink()));
	connect(ui->service, SIGNAL(currentIndexChanged(int)), this,
		SLOT(UpdateMoreInfoLink()));

	connect(ui->key, SIGNAL(textChanged(const QString &)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionUS, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionEU, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionAsia, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
	connect(ui->regionOther, SIGNAL(toggled(bool)), this,
		SLOT(UpdateCompleted()));
}

AutoConfigStreamPage::~AutoConfigStreamPage()
{
	delete ui;
}

bool AutoConfigStreamPage::isComplete() const
{
	return ready;
}

int AutoConfigStreamPage::nextId() const
{
	return AutoConfig::TestPage;
}

inline bool AutoConfigStreamPage::IsCustomService() const
{
	return ui->service->currentData().toInt() == (int)ListOpt::Custom;
}

bool AutoConfigStreamPage::validatePage()
{
	OBSData service_settings = obs_data_create();
	obs_data_release(service_settings);

	wiz->customServer = IsCustomService();

	const char *serverType = wiz->customServer ? "rtmp_custom"
						   : "rtmp_common";

	if (!wiz->customServer) {
		obs_data_set_string(service_settings, "service",
				    QT_TO_UTF8(ui->service->currentText()));
	}

	OBSService service = obs_service_create(serverType, "temp_service",
						service_settings, nullptr);
	obs_service_release(service);

	int bitrate = 10000;
	if (!ui->doBandwidthTest->isChecked()) {
		bitrate = ui->bitrate->value();
		wiz->idealBitrate = bitrate;
	}

	OBSData settings = obs_data_create();
	obs_data_release(settings);
	obs_data_set_int(settings, "bitrate", bitrate);
	obs_service_apply_encoder_settings(service, settings, nullptr);

	if (wiz->customServer) {
		QString server = ui->customServer->text();
		wiz->server = wiz->serverName = QT_TO_UTF8(server);
	} else {
		wiz->serverName = QT_TO_UTF8(ui->server->currentText());
		wiz->server = QT_TO_UTF8(ui->server->currentData().toString());
	}

	wiz->bandwidthTest = ui->doBandwidthTest->isChecked();
	wiz->startingBitrate = (int)obs_data_get_int(settings, "bitrate");
	wiz->idealBitrate = wiz->startingBitrate;
	wiz->regionUS = ui->regionUS->isChecked();
	wiz->regionEU = ui->regionEU->isChecked();
	wiz->regionAsia = ui->regionAsia->isChecked();
	wiz->regionOther = ui->regionOther->isChecked();
	wiz->serviceName = QT_TO_UTF8(ui->service->currentText());
	if (ui->preferHardware)
		wiz->preferHardware = ui->preferHardware->isChecked();
	wiz->key = QT_TO_UTF8(ui->key->text());

	if (!wiz->customServer) {
		if (wiz->serviceName == "Twitch")
			wiz->service = AutoConfig::Service::Twitch;
		else
			wiz->service = AutoConfig::Service::Other;
	} else {
		wiz->service = AutoConfig::Service::Other;
	}

	if (wiz->service != AutoConfig::Service::Twitch && wiz->bandwidthTest) {
		QMessageBox::StandardButton button;
#define WARNING_TEXT(x) QTStr("Basic.AutoConfig.StreamPage.StreamWarning." x)
		button = OBSMessageBox::question(this, WARNING_TEXT("Title"),
						 WARNING_TEXT("Text"));
#undef WARNING_TEXT

		if (button == QMessageBox::No)
			return false;
	}

	return true;
}

void AutoConfigStreamPage::on_show_clicked()
{
	if (ui->key->echoMode() == QLineEdit::Password) {
		ui->key->setEchoMode(QLineEdit::Normal);
		ui->show->setText(QTStr("Hide"));
	} else {
		ui->key->setEchoMode(QLineEdit::Password);
		ui->show->setText(QTStr("Show"));
	}
}

void AutoConfigStreamPage::OnOAuthStreamKeyConnected()
{
#ifdef BROWSER_AVAILABLE
	OAuthStreamKey *a = reinterpret_cast<OAuthStreamKey *>(auth.get());

	if (a) {
		bool validKey = !a->key().empty();

		if (validKey)
			ui->key->setText(QT_UTF8(a->key().c_str()));

		ui->streamKeyWidget->setVisible(!validKey);
		ui->streamKeyLabel->setVisible(!validKey);
		ui->connectAccount2->setVisible(!validKey);
		ui->disconnectAccount->setVisible(validKey);
	}

	ui->stackedWidget->setCurrentIndex((int)Section::StreamKey);
	UpdateCompleted();
#endif
}

void AutoConfigStreamPage::OnAuthConnected()
{
	std::string service = QT_TO_UTF8(ui->service->currentText());
	Auth::Type type = Auth::AuthType(service);

	if (type == Auth::Type::OAuth_StreamKey) {
		OnOAuthStreamKeyConnected();
	}
}

void AutoConfigStreamPage::on_connectAccount_clicked()
{
#ifdef BROWSER_AVAILABLE
	std::string service = QT_TO_UTF8(ui->service->currentText());

	OAuth::DeleteCookies(service);

	auth = OAuthStreamKey::Login(this, service);
	if (!!auth)
		OnAuthConnected();
#endif
}

#define DISCONNECT_COMFIRM_TITLE \
	"Basic.AutoConfig.StreamPage.DisconnectAccount.Confirm.Title"
#define DISCONNECT_COMFIRM_TEXT \
	"Basic.AutoConfig.StreamPage.DisconnectAccount.Confirm.Text"

void AutoConfigStreamPage::on_disconnectAccount_clicked()
{
	QMessageBox::StandardButton button;

	button = OBSMessageBox::question(this, QTStr(DISCONNECT_COMFIRM_TITLE),
					 QTStr(DISCONNECT_COMFIRM_TEXT));

	if (button == QMessageBox::No) {
		return;
	}

	OBSBasic *main = OBSBasic::Get();

	main->auth.reset();
	auth.reset();

	std::string service = QT_TO_UTF8(ui->service->currentText());

#ifdef BROWSER_AVAILABLE
	OAuth::DeleteCookies(service);
#endif

	ui->streamKeyWidget->setVisible(true);
	ui->streamKeyLabel->setVisible(true);
	ui->connectAccount2->setVisible(true);
	ui->disconnectAccount->setVisible(false);
	ui->key->setText("");
}

void AutoConfigStreamPage::on_useStreamKey_clicked()
{
	ui->stackedWidget->setCurrentIndex((int)Section::StreamKey);
	UpdateCompleted();
}

static inline bool is_auth_service(const std::string &service)
{
	return Auth::AuthType(service) != Auth::Type::None;
}

void AutoConfigStreamPage::ServiceChanged()
{
	bool showMore = ui->service->currentData().toInt() ==
			(int)ListOpt::ShowAll;
	if (showMore)
		return;

	std::string service = QT_TO_UTF8(ui->service->currentText());
	bool regionBased = service == "Twitch";
	bool testBandwidth = ui->doBandwidthTest->isChecked();
	bool custom = IsCustomService();

	ui->disconnectAccount->setVisible(false);

#ifdef BROWSER_AVAILABLE
	if (cef) {
		if (lastService != service.c_str()) {
			bool can_auth = is_auth_service(service);
			int page = can_auth ? (int)Section::Connect
					    : (int)Section::StreamKey;

			ui->stackedWidget->setCurrentIndex(page);
			ui->streamKeyWidget->setVisible(true);
			ui->streamKeyLabel->setVisible(true);
			ui->connectAccount2->setVisible(can_auth);
			auth.reset();

			if (lastService.isEmpty())
				lastService = service.c_str();
		}
	} else {
		ui->connectAccount2->setVisible(false);
	}
#else
	ui->connectAccount2->setVisible(false);
#endif

	/* Test three closest servers if "Auto" is available for Twitch */
	if (service == "Twitch" && wiz->twitchAuto)
		regionBased = false;

	ui->streamkeyPageLayout->removeWidget(ui->serverLabel);
	ui->streamkeyPageLayout->removeWidget(ui->serverStackedWidget);

	if (custom) {
		ui->streamkeyPageLayout->insertRow(1, ui->serverLabel,
						   ui->serverStackedWidget);

		ui->region->setVisible(false);
		ui->serverStackedWidget->setCurrentIndex(1);
		ui->serverStackedWidget->setVisible(true);
		ui->serverLabel->setVisible(true);
	} else {
		if (!testBandwidth)
			ui->streamkeyPageLayout->insertRow(
				2, ui->serverLabel, ui->serverStackedWidget);

		ui->region->setVisible(regionBased && testBandwidth);
		ui->serverStackedWidget->setCurrentIndex(0);
		ui->serverStackedWidget->setHidden(testBandwidth);
		ui->serverLabel->setHidden(testBandwidth);
	}

	wiz->testRegions = regionBased && testBandwidth;

	ui->bitrateLabel->setHidden(testBandwidth);
	ui->bitrate->setHidden(testBandwidth);

#ifdef BROWSER_AVAILABLE
	OBSBasic *main = OBSBasic::Get();

	if (!!main->auth &&
	    service.find(main->auth->service()) != std::string::npos) {
		auth = main->auth;
		OnAuthConnected();
	}
#endif

	UpdateCompleted();
}

void AutoConfigStreamPage::UpdateMoreInfoLink()
{
	if (IsCustomService()) {
		ui->moreInfoButton->hide();
		return;
	}

	QString serviceName = ui->service->currentText();
	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_property_t *services = obs_properties_get(props, "service");

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	obs_data_set_string(settings, "service", QT_TO_UTF8(serviceName));
	obs_property_modified(services, settings);

	const char *more_info_link =
		obs_data_get_string(settings, "more_info_link");

	if (!more_info_link || (*more_info_link == '\0')) {
		ui->moreInfoButton->hide();
	} else {
		ui->moreInfoButton->setTargetUrl(QUrl(more_info_link));
		ui->moreInfoButton->show();
	}
	obs_properties_destroy(props);
}

void AutoConfigStreamPage::UpdateKeyLink()
{
	QString serviceName = ui->service->currentText();
	QString customServer = ui->customServer->text();
	bool isYoutube = false;
	QString streamKeyLink;

	if (serviceName == "Twitch") {
		streamKeyLink = "https://dashboard.twitch.tv/settings/stream";
	} else if (serviceName.startsWith("YouTube")) {
		streamKeyLink = "https://www.youtube.com/live_dashboard";
		isYoutube = true;
	} else if (serviceName.startsWith("Restream.io")) {
		streamKeyLink =
			"https://restream.io/settings/streaming-setup?from=OBS";
	} else if (serviceName == "Facebook Live" ||
		   (customServer.contains("fbcdn.net") && IsCustomService())) {
		streamKeyLink =
			"https://www.facebook.com/live/producer?ref=OBS";
	} else if (serviceName.startsWith("Twitter")) {
		streamKeyLink = "https://www.pscp.tv/account/producer";
	} else if (serviceName.startsWith("YouStreamer")) {
		streamKeyLink = "https://www.app.youstreamer.com/stream/";
	} else if (serviceName == "Trovo") {
		streamKeyLink = "https://studio.trovo.live/mychannel/stream";
	} else if (serviceName == "Glimesh") {
		streamKeyLink = "https://glimesh.tv/users/settings/stream";
	} else if (serviceName.startsWith("OPENREC.tv")) {
		streamKeyLink =
			"https://www.openrec.tv/login?keep_login=true&url=https://www.openrec.tv/dashboard/live?from=obs";
	}

	if (QString(streamKeyLink).isNull()) {
		ui->streamKeyButton->hide();
	} else {
		ui->streamKeyButton->setTargetUrl(QUrl(streamKeyLink));
		ui->streamKeyButton->show();
	}

	if (isYoutube) {
		ui->doBandwidthTest->setChecked(false);
		ui->doBandwidthTest->setEnabled(false);
	} else {
		ui->doBandwidthTest->setEnabled(true);
	}
}

void AutoConfigStreamPage::LoadServices(bool showAll)
{
	obs_properties_t *props = obs_get_service_properties("rtmp_common");

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	obs_data_set_bool(settings, "show_all", showAll);

	obs_property_t *prop = obs_properties_get(props, "show_all");
	obs_property_modified(prop, settings);

	ui->service->blockSignals(true);
	ui->service->clear();

	QStringList names;

	obs_property_t *services = obs_properties_get(props, "service");
	size_t services_count = obs_property_list_item_count(services);
	for (size_t i = 0; i < services_count; i++) {
		const char *name = obs_property_list_item_string(services, i);
		names.push_back(name);
	}

	if (showAll)
		names.sort(Qt::CaseInsensitive);

	for (QString &name : names)
		ui->service->addItem(name);

	if (!showAll) {
		ui->service->addItem(
			QTStr("Basic.AutoConfig.StreamPage.Service.ShowAll"),
			QVariant((int)ListOpt::ShowAll));
	}

	ui->service->insertItem(
		0, QTStr("Basic.AutoConfig.StreamPage.Service.Custom"),
		QVariant((int)ListOpt::Custom));

	if (!lastService.isEmpty()) {
		int idx = ui->service->findText(lastService);
		if (idx != -1)
			ui->service->setCurrentIndex(idx);
	}

	obs_properties_destroy(props);

	ui->service->blockSignals(false);
}

void AutoConfigStreamPage::UpdateServerList()
{
	QString serviceName = ui->service->currentText();
	bool showMore = ui->service->currentData().toInt() ==
			(int)ListOpt::ShowAll;

	if (showMore) {
		LoadServices(true);
		ui->service->showPopup();
		return;
	} else {
		lastService = serviceName;
	}

	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_property_t *services = obs_properties_get(props, "service");

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	obs_data_set_string(settings, "service", QT_TO_UTF8(serviceName));
	obs_property_modified(services, settings);

	obs_property_t *servers = obs_properties_get(props, "server");

	ui->server->clear();

	size_t servers_count = obs_property_list_item_count(servers);
	for (size_t i = 0; i < servers_count; i++) {
		const char *name = obs_property_list_item_name(servers, i);
		const char *server = obs_property_list_item_string(servers, i);
		ui->server->addItem(name, server);
	}

	obs_properties_destroy(props);
}

void AutoConfigStreamPage::UpdateCompleted()
{
	if (ui->stackedWidget->currentIndex() == (int)Section::Connect ||
	    (ui->key->text().isEmpty() && !auth)) {
		ready = false;
	} else {
		bool custom = IsCustomService();
		if (custom) {
			ready = !ui->customServer->text().isEmpty();
		} else {
			ready = !wiz->testRegions ||
				ui->regionUS->isChecked() ||
				ui->regionEU->isChecked() ||
				ui->regionAsia->isChecked() ||
				ui->regionOther->isChecked();
		}
	}
	emit completeChanged();
}

/* ------------------------------------------------------------------------- */

AutoConfig::AutoConfig(QWidget *parent) : QWizard(parent)
{
	EnableThreadedMessageBoxes(true);

	calldata_t cd = {0};
	calldata_set_int(&cd, "seconds", 5);

	proc_handler_t *ph = obs_get_proc_handler();
	proc_handler_call(ph, "twitch_ingests_refresh", &cd);
	calldata_free(&cd);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(parent);
	main->EnableOutputs(false);

	installEventFilter(CreateShortcutFilter());

	std::string serviceType;
	GetServiceInfo(serviceType, serviceName, server, key);
#if defined(_WIN32) || defined(__APPLE__)
	setWizardStyle(QWizard::ModernStyle);
#endif
	streamPage = new AutoConfigStreamPage();

	setPage(StartPage, new AutoConfigStartPage());
	setPage(VideoPage, new AutoConfigVideoPage());
	setPage(StreamPage, streamPage);
	setPage(TestPage, new AutoConfigTestPage());
	setWindowTitle(QTStr("Basic.AutoConfig"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	baseResolutionCX = ovi.base_width;
	baseResolutionCY = ovi.base_height;

	/* ----------------------------------------- */
	/* check to see if Twitch's "auto" available */

	OBSData twitchSettings = obs_data_create();
	obs_data_release(twitchSettings);

	obs_data_set_string(twitchSettings, "service", "Twitch");

	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_properties_apply_settings(props, twitchSettings);

	obs_property_t *p = obs_properties_get(props, "server");
	const char *first = obs_property_list_item_string(p, 0);
	twitchAuto = strcmp(first, "auto") == 0;

	obs_properties_destroy(props);

	/* ----------------------------------------- */
	/* load service/servers                      */

	customServer = serviceType == "rtmp_custom";

	QComboBox *serviceList = streamPage->ui->service;

	if (!serviceName.empty()) {
		serviceList->blockSignals(true);

		int count = serviceList->count();
		bool found = false;

		for (int i = 0; i < count; i++) {
			QString name = serviceList->itemText(i);

			if (name == serviceName.c_str()) {
				serviceList->setCurrentIndex(i);
				found = true;
				break;
			}
		}

		if (!found) {
			serviceList->insertItem(0, serviceName.c_str());
			serviceList->setCurrentIndex(0);
		}

		serviceList->blockSignals(false);
	}

	streamPage->UpdateServerList();
	streamPage->UpdateKeyLink();
	streamPage->UpdateMoreInfoLink();
	streamPage->lastService.clear();

	if (!customServer) {
		QComboBox *serverList = streamPage->ui->server;
		int idx = serverList->findData(QString(server.c_str()));
		if (idx == -1)
			idx = 0;

		serverList->setCurrentIndex(idx);
	} else {
		streamPage->ui->customServer->setText(server.c_str());
		int idx = streamPage->ui->service->findData(
			QVariant((int)ListOpt::Custom));
		streamPage->ui->service->setCurrentIndex(idx);
	}

	if (!key.empty())
		streamPage->ui->key->setText(key.c_str());

	int bitrate =
		config_get_int(main->Config(), "SimpleOutput", "VBitrate");
	streamPage->ui->bitrate->setValue(bitrate);
	streamPage->ServiceChanged();

	TestHardwareEncoding();
	if (!hardwareEncodingAvailable) {
		delete streamPage->ui->preferHardware;
		streamPage->ui->preferHardware = nullptr;
	} else {
		/* Newer generations of NVENC have a high enough quality to
		 * bitrate ratio that if NVENC is available, it makes sense to
		 * just always prefer hardware encoding by default */
		bool preferHardware = nvencAvailable ||
				      os_get_physical_cores() <= 4;
		streamPage->ui->preferHardware->setChecked(preferHardware);
	}

	setOptions(QWizard::WizardOptions());
	setButtonText(QWizard::FinishButton,
		      QTStr("Basic.AutoConfig.ApplySettings"));
	setButtonText(QWizard::BackButton, QTStr("Back"));
	setButtonText(QWizard::NextButton, QTStr("Next"));
	setButtonText(QWizard::CancelButton, QTStr("Cancel"));
}

AutoConfig::~AutoConfig()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	main->EnableOutputs(true);
	EnableThreadedMessageBoxes(false);
}

void AutoConfig::TestHardwareEncoding()
{
	size_t idx = 0;
	const char *id;
	while (obs_enum_encoder_types(idx++, &id)) {
		if (strcmp(id, "ffmpeg_nvenc") == 0)
			hardwareEncodingAvailable = nvencAvailable = true;
		else if (strcmp(id, "obs_qsv11") == 0)
			hardwareEncodingAvailable = qsvAvailable = true;
		else if (strcmp(id, "amd_amf_h264") == 0)
			hardwareEncodingAvailable = vceAvailable = true;
	}
}

bool AutoConfig::CanTestServer(const char *server)
{
	if (!testRegions || (regionUS && regionEU && regionAsia && regionOther))
		return true;

	if (service == Service::Twitch) {
		if (astrcmp_n(server, "US West:", 8) == 0 ||
		    astrcmp_n(server, "US East:", 8) == 0 ||
		    astrcmp_n(server, "US Central:", 11) == 0) {
			return regionUS;
		} else if (astrcmp_n(server, "EU:", 3) == 0) {
			return regionEU;
		} else if (astrcmp_n(server, "Asia:", 5) == 0) {
			return regionAsia;
		} else if (regionOther) {
			return true;
		}
	} else {
		return true;
	}

	return false;
}

void AutoConfig::done(int result)
{
	QWizard::done(result);

	if (result == QDialog::Accepted) {
		if (type == Type::Streaming)
			SaveStreamSettings();
		SaveSettings();
	}
}

inline const char *AutoConfig::GetEncoderId(Encoder enc)
{
	switch (enc) {
	case Encoder::NVENC:
		return SIMPLE_ENCODER_NVENC;
	case Encoder::QSV:
		return SIMPLE_ENCODER_QSV;
	case Encoder::AMD:
		return SIMPLE_ENCODER_AMD;
	default:
		return SIMPLE_ENCODER_X264;
	}
};

void AutoConfig::SaveStreamSettings()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	/* ---------------------------------- */
	/* save service                       */

	const char *service_id = customServer ? "rtmp_custom" : "rtmp_common";

	obs_service_t *oldService = main->GetService();
	OBSData hotkeyData = obs_hotkeys_save_service(oldService);
	obs_data_release(hotkeyData);

	OBSData settings = obs_data_create();
	obs_data_release(settings);

	if (!customServer)
		obs_data_set_string(settings, "service", serviceName.c_str());
	obs_data_set_string(settings, "server", server.c_str());
	obs_data_set_string(settings, "key", key.c_str());

	OBSService newService = obs_service_create(
		service_id, "default_service", settings, hotkeyData);
	obs_service_release(newService);

	if (!newService)
		return;

	main->SetService(newService);
	main->SaveService();
	main->auth = streamPage->auth;
	if (!!main->auth)
		main->auth->LoadUI();

	/* ---------------------------------- */
	/* save stream settings               */

	config_set_int(main->Config(), "SimpleOutput", "VBitrate",
		       idealBitrate);
	config_set_string(main->Config(), "SimpleOutput", "StreamEncoder",
			  GetEncoderId(streamingEncoder));
	config_remove_value(main->Config(), "SimpleOutput", "UseAdvanced");
}

void AutoConfig::SaveSettings()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	if (recordingEncoder != Encoder::Stream)
		config_set_string(main->Config(), "SimpleOutput", "RecEncoder",
				  GetEncoderId(recordingEncoder));

	const char *quality = recordingQuality == Quality::High ? "Small"
								: "Stream";

	config_set_string(main->Config(), "Output", "Mode", "Simple");
	config_set_string(main->Config(), "SimpleOutput", "RecQuality",
			  quality);
	config_set_int(main->Config(), "Video", "BaseCX", baseResolutionCX);
	config_set_int(main->Config(), "Video", "BaseCY", baseResolutionCY);
	config_set_int(main->Config(), "Video", "OutputCX", idealResolutionCX);
	config_set_int(main->Config(), "Video", "OutputCY", idealResolutionCY);

	if (fpsType != FPSType::UseCurrent) {
		config_set_uint(main->Config(), "Video", "FPSType", 0);
		config_set_string(main->Config(), "Video", "FPSCommon",
				  std::to_string(idealFPSNum).c_str());
	}

	main->ResetVideo();
	main->ResetOutputs();
	config_save_safe(main->Config(), "tmp", nullptr);
}
