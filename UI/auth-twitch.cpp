#include "auth-twitch.hpp"

#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <qt-wrappers.hpp>
#include <obs-app.hpp>

#include "window-dock-browser.hpp"
#include "window-basic-main.hpp"
#include "remote-text.hpp"

#include <json11.hpp>

#include "ui-config.h"
#include "obf.h"

using namespace json11;

/* ------------------------------------------------------------------------- */

#define TWITCH_AUTH_URL "https://obsproject.com/app-auth/twitch?action=redirect"
#define TWITCH_TOKEN_URL "https://obsproject.com/app-auth/twitch-token"
#define ACCEPT_HEADER "Accept: application/vnd.twitchtv.v5+json"

#define TWITCH_SCOPE_VERSION 1

static Auth::Def twitchDef = {"Twitch", Auth::Type::OAuth_StreamKey};

/* ------------------------------------------------------------------------- */

TwitchAuth::TwitchAuth(const Def &d) : OAuthStreamKey(d)
{
	if (!cef)
		return;

	cef->add_popup_whitelist_url(
		"https://twitch.tv/popout/frankerfacez/chat?ffz-settings",
		this);

	/* enables javascript-based popups.  basically bttv popups */
	cef->add_popup_whitelist_url("about:blank#blocked", this);

	uiLoadTimer.setSingleShot(true);
	uiLoadTimer.setInterval(500);
	connect(&uiLoadTimer, &QTimer::timeout, this,
		&TwitchAuth::TryLoadSecondaryUIPanes);
}

bool TwitchAuth::GetChannelInfo()
try {
	std::string client_id = TWITCH_CLIENTID;
	deobfuscate_str(&client_id[0], TWITCH_HASH);

	if (!GetToken(TWITCH_TOKEN_URL, client_id, TWITCH_SCOPE_VERSION))
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
	headers.push_back(ACCEPT_HEADER);
	headers.push_back(std::move(auth));

	std::string output;
	std::string error;
	long error_code = 0;

	bool success = false;

	auto func = [&]() {
		success = GetRemoteFile("https://api.twitch.tv/kraken/channel",
					output, error, &error_code,
					"application/json", nullptr, headers,
					nullptr, 5);
	};

	ExecThreadedWithoutBlocking(
		func, QTStr("Auth.LoadingChannel.Title"),
		QTStr("Auth.LoadingChannel.Text").arg(service()));
	if (error_code == 403) {
		OBSMessageBox::warning(OBSBasic::Get(),
				       Str("TwitchAuth.TwoFactorFail.Title"),
				       Str("TwitchAuth.TwoFactorFail.Text"),
				       true);
		blog(LOG_WARNING, "%s: %s", __FUNCTION__,
		     "Got 403 from Twitch, user probably does not "
		     "have two-factor authentication enabled on "
		     "their account");
		return false;
	}

	if (!success || output.empty())
		throw ErrorInfo("Failed to get text from remote", error);

	Json json = Json::parse(output, error);
	if (!error.empty())
		throw ErrorInfo("Failed to parse json", error);

	error = json["error"].string_value();
	if (!error.empty()) {
		if (error == "Unauthorized") {
			if (RetryLogin()) {
				return GetChannelInfo();
			}
			throw ErrorInfo(error, json["message"].string_value());
		}
		throw ErrorInfo(error,
				json["error_description"].string_value());
	}

	name = json["name"].string_value();
	key_ = json["stream_key"].string_value();

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

void TwitchAuth::SaveInternal()
{
	OBSBasic *main = OBSBasic::Get();
	config_set_string(main->Config(), service(), "Name", name.c_str());
	if (uiLoaded) {
		config_set_string(main->Config(), service(), "DockState",
				  main->saveState().toBase64().constData());
	}
	OAuthStreamKey::SaveInternal();
}

static inline std::string get_config_str(OBSBasic *main, const char *section,
					 const char *name)
{
	const char *val = config_get_string(main->Config(), section, name);
	return val ? val : "";
}

bool TwitchAuth::LoadInternal()
{
	if (!cef)
		return false;

	OBSBasic *main = OBSBasic::Get();
	name = get_config_str(main, service(), "Name");
	firstLoad = false;
	return OAuthStreamKey::LoadInternal();
}

static const char *ffz_script = "\
var ffz = document.createElement('script');\
ffz.setAttribute('src','https://cdn.frankerfacez.com/script/script.min.js');\
document.head.appendChild(ffz);";

static const char *bttv_script = "\
localStorage.setItem('bttv_clickTwitchEmotes', true);\
localStorage.setItem('bttv_darkenedMode', true);\
localStorage.setItem('bttv_bttvGIFEmotes', true);\
var bttv = document.createElement('script');\
bttv.setAttribute('src','https://cdn.betterttv.net/betterttv.js');\
document.head.appendChild(bttv);";

static const char *referrer_script1 = "\
Object.defineProperty(document, 'referrer', {get : function() { return '";
static const char *referrer_script2 = "'; }});";

void TwitchAuth::LoadUI()
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

	std::string moderation_tools_url;
	moderation_tools_url = "https://www.twitch.tv/";
	moderation_tools_url += name;
	moderation_tools_url += "/dashboard/settings/moderation?no-reload=true";

	/* ----------------------------------- */

	url = "https://www.twitch.tv/popout/";
	url += name;
	url += "/chat";

	QSize size = main->frameSize();
	QPoint pos = main->pos();

	chat.reset(new BrowserDock());
	chat->setObjectName("twitchChat");
	chat->resize(300, 600);
	chat->setMinimumSize(200, 300);
	chat->setWindowTitle(QTStr("Auth.Chat"));
	chat->setAllowedAreas(Qt::AllDockWidgetAreas);

	browser = cef->create_widget(nullptr, url, panel_cookies);
	chat->SetWidget(browser);
	cef->add_force_popup_url(moderation_tools_url, chat.data());

	script = "localStorage.setItem('twilight.theme', 1);";

	const int twAddonChoice =
		config_get_int(main->Config(), service(), "AddonChoice");
	if (twAddonChoice) {
		if (twAddonChoice & 0x1)
			script += bttv_script;
		if (twAddonChoice & 0x2)
			script += ffz_script;
	}

	browser->setStartupScript(script);

	main->addDockWidget(Qt::RightDockWidgetArea, chat.data());
	chatMenu.reset(main->AddDockWidget(chat.data()));

	/* ----------------------------------- */

	chat->setFloating(true);
	chat->move(pos.x() + size.width() - chat->width() - 50, pos.y() + 50);

	if (firstLoad) {
		chat->setVisible(true);
	} else {
		const char *dockStateStr = config_get_string(
			main->Config(), service(), "DockState");
		QByteArray dockState =
			QByteArray::fromBase64(QByteArray(dockStateStr));
		main->restoreState(dockState);
	}

	TryLoadSecondaryUIPanes();

	uiLoaded = true;
}

