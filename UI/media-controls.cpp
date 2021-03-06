#include "window-basic-main.hpp"
#include "media-controls.hpp"
#include "obs-app.hpp"
#include <QToolTip>
#include <QStyle>
#include <QMenu>

#include "ui_media-controls.h"

void MediaControls::OBSMediaStopped(void *data, calldata_t *)
{
	MediaControls *media = static_cast<MediaControls *>(data);
	QMetaObject::invokeMethod(media, "SetRestartState");
}

void MediaControls::OBSMediaPlay(void *data, calldata_t *)
{
	MediaControls *media = static_cast<MediaControls *>(data);
	QMetaObject::invokeMethod(media, "SetPlayingState");
}

void MediaControls::OBSMediaPause(void *data, calldata_t *)
{
	MediaControls *media = static_cast<MediaControls *>(data);
	QMetaObject::invokeMethod(media, "SetPausedState");
}

void MediaControls::OBSMediaStarted(void *data, calldata_t *)
{
	MediaControls *media = static_cast<MediaControls *>(data);
	QMetaObject::invokeMethod(media, "SetPlayingState");
}

MediaControls::MediaControls(QWidget *parent)
	: QWidget(parent), ui(new Ui::MediaControls)
{
	ui->setupUi(this);
	ui->playPauseButton->setProperty("themeID", "playIcon");
	ui->previousButton->setProperty("themeID", "previousIcon");
	ui->nextButton->setProperty("themeID", "nextIcon");
	ui->stopButton->setProperty("themeID", "stopIcon");
	setFocusPolicy(Qt::StrongFocus);

	connect(&mediaTimer, SIGNAL(timeout()), this,
		SLOT(SetSliderPosition()));
	connect(&seekTimer, SIGNAL(timeout()), this, SLOT(SeekTimerCallback()));
	connect(ui->slider, SIGNAL(sliderPressed()), this,
		SLOT(MediaSliderClicked()));
	connect(ui->slider, SIGNAL(mediaSliderHovered(int)), this,
		SLOT(MediaSliderHovered(int)));
	connect(ui->slider, SIGNAL(sliderReleased()), this,
		SLOT(MediaSliderReleased()));
	connect(ui->slider, SIGNAL(sliderMoved(int)), this,
		SLOT(MediaSliderMoved(int)));

	countDownTimer = config_get_bool(App()->GlobalConfig(), "BasicWindow",
					 "MediaControlsCountdownTimer");

	QAction *restartAction = new QAction(this);
	restartAction->setShortcut({Qt::Key_R});
	connect(restartAction, SIGNAL(triggered()), this, SLOT(RestartMedia()));
	addAction(restartAction);

	QAction *sliderFoward = new QAction(this);
	sliderFoward->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(sliderFoward, SIGNAL(triggered()), this,
		SLOT(MoveSliderFoward()));
	sliderFoward->setShortcut({Qt::Key_Right});
	addAction(sliderFoward);

	QAction *sliderBack = new QAction(this);
	sliderBack->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(sliderBack, SIGNAL(triggered()), this,
		SLOT(MoveSliderBackwards()));
	sliderBack->setShortcut({Qt::Key_Left});
	addAction(sliderBack);
}

MediaControls::~MediaControls()
{
	delete ui;
}

bool MediaControls::MediaPaused()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return false;
	}

	obs_media_state state = obs_source_media_get_state(source);
	return state == OBS_MEDIA_STATE_PAUSED;
}

int64_t MediaControls::GetSliderTime(int val)
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return 0;
	}

	float percent = (float)val / (float)ui->slider->maximum();
	float duration = (float)obs_source_media_get_duration(source);
	int64_t seekTo = (int64_t)(percent * duration);

	return seekTo;
}

void MediaControls::MediaSliderClicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);

	if (state == OBS_MEDIA_STATE_PAUSED) {
		prevPaused = true;
	} else if (state == OBS_MEDIA_STATE_PLAYING) {
		prevPaused = false;
		PauseMedia();
		StopMediaTimer();
	}

	seek = ui->slider->value();
	seekTimer.start(100);
}

