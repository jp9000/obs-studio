#include <QFormLayout>
#include <QScrollBar>
#include <QLabel>
#include <QCheckBox>
#include <QFont>
#include <QFontDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QStandardItem>
#include <QFileDialog>
#include <QColorDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QMenu>
#include <QStackedWidget>
#include <QDir>
#include <QGroupBox>
#include "double-slider.hpp"
#include "slider-ignorewheel.hpp"
#include "spinbox-ignorewheel.hpp"
#include "combobox-ignorewheel.hpp"
#include "qt-wrappers.hpp"
#include "properties-view.hpp"
#include "properties-view.moc.hpp"
#include "obs-app.hpp"

#include <cstdlib>
#include <initializer_list>
#include <string>

using namespace std;

static inline QColor color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff,
		      (val >> 24) & 0xff);
}

static inline long long color_to_int(QColor color)
{
	auto shift = [&](unsigned val, int shift) {
		return ((val & 0xff) << shift);
	};

	return shift(color.red(), 0) | shift(color.green(), 8) |
	       shift(color.blue(), 16) | shift(color.alpha(), 24);
}

namespace {

struct frame_rate_tag {
	enum tag_type {
		SIMPLE,
		RATIONAL,
		USER,
	} type = SIMPLE;
	const char *val = nullptr;

	frame_rate_tag() = default;

	explicit frame_rate_tag(tag_type type) : type(type) {}

	explicit frame_rate_tag(const char *val) : type(USER), val(val) {}

	static frame_rate_tag simple() { return frame_rate_tag{SIMPLE}; }
	static frame_rate_tag rational() { return frame_rate_tag{RATIONAL}; }
};

struct common_frame_rate {
	const char *fps_name;
	media_frames_per_second fps;
};

}

Q_DECLARE_METATYPE(frame_rate_tag);
Q_DECLARE_METATYPE(media_frames_per_second);

void OBSPropertiesView::ReloadProperties()
{
	if (obj) {
		properties.reset(reloadCallback(obj));
	} else {
		properties.reset(reloadCallback((void *)type.c_str()));
		obs_properties_apply_settings(properties.get(), settings);
	}

	uint32_t flags = obs_properties_get_flags(properties.get());
	deferUpdate = (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;

	RefreshProperties();
}

#define NO_PROPERTIES_STRING QTStr("Basic.PropertiesWindow.NoProperties")

void OBSPropertiesView::RefreshProperties()
{
	int h, v;
	GetScrollPos(h, v);

	children.clear();
	if (widget)
		widget->deleteLater();

	widget = new QWidget();

	QFormLayout *layout = new QFormLayout;
	layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	widget->setLayout(layout);

	QSizePolicy mainPolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	layout->setLabelAlignment(Qt::AlignRight);

	obs_property_t *property = obs_properties_first(properties.get());
	bool hasNoProperties = !property;

	while (property) {
		AddProperty(property, layout);
		obs_property_next(&property);
	}

	setWidgetResizable(true);
	setWidget(widget);
	SetScrollPos(h, v);
	setSizePolicy(mainPolicy);

	lastFocused.clear();
	if (lastWidget) {
		lastWidget->setFocus(Qt::OtherFocusReason);
		lastWidget = nullptr;
	}

	if (hasNoProperties) {
		QLabel *noPropertiesLabel = new QLabel(NO_PROPERTIES_STRING);
		layout->addWidget(noPropertiesLabel);
	}

	emit PropertiesRefreshed();
}

void OBSPropertiesView::SetScrollPos(int h, int v)
{
	QScrollBar *scroll = horizontalScrollBar();
	if (scroll)
		scroll->setValue(h);

	scroll = verticalScrollBar();
	if (scroll)
		scroll->setValue(v);
}

void OBSPropertiesView::GetScrollPos(int &h, int &v)
{
	h = v = 0;

	QScrollBar *scroll = horizontalScrollBar();
	if (scroll)
		h = scroll->value();

	scroll = verticalScrollBar();
	if (scroll)
		v = scroll->value();
}

OBSPropertiesView::OBSPropertiesView(OBSData settings_, void *obj_,
				     PropertiesReloadCallback reloadCallback,
				     PropertiesUpdateCallback callback_,
				     int minSize_)
	: VScrollArea(nullptr),
	  properties(nullptr, obs_properties_destroy),
	  settings(settings_),
	  obj(obj_),
	  reloadCallback(reloadCallback),
	  callback(callback_),
	  minSize(minSize_)
{
	setFrameShape(QFrame::NoFrame);
	ReloadProperties();
}

OBSPropertiesView::OBSPropertiesView(OBSData settings_, const char *type_,
				     PropertiesReloadCallback reloadCallback_,
				     int minSize_)
	: VScrollArea(nullptr),
	  properties(nullptr, obs_properties_destroy),
	  settings(settings_),
	  type(type_),
	  reloadCallback(reloadCallback_),
	  minSize(minSize_)
{
	setFrameShape(QFrame::NoFrame);
	ReloadProperties();
}

void OBSPropertiesView::resizeEvent(QResizeEvent *event)
{
	emit PropertiesResized();
	VScrollArea::resizeEvent(event);
}

QWidget *OBSPropertiesView::NewWidget(obs_property_t *prop, QWidget *widget,
				      const char *signal)
{
	const char *long_desc = obs_property_long_description(prop);

	WidgetInfo *info = new WidgetInfo(this, prop, widget);
	connect(widget, signal, info, SLOT(ControlChanged()));
	children.emplace_back(info);

	widget->setToolTip(QT_UTF8(long_desc));
	return widget;
}

QWidget *OBSPropertiesView::AddCheckbox(obs_property_t *prop)
{
	const char *name = obs_property_name(prop);
	const char *desc = obs_property_description(prop);
	bool val = obs_data_get_bool(settings, name);

	QCheckBox *checkbox = new QCheckBox(QT_UTF8(desc));
	checkbox->setCheckState(val ? Qt::Checked : Qt::Unchecked);
	return NewWidget(prop, checkbox, SIGNAL(stateChanged(int)));
}

QWidget *OBSPropertiesView::AddText(obs_property_t *prop, QFormLayout *layout,
				    QLabel *&label)
{
	const char *name = obs_property_name(prop);
	const char *val = obs_data_get_string(settings, name);
	const bool monospace = obs_property_text_monospace(prop);
	obs_text_type type = obs_property_text_type(prop);

	if (type == OBS_TEXT_MULTILINE) {
		QPlainTextEdit *edit = new QPlainTextEdit(QT_UTF8(val));
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
		edit->setTabStopDistance(40);
#else
		edit->setTabStopWidth(40);
#endif
		if (monospace) {
			QFont f("Courier");
			f.setStyleHint(QFont::Monospace);
			edit->setFont(f);
		}
		return NewWidget(prop, edit, SIGNAL(textChanged()));

	} else if (type == OBS_TEXT_PASSWORD) {
		QLayout *subLayout = new QHBoxLayout();
		QLineEdit *edit = new QLineEdit();
		QPushButton *show = new QPushButton();

		show->setText(QTStr("Show"));
		show->setCheckable(true);
		edit->setText(QT_UTF8(val));
		edit->setEchoMode(QLineEdit::Password);

		subLayout->addWidget(edit);
		subLayout->addWidget(show);

		WidgetInfo *info = new WidgetInfo(this, prop, edit);
		connect(show, &QAbstractButton::toggled, info,
			&WidgetInfo::TogglePasswordText);
		connect(show, &QAbstractButton::toggled, [=](bool hide) {
			show->setText(hide ? QTStr("Hide") : QTStr("Show"));
		});
		children.emplace_back(info);

		label = new QLabel(QT_UTF8(obs_property_description(prop)));
		layout->addRow(label, subLayout);

		edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));

		connect(edit, SIGNAL(textEdited(const QString &)), info,
			SLOT(ControlChanged()));
		return nullptr;
	}

	QLineEdit *edit = new QLineEdit();

	edit->setText(QT_UTF8(val));
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	return NewWidget(prop, edit, SIGNAL(textEdited(const QString &)));
}

