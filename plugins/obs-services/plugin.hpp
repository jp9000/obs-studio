#pragma once

extern "C" {
#include <obs-module.h>
}

#define blog(level, msg, ...) blog(level, "[obs-services] " msg, ##__VA_ARGS__)