void MediaControls::MediaSliderReleased()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	if (seekTimer.isActive()) {
		seekTimer.stop();
		if (lastSeek != seek) {
			obs_source_media_set_time(source, GetSliderTime(seek));
		}

		seek = lastSeek = -1;
	}

	if (!prevPaused) {
		PlayMedia();
		StartMediaTimer();
	}
}

void MediaControls::MediaSliderHovered(int val)
{
	float seconds = ((float)GetSliderTime(val) / 1000.0f);
	QToolTip::showText(QCursor::pos(), FormatSeconds((int)seconds), this);
}

void MediaControls::MediaSliderMoved(int val)
{
	if (seekTimer.isActive()) {
		seek = val;
	}
}

void MediaControls::SeekTimerCallback()
{
	if (lastSeek != seek) {
		OBSSource source = OBSGetStrongRef(weakSource);
		if (source) {
			obs_source_media_set_time(source, GetSliderTime(seek));
		}
		lastSeek = seek;
	}
}

void MediaControls::StartMediaTimer()
{
	if (isSlideshow)
		return;

	if (!mediaTimer.isActive())
		mediaTimer.start(1000);
}

void MediaControls::StopMediaTimer()
{
	if (mediaTimer.isActive())
		mediaTimer.stop();
}

void MediaControls::SetPlayingState()
{
	ui->slider->setEnabled(true);
	ui->playPauseButton->setProperty("themeID", "pauseIcon");
	ui->playPauseButton->style()->unpolish(ui->playPauseButton);
	ui->playPauseButton->style()->polish(ui->playPauseButton);
	ui->playPauseButton->setToolTip(
		QTStr("ContextBar.MediaControls.PauseMedia"));

	prevPaused = false;

	StartMediaTimer();
}

void MediaControls::SetPausedState()
{
	ui->playPauseButton->setProperty("themeID", "playIcon");
	ui->playPauseButton->style()->unpolish(ui->playPauseButton);
	ui->playPauseButton->style()->polish(ui->playPauseButton);
	ui->playPauseButton->setToolTip(
		QTStr("ContextBar.MediaControls.PlayMedia"));

	StopMediaTimer();
}

void MediaControls::SetRestartState()
{
	ui->playPauseButton->setProperty("themeID", "restartIcon");
	ui->playPauseButton->style()->unpolish(ui->playPauseButton);
	ui->playPauseButton->style()->polish(ui->playPauseButton);
	ui->playPauseButton->setToolTip(
		QTStr("ContextBar.MediaControls.RestartMedia"));

	ui->slider->setValue(0);
	ui->timerLabel->setText("--:--:--");
	ui->durationLabel->setText("--:--:--");
	ui->slider->setEnabled(false);

	StopMediaTimer();
}

void MediaControls::RefreshControls()
{
	OBSSource source;
	source = OBSGetStrongRef(weakSource);

	uint32_t flags = 0;
	const char *id = nullptr;

	if (source) {
		flags = obs_source_get_output_flags(source);
		id = obs_source_get_unversioned_id(source);
	}

	if (!source || !(flags & OBS_SOURCE_CONTROLLABLE_MEDIA)) {
		SetRestartState();
		setEnabled(false);
		hide();
		return;
	} else {
		setEnabled(true);
		show();
	}

	bool has_playlist = strcmp(id, "ffmpeg_source") != 0;
	ui->previousButton->setVisible(has_playlist);
	ui->nextButton->setVisible(has_playlist);

	isSlideshow = strcmp(id, "slideshow") == 0;
	ui->slider->setVisible(!isSlideshow);
	ui->timerLabel->setVisible(!isSlideshow);
	ui->label->setVisible(!isSlideshow);
	ui->durationLabel->setVisible(!isSlideshow);
	ui->emptySpaceAgain->setVisible(isSlideshow);

	obs_media_state state = obs_source_media_get_state(source);

	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
	case OBS_MEDIA_STATE_NONE:
		SetRestartState();
		break;
	case OBS_MEDIA_STATE_PLAYING:
		SetPlayingState();
		break;
	case OBS_MEDIA_STATE_PAUSED:
		SetPausedState();
		break;
	default:
		break;
	}

	SetSliderPosition();
}