void OBSPropertiesView::AddPath(obs_property_t *prop, QFormLayout *layout,
				QLabel **label)
{
	const char *name = obs_property_name(prop);
	const char *val = obs_data_get_string(settings, name);
	QLayout *subLayout = new QHBoxLayout();
	QLineEdit *edit = new QLineEdit();
	QPushButton *button = new QPushButton(QTStr("Browse"));

	if (!obs_property_enabled(prop)) {
		edit->setEnabled(false);
		button->setEnabled(false);
	}

	button->setProperty("themeID", "settingsButtons");
	edit->setText(QT_UTF8(val));
	edit->setReadOnly(true);
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	subLayout->addWidget(edit);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, edit);
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(*label, subLayout);
}

void OBSPropertiesView::AddInt(obs_property_t *prop, QFormLayout *layout,
			       QLabel **label)
{
	obs_number_type type = obs_property_int_type(prop);
	QLayout *subLayout = new QHBoxLayout();

	const char *name = obs_property_name(prop);
	int val = (int)obs_data_get_int(settings, name);
	QSpinBox *spin = new SpinBoxIgnoreScroll();

	spin->setEnabled(obs_property_enabled(prop));

	int minVal = obs_property_int_min(prop);
	int maxVal = obs_property_int_max(prop);
	int stepVal = obs_property_int_step(prop);
	const char *suffix = obs_property_int_suffix(prop);

	spin->setMinimum(minVal);
	spin->setMaximum(maxVal);
	spin->setSingleStep(stepVal);
	spin->setValue(val);
	spin->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	spin->setSuffix(QT_UTF8(suffix));

	WidgetInfo *info = new WidgetInfo(this, prop, spin);
	children.emplace_back(info);

	if (type == OBS_NUMBER_SLIDER) {
		QSlider *slider = new SliderIgnoreScroll();
		slider->setMinimum(minVal);
		slider->setMaximum(maxVal);
		slider->setPageStep(stepVal);
		slider->setValue(val);
		slider->setOrientation(Qt::Horizontal);
		slider->setEnabled(obs_property_enabled(prop));
		subLayout->addWidget(slider);

		connect(slider, SIGNAL(valueChanged(int)), spin,
			SLOT(setValue(int)));
		connect(spin, SIGNAL(valueChanged(int)), slider,
			SLOT(setValue(int)));
	}

	connect(spin, SIGNAL(valueChanged(int)), info, SLOT(ControlChanged()));

	subLayout->addWidget(spin);

	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(*label, subLayout);
}

void OBSPropertiesView::AddFloat(obs_property_t *prop, QFormLayout *layout,
				 QLabel **label)
{
	obs_number_type type = obs_property_float_type(prop);
	QLayout *subLayout = new QHBoxLayout();

	const char *name = obs_property_name(prop);
	double val = obs_data_get_double(settings, name);
	QDoubleSpinBox *spin = new QDoubleSpinBox();

	if (!obs_property_enabled(prop))
		spin->setEnabled(false);

	double minVal = obs_property_float_min(prop);
	double maxVal = obs_property_float_max(prop);
	double stepVal = obs_property_float_step(prop);
	const char *suffix = obs_property_float_suffix(prop);

	spin->setMinimum(minVal);
	spin->setMaximum(maxVal);
	spin->setSingleStep(stepVal);
	spin->setValue(val);
	spin->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	spin->setSuffix(QT_UTF8(suffix));

	WidgetInfo *info = new WidgetInfo(this, prop, spin);
	children.emplace_back(info);

	if (type == OBS_NUMBER_SLIDER) {
		DoubleSlider *slider = new DoubleSlider();
		slider->setDoubleConstraints(minVal, maxVal, stepVal, val);
		slider->setOrientation(Qt::Horizontal);
		subLayout->addWidget(slider);

		connect(slider, SIGNAL(doubleValChanged(double)), spin,
			SLOT(setValue(double)));
		connect(spin, SIGNAL(valueChanged(double)), slider,
			SLOT(setDoubleVal(double)));
	}

	connect(spin, SIGNAL(valueChanged(double)), info,
		SLOT(ControlChanged()));

	subLayout->addWidget(spin);

	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(*label, subLayout);
}

static void AddComboItem(QComboBox *combo, obs_property_t *prop,
			 obs_combo_format format, size_t idx)
{
	const char *name = obs_property_list_item_name(prop, idx);
	QVariant var;

	if (format == OBS_COMBO_FORMAT_INT) {
		long long val = obs_property_list_item_int(prop, idx);
		var = QVariant::fromValue<long long>(val);

	} else if (format == OBS_COMBO_FORMAT_FLOAT) {
		double val = obs_property_list_item_float(prop, idx);
		var = QVariant::fromValue<double>(val);

	} else if (format == OBS_COMBO_FORMAT_STRING) {
		var = QByteArray(obs_property_list_item_string(prop, idx));
	}

	combo->addItem(QT_UTF8(name), var);

	if (!obs_property_list_item_disabled(prop, idx))
		return;

	int index = combo->findText(QT_UTF8(name));
	if (index < 0)
		return;

	QStandardItemModel *model =
		dynamic_cast<QStandardItemModel *>(combo->model());
	if (!model)
		return;

	QStandardItem *item = model->item(index);
	item->setFlags(Qt::NoItemFlags);
}

template<long long get_int(obs_data_t *, const char *),
	 double get_double(obs_data_t *, const char *),
	 const char *get_string(obs_data_t *, const char *)>
static string from_obs_data(obs_data_t *data, const char *name,
			    obs_combo_format format)
{
	switch (format) {
	case OBS_COMBO_FORMAT_INT:
		return to_string(get_int(data, name));
	case OBS_COMBO_FORMAT_FLOAT:
		return to_string(get_double(data, name));
	case OBS_COMBO_FORMAT_STRING:
		return get_string(data, name);
	default:
		return "";
	}
}

static string from_obs_data(obs_data_t *data, const char *name,
			    obs_combo_format format)
{
	return from_obs_data<obs_data_get_int, obs_data_get_double,
			     obs_data_get_string>(data, name, format);
}

static string from_obs_data_autoselect(obs_data_t *data, const char *name,
				       obs_combo_format format)
{
	return from_obs_data<obs_data_get_autoselect_int,
			     obs_data_get_autoselect_double,
			     obs_data_get_autoselect_string>(data, name,
							     format);
}

QWidget *OBSPropertiesView::AddList(obs_property_t *prop, bool &warning)
{
	const char *name = obs_property_name(prop);
	QComboBox *combo = new ComboBoxIgnoreScroll();
	obs_combo_type type = obs_property_list_type(prop);
	obs_combo_format format = obs_property_list_format(prop);
	size_t count = obs_property_list_item_count(prop);
	int idx = -1;

	for (size_t i = 0; i < count; i++)
		AddComboItem(combo, prop, format, i);

	if (type == OBS_COMBO_TYPE_EDITABLE)
		combo->setEditable(true);

	combo->setMaxVisibleItems(40);
	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	string value = from_obs_data(settings, name, format);

	if (format == OBS_COMBO_FORMAT_STRING &&
	    type == OBS_COMBO_TYPE_EDITABLE) {
		combo->lineEdit()->setText(QT_UTF8(value.c_str()));
	} else {
		idx = combo->findData(QByteArray(value.c_str()));
	}

	if (type == OBS_COMBO_TYPE_EDITABLE)
		return NewWidget(prop, combo,
				 SIGNAL(editTextChanged(const QString &)));

	if (idx != -1)
		combo->setCurrentIndex(idx);

	if (obs_data_has_autoselect_value(settings, name)) {
		string autoselect =
			from_obs_data_autoselect(settings, name, format);
		int id = combo->findData(QT_UTF8(autoselect.c_str()));

		if (id != -1 && id != idx) {
			QString actual = combo->itemText(id);
			QString selected = combo->itemText(idx);
			QString combined = QTStr(
				"Basic.PropertiesWindow.AutoSelectFormat");
			combo->setItemText(idx,
					   combined.arg(selected).arg(actual));
		}
	}

	QAbstractItemModel *model = combo->model();
	warning = idx != -1 &&
		  model->flags(model->index(idx, 0)) == Qt::NoItemFlags;

	WidgetInfo *info = new WidgetInfo(this, prop, combo);
	connect(combo, SIGNAL(currentIndexChanged(int)), info,
		SLOT(ControlChanged()));
	children.emplace_back(info);

	/* trigger a settings update if the index was not found */
	if (idx == -1)
		info->ControlChanged();

	return combo;
}

