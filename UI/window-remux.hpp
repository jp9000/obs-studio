/******************************************************************************
    Copyright (C) 2014 by Ruwen Hahn <palana@stunned.de>

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

#include <QFileInfo>
#include <QMutex>
#include <QPointer>
#include <QThread>
#include <QStyledItemDelegate>
#include <memory>
#include "ui_OBSRemux.h"

#include <media-io/media-remux.h>
#include <util/threading.h>

class RemuxQueueModel;
class RemuxWorker;

enum RemuxEntryState {
	Empty,
	Ready,
	Pending,
	InProgress,
	Complete,
	InvalidPath,
	Error
};
Q_DECLARE_METATYPE(RemuxEntryState);

class OBSRemux : public QDialog {
	Q_OBJECT

	QPointer<RemuxQueueModel> queueModel;
	QThread remuxer;
	QPointer<RemuxWorker> worker;

	std::unique_ptr<Ui::OBSRemux> ui;

	const char *recPath;

	virtual void closeEvent(QCloseEvent *event) override;
	virtual void reject() override;

	bool autoRemux;
	QString autoRemuxFile;

public:
	explicit OBSRemux(const char *recPath, QWidget *parent = nullptr,
			  bool autoRemux = false);
	virtual ~OBSRemux() override;

	using job_t = std::shared_ptr<struct media_remux_job>;

	void AutoRemux(QString inFile, QString outFile);

protected:
	virtual void dropEvent(QDropEvent *ev) override;
	virtual void dragEnterEvent(QDragEnterEvent *ev) override;

	void remuxNextEntry();

private slots:
	void rowCountChanged(const QModelIndex &parent, int first, int last);

public slots:
	void updateProgress(float percent);
	void remuxFinished(bool success);
	void beginRemux();
	bool stopRemux();
	void clearFinished();
	void clearAll();

signals:
	void remux(const QString &source, const QString &target);
};

class RemuxQueueModel : public QAbstractTableModel {
	Q_OBJECT

	friend class OBSRemux;

public:
	RemuxQueueModel(QObject *parent = 0)
		: QAbstractTableModel(parent), isProcessing(false)
	{
	}

	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation,
			    int role = Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);

	QFileInfoList checkForOverwrites() const;
	bool checkForErrors() const;
	void beginProcessing();
	void endProcessing();
	bool beginNextEntry(QString &inputPath, QString &outputPath);
	void finishEntry(bool success);
	bool canClearFinished() const;
	void clearFinished();
	void clearAll();

	bool autoRemux = false;

private:
	struct RemuxQueueEntry {
		RemuxEntryState state;

		QString sourcePath;
		QString targetPath;
	};

	QList<RemuxQueueEntry> queue;
	bool isProcessing;

	static QVariant getIcon(RemuxEntryState state);

	void checkInputPath(int row);
};

class RemuxWorker : public QObject {
	Q_OBJECT

	QMutex updateMutex;

	bool isWorking;

	float lastProgress;
	void UpdateProgress(float percent);

	explicit RemuxWorker() : isWorking(false) {}
	virtual ~RemuxWorker(){};

private slots:
	void remux(const QString &source, const QString &target);

signals:
	void updateProgress(float percent);
	void remuxFinished(bool success);

	friend class OBSRemux;
};

class RemuxEntryPathItemDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	RemuxEntryPathItemDelegate(bool isOutput, const QString &defaultPath);

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

private slots:
	void updateText();
};
