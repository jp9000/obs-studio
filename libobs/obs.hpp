/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

/* Useful C++ classes and bindings for base obs data */

#pragma once

#include "obs.h"

/* RAII wrappers */

template<typename T, void addref(T), void release(T)> class OBSRef;

using OBSSource = OBSRef<obs_source_t *, obs_source_addref, obs_source_release>;
using OBSScene = OBSRef<obs_scene_t *, obs_scene_addref, obs_scene_release>;
using OBSSceneItem =
	OBSRef<obs_sceneitem_t *, obs_sceneitem_addref, obs_sceneitem_release>;
using OBSData = OBSRef<obs_data_t *, obs_data_addref, obs_data_release>;
using OBSDataArray = OBSRef<obs_data_array_t *, obs_data_array_addref,
			    obs_data_array_release>;
using OBSOutput = OBSRef<obs_output_t *, obs_output_addref, obs_output_release>;
using OBSEncoder =
	OBSRef<obs_encoder_t *, obs_encoder_addref, obs_encoder_release>;
using OBSService =
	OBSRef<obs_service_t *, obs_service_addref, obs_service_release>;

using OBSWeakSource = OBSRef<obs_weak_source_t *, obs_weak_source_addref,
			     obs_weak_source_release>;
using OBSWeakOutput = OBSRef<obs_weak_output_t *, obs_weak_output_addref,
			     obs_weak_output_release>;
using OBSWeakEncoder = OBSRef<obs_weak_encoder_t *, obs_weak_encoder_addref,
			      obs_weak_encoder_release>;
using OBSWeakService = OBSRef<obs_weak_service_t *, obs_weak_service_addref,
			      obs_weak_service_release>;

template<typename T, void addref(T), void release(T)> class OBSRef {
	T val;

	inline OBSRef &Replace(T valIn)
	{
		addref(valIn);
		release(val);
		val = valIn;
		return *this;
	}

	struct TakeOwnership {
	};
	inline OBSRef(T val, TakeOwnership) : val(val) {}

public:
	inline OBSRef() : val(nullptr) {}
	inline OBSRef(T val_) : val(val_) { addref(val); }
	inline OBSRef(const OBSRef &ref) : val(ref.val) { addref(val); }
	inline OBSRef(OBSRef &&ref) : val(ref.val) { ref.val = nullptr; }

	inline ~OBSRef() { release(val); }

	inline OBSRef &operator=(T valIn) { return Replace(valIn); }
	inline OBSRef &operator=(const OBSRef &ref) { return Replace(ref.val); }

	inline OBSRef &operator=(OBSRef &&ref)
	{
		if (this != &ref) {
			release(val);
			val = ref.val;
			ref.val = nullptr;
		}

		return *this;
	}

	inline operator T() const { return val; }
	inline T Get() const { return val; }

	inline bool operator==(T p) const { return val == p; }
	inline bool operator!=(T p) const { return val != p; }

	friend OBSSource OBSGetStrongRef(obs_weak_source_t *weak);
	friend OBSWeakSource OBSGetWeakRef(obs_source_t *source);

	friend OBSOutput OBSGetStrongRef(obs_weak_output_t *weak);
	friend OBSWeakOutput OBSGetWeakRef(obs_output_t *output);

	friend OBSEncoder OBSGetStrongRef(obs_weak_encoder_t *weak);
	friend OBSWeakEncoder OBSGetWeakRef(obs_encoder_t *encoder);

	friend OBSService OBSGetStrongRef(obs_weak_service_t *weak);
	friend OBSWeakService OBSGetWeakRef(obs_service_t *service);
};

inline OBSSource OBSGetStrongRef(obs_weak_source_t *weak)
{
	return {obs_weak_source_get_source(weak), OBSSource::TakeOwnership()};
}

inline OBSWeakSource OBSGetWeakRef(obs_source_t *source)
{
	return {obs_source_get_weak_source(source),
		OBSWeakSource::TakeOwnership()};
}

inline OBSOutput OBSGetStrongRef(obs_weak_output_t *weak)
{
	return {obs_weak_output_get_output(weak), OBSOutput::TakeOwnership()};
}

