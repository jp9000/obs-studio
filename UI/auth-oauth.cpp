#include "auth-oauth.hpp"

#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <qt-wrappers.hpp>
#include <obs-app.hpp>

#include "window-basic-main.hpp"
#include "remote-text.hpp"

#include <unordered_map>

#include <json11.hpp>

using namespace json11;

#include <browser-panel.hpp>
extern QCef *cef;
extern QCefCookieManager *panel_cookies;

/* ------------------------------------------------------------------------- */

OAuthLogin::OAuthLogin(QWidget *parent, const std::string &url, bool token)
	: QDialog(parent), get_token(token)
{
	if (!cef) {
		return;
	}

	setWindowTitle("Auth");
	setMinimumSize(400, 400);
	resize(700, 700);

	Qt::WindowFlags flags = windowFlags();
	Qt::WindowFlags helpFlag = Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags & (~helpFlag));

	OBSBasic::InitBrowserPanelSafeBlock();

	cefWidget = cef->create_widget(nullptr, url, panel_cookies);
	if (!cefWidget) {
		fail = true;
		return;
	}

	connect(cefWidget, SIGNAL(titleChanged(const QString &)), this,
		SLOT(setWindowTitle(const QString &)));
	connect(cefWidget, SIGNAL(urlChanged(const QString &)), this,
		SLOT(urlChanged(const QString &)));

	QPushButton *close = new QPushButton(QTStr("Cancel"));
	connect(close, &QAbstractButton::clicked, this, &QDialog::reject);

	QHBoxLayout *bottomLayout = new QHBoxLayout();
	bottomLayout->addStretch();
	bottomLayout->addWidget(close);
	bottomLayout->addStretch();

	QVBoxLayout *topLayout = new QVBoxLayout(this);
	topLayout->addWidget(cefWidget);
	topLayout->addLayout(bottomLayout);
}

OAuthLogin::~OAuthLogin()
{
	delete cefWidget;
}

int OAuthLogin::exec()
{
	if (cefWidget) {
		return QDialog::exec();
	}

	return QDialog::Rejected;
}

void OAuthLogin::urlChanged(const QString &url)
{
	std::string uri = get_token ? "access_token=" : "code=";
	int code_idx = url.indexOf(uri.c_str());
	if (code_idx == -1)
		return;

	if (url.left(22) != "https://obsproject.com")
		return;

	code_idx += (int)uri.size();

	int next_idx = url.indexOf("&", code_idx);
	if (next_idx != -1)
		code = url.mid(code_idx, next_idx - code_idx);
	else
		code = url.right(url.size() - code_idx);

	accept();
}

/* ------------------------------------------------------------------------- */

struct OAuthInfo {
	Auth::Def def;
	OAuth::login_cb login;
	OAuth::delete_cookies_cb delete_cookies;
};

static std::vector<OAuthInfo> loginCBs;

void OAuth::RegisterOAuth(const Def &d, create_cb create, login_cb login,
			  delete_cookies_cb delete_cookies)
{
	OAuthInfo info = {d, login, delete_cookies};
	loginCBs.push_back(info);
	RegisterAuth(d, create);
}

std::shared_ptr<Auth> OAuth::Login(QWidget *parent, const std::string &service)
{
	for (auto &a : loginCBs) {
		if (service.find(a.def.service) != std::string::npos) {
			return a.login(parent);
		}
	}

	return nullptr;
}

void OAuth::DeleteCookies(const std::string &service)
{
	for (auto &a : loginCBs) {
		if (service.find(a.def.service) != std::string::npos) {
			a.delete_cookies();
		}
	}
}

void OAuth::SaveInternal()
{
	OBSBasic *main = OBSBasic::Get();
	config_set_string(main->Config(), service(), "RefreshToken",
			  refresh_token.c_str());
	config_set_string(main->Config(), service(), "Token", token.c_str());
	config_set_uint(main->Config(), service(), "ExpireTime", expire_time);
	config_set_int(main->Config(), service(), "ScopeVer", currentScopeVer);
}

static inline std::string get_config_str(OBSBasic *main, const char *section,
					 const char *name)
{
	const char *val = config_get_string(main->Config(), section, name);
	return val ? val : "";
}

