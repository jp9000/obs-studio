#pragma once

#include <windows.h>
#include <sapi.h>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <obs.h>
#include <util/circlebuf.h>
#include <util/windows/WinHandle.hpp>

#include <fstream>

class CircleBuf {
	circlebuf buf = {};

public:
	inline ~CircleBuf() { circlebuf_free(&buf); }
	inline operator circlebuf *() { return &buf; }
	inline circlebuf *operator->() { return &buf; }
};

class mssapi_captions;

class CaptionStream : public ISpAudio {
	volatile long refs = 1;
	SPAUDIOBUFFERINFO buf_info = {};
	mssapi_captions *handler;
	ULONG notify_size = 0;
	SPAUDIOSTATE state;
	WinHandle event;
	ULONG vol = 0;

	std::condition_variable cv;
	std::mutex m;
	std::vector<int16_t> temp_buf;
	WAVEFORMATEX format = {};

	CircleBuf buf;
	ULONG wait_size = 0;
	DWORD samplerate = 0;
	ULARGE_INTEGER pos = {};
	ULONGLONG write_pos = 0;

public:
	CaptionStream(DWORD samplerate, mssapi_captions *handler_);

	void Stop();
	void PushAudio(const void *data, size_t frames);

	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
	STDMETHODIMP_(ULONG) AddRef() override;
	STDMETHODIMP_(ULONG) Release() override;

	// ISequentialStream methods
	STDMETHODIMP Read(void *data, ULONG bytes, ULONG *read_bytes) override;
	STDMETHODIMP Write(const void *data, ULONG bytes,
			   ULONG *written_bytes) override;

	// IStream methods
	STDMETHODIMP Seek(LARGE_INTEGER move, DWORD origin,
			  ULARGE_INTEGER *new_pos) override;
	STDMETHODIMP SetSize(ULARGE_INTEGER new_size) override;
	STDMETHODIMP CopyTo(IStream *stream, ULARGE_INTEGER bytes,
			    ULARGE_INTEGER *read_bytes,
			    ULARGE_INTEGER *written_bytes) override;
	STDMETHODIMP Commit(DWORD commit_flags) override;
	STDMETHODIMP Revert(void) override;
	STDMETHODIMP LockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size,
				DWORD type) override;
	STDMETHODIMP UnlockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size,
				  DWORD type) override;
	STDMETHODIMP Stat(STATSTG *stg, DWORD flags) override;
	STDMETHODIMP Clone(IStream **stream) override;

	// ISpStreamFormat methods
	STDMETHODIMP GetFormat(GUID *guid,
			       WAVEFORMATEX **co_mem_wfex_out) override;

	// ISpAudio methods
	STDMETHODIMP SetState(SPAUDIOSTATE state, ULONGLONG reserved) override;
	STDMETHODIMP SetFormat(REFGUID guid_ref,
			       const WAVEFORMATEX *wfex) override;
	STDMETHODIMP GetStatus(SPAUDIOSTATUS *status) override;
	STDMETHODIMP SetBufferInfo(const SPAUDIOBUFFERINFO *buf_info) override;
	STDMETHODIMP GetBufferInfo(SPAUDIOBUFFERINFO *buf_info) override;
	STDMETHODIMP GetDefaultFormat(GUID *format,
				      WAVEFORMATEX **co_mem_wfex_out) override;
	STDMETHODIMP_(HANDLE) EventHandle(void) override;
	STDMETHODIMP GetVolumeLevel(ULONG *level) override;
	STDMETHODIMP SetVolumeLevel(ULONG level) override;
	STDMETHODIMP GetBufferNotifySize(ULONG *size) override;
	STDMETHODIMP SetBufferNotifySize(ULONG size) override;
};
