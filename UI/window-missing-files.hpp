/******************************************************************************
    Copyright (C) 2019 by Dillon Pentz <dillon@vodbox.io>

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

#pragma once

#include <QPointer>
#include <QStyledItemDelegate>
#include "obs-app.hpp"
#include "ui_OBSMissingFiles.h"

class MissingFilesModel;

enum MissingFilesState { Missing, Found, Replaced, Cleared };
Q_DECLARE_METATYPE(MissingFilesState);

class OBSMissingFiles : public QDialog {
	Q_OBJECT
	Q_PROPERTY(QIcon warningIcon READ GetWarningIcon WRITE SetWarningIcon
			   DESIGNABLE true)

	QPointer<MissingFilesModel> filesModel;
	std::unique_ptr<Ui::OBSMissingFiles> ui;

public:
	explicit OBSMissingFiles(obs_missing_files_t *files,
				 QWidget *parent = nullptr);
	virtual ~OBSMissingFiles() override;

	void addMissingFile(const char *originalPath, const char *sourceName);

	QIcon GetWarningIcon();
	void SetWarningIcon(const QIcon &icon);

private:
	void saveFiles();
	void browseFolders();

	obs_missing_files_t *fileStore;

public slots:
	void dataChanged();
};

class MissingFilesModel : public QAbstractTableModel {
	Q_OBJECT

	friend class OBSMissingFiles;

public:
	explicit MissingFilesModel(QObject *parent = 0);

	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;
	int found() const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation,
			    int role = Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);

	bool loop = true;

	QIcon warningIcon;

private:
	struct MissingFileEntry {
		MissingFilesState state = MissingFilesState::Missing;

		QString source;

		QString originalPath;
		QString newPath;
	};

	QList<MissingFileEntry> files;

	void fileCheckLoop(QList<MissingFileEntry> files, QString path,
			   bool skipPrompt);
};

class MissingFilesPathItemDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	MissingFilesPathItemDelegate(bool isOutput, const QString &defaultPath);

	virtual QWidget *createEditor(QWidget *parent,
				      const QStyleOptionViewItem & /* option */,
				      const QModelIndex &index) const override;

	virtual void setEditorData(QWidget *editor,
				   const QModelIndex &index) const override;
	virtual void setModelData(QWidget *editor, QAbstractItemModel *model,
				  const QModelIndex &index) const override;
	virtual void paint(QPainter *painter,
			   const QStyleOptionViewItem &option,
			   const QModelIndex &index) const override;

private:
	bool isOutput;
	QString defaultPath;
	const char *PATH_LIST_PROP = "pathList";

	void handleBrowse(QWidget *container);
	void handleClear(QWidget *container);
};
