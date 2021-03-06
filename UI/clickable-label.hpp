#pragma once

#include <QLabel>
#include <QMouseEvent>

class ClickableLabel : public QLabel {
	Q_OBJECT

public:
	inline ClickableLabel(QWidget *parent = 0) : QLabel(parent) {}

signals:
	void clicked();

protected:
	void mousePressEvent(QMouseEvent *event)
	{
		emit clicked();
		event->accept();
	}
};