static void NewButton(QLayout *layout, WidgetInfo *info, const char *themeIcon,
		      void (WidgetInfo::*method)())
{
	QPushButton *button = new QPushButton();
	button->setProperty("themeID", themeIcon);
	button->setFlat(true);
	button->setMaximumSize(22, 22);
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	QObject::connect(button, &QPushButton::clicked, info, method);

	layout->addWidget(button);
}

void OBSPropertiesView::AddEditableList(obs_property_t *prop,
					QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_array_t *array = obs_data_get_array(settings, name);
	QListWidget *list = new QListWidget();
	size_t count = obs_data_array_count(array);

	if (!obs_property_enabled(prop))
		list->setEnabled(false);

	list->setSortingEnabled(false);
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		list->addItem(QT_UTF8(obs_data_get_string(item, "value")));
		QListWidgetItem *const list_item = list->item((int)i);
		list_item->setSelected(obs_data_get_bool(item, "selected"));
		list_item->setHidden(obs_data_get_bool(item, "hidden"));
		obs_data_release(item);
	}

	WidgetInfo *info = new WidgetInfo(this, prop, list);

	list->setDragDropMode(QAbstractItemView::InternalMove);
	connect(list->model(),
		SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
		info,
		SLOT(EditListReordered(const QModelIndex &, int, int,
				       const QModelIndex &, int)));

	QVBoxLayout *sideLayout = new QVBoxLayout();
	NewButton(sideLayout, info, "addIconSmall", &WidgetInfo::EditListAdd);
	NewButton(sideLayout, info, "removeIconSmall",
		  &WidgetInfo::EditListRemove);
	NewButton(sideLayout, info, "configIconSmall",
		  &WidgetInfo::EditListEdit);
	NewButton(sideLayout, info, "upArrowIconSmall",
		  &WidgetInfo::EditListUp);
	NewButton(sideLayout, info, "downArrowIconSmall",
		  &WidgetInfo::EditListDown);
	sideLayout->addStretch(0);

	QHBoxLayout *subLayout = new QHBoxLayout();
	subLayout->addWidget(list);
	subLayout->addLayout(sideLayout);

	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(label, subLayout);

	obs_data_array_release(array);
}

QWidget *OBSPropertiesView::AddButton(obs_property_t *prop)
{
	const char *desc = obs_property_description(prop);

	QPushButton *button = new QPushButton(QT_UTF8(desc));
	button->setProperty("themeID", "settingsButtons");
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	return NewWidget(prop, button, SIGNAL(clicked()));
}

void OBSPropertiesView::AddColorInternal(obs_property_t *prop,
					 QFormLayout *layout, QLabel *&label,
					 bool supportAlpha)
{
	QPushButton *button = new QPushButton;
	QLabel *colorLabel = new QLabel;
	const char *name = obs_property_name(prop);
	long long val = obs_data_get_int(settings, name);
	QColor color = color_from_int(val);
	QColor::NameFormat format;

	if (!obs_property_enabled(prop)) {
		button->setEnabled(false);
		colorLabel->setEnabled(false);
	}

	button->setProperty("themeID", "settingsButtons");
	button->setText(QTStr("Basic.PropertiesWindow.SelectColor"));
	button->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	if (supportAlpha) {
		format = QColor::HexArgb;
	} else {
		format = QColor::HexRgb;
		color.setAlpha(255);
	}

	QPalette palette = QPalette(color);
	colorLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	colorLabel->setText(color.name(format));
	colorLabel->setPalette(palette);
	colorLabel->setStyleSheet(
		QString("background-color :%1; color: %2;")
			.arg(palette.color(QPalette::Window).name(format))
			.arg(palette.color(QPalette::WindowText).name(format)));
	colorLabel->setAutoFillBackground(true);
	colorLabel->setAlignment(Qt::AlignCenter);
	colorLabel->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	QHBoxLayout *subLayout = new QHBoxLayout;
	subLayout->setContentsMargins(0, 0, 0, 0);

	subLayout->addWidget(colorLabel);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, colorLabel);
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(label, subLayout);
}

void OBSPropertiesView::AddColor(obs_property_t *prop, QFormLayout *layout,
				 QLabel *&label)
{
	AddColorInternal(prop, layout, label, false);
}

void OBSPropertiesView::AddColorAlpha(obs_property_t *prop, QFormLayout *layout,
				      QLabel *&label)
{
	AddColorInternal(prop, layout, label, true);
}

void MakeQFont(obs_data_t *font_obj, QFont &font, bool limit = false)
{
	const char *face = obs_data_get_string(font_obj, "face");
	const char *style = obs_data_get_string(font_obj, "style");
	int size = (int)obs_data_get_int(font_obj, "size");
	uint32_t flags = (uint32_t)obs_data_get_int(font_obj, "flags");

	if (face) {
		font.setFamily(face);
		font.setStyleName(style);
	}

	if (size) {
		if (limit) {
			int max_size = font.pointSize();
			if (max_size < 28)
				max_size = 28;
			if (size > max_size)
				size = max_size;
		}
		font.setPointSize(size);
	}

	if (flags & OBS_FONT_BOLD)
		font.setBold(true);
	if (flags & OBS_FONT_ITALIC)
		font.setItalic(true);
	if (flags & OBS_FONT_UNDERLINE)
		font.setUnderline(true);
	if (flags & OBS_FONT_STRIKEOUT)
		font.setStrikeOut(true);
}

void OBSPropertiesView::AddFont(obs_property_t *prop, QFormLayout *layout,
				QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *font_obj = obs_data_get_obj(settings, name);
	const char *face = obs_data_get_string(font_obj, "face");
	const char *style = obs_data_get_string(font_obj, "style");
	QPushButton *button = new QPushButton;
	QLabel *fontLabel = new QLabel;
	QFont font;

	if (!obs_property_enabled(prop)) {
		button->setEnabled(false);
		fontLabel->setEnabled(false);
	}

	font = fontLabel->font();
	MakeQFont(font_obj, font, true);

	button->setProperty("themeID", "settingsButtons");
	button->setText(QTStr("Basic.PropertiesWindow.SelectFont"));
	button->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	fontLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	fontLabel->setFont(font);
	fontLabel->setText(QString("%1 %2").arg(face, style));
	fontLabel->setAlignment(Qt::AlignCenter);
	fontLabel->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	QHBoxLayout *subLayout = new QHBoxLayout;
	subLayout->setContentsMargins(0, 0, 0, 0);

	subLayout->addWidget(fontLabel);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, fontLabel);
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(label, subLayout);

	obs_data_release(font_obj);
}

namespace std {

template<> struct default_delete<obs_data_t> {
	void operator()(obs_data_t *data) { obs_data_release(data); }
};

template<> struct default_delete<obs_data_item_t> {
	void operator()(obs_data_item_t *item) { obs_data_item_release(&item); }
};

}

template<typename T> static double make_epsilon(T val)
{
	return val * 0.00001;
}

static bool matches_range(media_frames_per_second &match,
			  media_frames_per_second fps,
			  const frame_rate_range_t &pair)
{
	auto val = media_frames_per_second_to_frame_interval(fps);
	auto max_ = media_frames_per_second_to_frame_interval(pair.first);
	auto min_ = media_frames_per_second_to_frame_interval(pair.second);

	if (min_ <= val && val <= max_) {
		match = fps;
		return true;
	}

	return false;
}

