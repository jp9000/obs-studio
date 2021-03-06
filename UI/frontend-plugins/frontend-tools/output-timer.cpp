#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>
#include <QAction>
#include <QMainWindow>
#include <QTimer>
#include <QObject>
#include "output-timer.hpp"

using namespace std;

OutputTimer *ot;

OutputTimer::OutputTimer(QWidget *parent)
	: QDialog(parent), ui(new Ui_OutputTimer)
{
	ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	QObject::connect(ui->outputTimerStream, SIGNAL(clicked()), this,
			 SLOT(StreamingTimerButton()));
	QObject::connect(ui->outputTimerRecord, SIGNAL(clicked()), this,
			 SLOT(RecordingTimerButton()));
	QObject::connect(ui->buttonBox->button(QDialogButtonBox::Close),
			 SIGNAL(clicked()), this, SLOT(hide()));

	streamingTimer = new QTimer(this);
	streamingTimerDisplay = new QTimer(this);

	recordingTimer = new QTimer(this);
	recordingTimerDisplay = new QTimer(this);
}

void OutputTimer::closeEvent(QCloseEvent *)
{
	obs_frontend_save();
}

void OutputTimer::StreamingTimerButton()
{
	if (!obs_frontend_streaming_active()) {
		blog(LOG_INFO, "Starting stream due to OutputTimer");
		obs_frontend_streaming_start();
	} else if (streamingAlreadyActive) {
		StreamTimerStart();
		streamingAlreadyActive = false;
	} else if (obs_frontend_streaming_active()) {
		blog(LOG_INFO, "Stopping stream due to OutputTimer");
		obs_frontend_streaming_stop();
	}
}

void OutputTimer::RecordingTimerButton()
{
	if (!obs_frontend_recording_active()) {
		blog(LOG_INFO, "Starting recording due to OutputTimer");
		obs_frontend_recording_start();
	} else if (recordingAlreadyActive) {
		RecordTimerStart();
		recordingAlreadyActive = false;
	} else if (obs_frontend_recording_active()) {
		blog(LOG_INFO, "Stopping recording due to OutputTimer");
		obs_frontend_recording_stop();
	}
}

void OutputTimer::StreamTimerStart()
{
	if (!isVisible() && ui->autoStartStreamTimer->isChecked() == false) {
		streamingAlreadyActive = true;
		return;
	}

	int hours = ui->streamingTimerHours->value();
	int minutes = ui->streamingTimerMinutes->value();
	int seconds = ui->streamingTimerSeconds->value();

	int total = (((hours * 3600) + (minutes * 60)) + seconds) * 1000;

	if (total == 0)
		total = 1000;

	streamingTimer->setInterval(total);
	streamingTimer->setSingleShot(true);

	QObject::connect(streamingTimer, SIGNAL(timeout()),
			 SLOT(EventStopStreaming()));

	QObject::connect(streamingTimerDisplay, SIGNAL(timeout()), this,
			 SLOT(UpdateStreamTimerDisplay()));

	streamingTimer->start();
	streamingTimerDisplay->start(1000);
	ui->outputTimerStream->setText(obs_module_text("Stop"));

	UpdateStreamTimerDisplay();

	ui->outputTimerStream->setChecked(true);
}

void OutputTimer::RecordTimerStart()
{
	if (!isVisible() && ui->autoStartRecordTimer->isChecked() == false) {
		recordingAlreadyActive = true;
		return;
	}

	int hours = ui->recordingTimerHours->value();
	int minutes = ui->recordingTimerMinutes->value();
	int seconds = ui->recordingTimerSeconds->value();

	int total = (((hours * 3600) + (minutes * 60)) + seconds) * 1000;

	if (total == 0)
		total = 1000;

	recordingTimer->setInterval(total);
	recordingTimer->setSingleShot(true);

	QObject::connect(recordingTimer, SIGNAL(timeout()),
			 SLOT(EventStopRecording()));

	QObject::connect(recordingTimerDisplay, SIGNAL(timeout()), this,
			 SLOT(UpdateRecordTimerDisplay()));

	recordingTimer->start();
	recordingTimerDisplay->start(1000);
	ui->outputTimerRecord->setText(obs_module_text("Stop"));

	UpdateRecordTimerDisplay();

	ui->outputTimerRecord->setChecked(true);
}

void OutputTimer::StreamTimerStop()
{
	streamingAlreadyActive = false;

	if (!isVisible() && streamingTimer->isActive() == false)
		return;

	if (streamingTimer->isActive())
		streamingTimer->stop();

	ui->outputTimerStream->setText(obs_module_text("Start"));

	if (streamingTimerDisplay->isActive())
		streamingTimerDisplay->stop();

	ui->streamTime->setText("00:00:00");
	ui->outputTimerStream->setChecked(false);
}

void OutputTimer::RecordTimerStop()
{
	recordingAlreadyActive = false;

	if (!isVisible() && recordingTimer->isActive() == false)
		return;

	if (recordingTimer->isActive())
		recordingTimer->stop();

	ui->outputTimerRecord->setText(obs_module_text("Start"));

	if (recordingTimerDisplay->isActive())
		recordingTimerDisplay->stop();

	ui->recordTime->setText("00:00:00");
	ui->outputTimerRecord->setChecked(false);
}

