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

#include "window-remux.hpp"

#include "obs-app.hpp"

#include <QCloseEvent>
#include <QDirIterator>
#include <QItemDelegate>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTimer>

#include "qt-wrappers.hpp"

#include <memory>
#include <cmath>

using namespace std;

enum RemuxEntryColumn {
	State,
	InputPath,
	OutputPath,

	Count
};

enum RemuxEntryRole { EntryStateRole = Qt::UserRole, NewPathsToProcessRole };

/**********************************************************
  Delegate - Presents cells in the grid.
**********************************************************/

RemuxEntryPathItemDelegate::RemuxEntryPathItemDelegate(
	bool isOutput, const QString &defaultPath)
	: QStyledItemDelegate(), isOutput(isOutput), defaultPath(defaultPath)
{
}

QWidget *RemuxEntryPathItemDelegate::createEditor(
	QWidget *parent, const QStyleOptionViewItem & /* option */,
	const QModelIndex &index) const
{
	RemuxEntryState state =
		index.model()
			->index(index.row(), RemuxEntryColumn::State)
			.data(RemuxEntryRole::EntryStateRole)
			.value<RemuxEntryState>();
	if (state == RemuxEntryState::Pending ||
	    state == RemuxEntryState::InProgress) {
		// Never allow modification of rows that are
		// in progress.
		return Q_NULLPTR;
	} else if (isOutput && state != RemuxEntryState::Ready) {
		// Do not allow modification of output rows
		// that aren't associated with a valid input.
		return Q_NULLPTR;
	} else if (!isOutput && state == RemuxEntryState::Complete) {
		// Don't allow modification of rows that are
		// already complete.
		return Q_NULLPTR;
	} else {
		QSizePolicy buttonSizePolicy(
			QSizePolicy::Policy::Minimum,
			QSizePolicy::Policy::Expanding,
			QSizePolicy::ControlType::PushButton);

		QWidget *container = new QWidget(parent);

		auto browseCallback = [this, container]() {
			const_cast<RemuxEntryPathItemDelegate *>(this)
				->handleBrowse(container);
		};

		auto clearCallback = [this, container]() {
			const_cast<RemuxEntryPathItemDelegate *>(this)
				->handleClear(container);
		};

		QHBoxLayout *layout = new QHBoxLayout();
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		QLineEdit *text = new QLineEdit();
		text->setObjectName(QStringLiteral("text"));
		text->setSizePolicy(
			QSizePolicy(QSizePolicy::Policy::Expanding,
				    QSizePolicy::Policy::Expanding,
				    QSizePolicy::ControlType::LineEdit));
		layout->addWidget(text);

		QObject::connect(text, SIGNAL(editingFinished()), this,
				 SLOT(updateText()));

		QToolButton *browseButton = new QToolButton();
		browseButton->setText("...");
		browseButton->setSizePolicy(buttonSizePolicy);
		layout->addWidget(browseButton);

		container->connect(browseButton, &QToolButton::clicked,
				   browseCallback);

		// The "clear" button is not shown in output cells
		// or the insertion point's input cell.
		if (!isOutput && state != RemuxEntryState::Empty) {
			QToolButton *clearButton = new QToolButton();
			clearButton->setText("X");
			clearButton->setSizePolicy(buttonSizePolicy);
			layout->addWidget(clearButton);

			container->connect(clearButton, &QToolButton::clicked,
					   clearCallback);
		}

		container->setLayout(layout);
		container->setFocusProxy(text);
		return container;
	}
}

void RemuxEntryPathItemDelegate::setEditorData(QWidget *editor,
					       const QModelIndex &index) const
{
	QLineEdit *text = editor->findChild<QLineEdit *>();
	text->setText(index.data().toString());
	editor->setProperty(PATH_LIST_PROP, QVariant());
}

