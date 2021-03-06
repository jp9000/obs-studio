#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <obs-module.h>
#include "external/nvEncodeAPI.h"

typedef NVENCSTATUS(NVENCAPI *NV_CREATE_INSTANCE_FUNC)(
	NV_ENCODE_API_FUNCTION_LIST *);

extern const char *nv_error_name(NVENCSTATUS err);
extern NV_ENCODE_API_FUNCTION_LIST nv;
extern NV_CREATE_INSTANCE_FUNC nv_create_instance;
extern bool init_nvenc(obs_encoder_t *encoder);
bool nv_failed(obs_encoder_t *encoder, NVENCSTATUS err, const char *func,
	       const char *call);
