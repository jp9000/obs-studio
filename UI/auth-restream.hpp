#pragma once

#include "auth-oauth.hpp"

class BrowserDock;

class RestreamAuth : public OAuthStreamKey {
	Q_OBJECT

	QSharedPointer<BrowserDock> chat;
	QSharedPointer<BrowserDock> info;
	QSharedPointer<BrowserDock> channels;

	QSharedPointer<QAction> chatMenu;
	QSharedPointer<QAction> infoMenu;
	QSharedPointer<QAction> channelMenu;

	bool uiLoaded = false;

	virtual bool RetryLogin() override;

	virtual void SaveInternal() override;
	virtual bool LoadInternal() override;

	bool GetChannelInfo();

	virtual void LoadUI() override;

public:
	RestreamAuth(const Def &d);

	static std::shared_ptr<Auth> Login(QWidget *parent);
};
