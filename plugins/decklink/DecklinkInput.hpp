#pragma once

#include "DecklinkBase.h"

class DeckLinkInput : public DecklinkBase {
protected:
	bool isCapturing = false;
	obs_source_t *source;

	void SaveSettings();
	static void DevicesChanged(void *param, DeckLinkDevice *device,
				   bool added);

public:
	DeckLinkInput(obs_source_t *source, DeckLinkDeviceDiscovery *discovery);
	virtual ~DeckLinkInput(void);

	long long GetActiveModeId(void) const;
	obs_source_t *GetSource(void) const;

	inline BMDPixelFormat GetPixelFormat() const { return pixelFormat; }
	inline void SetPixelFormat(BMDPixelFormat format)
	{
		pixelFormat = format;
	}
	inline video_colorspace GetColorSpace() const { return colorSpace; }
	inline void SetColorSpace(video_colorspace format)
	{
		colorSpace = format;
	}
	inline video_range_type GetColorRange() const { return colorRange; }
	inline void SetColorRange(video_range_type format)
	{
		colorRange = format;
	}
	inline speaker_layout GetChannelFormat() const { return channelFormat; }
	inline void SetChannelFormat(speaker_layout format)
	{
		channelFormat = format;
	}

	bool Activate(DeckLinkDevice *device, long long modeId,
		      BMDVideoConnection bmdVideoConnection,
		      BMDAudioConnection bmdAudioConnection) override;
	void Deactivate() override;
	bool Capturing();

	bool buffering = false;
	bool dwns = false;
	std::string hash;
	long long id;
	bool swap = false;
	bool allow10Bit = false;
	BMDVideoConnection videoConnection;
	BMDAudioConnection audioConnection;
};