void RemuxEntryPathItemDelegate::setModelData(QWidget *editor,
					      QAbstractItemModel *model,
					      const QModelIndex &index) const
{
	// We use the PATH_LIST_PROP property to pass a list of
	// path strings from the editor widget into the model's
	// NewPathsToProcessRole. This is only used when paths
	// are selected through the "browse" or "delete" buttons
	// in the editor. If the user enters new text in the
	// text box, we simply pass that text on to the model
	// as normal text data in the default role.
	QVariant pathListProp = editor->property(PATH_LIST_PROP);
	if (pathListProp.isValid()) {
		QStringList list =
			editor->property(PATH_LIST_PROP).toStringList();
		if (isOutput) {
			if (list.size() > 0)
				model->setData(index, list);
		} else
			model->setData(index, list,
				       RemuxEntryRole::NewPathsToProcessRole);
	} else {
		QLineEdit *lineEdit = editor->findChild<QLineEdit *>();
		model->setData(index, lineEdit->text());
	}
}

void RemuxEntryPathItemDelegate::paint(QPainter *painter,
				       const QStyleOptionViewItem &option,
				       const QModelIndex &index) const
{
	RemuxEntryState state =
		index.model()
			->index(index.row(), RemuxEntryColumn::State)
			.data(RemuxEntryRole::EntryStateRole)
			.value<RemuxEntryState>();

	QStyleOptionViewItem localOption = option;
	initStyleOption(&localOption, index);

	if (isOutput) {
		if (state != Ready) {
			QColor background = localOption.palette.color(
				QPalette::ColorGroup::Disabled,
				QPalette::ColorRole::Window);

			localOption.backgroundBrush = QBrush(background);
		}
	}

	QApplication::style()->drawControl(QStyle::CE_ItemViewItem,
					   &localOption, painter);
}

void RemuxEntryPathItemDelegate::handleBrowse(QWidget *container)
{
	QString OutputPattern = "(*.mp4 *.flv *.mov *.mkv *.ts *.m3u8)";
	QString InputPattern = "(*.flv *.mov *.mkv *.ts *.m3u8)";

	QLineEdit *text = container->findChild<QLineEdit *>();

	QString currentPath = text->text();
	if (currentPath.isEmpty())
		currentPath = defaultPath;

	bool isSet = false;
	if (isOutput) {
		QString newPath = SaveFile(container,
					   QTStr("Remux.SelectTarget"),
					   currentPath, OutputPattern);

		if (!newPath.isEmpty()) {
			container->setProperty(PATH_LIST_PROP,
					       QStringList() << newPath);
			isSet = true;
		}
	} else {
		QStringList paths = OpenFiles(
			container, QTStr("Remux.SelectRecording"), currentPath,
			QTStr("Remux.OBSRecording") + QString(" ") +
				InputPattern);

		if (!paths.empty()) {
			container->setProperty(PATH_LIST_PROP, paths);
			isSet = true;
		}
	}

	if (isSet)
		emit commitData(container);
}

void RemuxEntryPathItemDelegate::handleClear(QWidget *container)
{
	// An empty string list will indicate that the entry is being
	// blanked and should be deleted.
	container->setProperty(PATH_LIST_PROP, QStringList());

	emit commitData(container);
}

void RemuxEntryPathItemDelegate::updateText()
{
	QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(sender());
	QWidget *editor = lineEdit->parentWidget();
	emit commitData(editor);
}

/**********************************************************
  Model - Manages the queue's data
**********************************************************/

int RemuxQueueModel::rowCount(const QModelIndex &) const
{
	return queue.length() + (isProcessing ? 0 : 1);
}

int RemuxQueueModel::columnCount(const QModelIndex &) const
{
	return RemuxEntryColumn::Count;
}

QVariant RemuxQueueModel::data(const QModelIndex &index, int role) const
{
	QVariant result = QVariant();

	if (index.row() >= queue.length()) {
		return QVariant();
	} else if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case RemuxEntryColumn::InputPath:
			result = queue[index.row()].sourcePath;
			break;
		case RemuxEntryColumn::OutputPath:
			result = queue[index.row()].targetPath;
			break;
		}
	} else if (role == Qt::DecorationRole &&
		   index.column() == RemuxEntryColumn::State) {
		result = getIcon(queue[index.row()].state);
	} else if (role == RemuxEntryRole::EntryStateRole) {
		result = queue[index.row()].state;
	}

	return result;
}