void OutputTimer::UpdateStreamTimerDisplay()
{
	int remainingTime = streamingTimer->remainingTime() / 1000;

	int seconds = remainingTime % 60;
	int minutes = (remainingTime % 3600) / 60;
	int hours = remainingTime / 3600;

	QString text =
		QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
	ui->streamTime->setText(text);
}

void OutputTimer::UpdateRecordTimerDisplay()
{
	int remainingTime = 0;

	if (obs_frontend_recording_paused() &&
	    ui->pauseRecordTimer->isChecked())
		remainingTime = recordingTimeLeft / 1000;
	else
		remainingTime = recordingTimer->remainingTime() / 1000;

	int seconds = remainingTime % 60;
	int minutes = (remainingTime % 3600) / 60;
	int hours = remainingTime / 3600;

	QString text =
		QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
	ui->recordTime->setText(text);
}

void OutputTimer::PauseRecordingTimer()
{
	if (!ui->pauseRecordTimer->isChecked())
		return;

	if (recordingTimer->isActive()) {
		recordingTimeLeft = recordingTimer->remainingTime();
		recordingTimer->stop();
	}
}

void OutputTimer::UnpauseRecordingTimer()
{
	if (!ui->pauseRecordTimer->isChecked())
		return;

	if (!recordingTimer->isActive())
		recordingTimer->start(recordingTimeLeft);
}

void OutputTimer::ShowHideDialog()
{
	if (!isVisible()) {
		setVisible(true);
		QTimer::singleShot(250, this, SLOT(show()));
	} else {
		setVisible(false);
		QTimer::singleShot(250, this, SLOT(hide()));
	}
}

void OutputTimer::EventStopStreaming()
{
	blog(LOG_INFO, "Stopping stream due to OutputTimer timeout");
	obs_frontend_streaming_stop();
}

void OutputTimer::EventStopRecording()
{
	blog(LOG_INFO, "Stopping recording due to OutputTimer timeout");
	obs_frontend_recording_stop();
}

static void SaveOutputTimer(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		obs_data_t *obj = obs_data_create();

		obs_data_set_int(obj, "streamTimerHours",
				 ot->ui->streamingTimerHours->value());
		obs_data_set_int(obj, "streamTimerMinutes",
				 ot->ui->streamingTimerMinutes->value());
		obs_data_set_int(obj, "streamTimerSeconds",
				 ot->ui->streamingTimerSeconds->value());

		obs_data_set_int(obj, "recordTimerHours",
				 ot->ui->recordingTimerHours->value());
		obs_data_set_int(obj, "recordTimerMinutes",
				 ot->ui->recordingTimerMinutes->value());
		obs_data_set_int(obj, "recordTimerSeconds",
				 ot->ui->recordingTimerSeconds->value());

		obs_data_set_bool(obj, "autoStartStreamTimer",
				  ot->ui->autoStartStreamTimer->isChecked());
		obs_data_set_bool(obj, "autoStartRecordTimer",
				  ot->ui->autoStartRecordTimer->isChecked());

		obs_data_set_bool(obj, "pauseRecordTimer",
				  ot->ui->pauseRecordTimer->isChecked());

		obs_data_set_obj(save_data, "output-timer", obj);

		obs_data_release(obj);
	} else {
		obs_data_t *obj = obs_data_get_obj(save_data, "output-timer");

		if (!obj)
			obj = obs_data_create();

		ot->ui->streamingTimerHours->setValue(
			obs_data_get_int(obj, "streamTimerHours"));
		ot->ui->streamingTimerMinutes->setValue(
			obs_data_get_int(obj, "streamTimerMinutes"));
		ot->ui->streamingTimerSeconds->setValue(
			obs_data_get_int(obj, "streamTimerSeconds"));

		ot->ui->recordingTimerHours->setValue(
			obs_data_get_int(obj, "recordTimerHours"));
		ot->ui->recordingTimerMinutes->setValue(
			obs_data_get_int(obj, "recordTimerMinutes"));
		ot->ui->recordingTimerSeconds->setValue(
			obs_data_get_int(obj, "recordTimerSeconds"));

		ot->ui->autoStartStreamTimer->setChecked(
			obs_data_get_bool(obj, "autoStartStreamTimer"));
		ot->ui->autoStartRecordTimer->setChecked(
			obs_data_get_bool(obj, "autoStartRecordTimer"));

		ot->ui->pauseRecordTimer->setChecked(
			obs_data_get_bool(obj, "pauseRecordTimer"));

		obs_data_release(obj);
	}
}

extern "C" void FreeOutputTimer() {}

static void OBSEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		obs_frontend_save();
		FreeOutputTimer();
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		ot->StreamTimerStart();
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING) {
		ot->StreamTimerStop();
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		ot->RecordTimerStart();
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPING) {
		ot->RecordTimerStop();
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_PAUSED) {
		ot->PauseRecordingTimer();
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_UNPAUSED) {
		ot->UnpauseRecordingTimer();
	}
}

extern "C" void InitOutputTimer()
{
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("OutputTimer"));

	obs_frontend_push_ui_translation(obs_module_get_string);

	QMainWindow *window = (QMainWindow *)obs_frontend_get_main_window();

	ot = new OutputTimer(window);

	auto cb = []() { ot->ShowHideDialog(); };

	obs_frontend_pop_ui_translation();

	obs_frontend_add_save_callback(SaveOutputTimer, nullptr);
	obs_frontend_add_event_callback(OBSEvent, nullptr);

	action->connect(action, &QAction::triggered, cb);
}