static bool matches_ranges(media_frames_per_second &best_match,
			   media_frames_per_second fps,
			   const frame_rate_ranges_t &fps_ranges,
			   bool exact = false)
{
	auto convert_fn = media_frames_per_second_to_frame_interval;
	auto val = convert_fn(fps);
	auto epsilon = make_epsilon(val);

	bool match = false;
	auto best_dist = numeric_limits<double>::max();
	for (auto &pair : fps_ranges) {
		auto max_ = convert_fn(pair.first);
		auto min_ = convert_fn(pair.second);
		/*blog(LOG_INFO, "%lg ≤ %lg ≤ %lg? %s %s %s",
				min_, val, max_,
				fabsl(min_ - val) < epsilon ? "true" : "false",
				min_ <= val && val <= max_  ? "true" : "false",
				fabsl(min_ - val) < epsilon ? "true" :
				"false");*/

		if (matches_range(best_match, fps, pair))
			return true;

		if (exact)
			continue;

		auto min_dist = fabsl(min_ - val);
		auto max_dist = fabsl(max_ - val);
		if (min_dist < epsilon && min_dist < best_dist) {
			best_match = pair.first;
			match = true;
			continue;
		}

		if (max_dist < epsilon && max_dist < best_dist) {
			best_match = pair.second;
			match = true;
			continue;
		}
	}

	return match;
}

static media_frames_per_second make_fps(uint32_t num, uint32_t den)
{
	media_frames_per_second fps{};
	fps.numerator = num;
	fps.denominator = den;
	return fps;
}

static const common_frame_rate common_fps[] = {
	{"60", {60, 1}}, {"59.94", {60000, 1001}}, {"50", {50, 1}},
	{"48", {48, 1}}, {"30", {30, 1}},          {"29.97", {30000, 1001}},
	{"25", {25, 1}}, {"24", {24, 1}},          {"23.976", {24000, 1001}},
};

static void UpdateSimpleFPSSelection(OBSFrameRatePropertyWidget *fpsProps,
				     const media_frames_per_second *current_fps)
{
	if (!current_fps || !media_frames_per_second_is_valid(*current_fps)) {
		fpsProps->simpleFPS->setCurrentIndex(0);
		return;
	}

	auto combo = fpsProps->simpleFPS;
	auto num = combo->count();
	for (int i = 0; i < num; i++) {
		auto variant = combo->itemData(i);
		if (!variant.canConvert<media_frames_per_second>())
			continue;

		auto fps = variant.value<media_frames_per_second>();
		if (fps != *current_fps)
			continue;

		combo->setCurrentIndex(i);
		return;
	}

	combo->setCurrentIndex(0);
}

static void AddFPSRanges(vector<common_frame_rate> &items,
			 const frame_rate_ranges_t &ranges)
{
	auto InsertFPS = [&](media_frames_per_second fps) {
		auto fps_val = media_frames_per_second_to_fps(fps);

		auto end_ = end(items);
		auto i = begin(items);
		for (; i != end_; i++) {
			auto i_fps_val = media_frames_per_second_to_fps(i->fps);
			if (fabsl(i_fps_val - fps_val) < 0.01)
				return;

			if (i_fps_val > fps_val)
				continue;

			break;
		}

		items.insert(i, {nullptr, fps});
	};

	for (auto &range : ranges) {
		InsertFPS(range.first);
		InsertFPS(range.second);
	}
}

static QWidget *
CreateSimpleFPSValues(OBSFrameRatePropertyWidget *fpsProps, bool &selected,
		      const media_frames_per_second *current_fps)
{
	auto widget = new QWidget{};
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto layout = new QVBoxLayout{};
	layout->setContentsMargins(0, 0, 0, 0);

	auto items = vector<common_frame_rate>{};
	items.reserve(sizeof(common_fps) / sizeof(common_frame_rate));

	auto combo = fpsProps->simpleFPS = new ComboBoxIgnoreScroll{};

	combo->addItem("", QVariant::fromValue(make_fps(0, 0)));
	for (const auto &fps : common_fps) {
		media_frames_per_second best_match{};
		if (!matches_ranges(best_match, fps.fps, fpsProps->fps_ranges))
			continue;

		items.push_back({fps.fps_name, best_match});
	}

	AddFPSRanges(items, fpsProps->fps_ranges);

	for (const auto &item : items) {
		auto var = QVariant::fromValue(item.fps);
		auto name = item.fps_name
				    ? QString(item.fps_name)
				    : QString("%1").arg(
					      media_frames_per_second_to_fps(
						      item.fps));
		combo->addItem(name, var);

		bool select = current_fps && *current_fps == item.fps;
		if (select) {
			combo->setCurrentIndex(combo->count() - 1);
			selected = true;
		}
	}

	layout->addWidget(combo, 0, Qt::AlignTop);
	widget->setLayout(layout);

	return widget;
}

static void UpdateRationalFPSWidgets(OBSFrameRatePropertyWidget *fpsProps,
				     const media_frames_per_second *current_fps)
{
	if (!current_fps || !media_frames_per_second_is_valid(*current_fps)) {
		fpsProps->numEdit->setValue(0);
		fpsProps->denEdit->setValue(0);
		return;
	}

	auto combo = fpsProps->fpsRange;
	auto num = combo->count();
	for (int i = 0; i < num; i++) {
		auto variant = combo->itemData(i);
		if (!variant.canConvert<size_t>())
			continue;

		auto idx = variant.value<size_t>();
		if (fpsProps->fps_ranges.size() < idx)
			continue;

		media_frames_per_second match{};
		if (!matches_range(match, *current_fps,
				   fpsProps->fps_ranges[idx]))
			continue;

		combo->setCurrentIndex(i);
		break;
	}

	fpsProps->numEdit->setValue(current_fps->numerator);
	fpsProps->denEdit->setValue(current_fps->denominator);
}

static QWidget *CreateRationalFPS(OBSFrameRatePropertyWidget *fpsProps,
				  bool &selected,
				  const media_frames_per_second *current_fps)
{
	auto widget = new QWidget{};
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto layout = new QFormLayout{};
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	auto str = QTStr("Basic.PropertiesView.FPS.ValidFPSRanges");
	auto rlabel = new QLabel{str};

	auto combo = fpsProps->fpsRange = new ComboBoxIgnoreScroll{};
	auto convert_fps = media_frames_per_second_to_fps;
	//auto convert_fi  = media_frames_per_second_to_frame_interval;

	for (size_t i = 0; i < fpsProps->fps_ranges.size(); i++) {
		auto &pair = fpsProps->fps_ranges[i];
		combo->addItem(QString{"%1 - %2"}
				       .arg(convert_fps(pair.first))
				       .arg(convert_fps(pair.second)),
			       QVariant::fromValue(i));

		media_frames_per_second match;
		if (!current_fps || !matches_range(match, *current_fps, pair))
			continue;

		combo->setCurrentIndex(combo->count() - 1);
		selected = true;
	}

	layout->addRow(rlabel, combo);

	auto num_edit = fpsProps->numEdit = new SpinBoxIgnoreScroll{};
	auto den_edit = fpsProps->denEdit = new SpinBoxIgnoreScroll{};

	num_edit->setRange(0, INT_MAX);
	den_edit->setRange(0, INT_MAX);

	if (current_fps) {
		num_edit->setValue(current_fps->numerator);
		den_edit->setValue(current_fps->denominator);
	}

	layout->addRow(QTStr("Basic.Settings.Video.Numerator"), num_edit);
	layout->addRow(QTStr("Basic.Settings.Video.Denominator"), den_edit);

	widget->setLayout(layout);

	return widget;
}