void TwitchAuth::LoadSecondaryUIPanes()
{
	OBSBasic *main = OBSBasic::Get();

	QCefWidget *browser;
	std::string url;
	std::string script;

	QSize size = main->frameSize();
	QPoint pos = main->pos();

	script = "localStorage.setItem('twilight.theme', 1);";
	script += referrer_script1;
	script += "https://www.twitch.tv/";
	script += name;
	script += "/dashboard/live";
	script += referrer_script2;

	const int twAddonChoice =
		config_get_int(main->Config(), service(), "AddonChoice");
	if (twAddonChoice) {
		if (twAddonChoice & 0x1)
			script += bttv_script;
		if (twAddonChoice & 0x2)
			script += ffz_script;
	}

	/* ----------------------------------- */

	url = "https://www.twitch.tv/popout/";
	url += name;
	url += "/dashboard/live/stream-info";

	info.reset(new BrowserDock());
	info->setObjectName("twitchInfo");
	info->resize(300, 650);
	info->setMinimumSize(200, 300);
	info->setWindowTitle(QTStr("Auth.StreamInfo"));
	info->setAllowedAreas(Qt::AllDockWidgetAreas);

	browser = cef->create_widget(nullptr, url, panel_cookies);
	info->SetWidget(browser);
	browser->setStartupScript(script);

	main->addDockWidget(Qt::RightDockWidgetArea, info.data());
	infoMenu.reset(main->AddDockWidget(info.data()));

	/* ----------------------------------- */

	url = "https://www.twitch.tv/popout/";
	url += name;
	url += "/dashboard/live/stats";

	stat.reset(new BrowserDock());
	stat->setObjectName("twitchStats");
	stat->resize(200, 250);
	stat->setMinimumSize(200, 150);
	stat->setWindowTitle(QTStr("TwitchAuth.Stats"));
	stat->setAllowedAreas(Qt::AllDockWidgetAreas);

	browser = cef->create_widget(nullptr, url, panel_cookies);
	stat->SetWidget(browser);
	browser->setStartupScript(script);

	main->addDockWidget(Qt::RightDockWidgetArea, stat.data());
	statMenu.reset(main->AddDockWidget(stat.data()));

	/* ----------------------------------- */

	url = "https://www.twitch.tv/popout/";
	url += name;
	url += "/dashboard/live/activity-feed";

	feed.reset(new BrowserDock());
	feed->setObjectName("twitchFeed");
	feed->resize(300, 650);
	feed->setMinimumSize(200, 300);
	feed->setWindowTitle(QTStr("TwitchAuth.Feed"));
	feed->setAllowedAreas(Qt::AllDockWidgetAreas);

	browser = cef->create_widget(nullptr, url, panel_cookies);
	feed->SetWidget(browser);
	browser->setStartupScript(script);

	main->addDockWidget(Qt::RightDockWidgetArea, feed.data());
	feedMenu.reset(main->AddDockWidget(feed.data()));

	/* ----------------------------------- */

	info->setFloating(true);
	stat->setFloating(true);
	feed->setFloating(true);

	QSize statSize = stat->frameSize();

	info->move(pos.x() + 50, pos.y() + 50);
	stat->move(pos.x() + size.width() / 2 - statSize.width() / 2,
		   pos.y() + size.height() / 2 - statSize.height() / 2);
	feed->move(pos.x() + 100, pos.y() + 100);

	if (firstLoad) {
		info->setVisible(true);
		stat->setVisible(false);
		feed->setVisible(false);
	} else {
		uint32_t lastVersion = config_get_int(App()->GlobalConfig(),
						      "General", "LastVersion");

		if (lastVersion <= MAKE_SEMANTIC_VERSION(23, 0, 2)) {
			feed->setVisible(false);
		}

		const char *dockStateStr = config_get_string(
			main->Config(), service(), "DockState");
		QByteArray dockState =
			QByteArray::fromBase64(QByteArray(dockStateStr));
		main->restoreState(dockState);
	}
}

