#pragma once

#include "auth-oauth.hpp"

class BrowserDock;

class TrovoAuthLogin : public OAuthLogin {
	Q_OBJECT

public:
	TrovoAuthLogin(QWidget *parent, const std::string &url, bool token);

	std::string token;
	uint64_t expire_time = 0;

public slots:
	void urlChanged(const QString &url);
};

class TrovoAuth : public OAuthStreamKey {
	Q_OBJECT

	QSharedPointer<BrowserDock> chat;

	QSharedPointer<QAction> chatMenu;

	std::string name;

	bool uiLoaded = false;

	virtual bool RetryLogin() override;

	virtual void SaveInternal() override;
	virtual bool LoadInternal() override;

	bool GetChannelInfo();

	virtual void LoadUI() override;

public:
	TrovoAuth(const Def &d);

	static std::shared_ptr<Auth> Login(QWidget *parent);

	bool CheckToken(int scope_ver);
};