static OBSFrameRatePropertyWidget *
CreateFrameRateWidget(obs_property_t *prop, bool &warning, const char *option,
		      media_frames_per_second *current_fps,
		      frame_rate_ranges_t &fps_ranges)
{
	auto widget = new OBSFrameRatePropertyWidget{};
	auto hlayout = new QHBoxLayout{};
	hlayout->setContentsMargins(0, 0, 0, 0);

	swap(widget->fps_ranges, fps_ranges);

	auto combo = widget->modeSelect = new ComboBoxIgnoreScroll{};
	combo->addItem(QTStr("Basic.PropertiesView.FPS.Simple"),
		       QVariant::fromValue(frame_rate_tag::simple()));
	combo->addItem(QTStr("Basic.PropertiesView.FPS.Rational"),
		       QVariant::fromValue(frame_rate_tag::rational()));

	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	auto num = obs_property_frame_rate_options_count(prop);
	if (num)
		combo->insertSeparator(combo->count());

	bool option_found = false;
	for (size_t i = 0; i < num; i++) {
		auto name = obs_property_frame_rate_option_name(prop, i);
		auto desc = obs_property_frame_rate_option_description(prop, i);
		combo->addItem(desc, QVariant::fromValue(frame_rate_tag{name}));

		if (!name || !option || string(name) != option)
			continue;

		option_found = true;
		combo->setCurrentIndex(combo->count() - 1);
	}

	hlayout->addWidget(combo, 0, Qt::AlignTop);

	auto stack = widget->modeDisplay = new QStackedWidget{};

	bool match_found = option_found;
	auto AddWidget = [&](decltype(CreateRationalFPS) func) {
		bool selected = false;
		stack->addWidget(func(widget, selected, current_fps));

		if (match_found || !selected)
			return;

		match_found = true;

		stack->setCurrentIndex(stack->count() - 1);
		combo->setCurrentIndex(stack->count() - 1);
	};

	AddWidget(CreateSimpleFPSValues);
	AddWidget(CreateRationalFPS);
	stack->addWidget(new QWidget{});

	if (option_found)
		stack->setCurrentIndex(stack->count() - 1);
	else if (!match_found) {
		int idx = current_fps ? 1 : 0; // Rational for "unsupported"
					       // Simple as default
		stack->setCurrentIndex(idx);
		combo->setCurrentIndex(idx);
		warning = true;
	}

	hlayout->addWidget(stack, 0, Qt::AlignTop);

	auto label_area = widget->labels = new QWidget{};
	label_area->setSizePolicy(QSizePolicy::Expanding,
				  QSizePolicy::Expanding);

	auto vlayout = new QVBoxLayout{};
	vlayout->setContentsMargins(0, 0, 0, 0);

	auto fps_label = widget->currentFPS = new QLabel{"FPS: 22"};
	auto time_label = widget->timePerFrame =
		new QLabel{"Frame Interval: 0.123 ms"};
	auto min_label = widget->minLabel = new QLabel{"Min FPS: 1/1"};
	auto max_label = widget->maxLabel = new QLabel{"Max FPS: 2/1"};

	min_label->setHidden(true);
	max_label->setHidden(true);

	auto flags = Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard;
	min_label->setTextInteractionFlags(flags);
	max_label->setTextInteractionFlags(flags);

	vlayout->addWidget(fps_label);
	vlayout->addWidget(time_label);
	vlayout->addWidget(min_label);
	vlayout->addWidget(max_label);
	label_area->setLayout(vlayout);

	hlayout->addWidget(label_area, 0, Qt::AlignTop);

	widget->setLayout(hlayout);

	return widget;
}

static void UpdateMinMaxLabels(OBSFrameRatePropertyWidget *w)
{
	auto Hide = [&](bool hide) {
		w->minLabel->setHidden(hide);
		w->maxLabel->setHidden(hide);
	};

	auto variant = w->modeSelect->currentData();
	if (!variant.canConvert<frame_rate_tag>() ||
	    variant.value<frame_rate_tag>().type != frame_rate_tag::RATIONAL) {
		Hide(true);
		return;
	}

	variant = w->fpsRange->currentData();
	if (!variant.canConvert<size_t>()) {
		Hide(true);
		return;
	}

	auto idx = variant.value<size_t>();
	if (idx >= w->fps_ranges.size()) {
		Hide(true);
		return;
	}

	Hide(false);

	auto min = w->fps_ranges[idx].first;
	auto max = w->fps_ranges[idx].second;

	w->minLabel->setText(QString("Min FPS: %1/%2")
				     .arg(min.numerator)
				     .arg(min.denominator));
	w->maxLabel->setText(QString("Max FPS: %1/%2")
				     .arg(max.numerator)
				     .arg(max.denominator));
}

static void UpdateFPSLabels(OBSFrameRatePropertyWidget *w)
{
	UpdateMinMaxLabels(w);

	unique_ptr<obs_data_item_t> obj{
		obs_data_item_byname(w->settings, w->name)};

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_autoselect_frames_per_second(obj.get(), &fps,
							   nullptr) ||
	    obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	if (!valid_fps) {
		w->currentFPS->setHidden(true);
		w->timePerFrame->setHidden(true);
		if (!option)
			w->warningLabel->setStyleSheet(
				"QLabel { color: red; }");

		return;
	}

	w->currentFPS->setHidden(false);
	w->timePerFrame->setHidden(false);

	media_frames_per_second match{};
	if (!option && !matches_ranges(match, *valid_fps, w->fps_ranges, true))
		w->warningLabel->setStyleSheet("QLabel { color: red; }");
	else
		w->warningLabel->setStyleSheet("");

	auto convert_to_fps = media_frames_per_second_to_fps;
	auto convert_to_frame_interval =
		media_frames_per_second_to_frame_interval;

	w->currentFPS->setText(
		QString("FPS: %1").arg(convert_to_fps(*valid_fps)));
	w->timePerFrame->setText(
		QString("Frame Interval: %1 ms")
			.arg(convert_to_frame_interval(*valid_fps) * 1000));
}

void OBSPropertiesView::AddFrameRate(obs_property_t *prop, bool &warning,
				     QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	bool enabled = obs_property_enabled(prop);
	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(settings, name)};

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	frame_rate_ranges_t fps_ranges;
	size_t num = obs_property_frame_rate_fps_ranges_count(prop);
	fps_ranges.reserve(num);
	for (size_t i = 0; i < num; i++)
		fps_ranges.emplace_back(
			obs_property_frame_rate_fps_range_min(prop, i),
			obs_property_frame_rate_fps_range_max(prop, i));

	auto widget = CreateFrameRateWidget(prop, warning, option, valid_fps,
					    fps_ranges);
	auto info = new WidgetInfo(this, prop, widget);

	widget->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	widget->name = name;
	widget->settings = settings;

	widget->modeSelect->setEnabled(enabled);
	widget->simpleFPS->setEnabled(enabled);
	widget->fpsRange->setEnabled(enabled);
	widget->numEdit->setEnabled(enabled);
	widget->denEdit->setEnabled(enabled);

	label = widget->warningLabel =
		new QLabel{obs_property_description(prop)};

	layout->addRow(label, widget);

	children.emplace_back(info);

	UpdateFPSLabels(widget);

	auto stack = widget->modeDisplay;
	auto combo = widget->modeSelect;

	stack->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	auto comboIndexChanged = static_cast<void (QComboBox::*)(int)>(
		&QComboBox::currentIndexChanged);
	connect(combo, comboIndexChanged, stack, [=](int index) {
		bool out_of_bounds = index >= stack->count();
		auto idx = out_of_bounds ? stack->count() - 1 : index;
		stack->setCurrentIndex(idx);

		if (widget->updating)
			return;

		UpdateFPSLabels(widget);
		emit info->ControlChanged();
	});

	connect(widget->simpleFPS, comboIndexChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});

	connect(widget->fpsRange, comboIndexChanged, [=](int) {
		if (widget->updating)
			return;

		UpdateFPSLabels(widget);
	});

	auto sbValueChanged =
		static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged);
	connect(widget->numEdit, sbValueChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});

	connect(widget->denEdit, sbValueChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});
}

void OBSPropertiesView::AddGroup(obs_property_t *prop, QFormLayout *layout)
{
	const char *name = obs_property_name(prop);
	bool val = obs_data_get_bool(settings, name);
	const char *desc = obs_property_description(prop);
	enum obs_group_type type = obs_property_group_type(prop);

	// Create GroupBox
	QGroupBox *groupBox = new QGroupBox(QT_UTF8(desc));
	groupBox->setCheckable(type == OBS_GROUP_CHECKABLE);
	groupBox->setChecked(groupBox->isCheckable() ? val : true);
	groupBox->setAccessibleName("group");
	groupBox->setEnabled(obs_property_enabled(prop));

	// Create Layout and build content
	QFormLayout *subLayout = new QFormLayout();
	subLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	groupBox->setLayout(subLayout);

	obs_properties_t *content = obs_property_group_content(prop);
	obs_property_t *el = obs_properties_first(content);
	while (el != nullptr) {
		AddProperty(el, subLayout);
		obs_property_next(&el);
	}

	// Insert into UI
	layout->setWidget(layout->rowCount(),
			  QFormLayout::ItemRole::SpanningRole, groupBox);

	// Register Group Widget
	WidgetInfo *info = new WidgetInfo(this, prop, groupBox);
	children.emplace_back(info);

	// Signals
	connect(groupBox, SIGNAL(toggled(bool)), info, SLOT(ControlChanged()));
}

