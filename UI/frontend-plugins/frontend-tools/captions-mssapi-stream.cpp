#include "captions-mssapi-stream.hpp"
#include "captions-mssapi.hpp"
#include <mmreg.h>
#include <util/windows/CoTaskMemPtr.hpp>
#include <util/threading.h>
#include <util/base.h>

using namespace std;

#if 0
#define debugfunc(format, ...)                                     \
	blog(LOG_DEBUG, "[Captions] %s(" format ")", __FUNCTION__, \
	     ##__VA_ARGS__)
#else
#define debugfunc(format, ...)
#endif

CaptionStream::CaptionStream(DWORD samplerate_, mssapi_captions *handler_)
	: handler(handler_),
	  samplerate(samplerate_),
	  event(CreateEvent(nullptr, false, false, nullptr))
{
	buf_info.ulMsMinNotification = 50;
	buf_info.ulMsBufferSize = 500;
	buf_info.ulMsEventBias = 0;

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 1;
	format.nSamplesPerSec = 16000;
	format.nAvgBytesPerSec = format.nSamplesPerSec * sizeof(uint16_t);
	format.nBlockAlign = 2;
	format.wBitsPerSample = 16;
	format.cbSize = sizeof(format);
}

void CaptionStream::Stop()
{
	{
		lock_guard<mutex> lock(m);
		circlebuf_free(buf);
	}

	cv.notify_one();
}

void CaptionStream::PushAudio(const void *data, size_t frames)
{
	bool ready = false;

	lock_guard<mutex> lock(m);
	circlebuf_push_back(buf, data, frames * sizeof(int16_t));
	write_pos += frames * sizeof(int16_t);

	if (wait_size && buf->size >= wait_size)
		ready = true;
	if (ready)
		cv.notify_one();
}

// IUnknown methods

STDMETHODIMP CaptionStream::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown) {
		AddRef();
		*ppv = this;

	} else if (riid == IID_IStream) {
		AddRef();
		*ppv = (IStream *)this;

	} else if (riid == IID_ISpStreamFormat) {
		AddRef();
		*ppv = (ISpStreamFormat *)this;

	} else if (riid == IID_ISpAudio) {
		AddRef();
		*ppv = (ISpAudio *)this;

	} else {
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	return NOERROR;
}

STDMETHODIMP_(ULONG) CaptionStream::AddRef()
{
	return (ULONG)os_atomic_inc_long(&refs);
}

STDMETHODIMP_(ULONG) CaptionStream::Release()
{
	ULONG new_refs = (ULONG)os_atomic_dec_long(&refs);
	if (!new_refs)
		delete this;

	return new_refs;
}

// ISequentialStream methods

STDMETHODIMP CaptionStream::Read(void *data, ULONG bytes, ULONG *read_bytes)
{
	HRESULT hr = S_OK;
	size_t cur_size;

	debugfunc("data, %lu, read_bytes", bytes);
	if (!data)
		return STG_E_INVALIDPOINTER;

	{
		lock_guard<mutex> lock1(m);
		wait_size = bytes;
		cur_size = buf->size;
	}

	unique_lock<mutex> lock(m);

	if (bytes > cur_size)
		cv.wait(lock);

	if (bytes > (ULONG)buf->size) {
		bytes = (ULONG)buf->size;
		hr = S_FALSE;
	}
	if (bytes)
		circlebuf_pop_front(buf, data, bytes);
	if (read_bytes)
		*read_bytes = bytes;

	wait_size = 0;
	pos.QuadPart += bytes;
	return hr;
}

STDMETHODIMP CaptionStream::Write(const void *, ULONG bytes, ULONG *)
{
	debugfunc("data, %lu, written_bytes", bytes);
	UNUSED_PARAMETER(bytes);

	return STG_E_INVALIDFUNCTION;
}

// IStream methods

