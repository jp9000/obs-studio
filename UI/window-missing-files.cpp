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

#include "window-missing-files.hpp"
#include "window-basic-main.hpp"

#include "obs-app.hpp"

#include <QLineEdit>
#include <QToolButton>
#include <QFileDialog>

#include "qt-wrappers.hpp"

enum MissingFilesColumn {
	Source,
	OriginalPath,
	NewPath,
	State,

	Count
};

enum MissingFilesRole { EntryStateRole = Qt::UserRole, NewPathsToProcessRole };

/**********************************************************
  Delegate - Presents cells in the grid.
**********************************************************/

MissingFilesPathItemDelegate::MissingFilesPathItemDelegate(
	bool isOutput, const QString &defaultPath)
	: QStyledItemDelegate(), isOutput(isOutput), defaultPath(defaultPath)
{
}

QWidget *MissingFilesPathItemDelegate::createEditor(
	QWidget *parent, const QStyleOptionViewItem & /* option */,
	const QModelIndex &index) const
{
	QSizePolicy buttonSizePolicy(QSizePolicy::Policy::Minimum,
				     QSizePolicy::Policy::Expanding,
				     QSizePolicy::ControlType::PushButton);

	QWidget *container = new QWidget(parent);

	auto browseCallback = [this, container]() {
		const_cast<MissingFilesPathItemDelegate *>(this)->handleBrowse(
			container);
	};

	auto clearCallback = [this, container]() {
		const_cast<MissingFilesPathItemDelegate *>(this)->handleClear(
			container);
	};

	QHBoxLayout *layout = new QHBoxLayout();
	layout->setMargin(0);
	layout->setSpacing(0);

	QLineEdit *text = new QLineEdit();
	text->setObjectName(QStringLiteral("text"));
	text->setSizePolicy(QSizePolicy(QSizePolicy::Policy::Expanding,
					QSizePolicy::Policy::Expanding,
					QSizePolicy::ControlType::LineEdit));
	layout->addWidget(text);

	QToolButton *browseButton = new QToolButton();
	browseButton->setText("...");
	browseButton->setSizePolicy(buttonSizePolicy);
	layout->addWidget(browseButton);

	container->connect(browseButton, &QToolButton::clicked, browseCallback);

	// The "clear" button is not shown in input cells
	if (isOutput) {
		QToolButton *clearButton = new QToolButton();
		clearButton->setText("X");
		clearButton->setSizePolicy(buttonSizePolicy);
		layout->addWidget(clearButton);

		container->connect(clearButton, &QToolButton::clicked,
				   clearCallback);
	}

	container->setLayout(layout);
	container->setFocusProxy(text);

	UNUSED_PARAMETER(index);

	return container;
}

void MissingFilesPathItemDelegate::setEditorData(QWidget *editor,
						 const QModelIndex &index) const
{
	QLineEdit *text = editor->findChild<QLineEdit *>();
	text->setText(index.data().toString());

	editor->setProperty(PATH_LIST_PROP, QVariant());
}

void MissingFilesPathItemDelegate::setModelData(QWidget *editor,
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
			model->setData(index, list);
		} else
			model->setData(index, list,
				       MissingFilesRole::NewPathsToProcessRole);
	} else {
		QLineEdit *lineEdit = editor->findChild<QLineEdit *>();
		model->setData(index, lineEdit->text(), 0);
	}
}

void MissingFilesPathItemDelegate::paint(QPainter *painter,
					 const QStyleOptionViewItem &option,
					 const QModelIndex &index) const
{
	QStyleOptionViewItem localOption = option;
	initStyleOption(&localOption, index);

	QApplication::style()->drawControl(QStyle::CE_ItemViewItem,
					   &localOption, painter);
}

void MissingFilesPathItemDelegate::handleBrowse(QWidget *container)
{

	QLineEdit *text = container->findChild<QLineEdit *>();

	QString currentPath = text->text();
	if (currentPath.isEmpty() ||
	    currentPath.compare(QTStr("MissingFiles.Clear")) == 0)
		currentPath = defaultPath;

	bool isSet = false;
	if (isOutput) {
		QString newPath = QFileDialog::getOpenFileName(
			container, QTStr("MissingFiles.SelectFile"),
			currentPath, nullptr);

		if (!newPath.isEmpty()) {
			container->setProperty(PATH_LIST_PROP,
					       QStringList() << newPath);
			isSet = true;
		}
	}

	if (isSet)
		emit commitData(container);
}

