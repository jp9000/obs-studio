#pragma once

#include <QComboBox>
#include <QInputEvent>
#include <QtCore/QObject>

class ComboBoxIgnoreScroll : public QComboBox {
	Q_OBJECT

public:
	ComboBoxIgnoreScroll(QWidget *parent = nullptr);

protected:
	virtual void wheelEvent(QWheelEvent *event) override;
};
