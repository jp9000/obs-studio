#pragma once

#include "DecklinkBase.h"

#include "../../libobs/media-io/video-scaler.h"

class DeckLinkOutput : public DecklinkBase {
protected:
	obs_output_t *output;
	int width;
	int height;

	static void DevicesChanged(void *param, DeckLinkDevice *device,
				   bool added);

public:
	const char *deviceHash;
	long long modeID;
	uint64_t start_timestamp;
	uint32_t audio_samplerate;
	size_t audio_planes;
	size_t audio_size;
	int keyerMode;

	DeckLinkOutput(obs_output_t *output,
		       DeckLinkDeviceDiscovery *discovery);
	virtual ~DeckLinkOutput(void);
	obs_output_t *GetOutput(void) const;
	bool Activate(DeckLinkDevice *device, long long modeId) override;
	void Deactivate() override;
	void DisplayVideoFrame(video_data *pData);
	void WriteAudio(audio_data *frames);
	void SetSize(int width, int height);
	int GetWidth();
	int GetHeight();
};
