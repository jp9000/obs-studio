#pragma once

#include <QDialog>
#include <QTextEdit>
#include "obs-app.hpp"

class OBSLogViewer : public QDialog {
	Q_OBJECT

	QPointer<QTextEdit> textArea;

	void InitLog();

private slots:
	void AddLine(int type, const QString &text);
	void ClearText();
	void ToggleShowStartup(bool checked);
	void OpenFile();

public:
	OBSLogViewer(QWidget *parent = 0);
	~OBSLogViewer();
};