void OBSPropertiesView::AddProperty(obs_property_t *property,
				    QFormLayout *layout)
{
	const char *name = obs_property_name(property);
	obs_property_type type = obs_property_get_type(property);

	if (!obs_property_visible(property))
		return;

	QLabel *label = nullptr;
	QWidget *widget = nullptr;
	bool warning = false;

	switch (type) {
	case OBS_PROPERTY_INVALID:
		return;
	case OBS_PROPERTY_BOOL:
		widget = AddCheckbox(property);
		break;
	case OBS_PROPERTY_INT:
		AddInt(property, layout, &label);
		break;
	case OBS_PROPERTY_FLOAT:
		AddFloat(property, layout, &label);
		break;
	case OBS_PROPERTY_TEXT:
		widget = AddText(property, layout, label);
		break;
	case OBS_PROPERTY_PATH:
		AddPath(property, layout, &label);
		break;
	case OBS_PROPERTY_LIST:
		widget = AddList(property, warning);
		break;
	case OBS_PROPERTY_COLOR:
		AddColor(property, layout, label);
		break;
	case OBS_PROPERTY_FONT:
		AddFont(property, layout, label);
		break;
	case OBS_PROPERTY_BUTTON:
		widget = AddButton(property);
		break;
	case OBS_PROPERTY_EDITABLE_LIST:
		AddEditableList(property, layout, label);
		break;
	case OBS_PROPERTY_FRAME_RATE:
		AddFrameRate(property, warning, layout, label);
		break;
	case OBS_PROPERTY_GROUP:
		AddGroup(property, layout);
		break;
	case OBS_PROPERTY_COLOR_ALPHA:
		AddColorAlpha(property, layout, label);
	}

	if (widget && !obs_property_enabled(property))
		widget->setEnabled(false);

	if (!label && type != OBS_PROPERTY_BOOL &&
	    type != OBS_PROPERTY_BUTTON && type != OBS_PROPERTY_GROUP)
		label = new QLabel(QT_UTF8(obs_property_description(property)));

	if (warning && label) //TODO: select color based on background color
		label->setStyleSheet("QLabel { color: red; }");

	if (label && minSize) {
		label->setMinimumWidth(minSize);
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	}

	if (label && !obs_property_enabled(property))
		label->setEnabled(false);

	if (!widget)
		return;

	if (obs_property_long_description(property)) {
		bool lightTheme = palette().text().color().redF() < 0.5;
		QString file = lightTheme ? ":/res/images/help.svg"
					  : ":/res/images/help_light.svg";
		if (label) {
			QString lStr = "<html>%1 <img src='%2' style=' \
				vertical-align: bottom;  \
				' /></html>";

			label->setText(lStr.arg(label->text(), file));
			label->setToolTip(
				obs_property_long_description(property));
		} else if (type == OBS_PROPERTY_BOOL) {

			QString bStr = "<html> <img src='%1' style=' \
				vertical-align: bottom;  \
				' /></html>";

			const char *desc = obs_property_description(property);

			QWidget *newWidget = new QWidget();

			QHBoxLayout *boxLayout = new QHBoxLayout(newWidget);
			boxLayout->setContentsMargins(0, 0, 0, 0);
			boxLayout->setAlignment(Qt::AlignLeft);
			boxLayout->setSpacing(0);

			QCheckBox *check = qobject_cast<QCheckBox *>(widget);
			check->setText(desc);
			check->setToolTip(
				obs_property_long_description(property));

			QLabel *help = new QLabel(check);
			help->setText(bStr.arg(file));
			help->setToolTip(
				obs_property_long_description(property));

			boxLayout->addWidget(check);
			boxLayout->addWidget(help);
			widget = newWidget;
		}
	}

	layout->addRow(label, widget);

	if (!lastFocused.empty())
		if (lastFocused.compare(name) == 0)
			lastWidget = widget;
}

void OBSPropertiesView::SignalChanged()
{
	emit Changed();
}

static bool FrameRateChangedVariant(const QVariant &variant,
				    media_frames_per_second &fps,
				    obs_data_item_t *&obj,
				    const media_frames_per_second *valid_fps)
{
	if (!variant.canConvert<media_frames_per_second>())
		return false;

	fps = variant.value<media_frames_per_second>();
	if (valid_fps && fps == *valid_fps)
		return false;

	obs_data_item_set_frames_per_second(&obj, fps, nullptr);
	return true;
}

static bool FrameRateChangedCommon(OBSFrameRatePropertyWidget *w,
				   obs_data_item_t *&obj,
				   const media_frames_per_second *valid_fps)
{
	media_frames_per_second fps{};
	if (!FrameRateChangedVariant(w->simpleFPS->currentData(), fps, obj,
				     valid_fps))
		return false;

	UpdateRationalFPSWidgets(w, &fps);
	return true;
}

static bool FrameRateChangedRational(OBSFrameRatePropertyWidget *w,
				     obs_data_item_t *&obj,
				     const media_frames_per_second *valid_fps)
{
	auto num = w->numEdit->value();
	auto den = w->denEdit->value();

	auto fps = make_fps(num, den);
	if (valid_fps && media_frames_per_second_is_valid(fps) &&
	    fps == *valid_fps)
		return false;

	obs_data_item_set_frames_per_second(&obj, fps, nullptr);
	UpdateSimpleFPSSelection(w, &fps);
	return true;
}

static bool FrameRateChanged(QWidget *widget, const char *name,
			     OBSData &settings)
{
	auto w = qobject_cast<OBSFrameRatePropertyWidget *>(widget);
	if (!w)
		return false;

	auto variant = w->modeSelect->currentData();
	if (!variant.canConvert<frame_rate_tag>())
		return false;

	auto StopUpdating = [&](void *) { w->updating = false; };
	unique_ptr<void, decltype(StopUpdating)> signalGuard(
		static_cast<void *>(w), StopUpdating);
	w->updating = true;

	if (!obs_data_has_user_value(settings, name))
		obs_data_set_obj(settings, name, nullptr);

	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(settings, name)};
	auto obj_ptr = obj.get();
	auto CheckObj = [&]() {
		if (!obj_ptr)
			obj.release();
	};

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	auto tag = variant.value<frame_rate_tag>();
	switch (tag.type) {
	case frame_rate_tag::SIMPLE:
		if (!FrameRateChangedCommon(w, obj_ptr, valid_fps))
			return false;
		break;

	case frame_rate_tag::RATIONAL:
		if (!FrameRateChangedRational(w, obj_ptr, valid_fps))
			return false;
		break;

	case frame_rate_tag::USER:
		if (tag.val && option && strcmp(tag.val, option) == 0)
			return false;

		obs_data_item_set_frames_per_second(&obj_ptr, {}, tag.val);
		break;
	}

	UpdateFPSLabels(w);
	CheckObj();
	return true;
}

void WidgetInfo::BoolChanged(const char *setting)
{
	QCheckBox *checkbox = static_cast<QCheckBox *>(widget);
	obs_data_set_bool(view->settings, setting,
			  checkbox->checkState() == Qt::Checked);
}

void WidgetInfo::IntChanged(const char *setting)
{
	QSpinBox *spin = static_cast<QSpinBox *>(widget);
	obs_data_set_int(view->settings, setting, spin->value());
}

void WidgetInfo::FloatChanged(const char *setting)
{
	QDoubleSpinBox *spin = static_cast<QDoubleSpinBox *>(widget);
	obs_data_set_double(view->settings, setting, spin->value());
}

