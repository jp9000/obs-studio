#include "obs.hpp"
#include "scene-tree.hpp"
#include "obs-app.hpp"

#include <QSizePolicy>
#include <QScrollBar>
#include <QDropEvent>
#include <QPushButton>
#include <QTimer>

SceneTree::SceneTree(QWidget *parent_) : QListWidget(parent_)
{
	installEventFilter(this);
	setDragDropMode(InternalMove);
	setMovement(QListView::Snap);
}

void SceneTree::SetGridMode(bool grid)
{
	config_set_bool(App()->GlobalConfig(), "BasicWindow", "gridMode", grid);
	parent()->setProperty("gridMode", grid);
	gridMode = grid;

	if (gridMode) {
		setResizeMode(QListView::Adjust);
		setViewMode(QListView::IconMode);
		setUniformItemSizes(true);
		setStyleSheet("*{padding: 0; margin: 0;}");
	} else {
		setViewMode(QListView::ListMode);
		setResizeMode(QListView::Fixed);
		setStyleSheet("");
	}

	QResizeEvent event(size(), size());
	resizeEvent(&event);
}

bool SceneTree::GetGridMode()
{
	return gridMode;
}

void SceneTree::SetGridItemWidth(int width)
{
	maxWidth = width;
}

void SceneTree::SetGridItemHeight(int height)
{
	itemHeight = height;
}

int SceneTree::GetGridItemWidth()
{
	return maxWidth;
}

int SceneTree::GetGridItemHeight()
{
	return itemHeight;
}

bool SceneTree::eventFilter(QObject *obj, QEvent *event)
{
	return QObject::eventFilter(obj, event);
}

void SceneTree::resizeEvent(QResizeEvent *event)
{
	if (gridMode) {
		int scrollWid = verticalScrollBar()->sizeHint().width();
		int h = visualItemRect(item(count() - 1)).bottom();

		if (h < height()) {
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			scrollWid = 0;
		} else {
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		}

		int wid = contentsRect().width() - scrollWid - 1;
		int items = (int)ceil((float)wid / maxWidth);
		int itemWidth = wid / items;

		setGridSize(QSize(itemWidth, itemHeight));

		for (int i = 0; i < count(); i++) {
			item(i)->setSizeHint(QSize(itemWidth, itemHeight));
		}
	} else {
		setGridSize(QSize());
		setSpacing(0);
		for (int i = 0; i < count(); i++) {
			item(i)->setData(Qt::SizeHintRole, QVariant());
		}
	}

	QListWidget::resizeEvent(event);
}

void SceneTree::startDrag(Qt::DropActions supportedActions)
{
	QListWidget::startDrag(supportedActions);
}

void SceneTree::dropEvent(QDropEvent *event)
{
	if (event->source() != this) {
		QListWidget::dropEvent(event);
		return;
	}

	if (gridMode) {
		int scrollWid = verticalScrollBar()->sizeHint().width();
		int h = visualItemRect(item(count() - 1)).bottom();

		if (h < height()) {
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			scrollWid = 0;
		} else {
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		}

		float wid = contentsRect().width() - scrollWid - 1;

		QPoint point = event->pos();

		int x = (float)point.x() / wid * ceil(wid / maxWidth);
		int y = point.y() / itemHeight;

		int r = x + y * ceil(wid / maxWidth);

		QListWidgetItem *item = takeItem(selectedIndexes()[0].row());
		insertItem(r, item);
		setCurrentItem(item);
		resize(size());
	}

	QListWidget::dropEvent(event);

	QTimer::singleShot(100, [this]() { emit scenesReordered(); });
}

void SceneTree::dragMoveEvent(QDragMoveEvent *event)
{
	if (gridMode) {
		int scrollWid = verticalScrollBar()->sizeHint().width();
		int h = visualItemRect(item(count() - 1)).bottom();

		if (h < height()) {
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			scrollWid = 0;
		} else {
			setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		}

		float wid = contentsRect().width() - scrollWid - 1;

		QPoint point = event->pos();

		int x = (float)point.x() / wid * ceil(wid / maxWidth);
		int y = point.y() / itemHeight;

		int r = x + y * ceil(wid / maxWidth);
		int orig = selectedIndexes()[0].row();

		for (int i = 0; i < count(); i++) {
			auto *wItem = item(i);

			if (wItem->isSelected())
				continue;

			QModelIndex index = indexFromItem(wItem);

			int off = (i >= r ? 1 : 0) -
				  (i > orig && i > r ? 1 : 0) -
				  (i > orig && i == r ? 2 : 0);

			int xPos = (i + off) % (int)ceil(wid / maxWidth);
			int yPos = (i + off) / (int)ceil(wid / maxWidth);
			QSize g = gridSize();

			QPoint position(xPos * g.width(), yPos * g.height());
			setPositionForIndex(position, index);
		}
	}

	QListWidget::dragMoveEvent(event);
}

void SceneTree::rowsInserted(const QModelIndex &parent, int start, int end)
{
	QResizeEvent event(size(), size());
	SceneTree::resizeEvent(&event);

	QListWidget::rowsInserted(parent, start, end);
}
