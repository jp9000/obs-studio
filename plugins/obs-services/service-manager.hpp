#pragma once

#include <map>
#include <string>
#include <memory>

#include "service-factory.hpp"

class service_manager {
	std::map<std::string, std::shared_ptr<service_factory>> _factories;

public:
	service_manager(){};
	~service_manager();

	void register_services();

	static void initialize();

	static void finalize();
};
