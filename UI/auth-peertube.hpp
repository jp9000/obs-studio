#pragma once

#include "auth-oauth.hpp"

class PeerTubeAuth : public BasicOAuth {
	Q_OBJECT

	std::string client_id;
	std::string client_secret;

	virtual void SaveInternal() override;
	virtual bool LoadInternal() override;

	bool IsValidInstance(const std::string &instance);
	bool GetClientInfo(const std::string &instance);
	bool GetToken(const std::string &instance,
		      const std::string &username = nullptr,
		      const std::string &password = nullptr,
		      bool retry = false);

	bool uiLoaded = false;

	virtual bool RetryLogin() override;

	std::string videoId_;
	std::string server_;
	std::string key_;

public:
	inline PeerTubeAuth(const Def &d) : BasicOAuth(d) {}

	inline const std::string &videoId() const { return videoId_; }
	inline const std::string &server() const { return server_; }
	inline const std::string &key() const { return key_; }

	virtual void OnStreamConfig() override;

	static std::shared_ptr<Auth> Login(QWidget *parent);
};
