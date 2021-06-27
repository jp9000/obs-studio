#include "auth-peertube.hpp"

#include <qt-wrappers.hpp>

#include "window-basic-main.hpp"
#include "remote-text.hpp"

#include <json11.hpp>

using namespace json11;

/* ------------------------------------------------------------------------- */
#define PEERTUBE_CONFIG_SUFFIX "/api/v1/config"
#define PEERTUBE_CLIENT_SUFFIX "/api/v1/oauth-clients/local"
#define PEERTUBE_TOKEN_SUFFIX "/api/v1/users/token"
#define PEERTUBE_SCOPE_VERSION 1

static Auth::Def peertubeDef = {"PeerTube", Auth::Type::OAuth_PeerTube};

/* ------------------------------------------------------------------------- */

void PeerTubeAuth::SaveInternal()
{
	OBSBasic *main = OBSBasic::Get();
	config_set_string(main->Config(), service(), "ClientId",
			  client_id.c_str());
	config_set_string(main->Config(), service(), "ClientSecret",
			  client_secret.c_str());
	config_set_string(main->Config(), service(), "RefreshToken",
			  refresh_token.c_str());
	config_set_string(main->Config(), service(), "Token", token.c_str());
	config_set_uint(main->Config(), service(), "RefreshExpireTime",
			refresh_expire_time);
	config_set_uint(main->Config(), service(), "ExpireTime", expire_time);
	config_set_int(main->Config(), service(), "ScopeVer", currentScopeVer);
}

static inline std::string get_config_str(OBSBasic *main, const char *section,
					 const char *name)
{
	const char *val = config_get_string(main->Config(), section, name);
	return val ? val : "";
}

bool PeerTubeAuth::LoadInternal()
{
	OBSBasic *main = OBSBasic::Get();
	client_id = get_config_str(main, service(), "ClientId");
	client_secret = get_config_str(main, service(), "ClientSecret");
	refresh_token = get_config_str(main, service(), "RefreshToken");
	token = get_config_str(main, service(), "Token");
	refresh_expire_time =
		config_get_uint(main->Config(), service(), "RefreshExpireTime");
	expire_time = config_get_uint(main->Config(), service(), "ExpireTime");
	currentScopeVer =
		(int)config_get_int(main->Config(), service(), "ScopeVer");
	return implicit ? !token.empty() : !refresh_token.empty();
}

void PeerTubeAuth::OnStreamConfig()
{
	if (key_.empty())
		return;

	OBSBasic *main = OBSBasic::Get();
	obs_service_t *service = main->GetService();

	obs_data_t *settings = obs_service_get_settings(service);

	obs_data_set_string(settings, "videoId", videoId_.c_str());
	obs_data_set_string(settings, "server", server_.c_str());
	obs_data_set_string(settings, "key", key_.c_str());

	obs_service_update(service, settings);

	obs_data_release(settings);
}

bool PeerTubeAuth::IsValidInstance(const std::string &instance)
try {
	std::string output;
	std::string error;

	std::string url;
	url += instance;
	url += PEERTUBE_CONFIG_SUFFIX;

	bool success = false;

	auto func = [&]() {
		success = GetRemoteFile(url.c_str(), output, error, nullptr,
					"application/x-www-form-urlencoded",
					nullptr, std::vector<std::string>(),
					nullptr, 5);
	};

	ExecThreadedWithoutBlocking(func,
				    QTStr("PeerTubeAuth.InstanceCheck.Title"),
				    QTStr("PeerTubeAuth.InstanceCheck.Text"));
	if (!success || output.empty()) {
		QString message =
			QTStr("PeerTubeAuth.InstanceCheck.RemoteFail.Text")
				.arg(error.c_str());
		QMessageBox::warning(OBSBasic::Get(),
				     QTStr("PeerTubeAuth.RemoteFail.Title"),
				     message);
		throw ErrorInfo(
			"Failed to get PeerTube instance config from remote",
			error);
	}

	Json json = Json::parse(output, error);
	if (!error.empty()) {
		QString message =
			QTStr("PeerTubeAuth.InstanceCheck.JsonFail.Text")
				.arg(error.c_str());
		QMessageBox::warning(OBSBasic::Get(),
				     QTStr("PeerTubeAuth.JsonFail.Title"),
				     message);
		throw ErrorInfo("Failed to parse PeerTube instance config json",
				error);
	}

	QString version;
	version = json["serverVersion"].string_value().c_str();
	if (version.isEmpty()) {
		QMessageBox::warning(
			OBSBasic::Get(),
			QTStr("PeerTubeAuth.InstanceCheck.NoVersion.Title"),
			QTStr("PeerTubeAuth.InstanceCheck.NoVersion.Text"));
		throw ErrorInfo("Failed to get PeerTube instance version",
				error);
	}

	QStringList split_version;
	split_version.append(version.split("."));
	if (split_version[0].toUInt() < 3) {
		QMessageBox::warning(
			OBSBasic::Get(),
			QTStr("PeerTubeAuth.InstanceCheck.LowVersion.Title"),
			QTStr("PeerTubeAuth.InstanceCheck.LowVersion.Text"));
		throw ErrorInfo(
			"This instance version is unable to do livestreaming",
			error);
	}

	bool live_enabled = json["live"]["enabled"].bool_value();
	if (!live_enabled) {
		QMessageBox::warning(
			OBSBasic::Get(),
			QTStr("PeerTubeAuth.InstanceCheck.LiveDisabled.Title"),
			QTStr("PeerTubeAuth.InstanceCheck.LiveDisabled.Text"));
		throw ErrorInfo("This instance does not allow livestreaming",
				error);
	}

	return true;

} catch (ErrorInfo &info) {
	blog(LOG_WARNING, "%s: %s: %s", __FUNCTION__, info.message.c_str(),
	     info.error.c_str());
	return false;
}

