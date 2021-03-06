#pragma once

#include <QWizard>
#include <QPointer>
#include <QFormLayout>
#include <QWizardPage>

#include <condition_variable>
#include <utility>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

class Ui_AutoConfigStartPage;
class Ui_AutoConfigVideoPage;
class Ui_AutoConfigStreamPage;
class Ui_AutoConfigTestPage;

class AutoConfigStreamPage;
class Auth;

class AutoConfig : public QWizard {
	Q_OBJECT

	friend class AutoConfigStartPage;
	friend class AutoConfigVideoPage;
	friend class AutoConfigStreamPage;
	friend class AutoConfigTestPage;

	enum class Type {
		Invalid,
		Streaming,
		Recording,
		VirtualCam,
	};

	enum class Service {
		Twitch,
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

	enum class FPSType : int {
		PreferHighFPS,
		PreferHighRes,
		UseCurrent,
		fps30,
		fps60,
	};

	static inline const char *GetEncoderId(Encoder enc);

	AutoConfigStreamPage *streamPage = nullptr;

	Service service = Service::Other;
	Quality recordingQuality = Quality::Stream;
	Encoder recordingEncoder = Encoder::Stream;
	Encoder streamingEncoder = Encoder::x264;
	Type type = Type::Streaming;
	FPSType fpsType = FPSType::PreferHighFPS;
	int idealBitrate = 2500;
	int baseResolutionCX = 1920;
	int baseResolutionCY = 1080;
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
	bool preferHighFPS = false;
	bool preferHardware = false;
	int specificFPSNum = 0;
	int specificFPSDen = 0;

	void TestHardwareEncoding();
	bool CanTestServer(const char *server);

	virtual void done(int result) override;

	void SaveStreamSettings();
	void SaveSettings();

public:
	AutoConfig(QWidget *parent);
	~AutoConfig();

	enum Page {
		StartPage,
		VideoPage,
		StreamPage,
		TestPage,
	};
};

class AutoConfigStartPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

	Ui_AutoConfigStartPage *ui;

public:
	AutoConfigStartPage(QWidget *parent = nullptr);
	~AutoConfigStartPage();

	virtual int nextId() const override;

public slots:
	void on_prioritizeStreaming_clicked();
	void on_prioritizeRecording_clicked();
	void PrioritizeVCam();
};

class AutoConfigVideoPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

	Ui_AutoConfigVideoPage *ui;

public:
	AutoConfigVideoPage(QWidget *parent = nullptr);
	~AutoConfigVideoPage();

	virtual int nextId() const override;
	virtual bool validatePage() override;
};

class AutoConfigStreamPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

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

	friend class AutoConfig;

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
