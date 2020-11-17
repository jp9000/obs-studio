#include "auth-trovo.hpp"

#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <qt-wrappers.hpp>
#include <json11.hpp>
#include <ctime>
#include <sstream>

#include <obs-app.hpp>
#include "window-dock-browser.hpp"
#include "window-basic-main.hpp"
#include "remote-text.hpp"
#include "ui-config.h"
#include "obf.h"

using namespace json11;

/* ------------------------------------------------------------------------- */
#define TROVO_AUTH_URL \
	"https://open.trovo.live/page/login.html?client_id=6W4TpDJnE95U68zWJHZLDfn4FA5wf7W4&response_type=token&scope=channel_details_self+channel_update_self+user_details_self&redirect_uri=https%3A%2F%2Fobsproject.com%2Fapp-auth%2Ftrovo-auth.php&state=Nlc0VHBESm5FOTVVNjh6V0pIWkxEZm40RkE1d2Y3VzQ"
#define TROVO_STREAMKEY_URL "https://open-api.trovo.live/openplatform/channel"
#define TROVO_SCOPE_VERSION 1

static Auth::Def trovoDef = {"Trovo", Auth::Type::OAuth_StreamKey};

/* ------------------------------------------------------------------------- */

TrovoAuth::TrovoAuth(const Def &d) : OAuthStreamKey(d)
{
	if (!cef)
		return;
	implicit = true;
}

bool TrovoAuth::GetChannelInfo()
try {
	std::string client_id = TROVO_CLIENTID;
	deobfuscate_str(&client_id[0], TROVO_HASH);

	if (!CheckToken(TROVO_SCOPE_VERSION))
		return false;
	if (token.empty())
		return false;
	if (!key_.empty())
		return true;

	std::string auth;
	auth += "Authorization: OAuth ";
	auth += token;

	std::vector<std::string> headers;
	headers.push_back(std::string("Client-ID: ") + client_id);
	headers.push_back(std::move(auth));

	std::string output;
	std::string error;
	Json json;
	bool success;

	auto func = [&]() {
		success = GetRemoteFile(TROVO_STREAMKEY_URL, output, error,
					nullptr, "application/json", nullptr,
					headers, nullptr, 5);
	};

	ExecThreadedWithoutBlocking(
		func, QTStr("Auth.LoadingChannel.Title"),
		QTStr("Auth.LoadingChannel.Text").arg(service()));
	if (!success || output.empty())
		throw ErrorInfo("Failed to get stream key from remote", error);

	json = Json::parse(output, error);
	if (!error.empty())
		throw ErrorInfo("Failed to parse json", error);

	error = json["error"].string_value();
	if (!error.empty())
		throw ErrorInfo(error,
				json["error_description"].string_value());

	key_ = json["stream_key"].string_value();

	std::string channelUrl = json["channel_url"].string_value();
	int index = channelUrl.find_last_of("/") + 1;
	if (index > 0 && index < (int)channelUrl.size()) {
		name = channelUrl.substr(index);
	}

	// Trovo host url already has "live/", so ignore "live/" in stream_key
	std::string live = "live/";
	if (key_.find(live) != 0)
		throw ErrorInfo("Invalid stream key", "Invalid stream key");
	index = (int)live.size();
	key_ = key_.substr(index);
	return true;
} catch (ErrorInfo info) {
	QString title = QTStr("Auth.ChannelFailure.Title");
	QString text = QTStr("Auth.ChannelFailure.Text")
			       .arg(service(), info.message.c_str(),
				    info.error.c_str());

	QMessageBox::warning(OBSBasic::Get(), title, text);

	blog(LOG_WARNING, "%s: %s: %s", __FUNCTION__, info.message.c_str(),
	     info.error.c_str());
	return false;
}

void TrovoAuth::SaveInternal()
{
	OBSBasic *main = OBSBasic::Get();
	config_set_string(main->Config(), service(), "Name", name.c_str());
	config_set_string(main->Config(), service(), "DockState",
			  main->saveState().toBase64().constData());
	OAuthStreamKey::SaveInternal();
}

static inline std::string get_config_str(OBSBasic *main, const char *section,
					 const char *name)
{
	const char *val = config_get_string(main->Config(), section, name);
	return val ? val : "";
}

bool TrovoAuth::LoadInternal()
{
	if (!cef)
		return false;
	OBSBasic *main = OBSBasic::Get();
	name = get_config_str(main, service(), "Name");
	firstLoad = false;
	return OAuthStreamKey::LoadInternal();
}

