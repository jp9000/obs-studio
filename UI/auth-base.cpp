#include "auth-base.hpp"
#include "window-basic-main.hpp"

#include <vector>
#include <map>

struct AuthInfo {
	Auth::Def def;
	Auth::create_cb create;
};

static std::vector<AuthInfo> authDefs;

void Auth::RegisterAuth(const Def &d, create_cb create)
{
	AuthInfo info = {d, create};
	authDefs.push_back(info);
}

std::shared_ptr<Auth> Auth::Create(const std::string &service)
{
	for (auto &a : authDefs) {
		if (service.find(a.def.service) != std::string::npos) {
			return a.create();
		}
	}

	return nullptr;
}

Auth::Type Auth::AuthType(const std::string &service)
{
	for (auto &a : authDefs) {
		if (service.find(a.def.service) != std::string::npos) {
			return a.def.type;
		}
	}

	return Type::None;
}

void Auth::Load()
{
	OBSBasic *main = OBSBasic::Get();
	const char *typeStr = config_get_string(main->Config(), "Auth", "Type");
	if (!typeStr)
		typeStr = "";

	main->auth = Create(typeStr);
	if (main->auth) {
		if (main->auth->LoadInternal()) {
			main->auth->LoadUI();
		}
	}
}

void Auth::Save()
{
	OBSBasic *main = OBSBasic::Get();
	Auth *auth = main->auth.get();
	if (!auth) {
		if (config_has_user_value(main->Config(), "Auth", "Type")) {
			config_remove_value(main->Config(), "Auth", "Type");
			config_save_safe(main->Config(), "tmp", nullptr);
		}
		return;
	}

	config_set_string(main->Config(), "Auth", "Type", auth->service());
	auth->SaveInternal();
	config_save_safe(main->Config(), "tmp", nullptr);
}