void WidgetInfo::TextChanged(const char *setting)
{
	obs_text_type type = obs_property_text_type(property);

	if (type == OBS_TEXT_MULTILINE) {
		QPlainTextEdit *edit = static_cast<QPlainTextEdit *>(widget);
		obs_data_set_string(view->settings, setting,
				    QT_TO_UTF8(edit->toPlainText()));
		return;
	}

	QLineEdit *edit = static_cast<QLineEdit *>(widget);
	obs_data_set_string(view->settings, setting, QT_TO_UTF8(edit->text()));
}

bool WidgetInfo::PathChanged(const char *setting)
{
	const char *desc = obs_property_description(property);
	obs_path_type type = obs_property_path_type(property);
	const char *filter = obs_property_path_filter(property);
	const char *default_path = obs_property_path_default_path(property);
	QString path;

	if (type == OBS_PATH_DIRECTORY)
		path = SelectDirectory(view, QT_UTF8(desc),
				       QT_UTF8(default_path));
	else if (type == OBS_PATH_FILE)
		path = OpenFile(view, QT_UTF8(desc), QT_UTF8(default_path),
				QT_UTF8(filter));
	else if (type == OBS_PATH_FILE_SAVE)
		path = SaveFile(view, QT_UTF8(desc), QT_UTF8(default_path),
				QT_UTF8(filter));

	if (path.isEmpty())
		return false;

	QLineEdit *edit = static_cast<QLineEdit *>(widget);
	edit->setText(path);
	obs_data_set_string(view->settings, setting, QT_TO_UTF8(path));
	return true;
}

void WidgetInfo::ListChanged(const char *setting)
{
	QComboBox *combo = static_cast<QComboBox *>(widget);
	obs_combo_format format = obs_property_list_format(property);
	obs_combo_type type = obs_property_list_type(property);
	QVariant data;

	if (type == OBS_COMBO_TYPE_EDITABLE) {
		data = combo->currentText().toUtf8();
	} else {
		int index = combo->currentIndex();
		if (index != -1)
			data = combo->itemData(index);
		else
			return;
	}

	switch (format) {
	case OBS_COMBO_FORMAT_INVALID:
		return;
	case OBS_COMBO_FORMAT_INT:
		obs_data_set_int(view->settings, setting,
				 data.value<long long>());
		break;
	case OBS_COMBO_FORMAT_FLOAT:
		obs_data_set_double(view->settings, setting,
				    data.value<double>());
		break;
	case OBS_COMBO_FORMAT_STRING:
		obs_data_set_string(view->settings, setting,
				    data.toByteArray().constData());
		break;
	}
}

bool WidgetInfo::ColorChangedInternal(const char *setting, bool supportAlpha)
{
	const char *desc = obs_property_description(property);
	long long val = obs_data_get_int(view->settings, setting);
	QColor color = color_from_int(val);
	QColor::NameFormat format;

	QColorDialog::ColorDialogOptions options;

	if (supportAlpha) {
		options |= QColorDialog::ShowAlphaChannel;
	}

	/* The native dialog on OSX has all kinds of problems, like closing
	 * other open QDialogs on exit, and
	 * https://bugreports.qt-project.org/browse/QTBUG-34532
	 */
#ifndef _WIN32
	options |= QColorDialog::DontUseNativeDialog;
#endif

	color = QColorDialog::getColor(color, view, QT_UTF8(desc), options);
	if (!color.isValid())
		return false;

	if (supportAlpha) {
		format = QColor::HexArgb;
	} else {
		color.setAlpha(255);
		format = QColor::HexRgb;
	}

	QLabel *label = static_cast<QLabel *>(widget);
	label->setText(color.name(format));
	QPalette palette = QPalette(color);
	label->setPalette(palette);
	label->setStyleSheet(
		QString("background-color :%1; color: %2;")
			.arg(palette.color(QPalette::Window).name(format))
			.arg(palette.color(QPalette::WindowText).name(format)));

	obs_data_set_int(view->settings, setting, color_to_int(color));

	return true;
}

bool WidgetInfo::ColorChanged(const char *setting)
{
	return ColorChangedInternal(setting, false);
}

bool WidgetInfo::ColorAlphaChanged(const char *setting)
{
	return ColorChangedInternal(setting, true);
}

bool WidgetInfo::FontChanged(const char *setting)
{
	obs_data_t *font_obj = obs_data_get_obj(view->settings, setting);
	bool success;
	uint32_t flags;
	QFont font;

	QFontDialog::FontDialogOptions options;

#ifndef _WIN32
	options = QFontDialog::DontUseNativeDialog;
#endif

	if (!font_obj) {
		QFont initial;
		font = QFontDialog::getFont(&success, initial, view,
					    "Pick a Font", options);
	} else {
		MakeQFont(font_obj, font);
		font = QFontDialog::getFont(&success, font, view, "Pick a Font",
					    options);
		obs_data_release(font_obj);
	}

	if (!success)
		return false;

	font_obj = obs_data_create();

	obs_data_set_string(font_obj, "face", QT_TO_UTF8(font.family()));
	obs_data_set_string(font_obj, "style", QT_TO_UTF8(font.styleName()));
	obs_data_set_int(font_obj, "size", font.pointSize());
	flags = font.bold() ? OBS_FONT_BOLD : 0;
	flags |= font.italic() ? OBS_FONT_ITALIC : 0;
	flags |= font.underline() ? OBS_FONT_UNDERLINE : 0;
	flags |= font.strikeOut() ? OBS_FONT_STRIKEOUT : 0;
	obs_data_set_int(font_obj, "flags", flags);

	QLabel *label = static_cast<QLabel *>(widget);
	QFont labelFont;
	MakeQFont(font_obj, labelFont, true);
	label->setFont(labelFont);
	label->setText(QString("%1 %2").arg(font.family(), font.styleName()));

	obs_data_set_obj(view->settings, setting, font_obj);
	obs_data_release(font_obj);
	return true;
}

void WidgetInfo::GroupChanged(const char *setting)
{
	QGroupBox *groupbox = static_cast<QGroupBox *>(widget);
	obs_data_set_bool(view->settings, setting,
			  groupbox->isCheckable() ? groupbox->isChecked()
						  : true);
}

void WidgetInfo::EditListReordered(const QModelIndex &parent, int start,
				   int end, const QModelIndex &destination,
				   int row)
{
	UNUSED_PARAMETER(parent);
	UNUSED_PARAMETER(start);
	UNUSED_PARAMETER(end);
	UNUSED_PARAMETER(destination);
	UNUSED_PARAMETER(row);

	EditableListChanged();
}

void WidgetInfo::EditableListChanged()
{
	const char *setting = obs_property_name(property);
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	obs_data_array *array = obs_data_array_create();

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		obs_data_t *arrayItem = obs_data_create();
		obs_data_set_string(arrayItem, "value",
				    QT_TO_UTF8(item->text()));
		obs_data_set_bool(arrayItem, "selected", item->isSelected());
		obs_data_set_bool(arrayItem, "hidden", item->isHidden());
		obs_data_array_push_back(array, arrayItem);
		obs_data_release(arrayItem);
	}

	obs_data_set_array(view->settings, setting, array);
	obs_data_array_release(array);

	ControlChanged();
}

void WidgetInfo::ButtonClicked()
{
	if (obs_property_button_clicked(property, view->obj)) {
		QMetaObject::invokeMethod(view, "RefreshProperties",
					  Qt::QueuedConnection);
	}
}

void WidgetInfo::TogglePasswordText(bool show)
{
	reinterpret_cast<QLineEdit *>(widget)->setEchoMode(
		show ? QLineEdit::Normal : QLineEdit::Password);
}