void TrovoAuth::LoadUI()
{
	if (!cef)
		return;
	if (uiLoaded)
		return;
	if (!GetChannelInfo())
		return;

	OBSBasic::InitBrowserPanelSafeBlock();
	OBSBasic *main = OBSBasic::Get();

	QCefWidget *browser;
	std::string url;
	std::string script;

	/* ----------------------------------- */

	url = "https://trovo.live/chat/" + name + "?darkMode=0";

	QSize size = main->frameSize();
	QPoint pos = main->pos();

	chat.reset(new BrowserDock());
	chat->setObjectName("trovoChat");
	chat->resize(420, 600);
	chat->setMinimumSize(200, 300);
	chat->setWindowTitle(QTStr("Auth.Chat"));
	chat->setAllowedAreas(Qt::AllDockWidgetAreas);

	browser = cef->create_widget(nullptr, url, panel_cookies);
	chat->SetWidget(browser);

	main->addDockWidget(Qt::RightDockWidgetArea, chat.data());
	chatMenu.reset(main->AddDockWidget(chat.data()));
	chat->setFloating(true);
	chat->move(pos.x() + size.width() - chat->width() - 30, pos.y() + 60);

	if (firstLoad) {
		chat->setVisible(true);
	} else {
		const char *dockStateStr = config_get_string(
			main->Config(), service(), "DockState");
		QByteArray dockState =
			QByteArray::fromBase64(QByteArray(dockStateStr));
		main->restoreState(dockState);
	}

	uiLoaded = true;
}

bool TrovoAuth::RetryLogin()
{
	TrovoAuthLogin login(OBSBasic::Get(), TROVO_AUTH_URL, false);
	cef->add_popup_whitelist_url("about:blank", &login);
	if (login.exec() == QDialog::Rejected) {
		return false;
	}

	std::shared_ptr<TrovoAuth> auth = std::make_shared<TrovoAuth>(trovoDef);

	auth->token = login.token;
	auth->expire_time = login.expire_time;

	bool result = CheckToken(TROVO_SCOPE_VERSION);
	if (result) {
		auth->currentScopeVer = TROVO_SCOPE_VERSION;
	}

	return result;
}

std::shared_ptr<Auth> TrovoAuth::Login(QWidget *parent)
{
	TrovoAuthLogin login(parent, TROVO_AUTH_URL, false);
	cef->add_popup_whitelist_url("about:blank", &login);

	if (login.exec() == QDialog::Rejected) {
		return nullptr;
	}

	std::shared_ptr<TrovoAuth> auth = std::make_shared<TrovoAuth>(trovoDef);

	auth->token = login.token;
	auth->expire_time = login.expire_time;

	if (!auth->CheckToken(TROVO_SCOPE_VERSION)) {
		return nullptr;
	}

	auth->currentScopeVer = TROVO_SCOPE_VERSION;
	if (auth->GetChannelInfo()) {
		return auth;
	}

	return nullptr;
}

bool TrovoAuth::CheckToken(int scope_ver)
{
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

	if (!TokenExpired()) {
		return true;
	}

	return false;
}

/* ------------------------------------------------------------------------- */

TrovoAuthLogin::TrovoAuthLogin(QWidget *parent, const std::string &url,
			       bool token)
	: OAuthLogin(parent, url, token)
{
}

void TrovoAuthLogin::urlChanged(const QString &url)
{
	std::string access_token = "access_token=";
	std::string token_expires = "expires_in=";
	int token_idx = url.indexOf(access_token.c_str());
	int expires_idx = url.indexOf(token_expires.c_str());
	if (token_idx == -1 || expires_idx == -1)
		return;

	if (url.left(22) != "https://obsproject.com")
		return;

	token_idx += (int)access_token.size();
	int token_next_idx = url.indexOf("&", token_idx);
	if (token_next_idx != -1)
		token = url.mid(token_idx, token_next_idx - token_idx)
				.toStdString();
	else
		token = url.right(url.size() - token_idx).toStdString();

	expires_idx += (int)token_expires.size();
	int expires_next_idx = url.indexOf("&", expires_idx);
	if (expires_next_idx != -1)
		expire_time =
			(uint64_t)time(nullptr) +
			url.mid(expires_idx, expires_next_idx - expires_idx)
				.toInt();
	else
		expire_time = (uint64_t)time(nullptr) +
			      url.right(url.size() - expires_idx).toInt();
	accept();
}

/* ------------------------------------------------------------------------- */

static std::shared_ptr<Auth> CreateTrovoAuth()
{
	return std::make_shared<TrovoAuth>(trovoDef);
}

static void DeleteCookies()
{
	if (panel_cookies) {
		panel_cookies->DeleteCookies("open.trovo.live", std::string());
	}
}

void RegisterTrovoAuth()
{
	OAuth::RegisterOAuth(trovoDef, CreateTrovoAuth, TrovoAuth::Login,
			     DeleteCookies);
}
