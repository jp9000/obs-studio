#include "virtualcam-filter.hpp"
#include "sleepto.h"

#include <shlobj_core.h>
#include <strsafe.h>
#include <inttypes.h>

using namespace DShow;

extern const uint8_t *get_placeholder();

/* ========================================================================= */

VCamFilter::VCamFilter()
	: OutputFilter(VideoFormat::NV12, DEFAULT_CX, DEFAULT_CY,
		       DEFAULT_INTERVAL)
{
	thread_start = CreateEvent(nullptr, true, false, nullptr);
	thread_stop = CreateEvent(nullptr, true, false, nullptr);

	AddVideoFormat(VideoFormat::I420, DEFAULT_CX, DEFAULT_CY,
		       DEFAULT_INTERVAL);
	AddVideoFormat(VideoFormat::YUY2, DEFAULT_CX, DEFAULT_CY,
		       DEFAULT_INTERVAL);

	/* ---------------------------------------- */
	/* load placeholder image                   */

	placeholder = get_placeholder();

	/* ---------------------------------------- */
	/* detect if this filter is within obs      */

	wchar_t file[MAX_PATH];
	if (!GetModuleFileNameW(nullptr, file, MAX_PATH)) {
		file[0] = 0;
	}

#ifdef _WIN64
	const wchar_t *obs_process = L"obs64.exe";
#else
	const wchar_t *obs_process = L"obs32.exe";
#endif

	in_obs = !!wcsstr(file, obs_process);

	/* ---------------------------------------- */
	/* add last/current obs res/interval        */

	uint32_t new_cx = cx;
	uint32_t new_cy = cy;
	uint64_t new_interval = interval;

	vq = video_queue_open();
	if (vq) {
		if (video_queue_state(vq) == SHARED_QUEUE_STATE_READY) {
			video_queue_get_info(vq, &new_cx, &new_cy,
					     &new_interval);
		}

		/* don't keep it open until the filter actually starts */
		video_queue_close(vq);
		vq = nullptr;
	} else {
		wchar_t res_file[MAX_PATH];
		SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr,
				 SHGFP_TYPE_CURRENT, res_file);
		StringCbCat(res_file, sizeof(res_file),
			    L"\\obs-virtualcam.txt");

		HANDLE file = CreateFileW(res_file, GENERIC_READ, 0, nullptr,
					  OPEN_EXISTING, 0, nullptr);
		if (file) {
			char res[128];
			DWORD len = 0;

			ReadFile(file, res, sizeof(res), &len, nullptr);
			CloseHandle(file);

			res[len] = 0;
			int vals = sscanf(res,
					  "%" PRIu32 "x%" PRIu32 "x%" PRIu64,
					  &new_cx, &new_cy, &new_interval);
			if (vals != 3) {
				new_cx = cx;
				new_cy = cy;
				new_interval = interval;
			}
		}
	}

	if (new_cx != cx || new_cy != cy || new_interval != interval) {
		AddVideoFormat(VideoFormat::NV12, new_cx, new_cy, new_interval);
		AddVideoFormat(VideoFormat::I420, new_cx, new_cy, new_interval);
		AddVideoFormat(VideoFormat::YUY2, new_cx, new_cy, new_interval);
		SetVideoFormat(VideoFormat::NV12, new_cx, new_cy, new_interval);
		cx = new_cx;
		cy = new_cy;
		interval = new_interval;
	}

	nv12_scale_init(&scaler, TARGET_FORMAT_NV12, cx, cy, cx, cy);

	/* ---------------------------------------- */

	th = std::thread([this] { Thread(); });

	AddRef();
}

VCamFilter::~VCamFilter()
{
	SetEvent(thread_stop);
	th.join();
	video_queue_close(vq);
}

const wchar_t *VCamFilter::FilterName() const
{
	return L"VCamFilter";
}

STDMETHODIMP VCamFilter::Pause()
{
	HRESULT hr;

	hr = OutputFilter::Pause();
	if (FAILED(hr)) {
		return hr;
	}

	SetEvent(thread_start);
	return S_OK;
}

inline uint64_t VCamFilter::GetTime()
{
	if (!!clock) {
		REFERENCE_TIME rt;
		HRESULT hr = clock->GetTime(&rt);
		if (SUCCEEDED(hr)) {
			return (uint64_t)rt;
		}
	}

	return gettime_100ns();
}

void VCamFilter::Thread()
{
	HANDLE h[2] = {thread_start, thread_stop};
	DWORD ret = WaitForMultipleObjects(2, h, false, INFINITE);
	if (ret != WAIT_OBJECT_0)
		return;

	uint64_t cur_time = gettime_100ns();
	uint64_t filter_time = GetTime();

	cx = GetCX();
	cy = GetCY();
	interval = GetInterval();

	nv12_scale_init(&scaler, TARGET_FORMAT_NV12, GetCX(), GetCY(), cx, cy);

	while (!stopped()) {
		Frame(filter_time);
		sleepto_100ns(cur_time += interval);
		filter_time += interval;
	}
}

void VCamFilter::Frame(uint64_t ts)
{
	uint32_t new_cx = cx;
	uint32_t new_cy = cy;
	uint64_t new_interval = interval;

	if (!vq) {
		vq = video_queue_open();
	}

	enum queue_state state = video_queue_state(vq);
	if (state != prev_state) {
		if (state == SHARED_QUEUE_STATE_READY) {
			video_queue_get_info(vq, &new_cx, &new_cy,
					     &new_interval);
		} else if (state == SHARED_QUEUE_STATE_STOPPING) {
			video_queue_close(vq);
			vq = nullptr;
		}

		prev_state = state;
	}

	if (state != SHARED_QUEUE_STATE_READY) {
		new_cx = DEFAULT_CX;
		new_cy = DEFAULT_CY;
		new_interval = DEFAULT_INTERVAL;
	}

	if (new_cx != cx || new_cy != cy || new_interval != interval) {
		if (in_obs) {
			SetVideoFormat(GetVideoFormat(), new_cx, new_cy,
				       new_interval);
		}

		nv12_scale_init(&scaler, TARGET_FORMAT_NV12, GetCX(), GetCY(),
				new_cx, new_cy);

		cx = new_cx;
		cy = new_cy;
		interval = new_interval;
	}

	if (GetVideoFormat() == VideoFormat::I420)
		scaler.format = TARGET_FORMAT_I420;
	else if (GetVideoFormat() == VideoFormat::YUY2)
		scaler.format = TARGET_FORMAT_YUY2;
	else
		scaler.format = TARGET_FORMAT_NV12;

	uint8_t *ptr;
	if (LockSampleData(&ptr)) {
		if (state == SHARED_QUEUE_STATE_READY)
			ShowOBSFrame(ptr);
		else
			ShowDefaultFrame(ptr);

		UnlockSampleData(ts, ts + interval);
	}
}

void VCamFilter::ShowOBSFrame(uint8_t *ptr)
{
	uint64_t temp;
	if (!video_queue_read(vq, &scaler, ptr, &temp)) {
		video_queue_close(vq);
		vq = nullptr;
	}
}

void VCamFilter::ShowDefaultFrame(uint8_t *ptr)
{
	if (placeholder) {
		nv12_do_scale(&scaler, ptr, placeholder);
	} else {
		memset(ptr, 127, GetCX() * GetCY() * 3 / 2);
	}
}
