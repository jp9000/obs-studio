#pragma once

#include <QListWidget>
#include <QEvent>
#include <QItemDelegate>

class SceneTree : public QListWidget {
	Q_OBJECT
	Q_PROPERTY(int gridItemWidth READ GetGridItemWidth WRITE
			   SetGridItemWidth DESIGNABLE true)
	Q_PROPERTY(int gridItemHeight READ GetGridItemHeight WRITE
			   SetGridItemHeight DESIGNABLE true)

	bool gridMode = false;
	int maxWidth = 150;
	int itemHeight = 24;

public:
	void SetGridMode(bool grid);
	bool GetGridMode();

	void SetGridItemWidth(int width);
	void SetGridItemHeight(int height);
	int GetGridItemWidth();
	int GetGridItemHeight();

	explicit SceneTree(QWidget *parent = nullptr);

protected:
	virtual bool eventFilter(QObject *obj, QEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void startDrag(Qt::DropActions supportedActions) override;
	virtual void dropEvent(QDropEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void rowsInserted(const QModelIndex &parent, int start,
				  int end) override;

signals:
	void scenesReordered();
};
