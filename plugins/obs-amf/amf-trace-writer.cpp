#include "amf-trace-writer.hpp"
#include <iostream>
#include <iomanip>
#include <obs-module.h>
#include <obs-encoder.h>

void obs_amf_trace_writer::Write(const wchar_t *scope, const wchar_t *message)
{
	blog(LOG_INFO, "[AMF] [%ls] %ls", scope, message);
}
