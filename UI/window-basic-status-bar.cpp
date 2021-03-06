#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "window-basic-status-bar.hpp"
#include "window-basic-main-outputs.hpp"

OBSBasicStatusBar::OBSBasicStatusBar(QWidget *parent)
	: QStatusBar(parent),
	  delayInfo(new QLabel),
	  droppedFrames(new QLabel),
	  streamIcon(new QLabel),
	  streamTime(new QLabel),
	  recordTime(new QLabel),
	  recordIcon(new QLabel),
	  cpuUsage(new QLabel),
	  transparentPixmap(20, 20),
	  greenPixmap(20, 20),
	  grayPixmap(20, 20),
	  redPixmap(20, 20),
	  recordingActivePixmap(QIcon(":/res/images/recording-active.svg")
					.pixmap(QSize(20, 20))),
	  recordingPausePixmap(QIcon(":/res/images/recording-pause.svg")
				       .pixmap(QSize(20, 20))),
	  recordingPauseInactivePixmap(
		  QIcon(":/res/images/recording-pause-inactive.svg")
			  .pixmap(QSize(20, 20))),
	  recordingInactivePixmap(QIcon(":/res/images/recording-inactive.svg")
					  .pixmap(QSize(20, 20))),
	  streamingActivePixmap(QIcon(":/res/images/streaming-active.svg")
					.pixmap(QSize(20, 20))),
	  streamingInactivePixmap(QIcon(":/res/images/streaming-inactive.svg")
					  .pixmap(QSize(20, 20)))
{
	streamTime->setText(QString("LIVE: 00:00:00"));
	recordTime->setText(QString("REC: 00:00:00"));
	cpuUsage->setText(QString("CPU: 0.0%, 0.00 fps"));

	streamIcon->setPixmap(streamingInactivePixmap);
	recordIcon->setPixmap(recordingInactivePixmap);

	QWidget *brWidget = new QWidget(this);
	QHBoxLayout *brLayout = new QHBoxLayout(brWidget);
	brLayout->setContentsMargins(0, 0, 0, 0);

	statusSquare = new QLabel(brWidget);
	brLayout->addWidget(statusSquare);

	kbps = new QLabel(brWidget);
	brLayout->addWidget(kbps);

	brWidget->setLayout(brLayout);

	delayInfo->setAlignment(Qt::AlignRight);
	delayInfo->setAlignment(Qt::AlignVCenter);
	droppedFrames->setAlignment(Qt::AlignRight);
	droppedFrames->setAlignment(Qt::AlignVCenter);
	streamIcon->setAlignment(Qt::AlignRight);
	streamIcon->setAlignment(Qt::AlignVCenter);
	streamTime->setAlignment(Qt::AlignRight);
	streamTime->setAlignment(Qt::AlignVCenter);
	recordIcon->setAlignment(Qt::AlignRight);
	recordIcon->setAlignment(Qt::AlignVCenter);
	recordTime->setAlignment(Qt::AlignRight);
	recordTime->setAlignment(Qt::AlignVCenter);
	cpuUsage->setAlignment(Qt::AlignRight);
	cpuUsage->setAlignment(Qt::AlignVCenter);
	kbps->setAlignment(Qt::AlignRight);
	kbps->setAlignment(Qt::AlignVCenter);

	delayInfo->setIndent(20);
	droppedFrames->setIndent(20);
	streamIcon->setIndent(20);
	recordIcon->setIndent(20);
	cpuUsage->setIndent(20);
	kbps->setIndent(10);

	addPermanentWidget(droppedFrames);
	addPermanentWidget(streamIcon);
	addPermanentWidget(streamTime);
	addPermanentWidget(recordIcon);
	addPermanentWidget(recordTime);
	addPermanentWidget(cpuUsage);
	addPermanentWidget(delayInfo);
	addPermanentWidget(brWidget);

	transparentPixmap.fill(QColor(0, 0, 0, 0));
	greenPixmap.fill(QColor(0, 255, 0));
	grayPixmap.fill(QColor(72, 72, 72));
	redPixmap.fill(QColor(255, 0, 0));

	statusSquare->setPixmap(transparentPixmap);
}

