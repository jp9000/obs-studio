#pragma once

#include <obs.hpp>
#include <QWidget>
#include <QPaintEvent>
#include <QSharedPointer>
#include <QTimer>
#include <QMutex>
#include <QList>
#include <QMenu>

class QPushButton;
class VolumeMeterTimer;

class VolumeMeter : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor backgroundNominalColor READ getBackgroundNominalColor
			   WRITE setBackgroundNominalColor DESIGNABLE true)
	Q_PROPERTY(QColor backgroundWarningColor READ getBackgroundWarningColor
			   WRITE setBackgroundWarningColor DESIGNABLE true)
	Q_PROPERTY(QColor backgroundErrorColor READ getBackgroundErrorColor
			   WRITE setBackgroundErrorColor DESIGNABLE true)
	Q_PROPERTY(QColor foregroundNominalColor READ getForegroundNominalColor
			   WRITE setForegroundNominalColor DESIGNABLE true)
	Q_PROPERTY(QColor foregroundWarningColor READ getForegroundWarningColor
			   WRITE setForegroundWarningColor DESIGNABLE true)
	Q_PROPERTY(QColor foregroundErrorColor READ getForegroundErrorColor
			   WRITE setForegroundErrorColor DESIGNABLE true)
	Q_PROPERTY(QColor clipColor READ getClipColor WRITE setClipColor
			   DESIGNABLE true)
	Q_PROPERTY(QColor magnitudeColor READ getMagnitudeColor WRITE
			   setMagnitudeColor DESIGNABLE true)
	Q_PROPERTY(QColor majorTickColor READ getMajorTickColor WRITE
			   setMajorTickColor DESIGNABLE true)
	Q_PROPERTY(QColor minorTickColor READ getMinorTickColor WRITE
			   setMinorTickColor DESIGNABLE true)

	// Levels are denoted in dBFS.
	Q_PROPERTY(qreal minimumLevel READ getMinimumLevel WRITE setMinimumLevel
			   DESIGNABLE true)
	Q_PROPERTY(qreal warningLevel READ getWarningLevel WRITE setWarningLevel
			   DESIGNABLE true)
	Q_PROPERTY(qreal errorLevel READ getErrorLevel WRITE setErrorLevel
			   DESIGNABLE true)
	Q_PROPERTY(qreal clipLevel READ getClipLevel WRITE setClipLevel
			   DESIGNABLE true)
	Q_PROPERTY(qreal minimumInputLevel READ getMinimumInputLevel WRITE
			   setMinimumInputLevel DESIGNABLE true)

	// Rates are denoted in dB/second.
	Q_PROPERTY(qreal peakDecayRate READ getPeakDecayRate WRITE
			   setPeakDecayRate DESIGNABLE true)

	// Time in seconds for the VU meter to integrate over.
	Q_PROPERTY(
		qreal magnitudeIntegrationTime READ getMagnitudeIntegrationTime
			WRITE setMagnitudeIntegrationTime DESIGNABLE true)

	// Duration is denoted in seconds.
	Q_PROPERTY(qreal peakHoldDuration READ getPeakHoldDuration WRITE
			   setPeakHoldDuration DESIGNABLE true)
	Q_PROPERTY(qreal inputPeakHoldDuration READ getInputPeakHoldDuration
			   WRITE setInputPeakHoldDuration DESIGNABLE true)

private slots:
	void ClipEnding();

private:
	obs_volmeter_t *obs_volmeter;
	static QWeakPointer<VolumeMeterTimer> updateTimer;
	QSharedPointer<VolumeMeterTimer> updateTimerRef;

	inline void resetLevels();
	inline void handleChannelCofigurationChange();
	inline bool detectIdle(uint64_t ts);
	inline void calculateBallistics(uint64_t ts,
					qreal timeSinceLastRedraw = 0.0);
	inline void calculateBallisticsForChannel(int channelNr, uint64_t ts,
						  qreal timeSinceLastRedraw);

	void paintInputMeter(QPainter &painter, int x, int y, int width,
			     int height, float peakHold);
	void paintHMeter(QPainter &painter, int x, int y, int width, int height,
			 float magnitude, float peak, float peakHold);
	void paintHTicks(QPainter &painter, int x, int y, int width,
			 int height);
	void paintVMeter(QPainter &painter, int x, int y, int width, int height,
			 float magnitude, float peak, float peakHold);
	void paintVTicks(QPainter &painter, int x, int y, int height);

	QMutex dataMutex;

	uint64_t currentLastUpdateTime = 0;
	float currentMagnitude[MAX_AUDIO_CHANNELS];
	float currentPeak[MAX_AUDIO_CHANNELS];
	float currentInputPeak[MAX_AUDIO_CHANNELS];

	QPixmap *tickPaintCache = nullptr;
	int displayNrAudioChannels = 0;
	float displayMagnitude[MAX_AUDIO_CHANNELS];
	float displayPeak[MAX_AUDIO_CHANNELS];
	float displayPeakHold[MAX_AUDIO_CHANNELS];
	uint64_t displayPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
	float displayInputPeakHold[MAX_AUDIO_CHANNELS];
	uint64_t displayInputPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];

	QFont tickFont;
	QColor backgroundNominalColor;
	QColor backgroundWarningColor;
	QColor backgroundErrorColor;
	QColor foregroundNominalColor;
	QColor foregroundWarningColor;
	QColor foregroundErrorColor;
	QColor clipColor;
	QColor magnitudeColor;
	QColor majorTickColor;
	QColor minorTickColor;
	qreal minimumLevel;
	qreal warningLevel;
	qreal errorLevel;
	qreal clipLevel;
	qreal minimumInputLevel;
	qreal peakDecayRate;
	qreal magnitudeIntegrationTime;
	qreal peakHoldDuration;
	qreal inputPeakHoldDuration;

	uint64_t lastRedrawTime = 0;
	int channels = 0;
	bool clipping = false;
	bool vertical;