QVariant RemuxQueueModel::headerData(int section, Qt::Orientation orientation,
				     int role) const
{
	QVariant result = QVariant();

	if (role == Qt::DisplayRole &&
	    orientation == Qt::Orientation::Horizontal) {
		switch (section) {
		case RemuxEntryColumn::State:
			result = QString();
			break;
		case RemuxEntryColumn::InputPath:
			result = QTStr("Remux.SourceFile");
			break;
		case RemuxEntryColumn::OutputPath:
			result = QTStr("Remux.TargetFile");
			break;
		}
	}

	return result;
}

Qt::ItemFlags RemuxQueueModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = QAbstractTableModel::flags(index);

	if (index.column() == RemuxEntryColumn::InputPath) {
		flags |= Qt::ItemIsEditable;
	} else if (index.column() == RemuxEntryColumn::OutputPath &&
		   index.row() != queue.length()) {
		flags |= Qt::ItemIsEditable;
	}

	return flags;
}

bool RemuxQueueModel::setData(const QModelIndex &index, const QVariant &value,
			      int role)
{
	bool success = false;

	if (role == RemuxEntryRole::NewPathsToProcessRole) {
		QStringList pathList = value.toStringList();

		if (pathList.size() == 0) {
			if (index.row() < queue.size()) {
				beginRemoveRows(QModelIndex(), index.row(),
						index.row());
				queue.removeAt(index.row());
				endRemoveRows();
			}
		} else {
			if (pathList.size() > 1 &&
			    index.row() < queue.length()) {
				queue[index.row()].sourcePath = pathList[0];
				checkInputPath(index.row());

				pathList.removeAt(0);

				success = true;
			}

			if (pathList.size() > 0) {
				int row = index.row();
				int lastRow = row + pathList.size() - 1;
				beginInsertRows(QModelIndex(), row, lastRow);

				for (QString path : pathList) {
					RemuxQueueEntry entry;
					entry.sourcePath = path;

					queue.insert(row, entry);
					row++;
				}
				endInsertRows();

				for (row = index.row(); row <= lastRow; row++) {
					checkInputPath(row);
				}

				success = true;
			}
		}
	} else if (index.row() == queue.length()) {
		QString path = value.toString();

		if (!path.isEmpty()) {
			RemuxQueueEntry entry;
			entry.sourcePath = path;

			beginInsertRows(QModelIndex(), queue.length() + 1,
					queue.length() + 1);
			queue.append(entry);
			endInsertRows();

			checkInputPath(index.row());
			success = true;
		}
	} else {
		QString path = value.toString();

		if (path.isEmpty()) {
			if (index.column() == RemuxEntryColumn::InputPath) {
				beginRemoveRows(QModelIndex(), index.row(),
						index.row());
				queue.removeAt(index.row());
				endRemoveRows();
			}
		} else {
			switch (index.column()) {
			case RemuxEntryColumn::InputPath:
				queue[index.row()].sourcePath =
					value.toString();
				checkInputPath(index.row());
				success = true;
				break;
			case RemuxEntryColumn::OutputPath:
				queue[index.row()].targetPath =
					value.toString();
				emit dataChanged(index, index);
				success = true;
				break;
			}
		}
	}

	return success;
}

QVariant RemuxQueueModel::getIcon(RemuxEntryState state)
{
	QVariant icon;
	QStyle *style = QApplication::style();

	switch (state) {
	case RemuxEntryState::Complete:
		icon = style->standardIcon(QStyle::SP_DialogApplyButton);
		break;

	case RemuxEntryState::InProgress:
		icon = style->standardIcon(QStyle::SP_ArrowRight);
		break;

	case RemuxEntryState::Error:
		icon = style->standardIcon(QStyle::SP_DialogCancelButton);
		break;

	case RemuxEntryState::InvalidPath:
		icon = style->standardIcon(QStyle::SP_MessageBoxWarning);
		break;

	default:
		break;
	}

	return icon;
}

