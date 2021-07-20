#include <memory>

#include "AMF/include/core/Factory.h"
#include "AMF/include/core/Trace.h"
#include "AMF/include/components/VideoEncoderVCE.h"
#include "amf-trace-writer.hpp"
#include <map>
#include <list>
#include <memory>

extern "C" {
#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif
#include <util/base.h>
}

#define AMF_LOG(level, ...) blog(level, "[AMF] " __VA_ARGS__)
#define AMF_LOG_ERROR(format, ...) AMF_LOG(LOG_ERROR, format, __VA_ARGS__)
#define AMF_LOG_WARNING(...) AMF_LOG(LOG_WARNING, __VA_ARGS__)
#define AMF_CLAMP(val, low, high) (val > high ? high : (val < low ? low : val))

struct EncoderCaps {
	struct NameValuePair {
		int32_t value;
		std::wstring name;
	};
	std::list<NameValuePair> rate_control_methods;
};

class AMF {
public:
	static void Initialize();
	static AMF *Instance();
	static void Finalize();

private: // Private Initializer & Finalizer
	AMF();
	~AMF();

public: // Remove all Copy operators
	AMF(AMF const &) = delete;
	void operator=(AMF const &) = delete;
#pragma endregion Singleton

public:
	amf::AMFFactory *GetFactory();
	amf::AMFTrace *GetTrace();
	uint64_t GetRuntimeVersion();
	void FillCaps();
	EncoderCaps GetH264Caps(int deviceNum) const;
	EncoderCaps GetHEVCCaps(int deviceNum) const;
	bool H264Available() const;
	bool HEVCAvailable() const;

private:
	/// AMF Values
	HMODULE amf_module;
	uint64_t amf_version;

	/// AMF Functions
	AMFQueryVersion_Fn amf_version_fun;
	AMFInit_Fn amf_init_fun;

	/// AMF Objects
	amf::AMFFactory *factory;
	amf::AMFTrace *trace;
	std::unique_ptr<obs_amf_trace_writer> trace_writer;
	std::map<int, EncoderCaps> h264_caps;
	std::map<int, EncoderCaps> hevc_caps;
};