void MissingFilesPathItemDelegate::handleClear(QWidget *container)
{
	// An empty string list will indicate that the entry is being
	// blanked and should be deleted.
	container->setProperty(PATH_LIST_PROP,
			       QStringList() << QTStr("MissingFiles.Clear"));
	container->findChild<QLineEdit *>()->clearFocus();
	((QWidget *)container->parent())->setFocus();
	emit commitData(container);
}

/**
	Model
**/

MissingFilesModel::MissingFilesModel(QObject *parent)
	: QAbstractTableModel(parent)
{
	QStyle *style = QApplication::style();

	warningIcon = style->standardIcon(QStyle::SP_MessageBoxWarning);
}

int MissingFilesModel::rowCount(const QModelIndex &) const
{
	return files.length();
}

int MissingFilesModel::columnCount(const QModelIndex &) const
{
	return MissingFilesColumn::Count;
}

int MissingFilesModel::found() const
{
	int res = 0;

	for (int i = 0; i < files.length(); i++) {
		if (files[i].state != Missing && files[i].state != Cleared)
			res++;
	}

	return res;
}

QVariant MissingFilesModel::data(const QModelIndex &index, int role) const
{
	QVariant result = QVariant();

	if (index.row() >= files.length()) {
		return QVariant();
	} else if (role == Qt::DisplayRole) {
		QFileInfo fi(files[index.row()].originalPath);

		switch (index.column()) {
		case MissingFilesColumn::Source:
			result = files[index.row()].source;
			break;
		case MissingFilesColumn::OriginalPath:
			result = fi.fileName();
			break;
		case MissingFilesColumn::NewPath:
			result = files[index.row()].newPath;
			break;
		case MissingFilesColumn::State:
			switch (files[index.row()].state) {
			case MissingFilesState::Missing:
				result = QTStr("MissingFiles.Missing");
				break;

			case MissingFilesState::Replaced:
				result = QTStr("MissingFiles.Replaced");
				break;

			case MissingFilesState::Found:
				result = QTStr("MissingFiles.Found");
				break;

			case MissingFilesState::Cleared:
				result = QTStr("MissingFiles.Cleared");
				break;
			}
			break;
		}
	} else if (role == Qt::DecorationRole &&
		   index.column() == MissingFilesColumn::Source) {
		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		obs_source_t *source = obs_get_source_by_name(
			files[index.row()].source.toStdString().c_str());

		result = main->GetSourceIcon(obs_source_get_id(source));

		obs_source_release(source);
	} else if (role == Qt::FontRole &&
		   index.column() == MissingFilesColumn::State) {
		QFont font = QFont();
		font.setBold(true);

		result = font;
	} else if (role == Qt::ToolTipRole &&
		   index.column() == MissingFilesColumn::State) {
		switch (files[index.row()].state) {
		case MissingFilesState::Missing:
			result = QTStr("MissingFiles.Missing");
			break;

		case MissingFilesState::Replaced:
			result = QTStr("MissingFiles.Replaced");
			break;

		case MissingFilesState::Found:
			result = QTStr("MissingFiles.Found");
			break;

		case MissingFilesState::Cleared:
			result = QTStr("MissingFiles.Cleared");
			break;

		default:
			break;
		}
	} else if (role == Qt::ToolTipRole) {
		switch (index.column()) {
		case MissingFilesColumn::OriginalPath:
			result = files[index.row()].originalPath;
			break;
		case MissingFilesColumn::NewPath:
			result = files[index.row()].newPath;
			break;
		default:
			break;
		}
	}

	return result;
}

Qt::ItemFlags MissingFilesModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = QAbstractTableModel::flags(index);

	if (index.column() == MissingFilesColumn::OriginalPath) {
		flags &= ~Qt::ItemIsEditable;
	} else if (index.column() == MissingFilesColumn::NewPath &&
		   index.row() != files.length()) {
		flags |= Qt::ItemIsEditable;
	}

	return flags;
}

