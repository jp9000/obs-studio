#pragma once

#include <QDialog>
#include <string>
#include <memory>

#include "auth-base.hpp"

class OAuth : public Auth {
	Q_OBJECT

public:
	inline OAuth(const Def &d) : Auth(d) {}

	static std::shared_ptr<Auth> Login(QWidget *parent,
					   const std::string &service);

protected:
	std::string refresh_token;
	std::string token;
	bool implicit = false;
	uint64_t refresh_expire_time = 0;
	uint64_t expire_time = 0;
	int currentScopeVer = 0;

	virtual void SaveInternal() override;
	virtual bool LoadInternal() override;

	virtual bool RetryLogin() = 0;

	bool RefreshTokenExpired();
	bool TokenExpired();
};

/* ------------------------------------------------------------------------- */

class Ui_BasicOAuthLogin;

class BasicOAuthLogin : public QDialog {
	Q_OBJECT

	Ui_BasicOAuthLogin *ui;

	QString username;
	QString password;

	bool fail = false;

public:
	BasicOAuthLogin(QWidget *parent, QString name = nullptr,
			bool isRetry = false);
	~BasicOAuthLogin();

	inline QString GetUsername() const { return username; };
	inline QString GetPassword() const { return password; };

public slots:
	void loginOAuth();
};

class BasicOAuth : public OAuth {
	Q_OBJECT

public:
	inline BasicOAuth(const Def &d) : OAuth(d) {}

	typedef std::function<std::shared_ptr<Auth>(QWidget *)> login_cb;

	static std::shared_ptr<Auth> Login(QWidget *parent,
					   const std::string &service);

	static void RegisterOAuth(const Def &d, create_cb create,
				  login_cb login);
};

/* ------------------------------------------------------------------------- */

#ifdef BROWSER_AVAILABLE
class QCefWidget;

class BrowserOAuthLogin : public QDialog {
	Q_OBJECT

	QCefWidget *cefWidget = nullptr;
	QString code;
	bool get_token = false;
	bool fail = false;

public:
	BrowserOAuthLogin(QWidget *parent, const std::string &url, bool token);
	~BrowserOAuthLogin();

	inline QString GetCode() const { return code; }
	inline bool LoadFail() const { return fail; }

	virtual int exec() override;

public slots:
	void urlChanged(const QString &url);
};

class BrowserOAuth : public OAuth {
	Q_OBJECT

public:
	inline BrowserOAuth(const Def &d) : OAuth(d) {}

	typedef std::function<std::shared_ptr<Auth>(QWidget *)> login_cb;
	typedef std::function<void()> delete_cookies_cb;

	static std::shared_ptr<Auth> Login(QWidget *parent,
					   const std::string &service);
	static void DeleteCookies(const std::string &service);

	static void RegisterOAuth(const Def &d, create_cb create,
				  login_cb login,
				  delete_cookies_cb delete_cookies);

protected:
	virtual bool RetryLogin() override;

	bool GetToken(const char *url, const std::string &client_id,
		      int scope_ver,
		      const std::string &auth_code = std::string(),
		      bool retry = false);
};

class BrowserOAuthStreamKey : public BrowserOAuth {
	Q_OBJECT

protected:
	std::string key_;

public:
	inline BrowserOAuthStreamKey(const Def &d) : BrowserOAuth(d) {}

	inline const std::string &key() const { return key_; }

	virtual void OnStreamConfig() override;
};
#endif
