#pragma once

#include <QScrollArea>

class QResizeEvent;

class VScrollArea : public QScrollArea {
	Q_OBJECT

public:
	inline VScrollArea(QWidget *parent = nullptr) : QScrollArea(parent)
	{
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};
