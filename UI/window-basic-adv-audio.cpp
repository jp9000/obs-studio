#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include "window-basic-adv-audio.hpp"
#include "window-basic-main.hpp"
#include "item-widget-helpers.hpp"
#include "adv-audio-control.hpp"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"

Q_DECLARE_METATYPE(OBSSource);

OBSBasicAdvAudio::OBSBasicAdvAudio(QWidget *parent)
	: QDialog(parent),
	  sourceAddedSignal(obs_get_signal_handler(), "source_activate",
			    OBSSourceAdded, this),
	  sourceRemovedSignal(obs_get_signal_handler(), "source_deactivate",
			      OBSSourceRemoved, this),
	  showInactive(false)
{
	QScrollArea *scrollArea;
	QVBoxLayout *vlayout;
	QWidget *widget;
	QLabel *label;

	QLabel *volLabel = new QLabel(QTStr("Basic.AdvAudio.Volume"));
	volLabel->setStyleSheet("font-weight: bold;");
	volLabel->setContentsMargins(0, 0, 6, 0);

	usePercent = new QCheckBox();
	usePercent->setStyleSheet("font-weight: bold;");
	usePercent->setText("%");
	connect(usePercent, SIGNAL(toggled(bool)), this,
		SLOT(SetVolumeType(bool)));

	VolumeType volType = (VolumeType)config_get_int(
		GetGlobalConfig(), "BasicWindow", "AdvAudioVolumeType");

	if (volType == VolumeType::Percent)
		usePercent->setChecked(true);

	QHBoxLayout *volLayout = new QHBoxLayout();
	volLayout->setContentsMargins(0, 0, 0, 0);
	volLayout->addWidget(volLabel);
	volLayout->addWidget(usePercent);
	volLayout->addStretch();

	int idx = 0;
	mainLayout = new QGridLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	label = new QLabel("");
	mainLayout->addWidget(label, 0, idx++);
	label = new QLabel(QTStr("Basic.AdvAudio.Name"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++);
	label = new QLabel(QTStr("Basic.Stats.Status"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++);
	mainLayout->addLayout(volLayout, 0, idx++);
	label = new QLabel(QTStr("Basic.AdvAudio.Mono"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++);
	label = new QLabel(QTStr("Basic.AdvAudio.Balance"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++);
	label = new QLabel(QTStr("Basic.AdvAudio.SyncOffset"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++);
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	label = new QLabel(QTStr("Basic.AdvAudio.Monitoring"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++);
#endif
	label = new QLabel(QTStr("Basic.AdvAudio.AudioTracks"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++);

	controlArea = new QWidget;
	controlArea->setLayout(mainLayout);
	controlArea->setSizePolicy(QSizePolicy::Preferred,
				   QSizePolicy::Preferred);

	vlayout = new QVBoxLayout;
	vlayout->addWidget(controlArea);
	//vlayout->setAlignment(controlArea, Qt::AlignTop);
	widget = new QWidget;
	widget->setLayout(vlayout);
	widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	scrollArea = new QScrollArea;
	scrollArea->setWidget(widget);
	scrollArea->setWidgetResizable(true);

	QPushButton *closeButton = new QPushButton(QTStr("Close"));

	activeOnly = new QCheckBox();
	activeOnly->setChecked(!showInactive);
	activeOnly->setText(QTStr("Basic.AdvAudio.ActiveOnly"));

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget(activeOnly);
	buttonLayout->addStretch();
	buttonLayout->addWidget(closeButton);

	vlayout = new QVBoxLayout;
	vlayout->setContentsMargins(11, 11, 11, 11);
	vlayout->addWidget(scrollArea);
	vlayout->addLayout(buttonLayout);
	setLayout(vlayout);

	connect(activeOnly, SIGNAL(clicked(bool)), this,
		SLOT(ActiveOnlyChanged(bool)));

	connect(closeButton, &QPushButton::clicked, [this]() { close(); });

	installEventFilter(CreateShortcutFilter());

	/* enum user scene/sources */
	obs_enum_sources(EnumSources, this);

	resize(1100, 340);
	setWindowTitle(QTStr("Basic.AdvAudio"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setSizeGripEnabled(true);
	setWindowModality(Qt::NonModal);
	setAttribute(Qt::WA_DeleteOnClose, true);

	setContextMenuPolicy(Qt::CustomContextMenu);

	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this,
		SLOT(ShowContextMenu(const QPoint &)));
}

OBSBasicAdvAudio::~OBSBasicAdvAudio()
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(parent());

	for (size_t i = 0; i < controls.size(); ++i)
		delete controls[i];

	main->SaveProject();
}

bool OBSBasicAdvAudio::EnumSources(void *param, obs_source_t *source)
{
	OBSBasicAdvAudio *dialog = reinterpret_cast<OBSBasicAdvAudio *>(param);
	uint32_t flags = obs_source_get_output_flags(source);

	if ((flags & OBS_SOURCE_AUDIO) != 0 &&
	    (dialog->showInactive || obs_source_active(source)))
		dialog->AddAudioSource(source);

	return true;
}

void OBSBasicAdvAudio::OBSSourceAdded(void *param, calldata_t *calldata)
{
	OBSSource source((obs_source_t *)calldata_ptr(calldata, "source"));

	QMetaObject::invokeMethod(reinterpret_cast<OBSBasicAdvAudio *>(param),
				  "SourceAdded", Q_ARG(OBSSource, source));
}

void OBSBasicAdvAudio::OBSSourceRemoved(void *param, calldata_t *calldata)
{
	OBSSource source((obs_source_t *)calldata_ptr(calldata, "source"));

	QMetaObject::invokeMethod(reinterpret_cast<OBSBasicAdvAudio *>(param),
				  "SourceRemoved", Q_ARG(OBSSource, source));
}

inline void OBSBasicAdvAudio::AddAudioSource(obs_source_t *source)
{
	for (size_t i = 0; i < controls.size(); i++) {
		if (controls[i]->GetSource() == source)
			return;
	}
	OBSAdvAudioCtrl *control = new OBSAdvAudioCtrl(mainLayout, source);

	InsertQObjectByName(controls, control);

	for (auto control : controls) {
		control->ShowAudioControl(mainLayout);
	}
}

void OBSBasicAdvAudio::SourceAdded(OBSSource source)
{
	uint32_t flags = obs_source_get_output_flags(source);

	if ((flags & OBS_SOURCE_AUDIO) == 0)
		return;

	AddAudioSource(source);
}

void OBSBasicAdvAudio::SourceRemoved(OBSSource source)
{
	uint32_t flags = obs_source_get_output_flags(source);

	if ((flags & OBS_SOURCE_AUDIO) == 0)
		return;

	for (size_t i = 0; i < controls.size(); i++) {
		if (controls[i]->GetSource() == source) {
			delete controls[i];
			controls.erase(controls.begin() + i);
			break;
		}
	}
}

void OBSBasicAdvAudio::SetVolumeType(bool percent)
{
	VolumeType type;

	if (percent)
		type = VolumeType::Percent;
	else
		type = VolumeType::dB;

	for (size_t i = 0; i < controls.size(); i++)
		controls[i]->SetVolumeWidget(type);

	config_set_int(GetGlobalConfig(), "BasicWindow", "AdvAudioVolumeType",
		       (int)type);
}

void OBSBasicAdvAudio::ActiveOnlyChanged(bool checked)
{
	SetShowInactive(!checked);
}

void OBSBasicAdvAudio::SetShowInactive(bool show)
{
	if (showInactive == show)
		return;

	showInactive = show;
	activeOnly->setChecked(!showInactive);
	sourceAddedSignal.Disconnect();
	sourceRemovedSignal.Disconnect();

	if (showInactive) {
		sourceAddedSignal.Connect(obs_get_signal_handler(),
					  "source_create", OBSSourceAdded,
					  this);
		sourceRemovedSignal.Connect(obs_get_signal_handler(),
					    "source_remove", OBSSourceRemoved,
					    this);

		obs_enum_sources(EnumSources, this);

		SetIconsVisible(showVisible);
	} else {
		sourceAddedSignal.Connect(obs_get_signal_handler(),
					  "source_activate", OBSSourceAdded,
					  this);
		sourceRemovedSignal.Connect(obs_get_signal_handler(),
					    "source_deactivate",
					    OBSSourceRemoved, this);

		for (size_t i = 0; i < controls.size(); i++) {
			const auto source = controls[i]->GetSource();
			if (!obs_source_active(source)) {
				delete controls[i];
				controls.erase(controls.begin() + i);
				i--;
			}
		}
	}
}

void OBSBasicAdvAudio::SetIconsVisible(bool visible)
{
	showVisible = visible;

	QLayoutItem *item = mainLayout->itemAtPosition(0, 0);
	QLabel *headerLabel = qobject_cast<QLabel *>(item->widget());
	visible ? headerLabel->show() : headerLabel->hide();

	for (size_t i = 0; i < controls.size(); i++) {
		controls[i]->SetIconVisible(visible);
	}
}