STDMETHODIMP CaptionStream::Seek(LARGE_INTEGER move, DWORD origin,
				 ULARGE_INTEGER *new_pos)
{
	debugfunc("%lld, %lx, new_pos", move, origin);
	UNUSED_PARAMETER(move);
	UNUSED_PARAMETER(origin);

	if (!new_pos)
		return E_POINTER;

	if (origin != SEEK_CUR || move.QuadPart != 0)
		return E_NOTIMPL;

	*new_pos = pos;
	return S_OK;
}

STDMETHODIMP CaptionStream::SetSize(ULARGE_INTEGER new_size)
{
	debugfunc("%llu", new_size);
	UNUSED_PARAMETER(new_size);
	return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP CaptionStream::CopyTo(IStream *stream, ULARGE_INTEGER bytes,
				   ULARGE_INTEGER *read_bytes,
				   ULARGE_INTEGER *written_bytes)
{
	HRESULT hr;

	debugfunc("stream, %llu, read_bytes, written_bytes", bytes);

	if (!stream)
		return STG_E_INVALIDPOINTER;

	ULONG written = 0;
	if (bytes.QuadPart > (ULONGLONG)buf->size)
		bytes.QuadPart = (ULONGLONG)buf->size;

	lock_guard<mutex> lock(m);
	temp_buf.resize((size_t)bytes.QuadPart);
	circlebuf_peek_front(buf, &temp_buf[0], (size_t)bytes.QuadPart);

	hr = stream->Write(temp_buf.data(), (ULONG)bytes.QuadPart, &written);

	if (read_bytes)
		*read_bytes = bytes;
	if (written_bytes)
		written_bytes->QuadPart = written;

	return hr;
}

STDMETHODIMP CaptionStream::Commit(DWORD commit_flags)
{
	debugfunc("%lx", commit_flags);
	UNUSED_PARAMETER(commit_flags);
	/* TODO? */
	return S_OK;
}

STDMETHODIMP CaptionStream::Revert(void)
{
	debugfunc("");
	return S_OK;
}

STDMETHODIMP CaptionStream::LockRegion(ULARGE_INTEGER offset,
				       ULARGE_INTEGER size, DWORD type)
{
	debugfunc("%llu, %llu, %ld", offset, size, type);
	UNUSED_PARAMETER(offset);
	UNUSED_PARAMETER(size);
	UNUSED_PARAMETER(type);
	/* TODO? */
	return STG_E_INVALIDFUNCTION;
}

STDMETHODIMP CaptionStream::UnlockRegion(ULARGE_INTEGER offset,
					 ULARGE_INTEGER size, DWORD type)
{
	debugfunc("%llu, %llu, %ld", offset, size, type);
	UNUSED_PARAMETER(offset);
	UNUSED_PARAMETER(size);
	UNUSED_PARAMETER(type);
	/* TODO? */
	return STG_E_INVALIDFUNCTION;
}

static const wchar_t *stat_name = L"Caption stream";

STDMETHODIMP CaptionStream::Stat(STATSTG *stg, DWORD flag)
{
	debugfunc("stg, %lu", flag);

	if (!stg)
		return E_POINTER;

	lock_guard<mutex> lock(m);
	*stg = {};
	stg->type = STGTY_STREAM;
	stg->cbSize.QuadPart = (ULONGLONG)buf->size;

	if (flag == STATFLAG_DEFAULT) {
		size_t byte_size = (wcslen(stat_name) + 1) * sizeof(wchar_t);
		stg->pwcsName = (wchar_t *)CoTaskMemAlloc(byte_size);
		memcpy(stg->pwcsName, stat_name, byte_size);
	}

	return S_OK;
}

STDMETHODIMP CaptionStream::Clone(IStream **stream)
{
	debugfunc("stream");
	*stream = nullptr;
	return E_NOTIMPL;
}

// ISpStreamFormat methods

STDMETHODIMP CaptionStream::GetFormat(GUID *guid,
				      WAVEFORMATEX **co_mem_wfex_out)
{
	debugfunc("guid, co_mem_wfex_out");

	if (!guid || !co_mem_wfex_out)
		return E_POINTER;

	if (format.wFormatTag == 0) {
		*co_mem_wfex_out = nullptr;
		return S_OK;
	}

	void *wfex = CoTaskMemAlloc(sizeof(format));
	memcpy(wfex, &format, sizeof(format));

	*co_mem_wfex_out = (WAVEFORMATEX *)wfex;
	return S_OK;
}

// ISpAudio methods

STDMETHODIMP CaptionStream::SetState(SPAUDIOSTATE state_, ULONGLONG)
{
	debugfunc("%lu, reserved", state_);
	state = state_;
	return S_OK;
}

STDMETHODIMP CaptionStream::SetFormat(REFGUID guid_ref,
				      const WAVEFORMATEX *wfex)
{
	debugfunc("guid, wfex");
	if (!wfex)
		return E_INVALIDARG;

	if (guid_ref == SPDFID_WaveFormatEx) {
		lock_guard<mutex> lock(m);
		memcpy(&format, wfex, sizeof(format));
		if (!handler->reset_resampler(AUDIO_FORMAT_16BIT,
					      wfex->nSamplesPerSec))
			return E_FAIL;

		/* 50 msec */
		DWORD size = format.nSamplesPerSec / 20;
		DWORD byte_size = size * format.nBlockAlign;
		circlebuf_reserve(buf, (size_t)byte_size);
	}
	return S_OK;
}

STDMETHODIMP CaptionStream::GetStatus(SPAUDIOSTATUS *status)
{
	debugfunc("status");

	if (!status)
		return E_POINTER;

	/* TODO? */
	lock_guard<mutex> lock(m);
	*status = {};
	status->cbNonBlockingIO = (ULONG)buf->size;
	status->State = state;
	status->CurSeekPos = pos.QuadPart;
	status->CurDevicePos = write_pos;
	return S_OK;
}

STDMETHODIMP CaptionStream::SetBufferInfo(const SPAUDIOBUFFERINFO *buf_info_)
{
	debugfunc("buf_info");

	/* TODO */
	buf_info = *buf_info_;
	return S_OK;
}

STDMETHODIMP CaptionStream::GetBufferInfo(SPAUDIOBUFFERINFO *buf_info_)
{
	debugfunc("buf_info");
	if (!buf_info_)
		return E_POINTER;

	*buf_info_ = buf_info;
	return S_OK;
}

STDMETHODIMP CaptionStream::GetDefaultFormat(GUID *format,
					     WAVEFORMATEX **co_mem_wfex_out)
{
	debugfunc("format, co_mem_wfex_out");

	if (!format || !co_mem_wfex_out)
		return E_POINTER;

	void *wfex = CoTaskMemAlloc(sizeof(format));
	memcpy(wfex, &format, sizeof(format));

	*format = SPDFID_WaveFormatEx;
	*co_mem_wfex_out = (WAVEFORMATEX *)wfex;
	return S_OK;
}

STDMETHODIMP_(HANDLE) CaptionStream::EventHandle(void)
{
	debugfunc("");
	return event;
}

STDMETHODIMP CaptionStream::GetVolumeLevel(ULONG *level)
{
	debugfunc("level");
	if (!level)
		return E_POINTER;

	*level = vol;
	return S_OK;
}

STDMETHODIMP CaptionStream::SetVolumeLevel(ULONG level)
{
	debugfunc("%lu", level);
	vol = level;
	return S_OK;
}

STDMETHODIMP CaptionStream::GetBufferNotifySize(ULONG *size)
{
	debugfunc("size");
	if (!size)
		return E_POINTER;
	*size = notify_size;
	return S_OK;
}

STDMETHODIMP CaptionStream::SetBufferNotifySize(ULONG size)
{
	debugfunc("%lu", size);
	notify_size = size;
	return S_OK;
}
