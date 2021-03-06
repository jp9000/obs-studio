#pragma once

#include <QWidget>
#include <QStyledItemDelegate>
#include <obs.hpp>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class VisibilityCheckBox;

class VisibilityItemWidget : public QWidget {
	Q_OBJECT

private:
	OBSSource source;
	QLabel *label = nullptr;
	VisibilityCheckBox *vis = nullptr;
	QString oldName;

	OBSSignal enabledSignal;
	OBSSignal renamedSignal;

	bool active = false;
	bool selected = false;

	static void OBSSourceEnabled(void *param, calldata_t *data);
	static void OBSSourceRenamed(void *param, calldata_t *data);

private slots:
	void VisibilityClicked(bool visible);
	void SourceEnabled(bool enabled);
	void SourceRenamed(QString name);

public:
	VisibilityItemWidget(obs_source_t *source);

	void SetColor(const QColor &color, bool active, bool selected);
};

class VisibilityItemDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	VisibilityItemDelegate(QObject *parent = nullptr);

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
		   const QModelIndex &index) const override;
};

void SetupVisibilityItem(QListWidget *list, QListWidgetItem *item,
			 obs_source_t *source);
