#pragma once

#include <QScrollArea>

class QResizeEvent;

class HScrollArea : public QScrollArea {
	Q_OBJECT

public:
	inline HScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
	{
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};
