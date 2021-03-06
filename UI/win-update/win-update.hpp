#pragma once

#include <QThread>
#include <QString>

class AutoUpdateThread : public QThread {
	Q_OBJECT

	bool manualUpdate;
	bool user_confirmed = false;

	virtual void run() override;

	void info(const QString &title, const QString &text);
	int queryUpdate(bool manualUpdate, const char *text_utf8);

private slots:
	void infoMsg(const QString &title, const QString &text);
	int queryUpdateSlot(bool manualUpdate, const QString &text);

public:
	AutoUpdateThread(bool manualUpdate_) : manualUpdate(manualUpdate_) {}
};

class WhatsNewInfoThread : public QThread {
	Q_OBJECT

	virtual void run() override;

signals:
	void Result(const QString &text);

public:
	inline WhatsNewInfoThread() {}
};

class WhatsNewBrowserInitThread : public QThread {
	Q_OBJECT

	QString url;

	virtual void run() override;

signals:
	void Result(const QString &url);

public:
	inline WhatsNewBrowserInitThread(const QString &url_) : url(url_) {}
};
