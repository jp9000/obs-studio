#pragma once

#include "captions-handler.hpp"
#include "captions-mssapi-stream.hpp"
#include <util/windows/HRError.hpp>
#include <util/windows/ComPtr.hpp>
#include <util/windows/WinHandle.hpp>
#include <util/windows/CoTaskMemPtr.hpp>
#include <util/threading.h>
#include <util/platform.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#include <sphelper.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <obs.hpp>

#include <thread>

class mssapi_captions : public captions_handler {
	friend class CaptionStream;

	ComPtr<CaptionStream> audio;
	ComPtr<ISpObjectToken> token;
	ComPtr<ISpRecoGrammar> grammar;
	ComPtr<ISpRecognizer> recognizer;
	ComPtr<ISpRecoContext> context;

	HANDLE notify;
	WinHandle stop;
	std::thread t;
	bool started = false;

	void main_thread();

public:
	mssapi_captions(captions_cb callback, const std::string &lang);
	virtual ~mssapi_captions();
	virtual void pcm_data(const void *data, size_t frames) override;
};
