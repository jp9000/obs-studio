#pragma once

#include <QDockWidget>

class OBSDock : public QDockWidget {
	Q_OBJECT

public:
	inline OBSDock(QWidget *parent = nullptr) : QDockWidget(parent) {}

	virtual void closeEvent(QCloseEvent *event);
};
