#pragma once

#include "vertical-scroll-area.hpp"
#include <obs.hpp>
#include <vector>
#include <memory>

class QFormLayout;
class OBSPropertiesView;
class QLabel;

typedef obs_properties_t *(*PropertiesReloadCallback)(void *obj);
typedef void (*PropertiesUpdateCallback)(void *obj, obs_data_t *settings);

/* ------------------------------------------------------------------------- */

class WidgetInfo : public QObject {
	Q_OBJECT

	friend class OBSPropertiesView;

private:
	OBSPropertiesView *view;
	obs_property_t *property;
	QWidget *widget;

	void BoolChanged(const char *setting);
	void IntChanged(const char *setting);
	void FloatChanged(const char *setting);
	void TextChanged(const char *setting);
	bool PathChanged(const char *setting);
	void ListChanged(const char *setting);
	bool ColorChangedInternal(const char *setting, bool supportAlpha);
	bool ColorChanged(const char *setting);
	bool ColorAlphaChanged(const char *setting);
	bool FontChanged(const char *setting);
	void GroupChanged(const char *setting);
	void EditableListChanged();
	void ButtonClicked();

	void TogglePasswordText(bool checked);

public:
	inline WidgetInfo(OBSPropertiesView *view_, obs_property_t *prop,
			  QWidget *widget_)
		: view(view_), property(prop), widget(widget_)
	{
	}

public slots:

	void ControlChanged();

	/* editable list */
	void EditListAdd();
	void EditListAddText();
	void EditListAddFiles();
	void EditListAddDir();
	void EditListRemove();
	void EditListEdit();
	void EditListUp();
	void EditListDown();
	void EditListReordered(const QModelIndex &parent, int start, int end,
			       const QModelIndex &destination, int row);
};

/* ------------------------------------------------------------------------- */

class OBSPropertiesView : public VScrollArea {
	Q_OBJECT

	friend class WidgetInfo;

	using properties_delete_t = decltype(&obs_properties_destroy);
	using properties_t =
		std::unique_ptr<obs_properties_t, properties_delete_t>;

private:
	QWidget *widget = nullptr;
	properties_t properties;
	OBSData settings;
	void *obj = nullptr;
	std::string type;
	PropertiesReloadCallback reloadCallback;
	PropertiesUpdateCallback callback = nullptr;
	int minSize;
	std::vector<std::unique_ptr<WidgetInfo>> children;
	std::string lastFocused;
	QWidget *lastWidget = nullptr;
	bool deferUpdate;

	QWidget *NewWidget(obs_property_t *prop, QWidget *widget,
			   const char *signal);

	QWidget *AddCheckbox(obs_property_t *prop);
	QWidget *AddText(obs_property_t *prop, QFormLayout *layout,
			 QLabel *&label);
	void AddPath(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	void AddInt(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	void AddFloat(obs_property_t *prop, QFormLayout *layout,
		      QLabel **label);
	QWidget *AddList(obs_property_t *prop, bool &warning);
	void AddEditableList(obs_property_t *prop, QFormLayout *layout,
			     QLabel *&label);
	QWidget *AddButton(obs_property_t *prop);
	void AddColorInternal(obs_property_t *prop, QFormLayout *layout,
			      QLabel *&label, bool supportAlpha);
	void AddColor(obs_property_t *prop, QFormLayout *layout,
		      QLabel *&label);
	void AddColorAlpha(obs_property_t *prop, QFormLayout *layout,
			   QLabel *&label);
	void AddFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFrameRate(obs_property_t *prop, bool &warning,
			  QFormLayout *layout, QLabel *&label);

	void AddGroup(obs_property_t *prop, QFormLayout *layout);

	void AddProperty(obs_property_t *property, QFormLayout *layout);

	void resizeEvent(QResizeEvent *event) override;

	void GetScrollPos(int &h, int &v);
	void SetScrollPos(int h, int v);

public slots:
	void ReloadProperties();
	void RefreshProperties();
	void SignalChanged();

signals:
	void PropertiesResized();
	void Changed();
	void PropertiesRefreshed();

public:
	OBSPropertiesView(OBSData settings, void *obj,
			  PropertiesReloadCallback reloadCallback,
			  PropertiesUpdateCallback callback, int minSize = 0);
	OBSPropertiesView(OBSData settings, const char *type,
			  PropertiesReloadCallback reloadCallback,
			  int minSize = 0);

	inline obs_data_t *GetSettings() const { return settings; }

	inline void UpdateSettings() { callback(obj, settings); }
	inline bool DeferUpdate() const { return deferUpdate; }
};