void OBSBasicStatusBar::Activate()
{
	if (!active) {
		refreshTimer = new QTimer(this);
		connect(refreshTimer, SIGNAL(timeout()), this,
			SLOT(UpdateStatusBar()));

		int skipped = video_output_get_skipped_frames(obs_get_video());
		int total = video_output_get_total_frames(obs_get_video());

		totalStreamSeconds = 0;
		totalRecordSeconds = 0;
		lastSkippedFrameCount = 0;
		startSkippedFrameCount = skipped;
		startTotalFrameCount = total;

		refreshTimer->start(1000);
		active = true;

		if (streamOutput) {
			statusSquare->setPixmap(grayPixmap);
		}
	}

	if (streamOutput) {
		streamIcon->setPixmap(streamingActivePixmap);
	}

	if (recordOutput) {
		recordIcon->setPixmap(recordingActivePixmap);
	}
}

void OBSBasicStatusBar::Deactivate()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());
	if (!main)
		return;

	if (!streamOutput) {
		streamTime->setText(QString("LIVE: 00:00:00"));
		streamIcon->setPixmap(streamingInactivePixmap);
		totalStreamSeconds = 0;
	}

	if (!recordOutput) {
		recordTime->setText(QString("REC: 00:00:00"));
		recordIcon->setPixmap(recordingInactivePixmap);
		totalRecordSeconds = 0;
	}

	if (!main->outputHandler->Active()) {
		delete refreshTimer;

		delayInfo->setText("");
		droppedFrames->setText("");
		kbps->setText("");

		delaySecTotal = 0;
		delaySecStarting = 0;
		delaySecStopping = 0;
		reconnectTimeout = 0;
		active = false;
		overloadedNotify = true;

		statusSquare->setPixmap(transparentPixmap);
	}
}

void OBSBasicStatusBar::UpdateDelayMsg()
{
	QString msg;

	if (delaySecTotal) {
		if (delaySecStarting && !delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStartingIn");
			msg = msg.arg(QString::number(delaySecStarting));

		} else if (!delaySecStarting && delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStoppingIn");
			msg = msg.arg(QString::number(delaySecStopping));

		} else if (delaySecStarting && delaySecStopping) {
			msg = QTStr("Basic.StatusBar.DelayStartingStoppingIn");
			msg = msg.arg(QString::number(delaySecStopping),
				      QString::number(delaySecStarting));
		} else {
			msg = QTStr("Basic.StatusBar.Delay");
			msg = msg.arg(QString::number(delaySecTotal));
		}
	}

	delayInfo->setText(msg);
}

#define BITRATE_UPDATE_SECONDS 2

void OBSBasicStatusBar::UpdateBandwidth()
{
	if (!streamOutput)
		return;

	if (++bitrateUpdateSeconds < BITRATE_UPDATE_SECONDS)
		return;

	uint64_t bytesSent = obs_output_get_total_bytes(streamOutput);
	uint64_t bytesSentTime = os_gettime_ns();

	if (bytesSent < lastBytesSent)
		bytesSent = 0;
	if (bytesSent == 0)
		lastBytesSent = 0;

	uint64_t bitsBetween = (bytesSent - lastBytesSent) * 8;

	double timePassed =
		double(bytesSentTime - lastBytesSentTime) / 1000000000.0;

	double kbitsPerSec = double(bitsBetween) / timePassed / 1000.0;

	QString text;
	text += QString("kb/s: ") + QString::number(kbitsPerSec, 'f', 0);

	kbps->setText(text);
	kbps->setMinimumWidth(kbps->width());

	lastBytesSent = bytesSent;
	lastBytesSentTime = bytesSentTime;
	bitrateUpdateSeconds = 0;
}

void OBSBasicStatusBar::UpdateCPUUsage()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());
	if (!main)
		return;

	QString text;
	text += QString("CPU: ") +
		QString::number(main->GetCPUUsage(), 'f', 1) + QString("%, ") +
		QString::number(obs_get_active_fps(), 'f', 2) + QString(" fps");

	cpuUsage->setText(text);
	cpuUsage->setMinimumWidth(cpuUsage->width());
}