OBSSource MediaControls::GetSource()
{
	return OBSGetStrongRef(weakSource);
}

void MediaControls::SetSource(OBSSource source)
{
	sigs.clear();

	if (source) {
		weakSource = OBSGetWeakRef(source);
		signal_handler_t *sh = obs_source_get_signal_handler(source);
		sigs.emplace_back(sh, "media_play", OBSMediaPlay, this);
		sigs.emplace_back(sh, "media_pause", OBSMediaPause, this);
		sigs.emplace_back(sh, "media_restart", OBSMediaPlay, this);
		sigs.emplace_back(sh, "media_stopped", OBSMediaStopped, this);
		sigs.emplace_back(sh, "media_started", OBSMediaStarted, this);
		sigs.emplace_back(sh, "media_ended", OBSMediaStopped, this);
	} else {
		weakSource = nullptr;
	}

	RefreshControls();
}

void MediaControls::SetSliderPosition()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	float time = (float)obs_source_media_get_time(source);
	float duration = (float)obs_source_media_get_duration(source);

	float sliderPosition = (time / duration) * (float)ui->slider->maximum();

	ui->slider->setValue((int)sliderPosition);

	ui->timerLabel->setText(FormatSeconds((int)(time / 1000.0f)));

	if (!countDownTimer)
		ui->durationLabel->setText(
			FormatSeconds((int)(duration / 1000.0f)));
	else
		ui->durationLabel->setText(
			QString("-") +
			FormatSeconds((int)((duration - time) / 1000.0f)));
}

QString MediaControls::FormatSeconds(int totalSeconds)
{
	int seconds = totalSeconds % 60;
	int totalMinutes = totalSeconds / 60;
	int minutes = totalMinutes % 60;
	int hours = totalMinutes / 60;

	return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

void MediaControls::on_playPauseButton_clicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);

	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
		RestartMedia();
		break;
	case OBS_MEDIA_STATE_PLAYING:
		PauseMedia();
		break;
	case OBS_MEDIA_STATE_PAUSED:
		PlayMedia();
		break;
	default:
		break;
	}
}

void MediaControls::RestartMedia()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_restart(source);
	}
}

void MediaControls::PlayMedia()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_play_pause(source, false);
	}
}

void MediaControls::PauseMedia()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_play_pause(source, true);
	}
}

void MediaControls::StopMedia()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_stop(source);
	}
}

void MediaControls::PlaylistNext()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_next(source);
	}
}

void MediaControls::PlaylistPrevious()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_previous(source);
	}
}

void MediaControls::on_stopButton_clicked()
{
	StopMedia();
}

void MediaControls::on_nextButton_clicked()
{
	PlaylistNext();
}

void MediaControls::on_previousButton_clicked()
{
	PlaylistPrevious();
}

void MediaControls::on_durationLabel_clicked()
{
	countDownTimer = !countDownTimer;

	config_set_bool(App()->GlobalConfig(), "BasicWindow",
			"MediaControlsCountdownTimer", countDownTimer);

	if (MediaPaused())
		SetSliderPosition();
}

void MediaControls::MoveSliderFoward(int seconds)
{
	OBSSource source = OBSGetStrongRef(weakSource);

	if (!source)
		return;

	int ms = obs_source_media_get_time(source);
	ms += seconds * 1000;

	obs_source_media_set_time(source, ms);
	SetSliderPosition();
}

void MediaControls::MoveSliderBackwards(int seconds)
{
	OBSSource source = OBSGetStrongRef(weakSource);

	if (!source)
		return;

	int ms = obs_source_media_get_time(source);
	ms -= seconds * 1000;

	obs_source_media_set_time(source, ms);
	SetSliderPosition();
}