bool PeerTubeAuth::GetClientInfo(const std::string &instance)
try {
	std::string output;
	std::string error;

	std::string url;
	url += instance;
	url += PEERTUBE_CLIENT_SUFFIX;

	bool success = false;

	auto func = [&]() {
		success = GetRemoteFile(url.c_str(), output, error, nullptr,
					"application/x-www-form-urlencoded",
					nullptr, std::vector<std::string>(),
					nullptr, 5);
	};

	ExecThreadedWithoutBlocking(func,
				    QTStr("PeerTubeAuth.ClientInfo.Title"),
				    QTStr("PeerTubeAuth.ClientInfo.Text"));
	if (!success || output.empty()) {
		QString message =
			QTStr("PeerTubeAuth.ClientInfo.RemoteFail.Text")
				.arg(error.c_str());
		QMessageBox::warning(OBSBasic::Get(),
				     QTStr("PeerTubeAuth.RemoteFail.Title"),
				     message);
		throw ErrorInfo(
			"Failed to get PeerTube client info from remote",
			error);
	}

	Json json = Json::parse(output, error);
	if (!error.empty()) {
		QString message = QTStr("PeerTubeAuth.ClientInfo.JsonFail.Text")
					  .arg(error.c_str());
		QMessageBox::warning(OBSBasic::Get(),
				     QTStr("PeerTubeAuth.JsonFail.Title"),
				     message);
		throw ErrorInfo(
			"Failed to parse PeerTube instance client info json",
			error);
	}

	client_id = json["client_id"].string_value();
	if (client_id.empty()) {
		QMessageBox::warning(
			OBSBasic::Get(),
			QTStr("PeerTubeAuth.ClientInfo.NoClientId.Title"),
			QTStr("PeerTubeAuth.ClientInfo.NoClientId.Text"));
		throw ErrorInfo("Failed to get PeerTube instance client ID",
				error);
	}

	client_secret = json["client_secret"].string_value();
	if (client_secret.empty()) {
		QMessageBox::warning(
			OBSBasic::Get(),
			QTStr("PeerTubeAuth.ClientInfo.NoClientSecret.Title"),
			QTStr("PeerTubeAuth.ClientInfo.NoClientSecret.Text"));
		throw ErrorInfo("Failed to get PeerTube instance client secret",
				error);
	}

	return true;

} catch (ErrorInfo &info) {
	blog(LOG_WARNING, "%s: %s: %s", __FUNCTION__, info.message.c_str(),
	     info.error.c_str());
	return false;
}

bool PeerTubeAuth::GetToken(const std::string &instance,
			    const std::string &username,
			    const std::string &password, bool retry)