void OBSBasicStatusBar::UpdateStreamTime()
{
	totalStreamSeconds++;

	int seconds = totalStreamSeconds % 60;
	int totalMinutes = totalStreamSeconds / 60;
	int minutes = totalMinutes % 60;
	int hours = totalMinutes / 60;

	QString text = QString::asprintf("LIVE: %02d:%02d:%02d", hours, minutes,
					 seconds);
	streamTime->setText(text);
	streamTime->setMinimumWidth(streamTime->width());

	if (reconnectTimeout > 0) {
		QString msg = QTStr("Basic.StatusBar.Reconnecting")
				      .arg(QString::number(retries),
					   QString::number(reconnectTimeout));
		showMessage(msg);
		reconnectTimeout--;

	} else if (retries > 0) {
		QString msg = QTStr("Basic.StatusBar.AttemptingReconnect");
		showMessage(msg.arg(QString::number(retries)));
	}

	if (delaySecStopping > 0 || delaySecStarting > 0) {
		if (delaySecStopping > 0)
			--delaySecStopping;
		if (delaySecStarting > 0)
			--delaySecStarting;
		UpdateDelayMsg();
	}
}

extern volatile bool recording_paused;

void OBSBasicStatusBar::UpdateRecordTime()
{
	bool paused = os_atomic_load_bool(&recording_paused);

	if (!paused) {
		totalRecordSeconds++;

		int seconds = totalRecordSeconds % 60;
		int totalMinutes = totalRecordSeconds / 60;
		int minutes = totalMinutes % 60;
		int hours = totalMinutes / 60;

		QString text = QString::asprintf("REC: %02d:%02d:%02d", hours,
						 minutes, seconds);

		recordTime->setText(text);
		recordTime->setMinimumWidth(recordTime->width());
	} else {
		recordIcon->setPixmap(streamPauseIconToggle
					      ? recordingPauseInactivePixmap
					      : recordingPausePixmap);

		streamPauseIconToggle = !streamPauseIconToggle;
	}
}

void OBSBasicStatusBar::UpdateDroppedFrames()
{
	if (!streamOutput)
		return;

	int totalDropped = obs_output_get_frames_dropped(streamOutput);
	int totalFrames = obs_output_get_total_frames(streamOutput);
	double percent = (double)totalDropped / (double)totalFrames * 100.0;

	if (!totalFrames)
		return;

	QString text = QTStr("DroppedFrames");
	text = text.arg(QString::number(totalDropped),
			QString::number(percent, 'f', 1));
	droppedFrames->setText(text);
	droppedFrames->setMinimumWidth(droppedFrames->width());

	/* ----------------------------------- *
	 * calculate congestion color          */

	float congestion = obs_output_get_congestion(streamOutput);
	float avgCongestion = (congestion + lastCongestion) * 0.5f;
	if (avgCongestion < congestion)
		avgCongestion = congestion;
	if (avgCongestion > 1.0f)
		avgCongestion = 1.0f;

	if (avgCongestion < EPSILON) {
		statusSquare->setPixmap(greenPixmap);
	} else if (fabsf(avgCongestion - 1.0f) < EPSILON) {
		statusSquare->setPixmap(redPixmap);
	} else {
		QPixmap pixmap(20, 20);

		float red = avgCongestion * 2.0f;
		if (red > 1.0f)
			red = 1.0f;
		red *= 255.0;

		float green = (1.0f - avgCongestion) * 2.0f;
		if (green > 1.0f)
			green = 1.0f;
		green *= 255.0;

		pixmap.fill(QColor(int(red), int(green), 0));
		statusSquare->setPixmap(pixmap);
	}

	lastCongestion = congestion;
}

void OBSBasicStatusBar::OBSOutputReconnect(void *data, calldata_t *params)
{
	OBSBasicStatusBar *statusBar =
		reinterpret_cast<OBSBasicStatusBar *>(data);

	int seconds = (int)calldata_int(params, "timeout_sec");
	QMetaObject::invokeMethod(statusBar, "Reconnect", Q_ARG(int, seconds));
	UNUSED_PARAMETER(params);
}

void OBSBasicStatusBar::OBSOutputReconnectSuccess(void *data,
						  calldata_t *params)
{
	OBSBasicStatusBar *statusBar =
		reinterpret_cast<OBSBasicStatusBar *>(data);

	QMetaObject::invokeMethod(statusBar, "ReconnectSuccess");
	UNUSED_PARAMETER(params);
}

