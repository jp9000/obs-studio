#pragma once

#include <QPushButton>
#include <QUrl>

class UrlPushButton : public QPushButton {
	Q_OBJECT
	Q_PROPERTY(QUrl targetUrl READ targetUrl WRITE setTargetUrl)

public:
	inline UrlPushButton(QWidget *parent = nullptr) : QPushButton(parent) {}
	void setTargetUrl(QUrl url);
	QUrl targetUrl();

protected:
	void mousePressEvent(QMouseEvent *event) override;

private:
	QUrl m_targetUrl;
};
