#ifndef AMFCONSOLETRACEWRITER_H
#define AMFCONSOLETRACEWRITER_H

#include <string>
#include <memory>
#include <sstream>
#include <ios>
#include "AMF/include/core/Result.h"
#include "AMF/include/core/Trace.h"
#include "AMF/include/core/Data.h"
#define OBS_AMF_TRACE_WRITER L"OBS_AMF_TRACE_WRITER"

class obs_amf_trace_writer : public amf::AMFTraceWriter {
public:
	obs_amf_trace_writer() {}
	virtual void AMF_CDECL_CALL Write(const wchar_t *scope,
					  const wchar_t *message) override;
	virtual void AMF_CDECL_CALL Flush() override {}
};
#endif // AMFCONSOLETRACEWRITER_H
