#include "mixer-tree.hpp"

MixerTree::MixerTree(QWidget *parent_) : QListWidget(parent_) {}

void MixerTree::mousePressEvent(QMouseEvent *event)
{
	QListView::mousePressEvent(event);

	QModelIndex idx = indexAt(event->pos());

	if (idx.row() == -1)
		clearSelection();
}

void MixerTree::focusOutEvent(QFocusEvent *event)
{
	QListView::focusOutEvent(event);
	clearSelection();
}
