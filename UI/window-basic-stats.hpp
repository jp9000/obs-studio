#pragma once

#include <obs.hpp>
#include <util/platform.h>
#include <obs-frontend-api.h>
#include <QPointer>
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QList>

class QGridLayout;
class QCloseEvent;

class OBSBasicStats : public QWidget {
	Q_OBJECT

	QLabel *fps = nullptr;
	QLabel *cpuUsage = nullptr;
	QLabel *hddSpace = nullptr;
	QLabel *recordTimeLeft = nullptr;
	QLabel *memUsage = nullptr;

	QLabel *renderTime = nullptr;
	QLabel *skippedFrames = nullptr;
	QLabel *missedFrames = nullptr;

	QGridLayout *outputLayout = nullptr;

	os_cpu_usage_info_t *cpu_info = nullptr;

	QTimer timer;
	QTimer recTimeLeft;
	uint64_t num_bytes = 0;
	std::vector<long double> bitrates;

	struct OutputLabels {
		QPointer<QLabel> name;
		QPointer<QLabel> status;
		QPointer<QLabel> droppedFrames;
		QPointer<QLabel> megabytesSent;
		QPointer<QLabel> bitrate;

		uint64_t lastBytesSent = 0;
		uint64_t lastBytesSentTime = 0;

		int first_total = 0;
		int first_dropped = 0;

		void Update(obs_output_t *output, bool rec);
		void Reset(obs_output_t *output);

		long double kbps = 0.0l;
	};

	QList<OutputLabels> outputLabels;

	void AddOutputLabels(QString name);
	void Update();

	virtual void closeEvent(QCloseEvent *event) override;

	static void OBSFrontendEvent(enum obs_frontend_event event, void *ptr);

public:
	OBSBasicStats(QWidget *parent = nullptr, bool closable = true);
	~OBSBasicStats();

	static void InitializeValues();

	void StartRecTimeLeft();
	void ResetRecTimeLeft();

private:
	QPointer<QObject> shortcutFilter;

private slots:
	void RecordingTimeLeft();

public slots:
	void Reset();

protected:
	virtual void showEvent(QShowEvent *event) override;
	virtual void hideEvent(QHideEvent *event) override;
};
