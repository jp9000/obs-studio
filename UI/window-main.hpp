#pragma once

#include <QMainWindow>

#include <util/config-file.h>

class OBSMainWindow : public QMainWindow {
	Q_OBJECT

public:
	inline OBSMainWindow(QWidget *parent) : QMainWindow(parent) {}

	virtual config_t *Config() const = 0;
	virtual void OBSInit() = 0;

	virtual int GetProfilePath(char *path, size_t size,
				   const char *file) const = 0;
};