void RemuxQueueModel::checkInputPath(int row)
{
	RemuxQueueEntry &entry = queue[row];

	if (entry.sourcePath.isEmpty()) {
		entry.state = RemuxEntryState::Empty;
	} else {
		entry.sourcePath = QDir::toNativeSeparators(entry.sourcePath);
		QFileInfo fileInfo(entry.sourcePath);
		if (fileInfo.exists())
			entry.state = RemuxEntryState::Ready;
		else
			entry.state = RemuxEntryState::InvalidPath;

		if (entry.state == RemuxEntryState::Ready)
			entry.targetPath = QDir::toNativeSeparators(
				fileInfo.path() + QDir::separator() +
				fileInfo.completeBaseName() + ".mp4");
	}

	if (entry.state == RemuxEntryState::Ready && isProcessing)
		entry.state = RemuxEntryState::Pending;

	emit dataChanged(index(row, 0), index(row, RemuxEntryColumn::Count));
}

QFileInfoList RemuxQueueModel::checkForOverwrites() const
{
	QFileInfoList list;

	for (const RemuxQueueEntry &entry : queue) {
		if (entry.state == RemuxEntryState::Ready) {
			QFileInfo fileInfo(entry.targetPath);
			if (fileInfo.exists()) {
				list.append(fileInfo);
			}
		}
	}

	return list;
}

bool RemuxQueueModel::checkForErrors() const
{
	bool hasErrors = false;

	for (const RemuxQueueEntry &entry : queue) {
		if (entry.state == RemuxEntryState::Error) {
			hasErrors = true;
			break;
		}
	}

	return hasErrors;
}

void RemuxQueueModel::clearAll()
{
	beginRemoveRows(QModelIndex(), 0, queue.size() - 1);
	queue.clear();
	endRemoveRows();
}

void RemuxQueueModel::clearFinished()
{
	int index = 0;

	for (index = 0; index < queue.size(); index++) {
		const RemuxQueueEntry &entry = queue[index];
		if (entry.state == RemuxEntryState::Complete) {
			beginRemoveRows(QModelIndex(), index, index);
			queue.removeAt(index);
			endRemoveRows();
			index--;
		}
	}
}

bool RemuxQueueModel::canClearFinished() const
{
	bool canClearFinished = false;
	for (const RemuxQueueEntry &entry : queue)
		if (entry.state == RemuxEntryState::Complete) {
			canClearFinished = true;
			break;
		}

	return canClearFinished;
}

void RemuxQueueModel::beginProcessing()
{
	for (RemuxQueueEntry &entry : queue)
		if (entry.state == RemuxEntryState::Ready)
			entry.state = RemuxEntryState::Pending;

	// Signal that the insertion point no longer exists.
	beginRemoveRows(QModelIndex(), queue.length(), queue.length());
	endRemoveRows();

	isProcessing = true;

	emit dataChanged(index(0, RemuxEntryColumn::State),
			 index(queue.length(), RemuxEntryColumn::State));
}

void RemuxQueueModel::endProcessing()
{
	for (RemuxQueueEntry &entry : queue) {
		if (entry.state == RemuxEntryState::Pending) {
			entry.state = RemuxEntryState::Ready;
		}
	}

	// Signal that the insertion point exists again.

	if (!autoRemux) {
		beginInsertRows(QModelIndex(), queue.length(), queue.length());
		endInsertRows();
	}

	isProcessing = false;

	emit dataChanged(index(0, RemuxEntryColumn::State),
			 index(queue.length(), RemuxEntryColumn::State));
}

