#pragma once

#include <QWizard>
#include <QPointer>
#include <QFormLayout>
#include <QWizardPage>
#include <QSharedPointer>

#include <condition_variable>
#include <utility>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

#include "auto-config-start-page.hpp"
#include "auto-config-video-page.hpp"
#include "auto-config-enums.hpp"
#include "auto-config-model.hpp"

class AutoConfigStreamPage;
class Ui_AutoConfigStreamPage;
class Ui_AutoConfigTestPage;

class Auth;

class AutoConfigWizard : public QWizard {
	Q_OBJECT

	friend class AutoConfigVideoPage;
	friend class AutoConfigStreamPage;
	friend class AutoConfigTestPage;

	enum class Service {
		Twitch,
		Smashcast,
		Other,
	};

	enum class Encoder {
		x264,
		NVENC,
		QSV,
		AMD,
		Stream,
	};

	enum class Quality {
		Stream,
		High,
	};

	static inline const char *GetEncoderId(Encoder enc);

	AutoConfigStreamPage *streamPage = nullptr;

	QSharedPointer<AutoConfig::AutoConfigModel> wizardModel;

	Service service = Service::Other;
	Quality recordingQuality = Quality::Stream;
	Encoder recordingEncoder = Encoder::Stream;
	Encoder streamingEncoder = Encoder::x264;
	int idealBitrate = 2500;
	int idealResolutionCX = 1280;
	int idealResolutionCY = 720;
	int idealFPSNum = 60;
	int idealFPSDen = 1;
	std::string serviceName;
	std::string serverName;
	std::string server;
	std::string key;

	bool hardwareEncodingAvailable = false;
	bool nvencAvailable = false;
	bool qsvAvailable = false;
	bool vceAvailable = false;

	int startingBitrate = 2500;
	bool customServer = false;
	bool bandwidthTest = false;
	bool testRegions = true;
	bool twitchAuto = false;
	bool regionUS = true;
	bool regionEU = true;
	bool regionAsia = true;
	bool regionOther = true;
	bool preferHardware = false;

	void TestHardwareEncoding();
	bool CanTestServer(const char *server);

	virtual void done(int result) override;

	void SaveStreamSettings();
	void SaveSettings();

public:
	AutoConfigWizard(
		QWidget *parent,
		QSharedPointer<AutoConfig::AutoConfigModel> wizardModel);
	~AutoConfigWizard();

public slots:
	void ChangedPriorityType(PriorityMode);
};

class AutoConfigStreamPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfigWizard;

	enum class Section : int {
		Connect,
		StreamKey,
	};

	std::shared_ptr<Auth> auth;

	Ui_AutoConfigStreamPage *ui;
	QString lastService;
	bool ready = false;

	void LoadServices(bool showAll);
	inline bool IsCustomService() const;

public:
	AutoConfigStreamPage(QWidget *parent = nullptr);
	~AutoConfigStreamPage();

	virtual bool isComplete() const override;
	virtual int nextId() const override;
	virtual bool validatePage() override;

	void OnAuthConnected();
	void OnOAuthStreamKeyConnected();

public slots:
	void on_show_clicked();
	void on_connectAccount_clicked();
	void on_disconnectAccount_clicked();
	void on_useStreamKey_clicked();
	void ServiceChanged();
	void UpdateKeyLink();
	void UpdateMoreInfoLink();
	void UpdateServerList();
	void UpdateCompleted();
};

class AutoConfigTestPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfigWizard;

	QPointer<QFormLayout> results;

	Ui_AutoConfigTestPage *ui;
	std::thread testThread;
	std::condition_variable cv;
	std::mutex m;
	bool cancel = false;
	bool started = false;

	enum class Stage {
		Starting,
		BandwidthTest,
		StreamEncoder,
		RecordingEncoder,
		Finished,
	};

	Stage stage = Stage::Starting;
	bool softwareTested = false;

	void StartBandwidthStage();
	void StartStreamEncoderStage();
	void StartRecordingEncoderStage();

	void FindIdealHardwareResolution();
	bool TestSoftwareEncoding();

	void TestBandwidthThread();
	void TestStreamEncoderThread();
	void TestRecordingEncoderThread();

	void FinalizeResults();

	struct ServerInfo {
		std::string name;
		std::string address;
		int bitrate = 0;
		int ms = -1;

		inline ServerInfo() {}

		inline ServerInfo(const char *name_, const char *address_)
			: name(name_), address(address_)
		{
		}
	};

	void GetServers(std::vector<ServerInfo> &servers);

public:
	AutoConfigTestPage(QWidget *parent = nullptr);
	~AutoConfigTestPage();

	virtual void initializePage() override;
	virtual void cleanupPage() override;
	virtual bool isComplete() const override;
	virtual int nextId() const override;

public slots:
	void NextStage();
	void UpdateMessage(QString message);
	void Failure(QString message);
	void Progress(int percentage);
};
