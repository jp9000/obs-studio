#pragma once

#include <QListWidget>
#include <QMouseEvent>
#include <QFocusEvent>

class MixerTree : public QListWidget {
	Q_OBJECT

public:
	explicit MixerTree(QWidget *parent = nullptr);

protected:
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void focusOutEvent(QFocusEvent *event);
};
