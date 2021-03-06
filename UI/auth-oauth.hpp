#pragma once

#include <QDialog>
#include <string>
#include <memory>

#include "auth-base.hpp"

class QCefWidget;

class OAuthLogin : public QDialog {
	Q_OBJECT

	QCefWidget *cefWidget = nullptr;
	QString code;
	bool get_token = false;
	bool fail = false;

public:
	OAuthLogin(QWidget *parent, const std::string &url, bool token);
	~OAuthLogin();

	inline QString GetCode() const { return code; }
	inline bool LoadFail() const { return fail; }

	virtual int exec() override;

public slots:
	void urlChanged(const QString &url);
};

class OAuth : public Auth {
	Q_OBJECT

public:
	inline OAuth(const Def &d) : Auth(d) {}

	typedef std::function<std::shared_ptr<Auth>(QWidget *)> login_cb;
	typedef std::function<void()> delete_cookies_cb;

	static std::shared_ptr<Auth> Login(QWidget *parent,
					   const std::string &service);
	static void DeleteCookies(const std::string &service);

	static void RegisterOAuth(const Def &d, create_cb create,
				  login_cb login,
				  delete_cookies_cb delete_cookies);

protected:
	std::string refresh_token;
	std::string token;
	bool implicit = false;
	uint64_t expire_time = 0;
	int currentScopeVer = 0;

	virtual void SaveInternal() override;
	virtual bool LoadInternal() override;

	virtual bool RetryLogin() = 0;
	bool TokenExpired();
	bool GetToken(const char *url, const std::string &client_id,
		      int scope_ver,
		      const std::string &auth_code = std::string(),
		      bool retry = false);
};

class OAuthStreamKey : public OAuth {
	Q_OBJECT

protected:
	std::string key_;

public:
	inline OAuthStreamKey(const Def &d) : OAuth(d) {}

	inline const std::string &key() const { return key_; }

	virtual void OnStreamConfig() override;
};