void MissingFilesModel::fileCheckLoop(QList<MissingFileEntry> files,
				      QString path, bool skipPrompt)
{
	loop = false;
	QUrl url = QUrl().fromLocalFile(path);
	QString dir =
		url.toDisplayString(QUrl::RemoveScheme | QUrl::RemoveFilename |
				    QUrl::PreferLocalFile);

	bool prompted = skipPrompt;

	for (int i = 0; i < files.length(); i++) {
		if (files[i].state != MissingFilesState::Missing)
			continue;

		QUrl origFile = QUrl().fromLocalFile(files[i].originalPath);
		QString filename = origFile.fileName();
		QString testFile = dir + filename;

		if (os_file_exists(testFile.toStdString().c_str())) {
			if (!prompted) {
				QMessageBox::StandardButton button =
					QMessageBox::question(
						nullptr,
						QTStr("MissingFiles.AutoSearch"),
						QTStr("MissingFiles.AutoSearchText"));

				if (button == QMessageBox::No)
					break;

				prompted = true;
			}
			QModelIndex in = index(i, MissingFilesColumn::NewPath);
			setData(in, testFile, 0);
		}
	}
	loop = true;
}

bool MissingFilesModel::setData(const QModelIndex &index, const QVariant &value,
				int role)
{
	bool success = false;

	if (role == MissingFilesRole::NewPathsToProcessRole) {
		QStringList list = value.toStringList();

		int row = index.row() + 1;
		beginInsertRows(QModelIndex(), row, row);

		MissingFileEntry entry;
		entry.originalPath = list[0].replace("\\", "/");
		entry.source = list[1];

		files.insert(row, entry);
		row++;

		endInsertRows();

		success = true;
	} else {
		QString path = value.toString();
		if (index.column() == MissingFilesColumn::NewPath) {
			files[index.row()].newPath = value.toString();
			QString fileName = QUrl(path).fileName();
			QString origFileName =
				QUrl(files[index.row()].originalPath).fileName();

			if (path.isEmpty()) {
				files[index.row()].state =
					MissingFilesState::Missing;
			} else if (path.compare(QTStr("MissingFiles.Clear")) ==
				   0) {
				files[index.row()].state =
					MissingFilesState::Cleared;
			} else if (fileName.compare(origFileName) == 0) {
				files[index.row()].state =
					MissingFilesState::Found;

				if (loop)
					fileCheckLoop(files, path, false);
			} else {
				files[index.row()].state =
					MissingFilesState::Replaced;

				if (loop)
					fileCheckLoop(files, path, false);
			}

			emit dataChanged(index, index);
			success = true;
		}
	}

	return success;
}

QVariant MissingFilesModel::headerData(int section, Qt::Orientation orientation,
				       int role) const
{
	QVariant result = QVariant();

	if (role == Qt::DisplayRole &&
	    orientation == Qt::Orientation::Horizontal) {
		switch (section) {
		case MissingFilesColumn::State:
			result = QTStr("MissingFiles.State");
			break;
		case MissingFilesColumn::Source:
			result = QTStr("Basic.Main.Source");
			break;
		case MissingFilesColumn::OriginalPath:
			result = QTStr("MissingFiles.MissingFile");
			break;
		case MissingFilesColumn::NewPath:
			result = QTStr("MissingFiles.NewFile");
			break;
		}
	}

	return result;
}