try {
	std::string output;
	std::string error;

	if (currentScopeVer > 0 && currentScopeVer < PEERTUBE_SCOPE_VERSION) {
		if (RetryLogin()) {
			return true;
		} else {
			QString title = QTStr("Auth.InvalidScope.Title");
			QString text =
				QTStr("Auth.InvalidScope.Text").arg(service());

			QMessageBox::warning(OBSBasic::Get(), title, text);
		}
	}

	if (!TokenExpired())
		return true;

	std::string url;
	url += instance;
	url += PEERTUBE_TOKEN_SUFFIX;

	std::string post_data;
	post_data += "client_id=";
	post_data += client_id;
	post_data += "&client_secret=";
	post_data += client_secret;

	if (!username.empty() && !password.empty()) {
		post_data +=
			"&grant_type=password&response_type=code&username=";
		post_data += username;
		post_data += "&password=";
		post_data += password;
	} else if (!RefreshTokenExpired()) {
		post_data +=
			"&grant_type=refresh_token&response_type=code&refresh_token=";
		post_data += refresh_token;
	} else if (retry) {
		return RetryLogin();
	} else {
		return false;
	}

	bool success = false;

	auto func = [&]() {
		success = GetRemoteFile(url.c_str(), output, error, nullptr,
					"application/x-www-form-urlencoded",
					post_data.c_str(),
					std::vector<std::string>(), nullptr, 5);
	};

	ExecThreadedWithoutBlocking(func, QTStr("Auth.Authing.Title"),
				    QTStr("PeerTubeAuth.Authing.Text"));
	if (!success || output.empty()) {
		if (!retry) {
			QString message =
				QTStr("PeerTubeAuth.Authing.RemoteFail.Text")
					.arg(error.c_str());
			QMessageBox::warning(
				OBSBasic::Get(),
				QTStr("PeerTubeAuth.RemoteFail.Title"),
				message);
		}
		throw ErrorInfo("Failed to authenticate to PeerTube instance",
				error);
	}

	Json json = Json::parse(output, error);
	if (!error.empty()) {
		if (!retry) {
			QString message =
				QTStr("PeerTubeAuth.Authing.JsonFail.Text")
					.arg(error.c_str());
			QMessageBox::warning(
				OBSBasic::Get(),
				QTStr("PeerTubeAuth.JsonFail.Title"), message);
		}
		throw ErrorInfo("Failed to parse PeerTube instance tokens",
				error);
	}

	error = json["code"].string_value();
	if (!retry && error == "invalid_grant") {
		if (RetryLogin()) {
			return true;
		}
	}

	if (!error.empty()) {
		error = json["error"].string_value();
		if (!retry) {
			QString message =
				QTStr("PeerTubeAuth.Authing.Invalid.Text")
					.arg(error.c_str());
			QMessageBox::warning(OBSBasic::Get(),
					     QTStr("Auth.AuthFailure.Title"),
					     message);
		}
		throw ErrorInfo(error, json["code"].string_value());
	}

	refresh_expire_time = (uint64_t)time(nullptr) +
			      json["refresh_token_expires_in"].int_value();
	expire_time = (uint64_t)time(nullptr) + json["expires_in"].int_value();

	refresh_token = json["refresh_token"].string_value();
	if (refresh_token.empty()) {
		if (!retry) {
			QMessageBox::warning(
				OBSBasic::Get(),
				QTStr("PeerTubeAuth.Authing.NoRefreshToken.Title"),
				QTStr("PeerTubeAuth.Authing.NoRefreshToken.Text"));
		}
		throw ErrorInfo("Failed to get PeerTube instance refresh token",
				error);
	}

	token = json["access_token"].string_value();
	if (token.empty()) {
		if (!retry) {
			QMessageBox::warning(
				OBSBasic::Get(),
				QTStr("PeerTubeAuth.Authing.NoToken.Title"),
				QTStr("PeerTubeAuth.Authing.NoToken.Text"));
		}
		throw ErrorInfo("Failed to get PeerTube instance access token",
				error);
	}

	currentScopeVer = PEERTUBE_SCOPE_VERSION;

	return true;

} catch (ErrorInfo &info) {
	blog(LOG_WARNING, "%s: %s: %s", __FUNCTION__, info.message.c_str(),
	     info.error.c_str());
	return false;
}

static inline std::string get_instance_str(obs_data_t *settings)
{
	const char *instance = obs_data_get_string(settings, "instance");
	return instance ? instance : "";
}

bool PeerTubeAuth::RetryLogin()
{
	OBSBasic *main = OBSBasic::Get();
	obs_service_t *service = main->GetService();
	obs_data_t *settings = obs_service_get_settings(service);

	std::string instance = get_instance_str(settings);
	obs_data_release(settings);

	if (instance.empty()) {
		//TODO: Put a warning ?
		return false;
	}

	BasicOAuthLogin login(main, QTStr("PeerTubeAuth.Dialog.Title"), true);

	if (login.exec() == QDialog::Rejected) {
		return false;
	}

	std::shared_ptr<PeerTubeAuth> auth =
		std::make_shared<PeerTubeAuth>(peertubeDef);

	return GetToken(instance, QT_TO_UTF8(login.GetUsername()),
			QT_TO_UTF8(login.GetPassword()), true);
}

std::shared_ptr<Auth> PeerTubeAuth::Login(QWidget *parent)
{
	OBSBasic *main = OBSBasic::Get();
	obs_service_t *service = main->GetService();
	obs_data_t *settings = obs_service_get_settings(service);

	std::string instance = get_instance_str(settings);
	obs_data_release(settings);

	if (instance.empty()) {
		//TODO: Put a warning ?
		return nullptr;
	}

	std::shared_ptr<PeerTubeAuth> auth =
		std::make_shared<PeerTubeAuth>(peertubeDef);

	if (!auth->IsValidInstance(instance)) {
		return nullptr;
	}

	BasicOAuthLogin login(parent, QTStr("PeerTubeAuth.Dialog.Title"));

	if (!auth->GetClientInfo(instance)) {
		return nullptr;
	}

	if (login.exec() == QDialog::Rejected) {
		return nullptr;
	}

	if (auth->GetToken(instance, QT_TO_UTF8(login.GetUsername()),
			   QT_TO_UTF8(login.GetPassword()))) {
		return auth;
	}

	//TODO: Add GetAccountInfo to test if the token work ?

	return nullptr;
}

static std::shared_ptr<Auth> CreatePeerTubeAuth()
{
	return std::make_shared<PeerTubeAuth>(peertubeDef);
}

void RegisterPeerTubeAuth()
{
	BasicOAuth::RegisterOAuth(peertubeDef, CreatePeerTubeAuth,
				  PeerTubeAuth::Login);
}
