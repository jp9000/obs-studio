/******************************************************************************
    Copyright (C) 2019-2020 by Dillon Pentz <dillon@vodbox.io>

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

#include "window-importer.hpp"

#include "obs-app.hpp"

#include <QPushButton>
#include <QLineEdit>
#include <QToolButton>
#include <QMimeData>
#include <QStyledItemDelegate>
#include <QDirIterator>
#include <QDropEvent>

#include "qt-wrappers.hpp"
#include "importers/importers.hpp"

enum ImporterColumn {
	Selected,
	Name,
	Path,
	Program,

	Count
};

enum ImporterEntryRole {
	EntryStateRole = Qt::UserRole,
	NewPath,
	AutoPath,
	CheckEmpty
};

/**********************************************************
  Delegate - Presents cells in the grid.
**********************************************************/

ImporterEntryPathItemDelegate::ImporterEntryPathItemDelegate()
	: QStyledItemDelegate()
{
}

QWidget *ImporterEntryPathItemDelegate::createEditor(
	QWidget *parent, const QStyleOptionViewItem & /* option */,
	const QModelIndex &index) const
{
	bool empty = index.model()
			     ->index(index.row(), ImporterColumn::Path)
			     .data(ImporterEntryRole::CheckEmpty)
			     .value<bool>();

	QSizePolicy buttonSizePolicy(QSizePolicy::Policy::Minimum,
				     QSizePolicy::Policy::Expanding,
				     QSizePolicy::ControlType::PushButton);

	QWidget *container = new QWidget(parent);

	auto browseCallback = [this, container]() {
		const_cast<ImporterEntryPathItemDelegate *>(this)->handleBrowse(
			container);
	};

	auto clearCallback = [this, container]() {
		const_cast<ImporterEntryPathItemDelegate *>(this)->handleClear(
			container);
	};

	QHBoxLayout *layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	QLineEdit *text = new QLineEdit();
	text->setObjectName(QStringLiteral("text"));
	text->setSizePolicy(QSizePolicy(QSizePolicy::Policy::Expanding,
					QSizePolicy::Policy::Expanding,
					QSizePolicy::ControlType::LineEdit));
	layout->addWidget(text);

	QObject::connect(text, SIGNAL(editingFinished()), this,
			 SLOT(updateText()));

	QToolButton *browseButton = new QToolButton();
	browseButton->setText("...");
	browseButton->setSizePolicy(buttonSizePolicy);
	layout->addWidget(browseButton);

	container->connect(browseButton, &QToolButton::clicked, browseCallback);

	// The "clear" button is not shown in output cells
	// or the insertion point's input cell.
	if (!empty) {
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

void ImporterEntryPathItemDelegate::setEditorData(
	QWidget *editor, const QModelIndex &index) const
{
	QLineEdit *text = editor->findChild<QLineEdit *>();
	text->setText(index.data().toString());
	editor->setProperty(PATH_LIST_PROP, QVariant());
}

void ImporterEntryPathItemDelegate::setModelData(QWidget *editor,
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
		model->setData(index, list, ImporterEntryRole::NewPath);
	} else {
		QLineEdit *lineEdit = editor->findChild<QLineEdit *>();
		model->setData(index, lineEdit->text());
	}
}

void ImporterEntryPathItemDelegate::paint(QPainter *painter,
					  const QStyleOptionViewItem &option,
					  const QModelIndex &index) const
{
	QStyleOptionViewItem localOption = option;
	initStyleOption(&localOption, index);

	QApplication::style()->drawControl(QStyle::CE_ItemViewItem,
					   &localOption, painter);
}

void ImporterEntryPathItemDelegate::handleBrowse(QWidget *container)
{
	QString Pattern = "(*.json *.bpres *.xml *.xconfig)";

	QLineEdit *text = container->findChild<QLineEdit *>();

	QString currentPath = text->text();

	bool isSet = false;
	QStringList paths = OpenFiles(
		container, QTStr("Importer.SelectCollection"), currentPath,
		QTStr("Importer.Collection") + QString(" ") + Pattern);

	if (!paths.empty()) {
		container->setProperty(PATH_LIST_PROP, paths);
		isSet = true;
	}

	if (isSet)
		emit commitData(container);
}

void ImporterEntryPathItemDelegate::handleClear(QWidget *container)
{
	// An empty string list will indicate that the entry is being
	// blanked and should be deleted.
	container->setProperty(PATH_LIST_PROP, QStringList());

	emit commitData(container);
}

void ImporterEntryPathItemDelegate::updateText()
{
	QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(sender());
	QWidget *editor = lineEdit->parentWidget();
	emit commitData(editor);
}

/**
	Model
**/

int ImporterModel::rowCount(const QModelIndex &) const
{
	return options.length() + 1;
}

int ImporterModel::columnCount(const QModelIndex &) const
{
	return ImporterColumn::Count;
}

QVariant ImporterModel::data(const QModelIndex &index, int role) const
{
	QVariant result = QVariant();

	if (index.row() >= options.length()) {
		if (role == ImporterEntryRole::CheckEmpty)
			result = true;
		else
			return QVariant();
	} else if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case ImporterColumn::Path:
			result = options[index.row()].path;
			break;
		case ImporterColumn::Program:
			result = options[index.row()].program;
			break;
		case ImporterColumn::Name:
			result = options[index.row()].name;
		}
	} else if (role == Qt::EditRole) {
		if (index.column() == ImporterColumn::Name) {
			result = options[index.row()].name;
		}
	} else if (role == Qt::CheckStateRole) {
		switch (index.column()) {
		case ImporterColumn::Selected:
			if (options[index.row()].program != "")
				result = options[index.row()].selected
						 ? Qt::Checked
						 : Qt::Unchecked;
			else
				result = Qt::Unchecked;
		}
	} else if (role == ImporterEntryRole::CheckEmpty) {
		result = options[index.row()].empty;
	}

	return result;
}