/* Twitch.tv has an OAuth for itself.  If we try to load multiple panel pages
 * at once before it's OAuth'ed itself, they will all try to perform the auth
 * process at the same time, get their own request codes, and only the last
 * code will be valid -- so one or more panels are guaranteed to fail.
 *
 * To solve this, we want to load just one panel first (the chat), and then all
 * subsequent panels should only be loaded once we know that Twitch has auth'ed
 * itself (if the cookie "auth-token" exists for twitch.tv).
 *
 * This is annoying to deal with. */
void TwitchAuth::TryLoadSecondaryUIPanes()
{
	QPointer<TwitchAuth> this_ = this;

	auto cb = [this_](bool found) {
		if (!this_) {
			return;
		}

		if (!found) {
			QMetaObject::invokeMethod(&this_->uiLoadTimer, "start");
		} else {
			QMetaObject::invokeMethod(this_,
						  "LoadSecondaryUIPanes");
		}
	};

	panel_cookies->CheckForCookie("https://www.twitch.tv", "auth-token",
				      cb);
}

bool TwitchAuth::RetryLogin()
{
	OAuthLogin login(OBSBasic::Get(), TWITCH_AUTH_URL, false);
	if (login.exec() == QDialog::Rejected) {
		return false;
	}

	std::shared_ptr<TwitchAuth> auth =
		std::make_shared<TwitchAuth>(twitchDef);
	std::string client_id = TWITCH_CLIENTID;
	deobfuscate_str(&client_id[0], TWITCH_HASH);

	return GetToken(TWITCH_TOKEN_URL, client_id, TWITCH_SCOPE_VERSION,
			QT_TO_UTF8(login.GetCode()), true);
}

std::shared_ptr<Auth> TwitchAuth::Login(QWidget *parent)
{
	OAuthLogin login(parent, TWITCH_AUTH_URL, false);
	if (login.exec() == QDialog::Rejected) {
		return nullptr;
	}

	std::shared_ptr<TwitchAuth> auth =
		std::make_shared<TwitchAuth>(twitchDef);

	std::string client_id = TWITCH_CLIENTID;
	deobfuscate_str(&client_id[0], TWITCH_HASH);

	if (!auth->GetToken(TWITCH_TOKEN_URL, client_id, TWITCH_SCOPE_VERSION,
			    QT_TO_UTF8(login.GetCode()))) {
		return nullptr;
	}

	if (auth->GetChannelInfo()) {
		return auth;
	}

	return nullptr;
}

static std::shared_ptr<Auth> CreateTwitchAuth()
{
	return std::make_shared<TwitchAuth>(twitchDef);
}

static void DeleteCookies()
{
	if (panel_cookies)
		panel_cookies->DeleteCookies("twitch.tv", std::string());
}

void RegisterTwitchAuth()
{
	OAuth::RegisterOAuth(twitchDef, CreateTwitchAuth, TwitchAuth::Login,
			     DeleteCookies);
}