OBSMissingFiles::OBSMissingFiles(obs_missing_files_t *files, QWidget *parent)
	: QDialog(parent),
	  filesModel(new MissingFilesModel),
	  ui(new Ui::OBSMissingFiles)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	ui->tableView->setModel(filesModel);
	ui->tableView->setItemDelegateForColumn(
		MissingFilesColumn::OriginalPath,
		new MissingFilesPathItemDelegate(false, ""));
	ui->tableView->setItemDelegateForColumn(
		MissingFilesColumn::NewPath,
		new MissingFilesPathItemDelegate(true, ""));
	ui->tableView->horizontalHeader()->setSectionResizeMode(
		QHeaderView::ResizeMode::Stretch);
	ui->tableView->horizontalHeader()->setSectionResizeMode(
		MissingFilesColumn::Source,
		QHeaderView::ResizeMode::ResizeToContents);
	ui->tableView->horizontalHeader()->setMaximumSectionSize(width() / 3);
	ui->tableView->horizontalHeader()->setSectionResizeMode(
		MissingFilesColumn::State,
		QHeaderView::ResizeMode::ResizeToContents);
	ui->tableView->setEditTriggers(
		QAbstractItemView::EditTrigger::CurrentChanged);

	ui->warningIcon->setPixmap(
		filesModel->warningIcon.pixmap(QSize(32, 32)));

	for (size_t i = 0; i < obs_missing_files_count(files); i++) {
		obs_missing_file_t *f =
			obs_missing_files_get_file(files, (int)i);

		const char *oldPath = obs_missing_file_get_path(f);
		const char *name = obs_missing_file_get_source_name(f);

		addMissingFile(oldPath, name);
	}

	QString found = QTStr("MissingFiles.NumFound");
	found.replace("$1", "0");
	found.replace("$2", QString::number(obs_missing_files_count(files)));

	ui->found->setText(found);

	fileStore = files;

	connect(ui->doneButton, &QPushButton::clicked, this,
		&OBSMissingFiles::saveFiles);
	connect(ui->browseButton, &QPushButton::clicked, this,
		&OBSMissingFiles::browseFolders);
	connect(ui->cancelButton, &QPushButton::clicked, this,
		&OBSMissingFiles::close);
	connect(filesModel, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this,
		SLOT(dataChanged()));

	QModelIndex index = filesModel->createIndex(0, 1);
	QMetaObject::invokeMethod(ui->tableView, "setCurrentIndex",
				  Qt::QueuedConnection,
				  Q_ARG(const QModelIndex &, index));
}

OBSMissingFiles::~OBSMissingFiles()
{
	obs_missing_files_destroy(fileStore);
}

void OBSMissingFiles::addMissingFile(const char *originalPath,
				     const char *sourceName)
{
	QStringList list;

	list.append(originalPath);
	list.append(sourceName);

	QModelIndex insertIndex = filesModel->index(filesModel->rowCount() - 1,
						    MissingFilesColumn::Source);

	filesModel->setData(insertIndex, list,
			    MissingFilesRole::NewPathsToProcessRole);
}

void OBSMissingFiles::saveFiles()
{
	for (int i = 0; i < filesModel->files.length(); i++) {
		MissingFilesState state = filesModel->files[i].state;
		if (state != MissingFilesState::Missing) {
			obs_missing_file_t *f =
				obs_missing_files_get_file(fileStore, i);

			QString path = filesModel->files[i].newPath;

			if (state == MissingFilesState::Cleared) {
				obs_missing_file_issue_callback(f, "");
			} else {
				char *p = bstrdup(path.toStdString().c_str());
				obs_missing_file_issue_callback(f, p);
				bfree(p);
			}
		}
	}

	QDialog::accept();
}

void OBSMissingFiles::browseFolders()
{
	QString dir = QFileDialog::getExistingDirectory(
		this, QTStr("MissingFiles.SelectDir"), "",
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (dir != "") {
		dir += "/";
		filesModel->fileCheckLoop(filesModel->files, dir, true);
	}
}

void OBSMissingFiles::dataChanged()
{
	QString found = QTStr("MissingFiles.NumFound");
	found.replace("$1", QString::number(filesModel->found()));
	found.replace("$2",
		      QString::number(obs_missing_files_count(fileStore)));

	ui->found->setText(found);

	ui->tableView->resizeColumnToContents(MissingFilesColumn::State);
	ui->tableView->resizeColumnToContents(MissingFilesColumn::Source);
}

QIcon OBSMissingFiles::GetWarningIcon()
{
	return filesModel->warningIcon;
}

void OBSMissingFiles::SetWarningIcon(const QIcon &icon)
{
	ui->warningIcon->setPixmap(icon.pixmap(QSize(32, 32)));
	filesModel->warningIcon = icon;
}