inline OBSWeakOutput OBSGetWeakRef(obs_output_t *output)
{
	return {obs_output_get_weak_output(output),
		OBSWeakOutput::TakeOwnership()};
}

inline OBSEncoder OBSGetStrongRef(obs_weak_encoder_t *weak)
{
	return {obs_weak_encoder_get_encoder(weak),
		OBSEncoder::TakeOwnership()};
}

inline OBSWeakEncoder OBSGetWeakRef(obs_encoder_t *encoder)
{
	return {obs_encoder_get_weak_encoder(encoder),
		OBSWeakEncoder::TakeOwnership()};
}

inline OBSService OBSGetStrongRef(obs_weak_service_t *weak)
{
	return {obs_weak_service_get_service(weak),
		OBSService::TakeOwnership()};
}

inline OBSWeakService OBSGetWeakRef(obs_service_t *service)
{
	return {obs_service_get_weak_service(service),
		OBSWeakService::TakeOwnership()};
}

/* objects that are not meant to be instanced */
template<typename T, void destroy(T)> class OBSObj {
	T obj;

public:
	inline OBSObj() : obj(nullptr) {}
	inline OBSObj(T obj_) : obj(obj_) {}
	inline OBSObj(const OBSObj &) = delete;
	inline OBSObj(OBSObj &&other) : obj(other.obj) { other.obj = nullptr; }

	inline ~OBSObj() { destroy(obj); }

	inline OBSObj &operator=(T obj_)
	{
		if (obj_ != obj)
			destroy(obj);
		obj = obj_;
		return *this;
	}
	inline OBSObj &operator=(const OBSObj &) = delete;
	inline OBSObj &operator=(OBSObj &&other)
	{
		if (obj)
			destroy(obj);
		obj = other.obj;
		other.obj = nullptr;
		return *this;
	}

	inline operator T() const { return obj; }

	inline bool operator==(T p) const { return obj == p; }
	inline bool operator!=(T p) const { return obj != p; }
};

using OBSDisplay = OBSObj<obs_display_t *, obs_display_destroy>;
using OBSView = OBSObj<obs_view_t *, obs_view_destroy>;

/* signal handler connection */
class OBSSignal {
	signal_handler_t *handler;
	const char *signal;
	signal_callback_t callback;
	void *param;

public:
	inline OBSSignal()
		: handler(nullptr),
		  signal(nullptr),
		  callback(nullptr),
		  param(nullptr)
	{
	}

	inline OBSSignal(signal_handler_t *handler_, const char *signal_,
			 signal_callback_t callback_, void *param_)
		: handler(handler_),
		  signal(signal_),
		  callback(callback_),
		  param(param_)
	{
		signal_handler_connect_ref(handler, signal, callback, param);
	}

	inline void Disconnect()
	{
		signal_handler_disconnect(handler, signal, callback, param);
		handler = nullptr;
		signal = nullptr;
		callback = nullptr;
		param = nullptr;
	}

	inline ~OBSSignal() { Disconnect(); }

	inline void Connect(signal_handler_t *handler_, const char *signal_,
			    signal_callback_t callback_, void *param_)
	{
		Disconnect();

		handler = handler_;
		signal = signal_;
		callback = callback_;
		param = param_;
		signal_handler_connect_ref(handler, signal, callback, param);
	}

	OBSSignal(const OBSSignal &) = delete;
	OBSSignal(OBSSignal &&other) noexcept
		: handler(other.handler),
		  signal(other.signal),
		  callback(other.callback),
		  param(other.param)
	{
		other.handler = nullptr;
		other.signal = nullptr;
		other.callback = nullptr;
		other.param = nullptr;
	}

	OBSSignal &operator=(const OBSSignal &) = delete;
	OBSSignal &operator=(OBSSignal &&other) noexcept
	{
		Disconnect();

		handler = other.handler;
		signal = other.signal;
		callback = other.callback;
		param = other.param;

		other.handler = nullptr;
		other.signal = nullptr;
		other.callback = nullptr;
		other.param = nullptr;

		return *this;
	}
};
