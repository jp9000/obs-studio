#pragma once

#include <obs.hpp>
#include <QDialog>
#include <vector>
#include <QCheckBox>
#include <QPointer>

class OBSAdvAudioCtrl;
class QGridLayout;

// "Basic advanced audio"?  ...

class OBSBasicAdvAudio : public QDialog {
	Q_OBJECT

private:
	QWidget *controlArea;
	QGridLayout *mainLayout;
	QPointer<QCheckBox> activeOnly;
	QPointer<QCheckBox> usePercent;
	OBSSignal sourceAddedSignal;
	OBSSignal sourceRemovedSignal;
	bool showInactive;
	bool showVisible;

	std::vector<OBSAdvAudioCtrl *> controls;

	inline void AddAudioSource(obs_source_t *source);

	static bool EnumSources(void *param, obs_source_t *source);

	static void OBSSourceAdded(void *param, calldata_t *calldata);
	static void OBSSourceRemoved(void *param, calldata_t *calldata);

public slots:
	void SourceAdded(OBSSource source);
	void SourceRemoved(OBSSource source);

	void SetVolumeType(bool percent);
	void ActiveOnlyChanged(bool checked);

public:
	OBSBasicAdvAudio(QWidget *parent);
	~OBSBasicAdvAudio();
	void SetShowInactive(bool showInactive);
	void SetIconsVisible(bool visible);
};