bool RemuxQueueModel::beginNextEntry(QString &inputPath, QString &outputPath)
{
	bool anyStarted = false;

	for (int row = 0; row < queue.length(); row++) {
		RemuxQueueEntry &entry = queue[row];
		if (entry.state == RemuxEntryState::Pending) {
			entry.state = RemuxEntryState::InProgress;

			inputPath = entry.sourcePath;
			outputPath = entry.targetPath;

			QModelIndex index =
				this->index(row, RemuxEntryColumn::State);
			emit dataChanged(index, index);

			anyStarted = true;
			break;
		}
	}

	return anyStarted;
}

void RemuxQueueModel::finishEntry(bool success)
{
	for (int row = 0; row < queue.length(); row++) {
		RemuxQueueEntry &entry = queue[row];
		if (entry.state == RemuxEntryState::InProgress) {
			if (success)
				entry.state = RemuxEntryState::Complete;
			else
				entry.state = RemuxEntryState::Error;

			QModelIndex index =
				this->index(row, RemuxEntryColumn::State);
			emit dataChanged(index, index);

			break;
		}
	}
}

/**********************************************************
  The actual remux window implementation
**********************************************************/

OBSRemux::OBSRemux(const char *path, QWidget *parent, bool autoRemux_)
	: QDialog(parent),
	  queueModel(new RemuxQueueModel),
	  worker(new RemuxWorker()),
	  ui(new Ui::OBSRemux),
	  recPath(path),
	  autoRemux(autoRemux_)
{
	setAcceptDrops(true);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	ui->progressBar->setVisible(false);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
	ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)
		->setEnabled(false);

	if (autoRemux) {
		resize(280, 40);
		ui->tableView->hide();
		ui->buttonBox->hide();
		ui->label->hide();
	}

	ui->progressBar->setMinimum(0);
	ui->progressBar->setMaximum(1000);
	ui->progressBar->setValue(0);

	ui->tableView->setModel(queueModel);
	ui->tableView->setItemDelegateForColumn(
		RemuxEntryColumn::InputPath,
		new RemuxEntryPathItemDelegate(false, recPath));
	ui->tableView->setItemDelegateForColumn(
		RemuxEntryColumn::OutputPath,
		new RemuxEntryPathItemDelegate(true, recPath));
	ui->tableView->horizontalHeader()->setSectionResizeMode(
		QHeaderView::ResizeMode::Stretch);
	ui->tableView->horizontalHeader()->setSectionResizeMode(
		RemuxEntryColumn::State, QHeaderView::ResizeMode::Fixed);
	ui->tableView->setEditTriggers(
		QAbstractItemView::EditTrigger::CurrentChanged);

	installEventFilter(CreateShortcutFilter());

	ui->buttonBox->button(QDialogButtonBox::Ok)
		->setText(QTStr("Remux.Remux"));
	ui->buttonBox->button(QDialogButtonBox::Reset)
		->setText(QTStr("Remux.ClearFinished"));
	ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)
		->setText(QTStr("Remux.ClearAll"));
	ui->buttonBox->button(QDialogButtonBox::Reset)->setDisabled(true);

	connect(ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()),
		this, SLOT(beginRemux()));
	connect(ui->buttonBox->button(QDialogButtonBox::Reset),
		SIGNAL(clicked()), this, SLOT(clearFinished()));
	connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults),
		SIGNAL(clicked()), this, SLOT(clearAll()));
	connect(ui->buttonBox->button(QDialogButtonBox::Close),
		SIGNAL(clicked()), this, SLOT(close()));

	worker->moveToThread(&remuxer);
	remuxer.start();

	//gcc-4.8 can't use QPointer<RemuxWorker> below
	RemuxWorker *worker_ = worker;
	connect(worker_, &RemuxWorker::updateProgress, this,
		&OBSRemux::updateProgress);
	connect(&remuxer, &QThread::finished, worker_, &QObject::deleteLater);
	connect(worker_, &RemuxWorker::remuxFinished, this,
		&OBSRemux::remuxFinished);
	connect(this, &OBSRemux::remux, worker_, &RemuxWorker::remux);

	// Guessing the GCC bug mentioned above would also affect
	// QPointer<RemuxQueueModel>? Unsure.
	RemuxQueueModel *queueModel_ = queueModel;
	connect(queueModel_,
		SIGNAL(rowsInserted(const QModelIndex &, int, int)), this,
		SLOT(rowCountChanged(const QModelIndex &, int, int)));
	connect(queueModel_, SIGNAL(rowsRemoved(const QModelIndex &, int, int)),
		this, SLOT(rowCountChanged(const QModelIndex &, int, int)));

	QModelIndex index = queueModel->createIndex(0, 1);
	QMetaObject::invokeMethod(ui->tableView, "setCurrentIndex",
				  Qt::QueuedConnection,
				  Q_ARG(const QModelIndex &, index));
}