public:
	explicit VolumeMeter(QWidget *parent = nullptr,
			     obs_volmeter_t *obs_volmeter = nullptr,
			     bool vertical = false);
	~VolumeMeter();

	void setLevels(const float magnitude[MAX_AUDIO_CHANNELS],
		       const float peak[MAX_AUDIO_CHANNELS],
		       const float inputPeak[MAX_AUDIO_CHANNELS]);

	QColor getBackgroundNominalColor() const;
	void setBackgroundNominalColor(QColor c);
	QColor getBackgroundWarningColor() const;
	void setBackgroundWarningColor(QColor c);
	QColor getBackgroundErrorColor() const;
	void setBackgroundErrorColor(QColor c);
	QColor getForegroundNominalColor() const;
	void setForegroundNominalColor(QColor c);
	QColor getForegroundWarningColor() const;
	void setForegroundWarningColor(QColor c);
	QColor getForegroundErrorColor() const;
	void setForegroundErrorColor(QColor c);
	QColor getClipColor() const;
	void setClipColor(QColor c);
	QColor getMagnitudeColor() const;
	void setMagnitudeColor(QColor c);
	QColor getMajorTickColor() const;
	void setMajorTickColor(QColor c);
	QColor getMinorTickColor() const;
	void setMinorTickColor(QColor c);
	qreal getMinimumLevel() const;
	void setMinimumLevel(qreal v);
	qreal getWarningLevel() const;
	void setWarningLevel(qreal v);
	qreal getErrorLevel() const;
	void setErrorLevel(qreal v);
	qreal getClipLevel() const;
	void setClipLevel(qreal v);
	qreal getMinimumInputLevel() const;
	void setMinimumInputLevel(qreal v);
	qreal getPeakDecayRate() const;
	void setPeakDecayRate(qreal v);
	qreal getMagnitudeIntegrationTime() const;
	void setMagnitudeIntegrationTime(qreal v);
	qreal getPeakHoldDuration() const;
	void setPeakHoldDuration(qreal v);
	qreal getInputPeakHoldDuration() const;
	void setInputPeakHoldDuration(qreal v);
	void setPeakMeterType(enum obs_peak_meter_type peakMeterType);
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void wheelEvent(QWheelEvent *event) override;

protected:
	void paintEvent(QPaintEvent *event) override;
};

class VolumeMeterTimer : public QTimer {
	Q_OBJECT

public:
	inline VolumeMeterTimer() : QTimer() {}

	void AddVolControl(VolumeMeter *meter);
	void RemoveVolControl(VolumeMeter *meter);

protected:
	void timerEvent(QTimerEvent *event) override;
	QList<VolumeMeter *> volumeMeters;
};

class QLabel;
class QSlider;
class MuteCheckBox;

class VolControl : public QWidget {
	Q_OBJECT

private:
	OBSSource source;
	QLabel *nameLabel;
	QLabel *volLabel;
	VolumeMeter *volMeter;
	QSlider *slider;
	MuteCheckBox *mute;
	QPushButton *config = nullptr;
	float levelTotal;
	float levelCount;
	obs_fader_t *obs_fader;
	obs_volmeter_t *obs_volmeter;
	bool vertical;
	QMenu *contextMenu;

	static void OBSVolumeChanged(void *param, float db);
	static void OBSVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);
	static void OBSVolumeMuted(void *data, calldata_t *calldata);

	void EmitConfigClicked();

private slots:
	void VolumeChanged();
	void VolumeMuted(bool muted);

	void SetMuted(bool checked);
	void SliderChanged(int vol);
	void updateText();

signals:
	void ConfigClicked();

public:
	explicit VolControl(OBSSource source, bool showConfig = false,
			    bool vertical = false);
	~VolControl();

	inline obs_source_t *GetSource() const { return source; }

	QString GetName() const;
	void SetName(const QString &newName);

	void SetMeterDecayRate(qreal q);
	void setPeakMeterType(enum obs_peak_meter_type peakMeterType);

	void EnableSlider(bool enable);
	inline void SetContextMenu(QMenu *cm) { contextMenu = cm; }
};
