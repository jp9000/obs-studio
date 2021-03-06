#pragma once

#define LOG(level, message, ...) \
	blog(level, "%s: " message, "decklink", ##__VA_ARGS__)

#include <obs-module.h>
#include "decklink-device.hpp"
#include "../../libobs/media-io/video-scaler.h"
#include "OBSVideoFrame.h"

class AudioRepacker;
class DecklinkBase;

class DeckLinkDeviceInstance : public IDeckLinkInputCallback {
protected:
	struct obs_source_frame2 currentFrame;
	struct obs_source_audio currentPacket;
	struct obs_source_cea_708 currentCaptions;
	DecklinkBase *decklink = nullptr;
	DeckLinkDevice *device = nullptr;
	DeckLinkDeviceMode *mode = nullptr;
	BMDVideoConnection videoConnection;
	BMDAudioConnection audioConnection;
	BMDDisplayMode displayMode = bmdModeNTSC;
	BMDPixelFormat pixelFormat = bmdFormat8BitYUV;
	video_colorspace colorSpace = VIDEO_CS_DEFAULT;
	video_colorspace activeColorSpace = VIDEO_CS_DEFAULT;
	video_range_type colorRange = VIDEO_RANGE_DEFAULT;
	ComPtr<IDeckLinkInput> input;
	ComPtr<IDeckLinkOutput> output;
	volatile long refCount = 1;
	int64_t audioOffset = 0;
	uint64_t nextAudioTS = 0;
	uint64_t lastVideoTS = 0;
	AudioRepacker *audioRepacker = nullptr;
	speaker_layout channelFormat = SPEAKERS_STEREO;
	bool swap;
	bool allow10Bit;

	OBSVideoFrame *convertFrame = nullptr;
	IDeckLinkMutableVideoFrame *decklinkOutputFrame = nullptr;

	void FinalizeStream();
	void SetupVideoFormat(DeckLinkDeviceMode *mode_);

	void HandleAudioPacket(IDeckLinkAudioInputPacket *audioPacket,
			       const uint64_t timestamp);
	void HandleVideoFrame(IDeckLinkVideoInputFrame *videoFrame,
			      const uint64_t timestamp);

public:
	DeckLinkDeviceInstance(DecklinkBase *decklink, DeckLinkDevice *device);
	virtual ~DeckLinkDeviceInstance();

	inline DeckLinkDevice *GetDevice() const { return device; }
	inline long long GetActiveModeId() const
	{
		return mode ? mode->GetId() : 0;
	}

	inline BMDPixelFormat GetActivePixelFormat() const
	{
		return pixelFormat;
	}
	inline video_colorspace GetActiveColorSpace() const
	{
		return colorSpace;
	}
	inline video_range_type GetActiveColorRange() const
	{
		return colorRange;
	}
	inline speaker_layout GetActiveChannelFormat() const
	{
		return channelFormat;
	}
	inline bool GetActiveSwapState() const { return swap; }
	inline BMDVideoConnection GetVideoConnection() const
	{
		return videoConnection;
	}
	inline BMDAudioConnection GetAudioConnection() const
	{
		return audioConnection;
	}

	inline DeckLinkDeviceMode *GetMode() const { return mode; }

	bool StartCapture(DeckLinkDeviceMode *mode, bool allow10Bit,
			  BMDVideoConnection bmdVideoConnection,
			  BMDAudioConnection bmdAudioConnection);
	bool StopCapture(void);

	bool StartOutput(DeckLinkDeviceMode *mode_);
	bool StopOutput(void);

	HRESULT STDMETHODCALLTYPE
	VideoInputFrameArrived(IDeckLinkVideoInputFrame *videoFrame,
			       IDeckLinkAudioInputPacket *audioPacket);
	HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
		BMDVideoInputFormatChangedEvents events,
		IDeckLinkDisplayMode *newMode,
		BMDDetectedVideoInputFormatFlags detectedSignalFlags);

	ULONG STDMETHODCALLTYPE AddRef(void);
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
	ULONG STDMETHODCALLTYPE Release(void);

	void DisplayVideoFrame(video_data *frame);
	void WriteAudio(audio_data *frames);
	void HandleCaptionPacket(IDeckLinkAncillaryPacket *packet,
				 const uint64_t timestamp);
};