bool OAuth::LoadInternal()
{
	OBSBasic *main = OBSBasic::Get();
	refresh_token = get_config_str(main, service(), "RefreshToken");
	token = get_config_str(main, service(), "Token");
	expire_time = config_get_uint(main->Config(), service(), "ExpireTime");
	currentScopeVer =
		(int)config_get_int(main->Config(), service(), "ScopeVer");
	return implicit ? !token.empty() : !refresh_token.empty();
}

bool OAuth::TokenExpired()
{
	if (token.empty())
		return true;
	if ((uint64_t)time(nullptr) > expire_time - 5)
		return true;
	return false;
}

bool OAuth::GetToken(const char *url, const std::string &client_id,
		     int scope_ver, const std::string &auth_code, bool retry)
try {
	std::string output;
	std::string error;
	std::string desc;

	if (currentScopeVer > 0 && currentScopeVer < scope_ver) {
		if (RetryLogin()) {
			return true;
		} else {
			QString title = QTStr("Auth.InvalidScope.Title");
			QString text =
				QTStr("Auth.InvalidScope.Text").arg(service());

			QMessageBox::warning(OBSBasic::Get(), title, text);
		}
	}

	if (auth_code.empty() && !TokenExpired()) {
		return true;
	}

	std::string post_data;
	post_data += "action=redirect&client_id=";
	post_data += client_id;

	if (!auth_code.empty()) {
		post_data += "&grant_type=authorization_code&code=";
		post_data += auth_code;
	} else {
		post_data += "&grant_type=refresh_token&refresh_token=";
		post_data += refresh_token;
	}

	bool success = false;

	auto func = [&]() {
		success = GetRemoteFile(url, output, error, nullptr,
					"application/x-www-form-urlencoded",
					post_data.c_str(),
					std::vector<std::string>(), nullptr, 5);
	};

	ExecThreadedWithoutBlocking(func, QTStr("Auth.Authing.Title"),
				    QTStr("Auth.Authing.Text").arg(service()));
	if (!success || output.empty())
		throw ErrorInfo("Failed to get token from remote", error);

	Json json = Json::parse(output, error);
	if (!error.empty())
		throw ErrorInfo("Failed to parse json", error);

	/* -------------------------- */
	/* error handling             */

	error = json["error"].string_value();
	if (!retry && error == "invalid_grant") {
		if (RetryLogin()) {
			return true;
		}
	}
	if (!error.empty())
		throw ErrorInfo(error,
				json["error_description"].string_value());

	/* -------------------------- */
	/* success!                   */

	expire_time = (uint64_t)time(nullptr) + json["expires_in"].int_value();
	token = json["access_token"].string_value();
	if (token.empty())
		throw ErrorInfo("Failed to get token from remote", error);

	if (!auth_code.empty()) {
		refresh_token = json["refresh_token"].string_value();
		if (refresh_token.empty())
			throw ErrorInfo("Failed to get refresh token from "
					"remote",
					error);

		currentScopeVer = scope_ver;
	}

	return true;

} catch (ErrorInfo &info) {
	if (!retry) {
		QString title = QTStr("Auth.AuthFailure.Title");
		QString text = QTStr("Auth.AuthFailure.Text")
				       .arg(service(), info.message.c_str(),
					    info.error.c_str());

		QMessageBox::warning(OBSBasic::Get(), title, text);
	}

	blog(LOG_WARNING, "%s: %s: %s", __FUNCTION__, info.message.c_str(),
	     info.error.c_str());
	return false;
}

void OAuthStreamKey::OnStreamConfig()
{
	if (key_.empty())
		return;

	OBSBasic *main = OBSBasic::Get();
	obs_service_t *service = main->GetService();

	obs_data_t *settings = obs_service_get_settings(service);

	bool bwtest = obs_data_get_bool(settings, "bwtest");

	if (bwtest && strcmp(this->service(), "Twitch") == 0)
		obs_data_set_string(settings, "key",
				    (key_ + "?bandwidthtest=true").c_str());
	else
		obs_data_set_string(settings, "key", key_.c_str());

	obs_service_update(service, settings);

	obs_data_release(settings);
}