Qt::ItemFlags ImporterModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = QAbstractTableModel::flags(index);

	if (index.column() == ImporterColumn::Selected &&
	    index.row() != options.length()) {
		flags |= Qt::ItemIsUserCheckable;
	} else if (index.column() == ImporterColumn::Path ||
		   (index.column() == ImporterColumn::Name &&
		    index.row() != options.length())) {
		flags |= Qt::ItemIsEditable;
	}

	return flags;
}

void ImporterModel::checkInputPath(int row)
{
	ImporterEntry &entry = options[row];

	if (entry.path.isEmpty()) {
		entry.program = "";
		entry.empty = true;
		entry.selected = false;
		entry.name = "";
	} else {
		entry.empty = false;

		std::string program = DetectProgram(entry.path.toStdString());
		entry.program = QTStr(program.c_str());

		if (program.empty()) {
			entry.selected = false;
		} else {
			std::string name =
				GetSCName(entry.path.toStdString(), program);
			entry.name = name.c_str();
		}
	}

	emit dataChanged(index(row, 0), index(row, ImporterColumn::Count));
}

bool ImporterModel::setData(const QModelIndex &index, const QVariant &value,
			    int role)
{
	if (role == ImporterEntryRole::NewPath) {
		QStringList list = value.toStringList();

		if (list.size() == 0) {
			if (index.row() < options.size()) {
				beginRemoveRows(QModelIndex(), index.row(),
						index.row());
				options.removeAt(index.row());
				endRemoveRows();
			}
		} else {
			if (list.size() > 0 && index.row() < options.length()) {
				options[index.row()].path = list[0];
				checkInputPath(index.row());

				list.removeAt(0);
			}

			if (list.size() > 0) {
				int row = index.row();
				int lastRow = row + list.size() - 1;
				beginInsertRows(QModelIndex(), row, lastRow);

				for (QString path : list) {
					ImporterEntry entry;
					entry.path = path;

					options.insert(row, entry);

					row++;
				}

				endInsertRows();

				for (row = index.row(); row <= lastRow; row++) {
					checkInputPath(row);
				}
			}
		}
	} else if (index.row() == options.length()) {
		QString path = value.toString();

		if (!path.isEmpty()) {
			ImporterEntry entry;
			entry.path = path;
			entry.selected = role != ImporterEntryRole::AutoPath;
			entry.empty = false;

			beginInsertRows(QModelIndex(), options.length() + 1,
					options.length() + 1);
			options.append(entry);
			endInsertRows();

			checkInputPath(index.row());
		}
	} else if (index.column() == ImporterColumn::Selected) {
		bool select = value.toBool();

		options[index.row()].selected = select;
	} else if (index.column() == ImporterColumn::Path) {
		QString path = value.toString();
		options[index.row()].path = path;

		checkInputPath(index.row());
	} else if (index.column() == ImporterColumn::Name) {
		QString name = value.toString();
		options[index.row()].name = name;
	}

	emit dataChanged(index, index);

	return true;
}

