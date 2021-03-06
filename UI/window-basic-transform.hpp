#pragma once

#include <obs.hpp>
#include <memory>

#include "ui_OBSBasicTransform.h"

class OBSBasic;

class OBSBasicTransform : public QDialog {
	Q_OBJECT

private:
	std::unique_ptr<Ui::OBSBasicTransform> ui;

	OBSBasic *main;
	OBSSceneItem item;
	OBSSignal channelChangedSignal;
	OBSSignal transformSignal;
	OBSSignal removeSignal;
	OBSSignal selectSignal;
	OBSSignal deselectSignal;

	bool ignoreTransformSignal = false;
	bool ignoreItemChange = false;

	void HookWidget(QWidget *widget, const char *signal, const char *slot);

	void SetScene(OBSScene scene);
	void SetItem(OBSSceneItem newItem);

	static void OBSChannelChanged(void *param, calldata_t *data);

	static void OBSSceneItemTransform(void *param, calldata_t *data);
	static void OBSSceneItemRemoved(void *param, calldata_t *data);
	static void OBSSceneItemSelect(void *param, calldata_t *data);
	static void OBSSceneItemDeselect(void *param, calldata_t *data);

private slots:
	void RefreshControls();
	void SetItemQt(OBSSceneItem newItem);
	void OnBoundsType(int index);
	void OnControlChanged();
	void OnCropChanged();
	void on_resetButton_clicked();

public:
	OBSBasicTransform(OBSBasic *parent);
};
