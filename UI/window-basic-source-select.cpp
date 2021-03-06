/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <QMessageBox>
#include "window-basic-main.hpp"
#include "window-basic-source-select.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"

struct AddSourceData {
	obs_source_t *source;
	bool visible;
};

bool OBSBasicSourceSelect::EnumSources(void *data, obs_source_t *source)
{
	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_unversioned_id(source);

	if (strcmp(id, window->id) == 0)
		window->ui->sourceList->addItem(QT_UTF8(name));

	return true;
}

bool OBSBasicSourceSelect::EnumGroups(void *data, obs_source_t *source)
{
	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_unversioned_id(source);

	if (strcmp(id, window->id) == 0) {
		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		OBSScene scene = main->GetCurrentScene();

		obs_sceneitem_t *existing = obs_scene_get_group(scene, name);
		if (!existing)
			window->ui->sourceList->addItem(QT_UTF8(name));
	}

	return true;
}

void OBSBasicSourceSelect::OBSSourceAdded(void *data, calldata_t *calldata)
{
	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceAdded",
				  Q_ARG(OBSSource, source));
}

void OBSBasicSourceSelect::OBSSourceRemoved(void *data, calldata_t *calldata)
{
	OBSBasicSourceSelect *window =
		static_cast<OBSBasicSourceSelect *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceRemoved",
				  Q_ARG(OBSSource, source));
}

void OBSBasicSourceSelect::SourceAdded(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	const char *sourceId = obs_source_get_unversioned_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	ui->sourceList->addItem(name);
}

void OBSBasicSourceSelect::SourceRemoved(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	const char *sourceId = obs_source_get_unversioned_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	QList<QListWidgetItem *> items =
		ui->sourceList->findItems(name, Qt::MatchFixedString);

	if (!items.count())
		return;

	delete items[0];
}

static void AddSource(void *_data, obs_scene_t *scene)
{
	AddSourceData *data = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data->source);
	obs_sceneitem_set_visible(sceneitem, data->visible);
}

static char *get_new_source_name(const char *name)
{
	struct dstr new_name = {0};
	int inc = 0;

	dstr_copy(&new_name, name);

	for (;;) {
		obs_source_t *existing_source =
			obs_get_source_by_name(new_name.array);
		if (!existing_source)
			break;

		obs_source_release(existing_source);

		dstr_printf(&new_name, "%s %d", name, ++inc + 1);
	}

	return new_name.array;
}

static void AddExisting(const char *name, bool visible, bool duplicate)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return;

	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		if (duplicate) {
			obs_source_t *from = source;
			char *new_name = get_new_source_name(name);
			source = obs_source_duplicate(from, new_name, false);
			bfree(new_name);
			obs_source_release(from);

			if (!source)
				return;
		}

		AddSourceData data;
		data.source = source;
		data.visible = visible;

		obs_enter_graphics();
		obs_scene_atomic_update(scene, AddSource, &data);
		obs_leave_graphics();

		obs_source_release(source);
	}
}

bool AddNew(QWidget *parent, const char *id, const char *name,
	    const bool visible, OBSSource &newSource)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();
	bool success = false;
	if (!scene)
		return false;

	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		OBSMessageBox::information(parent, QTStr("NameExists.Title"),
					   QTStr("NameExists.Text"));

	} else {
		const char *v_id = obs_get_latest_input_type_id(id);
		source = obs_source_create(v_id, name, NULL, nullptr);

		if (source) {
			AddSourceData data;
			data.source = source;
			data.visible = visible;

			obs_enter_graphics();
			obs_scene_atomic_update(scene, AddSource, &data);
			obs_leave_graphics();

			newSource = source;

			/* set monitoring if source monitors by default */
			uint32_t flags = obs_source_get_output_flags(source);
			if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
				obs_source_set_monitoring_type(
					source,
					OBS_MONITORING_TYPE_MONITOR_ONLY);
			}

			success = true;
		}
	}

	obs_source_release(source);
	return success;
}

void OBSBasicSourceSelect::on_buttonBox_accepted()
{
	bool useExisting = ui->selectExisting->isChecked();
	bool visible = ui->sourceVisible->isChecked();

	if (useExisting) {
		QListWidgetItem *item = ui->sourceList->currentItem();
		if (!item)
			return;

		AddExisting(QT_TO_UTF8(item->text()), visible, false);
	} else {
		if (ui->sourceName->text().isEmpty()) {
			OBSMessageBox::warning(this,
					       QTStr("NoNameEntered.Title"),
					       QTStr("NoNameEntered.Text"));
			return;
		}

		if (!AddNew(this, id, QT_TO_UTF8(ui->sourceName->text()),
			    visible, newSource))
			return;
	}

	done(DialogCode::Accepted);
}

void OBSBasicSourceSelect::on_buttonBox_rejected()
{
	done(DialogCode::Rejected);
}

static inline const char *GetSourceDisplayName(const char *id)
{
	if (strcmp(id, "scene") == 0)
		return Str("Basic.Scene");
	const char *v_id = obs_get_latest_input_type_id(id);
	return obs_source_get_display_name(v_id);
}

Q_DECLARE_METATYPE(OBSScene);

template<typename T> static inline T GetOBSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::OBSRef)).value<T>();
}

OBSBasicSourceSelect::OBSBasicSourceSelect(OBSBasic *parent, const char *id_)
	: QDialog(parent), ui(new Ui::OBSBasicSourceSelect), id(id_)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	ui->sourceList->setAttribute(Qt::WA_MacShowFocusRect, false);

	QString placeHolderText{QT_UTF8(GetSourceDisplayName(id))};

	QString text{placeHolderText};
	int i = 2;
	obs_source_t *source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(text)))) {
		obs_source_release(source);
		text = QString("%1 %2").arg(placeHolderText).arg(i++);
	}

	ui->sourceName->setText(text);
	ui->sourceName->setFocus(); //Fixes deselect of text.
	ui->sourceName->selectAll();

	installEventFilter(CreateShortcutFilter());

	if (strcmp(id_, "scene") == 0) {
		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		OBSSource curSceneSource = main->GetCurrentSceneSource();

		ui->selectExisting->setChecked(true);
		ui->createNew->setChecked(false);
		ui->createNew->setEnabled(false);
		ui->sourceName->setEnabled(false);

		int count = main->ui->scenes->count();
		for (int i = 0; i < count; i++) {
			QListWidgetItem *item = main->ui->scenes->item(i);
			OBSScene scene = GetOBSRef<OBSScene>(item);
			OBSSource sceneSource = obs_scene_get_source(scene);

			if (curSceneSource == sceneSource)
				continue;

			const char *name = obs_source_get_name(sceneSource);
			ui->sourceList->addItem(QT_UTF8(name));
		}
	} else if (strcmp(id_, "group") == 0) {
		obs_enum_sources(EnumGroups, this);
	} else {
		obs_enum_sources(EnumSources, this);
	}
}

void OBSBasicSourceSelect::SourcePaste(const char *name, bool visible, bool dup)
{
	AddExisting(name, visible, dup);
}