bool OBSRemux::stopRemux()
{
	if (!worker->isWorking)
		return true;

	// By locking the worker thread's mutex, we ensure that its
	// update poll will be blocked as long as we're in here with
	// the popup open.
	QMutexLocker lock(&worker->updateMutex);

	bool exit = false;

	if (QMessageBox::critical(nullptr, QTStr("Remux.ExitUnfinishedTitle"),
				  QTStr("Remux.ExitUnfinished"),
				  QMessageBox::Yes | QMessageBox::No,
				  QMessageBox::No) == QMessageBox::Yes) {
		exit = true;
	}

	if (exit) {
		// Inform the worker it should no longer be
		// working. It will interrupt accordingly in
		// its next update callback.
		worker->isWorking = false;
	}

	return exit;
}

OBSRemux::~OBSRemux()
{
	stopRemux();
	remuxer.quit();
	remuxer.wait();
}

void OBSRemux::rowCountChanged(const QModelIndex &, int, int)
{
	// See if there are still any rows ready to remux. Change
	// the state of the "go" button accordingly.
	// There must be more than one row, since there will always be
	// at least one row for the empty insertion point.
	if (queueModel->rowCount() > 1) {
		ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
		ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)
			->setEnabled(true);
		ui->buttonBox->button(QDialogButtonBox::Reset)
			->setEnabled(queueModel->canClearFinished());
	} else {
		ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
		ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)
			->setEnabled(false);
		ui->buttonBox->button(QDialogButtonBox::Reset)
			->setEnabled(false);
	}
}

void OBSRemux::dropEvent(QDropEvent *ev)
{
	QStringList urlList;

	for (QUrl url : ev->mimeData()->urls()) {
		QFileInfo fileInfo(url.toLocalFile());

		if (fileInfo.isDir()) {
			QStringList directoryFilter;
			directoryFilter << "*.flv"
					<< "*.mp4"
					<< "*.mov"
					<< "*.mkv"
					<< "*.ts"
					<< "*.m3u8";

			QDirIterator dirIter(fileInfo.absoluteFilePath(),
					     directoryFilter, QDir::Files,
					     QDirIterator::Subdirectories);

			while (dirIter.hasNext()) {
				urlList.append(dirIter.next());
			}
		} else {
			urlList.append(fileInfo.canonicalFilePath());
		}
	}

	if (urlList.empty()) {
		QMessageBox::information(nullptr,
					 QTStr("Remux.NoFilesAddedTitle"),
					 QTStr("Remux.NoFilesAdded"),
					 QMessageBox::Ok);
	} else if (!autoRemux) {
		QModelIndex insertIndex =
			queueModel->index(queueModel->rowCount() - 1,
					  RemuxEntryColumn::InputPath);
		queueModel->setData(insertIndex, urlList,
				    RemuxEntryRole::NewPathsToProcessRole);
	}
}

void OBSRemux::dragEnterEvent(QDragEnterEvent *ev)
{
	if (ev->mimeData()->hasUrls() && !worker->isWorking)
		ev->accept();
}

