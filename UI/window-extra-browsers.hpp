#pragma once

#include <QDialog>
#include <QScopedPointer>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>

class Ui_OBSExtraBrowsers;
class ExtraBrowsersModel;

class QCefWidget;

class OBSExtraBrowsers : public QDialog {
	Q_OBJECT

	Ui_OBSExtraBrowsers *ui;
	ExtraBrowsersModel *model;

public:
	OBSExtraBrowsers(QWidget *parent);
	~OBSExtraBrowsers();

	void closeEvent(QCloseEvent *event) override;

public slots:
	void on_apply_clicked();
};

class ExtraBrowsersModel : public QAbstractTableModel {
	Q_OBJECT

public:
	inline ExtraBrowsersModel(QObject *parent = nullptr)
		: QAbstractTableModel(parent)
	{
		Reset();
		QMetaObject::invokeMethod(this, "Init", Qt::QueuedConnection);
	}

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int
	columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
			    int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

	struct Item {
		int prevIdx;
		QString title;
		QString url;
	};

	void TabSelection(bool forward);

	void AddDeleteButton(int idx);
	void Reset();
	void CheckToAdd();
	void UpdateItem(Item &item);
	void DeleteItem();
	void Apply();

	QVector<Item> items;
	QVector<int> deleted;

	QString newTitle;
	QString newURL;

public slots:
	void Init();
};

class ExtraBrowsersDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	inline ExtraBrowsersDelegate(ExtraBrowsersModel *model_)
		: QStyledItemDelegate(nullptr), model(model_)
	{
	}

	QWidget *createEditor(QWidget *parent,
			      const QStyleOptionViewItem &option,
			      const QModelIndex &index) const override;

	void setEditorData(QWidget *editor,
			   const QModelIndex &index) const override;

	bool eventFilter(QObject *object, QEvent *event) override;
	void RevertText(QLineEdit *edit);
	bool UpdateText(QLineEdit *edit);
	bool ValidName(const QString &text) const;

	ExtraBrowsersModel *model;
};