void WidgetInfo::ControlChanged()
{
	const char *setting = obs_property_name(property);
	obs_property_type type = obs_property_get_type(property);

	switch (type) {
	case OBS_PROPERTY_INVALID:
		return;
	case OBS_PROPERTY_BOOL:
		BoolChanged(setting);
		break;
	case OBS_PROPERTY_INT:
		IntChanged(setting);
		break;
	case OBS_PROPERTY_FLOAT:
		FloatChanged(setting);
		break;
	case OBS_PROPERTY_TEXT:
		TextChanged(setting);
		break;
	case OBS_PROPERTY_LIST:
		ListChanged(setting);
		break;
	case OBS_PROPERTY_BUTTON:
		ButtonClicked();
		return;
	case OBS_PROPERTY_COLOR:
		if (!ColorChanged(setting))
			return;
		break;
	case OBS_PROPERTY_FONT:
		if (!FontChanged(setting))
			return;
		break;
	case OBS_PROPERTY_PATH:
		if (!PathChanged(setting))
			return;
		break;
	case OBS_PROPERTY_EDITABLE_LIST:
		break;
	case OBS_PROPERTY_FRAME_RATE:
		if (!FrameRateChanged(widget, setting, view->settings))
			return;
		break;
	case OBS_PROPERTY_GROUP:
		GroupChanged(setting);
		break;
	case OBS_PROPERTY_COLOR_ALPHA:
		if (!ColorAlphaChanged(setting))
			return;
		break;
	}

	if (view->callback && !view->deferUpdate)
		view->callback(view->obj, view->settings);

	view->SignalChanged();

	if (obs_property_modified(property, view->settings)) {
		view->lastFocused = setting;
		QMetaObject::invokeMethod(view, "RefreshProperties",
					  Qt::QueuedConnection);
	}
}

class EditableItemDialog : public QDialog {
	QLineEdit *edit;
	QString filter;
	QString default_path;

	void BrowseClicked()
	{
		QString curPath = QFileInfo(edit->text()).absoluteDir().path();

		if (curPath.isEmpty())
			curPath = default_path;

		QString path = OpenFile(App()->GetMainWindow(), QTStr("Browse"),
					curPath, filter);
		if (path.isEmpty())
			return;

		edit->setText(path);
	}

public:
	EditableItemDialog(QWidget *parent, const QString &text, bool browse,
			   const char *filter_ = nullptr,
			   const char *default_path_ = nullptr)
		: QDialog(parent),
		  filter(QT_UTF8(filter_)),
		  default_path(QT_UTF8(default_path_))
	{
		QHBoxLayout *topLayout = new QHBoxLayout();
		QVBoxLayout *mainLayout = new QVBoxLayout();

		edit = new QLineEdit();
		edit->setText(text);
		topLayout->addWidget(edit);
		topLayout->setAlignment(edit, Qt::AlignVCenter);

		if (browse) {
			QPushButton *browseButton =
				new QPushButton(QTStr("Browse"));
			browseButton->setProperty("themeID", "settingsButtons");
			topLayout->addWidget(browseButton);
			topLayout->setAlignment(browseButton, Qt::AlignVCenter);

			connect(browseButton, &QPushButton::clicked, this,
				&EditableItemDialog::BrowseClicked);
		}

		QDialogButtonBox::StandardButtons buttons =
			QDialogButtonBox::Ok | QDialogButtonBox::Cancel;

		QDialogButtonBox *buttonBox = new QDialogButtonBox(buttons);
		buttonBox->setCenterButtons(true);

		mainLayout->addLayout(topLayout);
		mainLayout->addWidget(buttonBox);

		setLayout(mainLayout);
		resize(QSize(400, 80));

		connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
		connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	}

	inline QString GetText() const { return edit->text(); }
};

void WidgetInfo::EditListAdd()
{
	enum obs_editable_list_type type =
		obs_property_editable_list_type(property);

	if (type == OBS_EDITABLE_LIST_TYPE_STRINGS) {
		EditListAddText();
		return;
	}

	/* Files and URLs */
	QMenu popup(view->window());

	QAction *action;

	action = new QAction(QTStr("Basic.PropertiesWindow.AddFiles"), this);
	connect(action, &QAction::triggered, this,
		&WidgetInfo::EditListAddFiles);
	popup.addAction(action);

	action = new QAction(QTStr("Basic.PropertiesWindow.AddDir"), this);
	connect(action, &QAction::triggered, this, &WidgetInfo::EditListAddDir);
	popup.addAction(action);

	if (type == OBS_EDITABLE_LIST_TYPE_FILES_AND_URLS) {
		action = new QAction(QTStr("Basic.PropertiesWindow.AddURL"),
				     this);
		connect(action, &QAction::triggered, this,
			&WidgetInfo::EditListAddText);
		popup.addAction(action);
	}

	popup.exec(QCursor::pos());
}

void WidgetInfo::EditListAddText()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);

	EditableItemDialog dialog(widget->window(), QString(), false);
	auto title = QTStr("Basic.PropertiesWindow.AddEditableListEntry")
			     .arg(QT_UTF8(desc));
	dialog.setWindowTitle(title);
	if (dialog.exec() == QDialog::Rejected)
		return;

	QString text = dialog.GetText();
	if (text.isEmpty())
		return;

	list->addItem(text);
	EditableListChanged();
}

void WidgetInfo::EditListAddFiles()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);
	const char *filter = obs_property_editable_list_filter(property);
	const char *default_path =
		obs_property_editable_list_default_path(property);

	QString title = QTStr("Basic.PropertiesWindow.AddEditableListFiles")
				.arg(QT_UTF8(desc));

	QStringList files = OpenFiles(App()->GetMainWindow(), title,
				      QT_UTF8(default_path), QT_UTF8(filter));

	if (files.count() == 0)
		return;

	list->addItems(files);
	EditableListChanged();
}

void WidgetInfo::EditListAddDir()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);
	const char *default_path =
		obs_property_editable_list_default_path(property);

	QString title = QTStr("Basic.PropertiesWindow.AddEditableListDir")
				.arg(QT_UTF8(desc));

	QString dir = SelectDirectory(App()->GetMainWindow(), title,
				      QT_UTF8(default_path));

	if (dir.isEmpty())
		return;

	list->addItem(dir);
	EditableListChanged();
}

void WidgetInfo::EditListRemove()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	QList<QListWidgetItem *> items = list->selectedItems();

	for (QListWidgetItem *item : items)
		delete item;
	EditableListChanged();
}

void WidgetInfo::EditListEdit()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	enum obs_editable_list_type type =
		obs_property_editable_list_type(property);
	const char *desc = obs_property_description(property);
	const char *filter = obs_property_editable_list_filter(property);
	QList<QListWidgetItem *> selectedItems = list->selectedItems();

	if (!selectedItems.count())
		return;

	QListWidgetItem *item = selectedItems[0];

	if (type == OBS_EDITABLE_LIST_TYPE_FILES) {
		QDir pathDir(item->text());
		QString path;

		if (pathDir.exists())
			path = SelectDirectory(App()->GetMainWindow(),
					       QTStr("Browse"), item->text());
		else
			path = OpenFile(App()->GetMainWindow(), QTStr("Browse"),
					item->text(), QT_UTF8(filter));

		if (path.isEmpty())
			return;

		item->setText(path);
		EditableListChanged();
		return;
	}

	EditableItemDialog dialog(widget->window(), item->text(),
				  type != OBS_EDITABLE_LIST_TYPE_STRINGS,
				  filter);
	auto title = QTStr("Basic.PropertiesWindow.EditEditableListEntry")
			     .arg(QT_UTF8(desc));
	dialog.setWindowTitle(title);
	if (dialog.exec() == QDialog::Rejected)
		return;

	QString text = dialog.GetText();
	if (text.isEmpty())
		return;

	item->setText(text);
	EditableListChanged();
}

void WidgetInfo::EditListUp()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	int lastItemRow = -1;

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		if (!item->isSelected())
			continue;

		int row = list->row(item);

		if ((row - 1) != lastItemRow) {
			lastItemRow = row - 1;
			list->takeItem(row);
			list->insertItem(lastItemRow, item);
			item->setSelected(true);
		} else {
			lastItemRow = row;
		}
	}

	EditableListChanged();
}

void WidgetInfo::EditListDown()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	int lastItemRow = list->count();

	for (int i = list->count() - 1; i >= 0; i--) {
		QListWidgetItem *item = list->item(i);
		if (!item->isSelected())
			continue;

		int row = list->row(item);

		if ((row + 1) != lastItemRow) {
			lastItemRow = row + 1;
			list->takeItem(row);
			list->insertItem(lastItemRow, item);
			item->setSelected(true);
		} else {
			lastItemRow = row;
		}
	}

	EditableListChanged();
}