void OBSBasicStatusBar::Reconnect(int seconds)
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	if (!retries)
		main->SysTrayNotify(
			QTStr("Basic.SystemTray.Message.Reconnecting"),
			QSystemTrayIcon::Warning);

	reconnectTimeout = seconds;

	if (streamOutput) {
		delaySecTotal = obs_output_get_active_delay(streamOutput);
		UpdateDelayMsg();

		retries++;
	}
}

void OBSBasicStatusBar::ReconnectClear()
{
	retries = 0;
	reconnectTimeout = 0;
	bitrateUpdateSeconds = -1;
	lastBytesSent = 0;
	lastBytesSentTime = os_gettime_ns();
	delaySecTotal = 0;
	UpdateDelayMsg();
}

void OBSBasicStatusBar::ReconnectSuccess()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	QString msg = QTStr("Basic.StatusBar.ReconnectSuccessful");
	showMessage(msg, 4000);
	main->SysTrayNotify(msg, QSystemTrayIcon::Information);
	ReconnectClear();

	if (streamOutput) {
		delaySecTotal = obs_output_get_active_delay(streamOutput);
		UpdateDelayMsg();
	}
}

void OBSBasicStatusBar::UpdateStatusBar()
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());

	UpdateBandwidth();

	if (streamOutput)
		UpdateStreamTime();

	if (recordOutput)
		UpdateRecordTime();

	UpdateDroppedFrames();

	int skipped = video_output_get_skipped_frames(obs_get_video());
	int total = video_output_get_total_frames(obs_get_video());

	skipped -= startSkippedFrameCount;
	total -= startTotalFrameCount;

	int diff = skipped - lastSkippedFrameCount;
	double percentage = double(skipped) / double(total) * 100.0;

	if (diff > 10 && percentage >= 0.1f) {
		showMessage(QTStr("HighResourceUsage"), 4000);
		if (!main->isVisible() && overloadedNotify) {
			main->SysTrayNotify(QTStr("HighResourceUsage"),
					    QSystemTrayIcon::Warning);
			overloadedNotify = false;
		}
	}

	lastSkippedFrameCount = skipped;
}

void OBSBasicStatusBar::StreamDelayStarting(int sec)
{
	OBSBasic *main = qobject_cast<OBSBasic *>(parent());
	if (!main || !main->outputHandler)
		return;

	streamOutput = main->outputHandler->streamOutput;

	delaySecTotal = delaySecStarting = sec;
	UpdateDelayMsg();
	Activate();
}

void OBSBasicStatusBar::StreamDelayStopping(int sec)
{
	delaySecTotal = delaySecStopping = sec;
	UpdateDelayMsg();
}

void OBSBasicStatusBar::StreamStarted(obs_output_t *output)
{
	streamOutput = output;

	signal_handler_connect(obs_output_get_signal_handler(streamOutput),
			       "reconnect", OBSOutputReconnect, this);
	signal_handler_connect(obs_output_get_signal_handler(streamOutput),
			       "reconnect_success", OBSOutputReconnectSuccess,
			       this);

	retries = 0;
	lastBytesSent = 0;
	lastBytesSentTime = os_gettime_ns();
	Activate();
}

void OBSBasicStatusBar::StreamStopped()
{
	if (streamOutput) {
		signal_handler_disconnect(
			obs_output_get_signal_handler(streamOutput),
			"reconnect", OBSOutputReconnect, this);
		signal_handler_disconnect(
			obs_output_get_signal_handler(streamOutput),
			"reconnect_success", OBSOutputReconnectSuccess, this);

		ReconnectClear();
		streamOutput = nullptr;
		clearMessage();
		Deactivate();
	}
}

void OBSBasicStatusBar::RecordingStarted(obs_output_t *output)
{
	recordOutput = output;
	Activate();
}

void OBSBasicStatusBar::RecordingStopped()
{
	recordOutput = nullptr;
	Deactivate();
}

void OBSBasicStatusBar::RecordingPaused()
{
	QString text = recordTime->text() + QStringLiteral(" (PAUSED)");
	recordTime->setText(text);

	if (recordOutput) {
		recordIcon->setPixmap(recordingPausePixmap);
		streamPauseIconToggle = true;
	}
}

void OBSBasicStatusBar::RecordingUnpaused()
{
	if (recordOutput) {
		recordIcon->setPixmap(recordingActivePixmap);
	}
}