void OBSRemux::beginRemux()
{
	if (worker->isWorking) {
		stopRemux();
		return;
	}

	bool proceedWithRemux = true;
	QFileInfoList overwriteFiles = queueModel->checkForOverwrites();

	if (!overwriteFiles.empty()) {
		QString message = QTStr("Remux.FileExists");
		message += "\n\n";

		for (QFileInfo fileInfo : overwriteFiles)
			message += fileInfo.canonicalFilePath() + "\n";

		if (OBSMessageBox::question(this,
					    QTStr("Remux.FileExistsTitle"),
					    message) != QMessageBox::Yes)
			proceedWithRemux = false;
	}

	if (!proceedWithRemux)
		return;

	// Set all jobs to "pending" first.
	queueModel->beginProcessing();

	ui->progressBar->setVisible(true);
	ui->buttonBox->button(QDialogButtonBox::Ok)
		->setText(QTStr("Remux.Stop"));
	setAcceptDrops(false);

	remuxNextEntry();
}

void OBSRemux::AutoRemux(QString inFile, QString outFile)
{
	if (inFile != "" && outFile != "" && autoRemux) {
		emit remux(inFile, outFile);
		autoRemuxFile = inFile;
	}
}

void OBSRemux::remuxNextEntry()
{
	worker->lastProgress = 0.f;

	QString inputPath, outputPath;
	if (queueModel->beginNextEntry(inputPath, outputPath)) {
		emit remux(inputPath, outputPath);
	} else {
		queueModel->autoRemux = autoRemux;
		queueModel->endProcessing();

		if (!autoRemux) {
			OBSMessageBox::information(
				this, QTStr("Remux.FinishedTitle"),
				queueModel->checkForErrors()
					? QTStr("Remux.FinishedError")
					: QTStr("Remux.Finished"));
		}

		ui->progressBar->setVisible(autoRemux);
		ui->buttonBox->button(QDialogButtonBox::Ok)
			->setText(QTStr("Remux.Remux"));
		ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)
			->setEnabled(true);
		ui->buttonBox->button(QDialogButtonBox::Reset)
			->setEnabled(queueModel->canClearFinished());
		setAcceptDrops(true);
	}
}

void OBSRemux::closeEvent(QCloseEvent *event)
{
	if (!stopRemux())
		event->ignore();
	else
		QDialog::closeEvent(event);
}

void OBSRemux::reject()
{
	if (!stopRemux())
		return;

	QDialog::reject();
}

void OBSRemux::updateProgress(float percent)
{
	ui->progressBar->setValue(percent * 10);
}

void OBSRemux::remuxFinished(bool success)
{
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);

	queueModel->finishEntry(success);

	if (autoRemux && autoRemuxFile != "") {
		QTimer::singleShot(3000, this, SLOT(close()));
	}

	remuxNextEntry();
}

void OBSRemux::clearFinished()
{
	queueModel->clearFinished();
}

void OBSRemux::clearAll()
{
	queueModel->clearAll();
}

/**********************************************************
  Worker thread - Executes the libobs remux operation as a
                  background process.
**********************************************************/

void RemuxWorker::UpdateProgress(float percent)
{
	if (abs(lastProgress - percent) < 0.1f)
		return;

	emit updateProgress(percent);
	lastProgress = percent;
}

void RemuxWorker::remux(const QString &source, const QString &target)
{
	isWorking = true;

	auto callback = [](void *data, float percent) {
		RemuxWorker *rw = static_cast<RemuxWorker *>(data);

		QMutexLocker lock(&rw->updateMutex);

		rw->UpdateProgress(percent);

		return rw->isWorking;
	};

	bool stopped = false;
	bool success = false;

	media_remux_job_t mr_job = nullptr;
	if (media_remux_job_create(&mr_job, QT_TO_UTF8(source),
				   QT_TO_UTF8(target))) {

		success = media_remux_job_process(mr_job, callback, this);

		media_remux_job_destroy(mr_job);

		stopped = !isWorking;
	}

	isWorking = false;

	emit remuxFinished(!stopped && success);
}