QVariant ImporterModel::headerData(int section, Qt::Orientation orientation,
				   int role) const
{
	QVariant result = QVariant();

	if (role == Qt::DisplayRole &&
	    orientation == Qt::Orientation::Horizontal) {
		switch (section) {
		case ImporterColumn::Path:
			result = QTStr("Importer.Path");
			break;
		case ImporterColumn::Program:
			result = QTStr("Importer.Program");
			break;
		case ImporterColumn::Name:
			result = QTStr("Name");
		}
	}

	return result;
}

/**
	Window
**/

OBSImporter::OBSImporter(QWidget *parent)
	: QDialog(parent),
	  optionsModel(new ImporterModel),
	  ui(new Ui::OBSImporter)
{
	setAcceptDrops(true);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	ui->tableView->setModel(optionsModel);
	ui->tableView->setItemDelegateForColumn(
		ImporterColumn::Path, new ImporterEntryPathItemDelegate());
	ui->tableView->horizontalHeader()->setSectionResizeMode(
		QHeaderView::ResizeMode::ResizeToContents);
	ui->tableView->horizontalHeader()->setSectionResizeMode(
		ImporterColumn::Path, QHeaderView::ResizeMode::Stretch);

	connect(optionsModel, SIGNAL(dataChanged(QModelIndex, QModelIndex)),
		this, SLOT(dataChanged()));

	ui->tableView->setEditTriggers(
		QAbstractItemView::EditTrigger::CurrentChanged);

	ui->buttonBox->button(QDialogButtonBox::Ok)->setText(QTStr("Import"));
	ui->buttonBox->button(QDialogButtonBox::Open)->setText(QTStr("Add"));

	connect(ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()),
		this, SLOT(importCollections()));
	connect(ui->buttonBox->button(QDialogButtonBox::Open),
		SIGNAL(clicked()), this, SLOT(browseImport()));
	connect(ui->buttonBox->button(QDialogButtonBox::Close),
		SIGNAL(clicked()), this, SLOT(close()));

	ImportersInit();

	bool autoSearchPrompt = config_get_bool(App()->GlobalConfig(),
						"General", "AutoSearchPrompt");

	if (!autoSearchPrompt) {
		QMessageBox::StandardButton button = OBSMessageBox::question(
			parent, QTStr("Importer.AutomaticCollectionPrompt"),
			QTStr("Importer.AutomaticCollectionText"));

		if (button == QMessageBox::Yes) {
			config_set_bool(App()->GlobalConfig(), "General",
					"AutomaticCollectionSearch", true);
		} else {
			config_set_bool(App()->GlobalConfig(), "General",
					"AutomaticCollectionSearch", false);
		}

		config_set_bool(App()->GlobalConfig(), "General",
				"AutoSearchPrompt", true);
	}

	bool autoSearch = config_get_bool(App()->GlobalConfig(), "General",
					  "AutomaticCollectionSearch");

	OBSImporterFiles f;
	if (autoSearch)
		f = ImportersFindFiles();

	for (size_t i = 0; i < f.size(); i++) {
		QString path = f[i].c_str();
		path.replace("\\", "/");
		addImportOption(path, true);
	}

	f.clear();

	ui->tableView->resizeColumnsToContents();

	QModelIndex index =
		optionsModel->createIndex(optionsModel->rowCount() - 1, 2);
	QMetaObject::invokeMethod(ui->tableView, "setCurrentIndex",
				  Qt::QueuedConnection,
				  Q_ARG(const QModelIndex &, index));
}

