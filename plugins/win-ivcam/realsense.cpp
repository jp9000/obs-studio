/*
Copyright(c) 2016, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

- Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and /
or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <obs-module.h>
#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>
#include <thread>
#include <atomic>

#include "seg_library/SegService.h"

using namespace std;

#include <Tlhelp32.h>
#include <shlobj.h>
#include <sstream>

#define do_log(level, format, ...) \
	blog(level, "[win-ivcam (%s): '%s'] " format, \
			__FUNCTION__, obs_source_get_name(source), \
			##__VA_ARGS__)

#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...)  do_log(LOG_INFO,    format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG,   format, ##__VA_ARGS__)

void TerminateProcessByName(wchar_t *procName)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(entry);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
	BOOL hRes = Process32First(snapshot, &entry);

	while (hRes) {
		if (wcscmp(entry.szExeFile, procName) == 0) {
			HANDLE process = OpenProcess(PROCESS_TERMINATE, 0,
					(DWORD)entry.th32ProcessID);

			if (process != NULL) {
				TerminateProcess(process, 0);
				CloseHandle(process);
			}
		}
		hRes = Process32Next(snapshot, &entry);
	}
	CloseHandle(snapshot);
}

struct IVCamSource {
	SegServer* pSegServer;
	obs_source_t *source;
	thread camThread;
	atomic<bool> stopThread = false;

	void IVCamSource::PushSegmentedFrameData(SegImage *segmented_image)
	{
		/*uint8_t* pix = (uint8_t*)segmented_image->GetData();
		if (pix != NULL) {
			int pixels = segmented_image->Width() *
				segmented_image->Height();

			for (int i = 0; i < pixels; i++) {
				if (pix[3]<100) {
					pix[0] /= 3; pix[1] /= 2; pix[2] /= 3;
				}
				pix += 4;
			}
		}*/

		obs_source_frame frame = {};
		frame.data[0] = (uint8_t*)segmented_image->GetData();
		frame.linesize[0] = segmented_image->Pitch();
		frame.format = VIDEO_FORMAT_BGRA;
		frame.width = segmented_image->Width();
		frame.height = segmented_image->Height();
		frame.timestamp = os_gettime_ns();
		obs_source_output_video(source, &frame);
	}

	void IVCamSource::CamThread()
	{
		pSegServer = SegServer::CreateServer();

		if (!pSegServer) {
			warn("SegServer::CreateServer failed\n");
			return;
		}

		SegServer::ServiceStatus status = pSegServer->Init();

		if (status != SegServer::ServiceStatus::SERVICE_NO_ERROR) {
			warn("SegServer initialization error (%d)\n", status);
			return;
		}

		while (!stopThread) {
			SegImage* image = nullptr;
			SegServer::ServiceStatus error =
				pSegServer->GetFrame(&image);
			if (error != SegServer::ServiceStatus::SERVICE_NO_ERROR
					|| image->Width() == 0
					|| image->Height() == 0) {
				//warn("AcquireFrame failed (%d)\n", error);
				continue;
			}

			PushSegmentedFrameData(image);

			delete image;
		}

		pSegServer->Stop();
		delete pSegServer;
	}

	inline IVCamSource::IVCamSource(obs_source_t* source_) :
		source(source_)
	{
		stopThread = false;
		camThread = std::thread(&IVCamSource::CamThread, this);
	}

	static void* IVCamSource::CreateIVCamSource(obs_data_t*,
			obs_source_t *source)
	{
		//kill whatever is running just in case
		TerminateProcessByName(L"seg_service.exe");
		return new IVCamSource(source);
	}

	inline IVCamSource::~IVCamSource()
	{
		//done using it, kill the seg_service
		stopThread = true;
		if (camThread.joinable()) camThread.join();
		TerminateProcessByName(L"seg_service.exe");
	}
};

static const char *GetIVCamName(void*)
{
	return "Intel(R) RealSense(TM) 3D Camera GreenScreen";
}

static void DestroyIVCamSource(void *data)
{
	delete reinterpret_cast<IVCamSource*>(data);
}

OBS_DECLARE_MODULE()

bool obs_module_load(void)
{
	obs_source_info info = {};
	info.id = "win-ivcam";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO;
	info.get_name = GetIVCamName;
	info.create = IVCamSource::CreateIVCamSource;
	info.destroy = DestroyIVCamSource;
	info.icon_type = OBS_ICON_TYPE_CAMERA;
	obs_register_source(&info);
	return true;
}