void OBSImporter::addImportOption(QString path, bool automatic)
{
	QStringList list;

	list.append(path);

	QModelIndex insertIndex = optionsModel->index(
		optionsModel->rowCount() - 1, ImporterColumn::Path);

	optionsModel->setData(insertIndex, list,
			      automatic ? ImporterEntryRole::AutoPath
					: ImporterEntryRole::NewPath);
}

void OBSImporter::dropEvent(QDropEvent *ev)
{
	for (QUrl url : ev->mimeData()->urls()) {
		QFileInfo fileInfo(url.toLocalFile());
		if (fileInfo.isDir()) {

			QDirIterator dirIter(fileInfo.absoluteFilePath(),
					     QDir::Files);

			while (dirIter.hasNext()) {
				addImportOption(dirIter.next(), false);
			}
		} else {
			addImportOption(fileInfo.canonicalFilePath(), false);
		}
	}
}

void OBSImporter::dragEnterEvent(QDragEnterEvent *ev)
{
	if (ev->mimeData()->hasUrls())
		ev->accept();
}

void OBSImporter::browseImport()
{
	QString Pattern = "(*.json *.bpres *.xml *.xconfig)";

	QStringList paths = OpenFiles(
		this, QTStr("Importer.SelectCollection"), "",
		QTStr("Importer.Collection") + QString(" ") + Pattern);

	if (!paths.empty()) {
		for (int i = 0; i < paths.count(); i++) {
			addImportOption(paths[i], false);
		}
	}
}

void OBSImporter::importCollections()
{
	setEnabled(false);

	char dst[512];
	GetConfigPath(dst, 512, "obs-studio/basic/scenes/");

	for (int i = 0; i < optionsModel->rowCount() - 1; i++) {
		int selected = optionsModel->index(i, ImporterColumn::Selected)
				       .data(Qt::CheckStateRole)
				       .value<int>();

		if (selected == Qt::Unchecked)
			continue;

		std::string pathStr =
			optionsModel->index(i, ImporterColumn::Path)
				.data(Qt::DisplayRole)
				.value<QString>()
				.toStdString();
		std::string nameStr =
			optionsModel->index(i, ImporterColumn::Name)
				.data(Qt::DisplayRole)
				.value<QString>()
				.toStdString();

		json11::Json res;
		ImportSC(pathStr, nameStr, res);

		if (res != json11::Json()) {
			json11::Json::object out = res.object_items();
			std::string name = res["name"].string_value();
			std::string file;

			GetUnusedSceneCollectionFile(name, file);

			std::string save = dst;
			save += "/";
			save += file;
			save += ".json";

			std::string out_str = json11::Json(out).dump();

			bool success = os_quick_write_utf8_file(save.c_str(),
								out_str.c_str(),
								out_str.size(),
								false);

			blog(LOG_INFO, "Import Scene Collection: %s (%s) - %s",
			     name.c_str(), file.c_str(),
			     success ? "SUCCESS" : "FAILURE");
		}
	}

	close();
}

void OBSImporter::dataChanged()
{
	ui->tableView->resizeColumnToContents(ImporterColumn::Name);
}
