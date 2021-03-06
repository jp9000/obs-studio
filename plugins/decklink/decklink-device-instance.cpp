#include "decklink-device-instance.hpp"
#include "audio-repack.hpp"

#include "DecklinkInput.hpp"
#include "DecklinkOutput.hpp"

#include <util/platform.h>
#include <util/threading.h>
#include <util/util_uint64.h>

#include <sstream>
#include <iomanip>
#include <algorithm>

#include "OBSVideoFrame.h"

#include <caption/caption.h>
#include <util/bitstream.h>

static inline enum video_format ConvertPixelFormat(BMDPixelFormat format)
{
	switch (format) {
	case bmdFormat8BitBGRA:
		return VIDEO_FORMAT_BGRX;

	default:
	case bmdFormat8BitYUV:
	case bmdFormat10BitYUV:;
		return VIDEO_FORMAT_UYVY;
	}
}

static inline int ConvertChannelFormat(speaker_layout format)
{
	switch (format) {
	case SPEAKERS_2POINT1:
	case SPEAKERS_4POINT0:
	case SPEAKERS_4POINT1:
	case SPEAKERS_5POINT1:
	case SPEAKERS_7POINT1:
		return 8;

	default:
	case SPEAKERS_STEREO:
		return 2;
	}
}

static inline audio_repack_mode_t ConvertRepackFormat(speaker_layout format,
						      bool swap)
{
	switch (format) {
	case SPEAKERS_2POINT1:
		return repack_mode_8to3ch;
	case SPEAKERS_4POINT0:
		return repack_mode_8to4ch;
	case SPEAKERS_4POINT1:
		return swap ? repack_mode_8to5ch_swap : repack_mode_8to5ch;
	case SPEAKERS_5POINT1:
		return swap ? repack_mode_8to6ch_swap : repack_mode_8to6ch;
	case SPEAKERS_7POINT1:
		return swap ? repack_mode_8ch_swap : repack_mode_8ch;
	default:
		assert(false && "No repack requested");
		return (audio_repack_mode_t)-1;
	}
}

DeckLinkDeviceInstance::DeckLinkDeviceInstance(DecklinkBase *decklink_,
					       DeckLinkDevice *device_)
	: currentFrame(),
	  currentPacket(),
	  currentCaptions(),
	  decklink(decklink_),
	  device(device_)
{
	currentPacket.samples_per_sec = 48000;
	currentPacket.speakers = SPEAKERS_STEREO;
	currentPacket.format = AUDIO_FORMAT_16BIT;
}

DeckLinkDeviceInstance::~DeckLinkDeviceInstance()
{
	if (convertFrame) {
		delete convertFrame;
	}
}

void DeckLinkDeviceInstance::HandleAudioPacket(
	IDeckLinkAudioInputPacket *audioPacket, const uint64_t timestamp)
{
	if (audioPacket == nullptr)
		return;

	void *bytes;
	if (audioPacket->GetBytes(&bytes) != S_OK) {
		LOG(LOG_WARNING, "Failed to get audio packet data");
		return;
	}

	const uint32_t frameCount =
		(uint32_t)audioPacket->GetSampleFrameCount();
	currentPacket.frames = frameCount;
	currentPacket.timestamp = timestamp;

	if (decklink && !static_cast<DeckLinkInput *>(decklink)->buffering) {
		currentPacket.timestamp = os_gettime_ns();
		currentPacket.timestamp -=
			util_mul_div64(frameCount, 1000000000ULL,
				       currentPacket.samples_per_sec);
	}

	int maxdevicechannel = device->GetMaxChannel();

	if (channelFormat != SPEAKERS_UNKNOWN &&
	    channelFormat != SPEAKERS_MONO &&
	    channelFormat != SPEAKERS_STEREO &&
	    (channelFormat != SPEAKERS_7POINT1 ||
	     static_cast<DeckLinkInput *>(decklink)->swap) &&
	    maxdevicechannel >= 8) {

		if (audioRepacker->repack((uint8_t *)bytes, frameCount) < 0) {
			LOG(LOG_ERROR, "Failed to convert audio packet data");
			return;
		}
		currentPacket.data[0] = (*audioRepacker)->packet_buffer;
	} else {
		currentPacket.data[0] = (uint8_t *)bytes;
	}

	nextAudioTS = timestamp +
		      util_mul_div64(frameCount, 1000000000ULL, 48000ULL) + 1;

	obs_source_output_audio(
		static_cast<DeckLinkInput *>(decklink)->GetSource(),
		&currentPacket);
}

void DeckLinkDeviceInstance::HandleVideoFrame(
	IDeckLinkVideoInputFrame *videoFrame, const uint64_t timestamp)
{
	if (videoFrame == nullptr)
		return;

	IDeckLinkVideoFrameAncillaryPackets *packets;

	if (videoFrame->QueryInterface(IID_IDeckLinkVideoFrameAncillaryPackets,
				       (void **)&packets) == S_OK) {
		IDeckLinkAncillaryPacketIterator *iterator;
		packets->GetPacketIterator(&iterator);

		IDeckLinkAncillaryPacket *packet;
		iterator->Next(&packet);

		if (packet) {
			auto did = packet->GetDID();
			auto sdid = packet->GetSDID();

			// Caption data
			if (did == 0x61 && sdid == 0x01) {
				this->HandleCaptionPacket(packet, timestamp);
			}

			packet->Release();
		}

		iterator->Release();
		packets->Release();
	}

	IDeckLinkVideoFrame *frame;
	if (videoFrame->GetPixelFormat() != convertFrame->GetPixelFormat()) {
		IDeckLinkVideoConversion *frameConverter =
			CreateVideoConversionInstance();

		frameConverter->ConvertFrame(videoFrame, convertFrame);

		frame = convertFrame;
	} else {
		frame = videoFrame;
	}

	void *bytes;
	if (frame->GetBytes(&bytes) != S_OK) {
		LOG(LOG_WARNING, "Failed to get video frame data");
		return;
	}

	currentFrame.data[0] = (uint8_t *)bytes;
	currentFrame.linesize[0] = (uint32_t)frame->GetRowBytes();
	currentFrame.width = (uint32_t)frame->GetWidth();
	currentFrame.height = (uint32_t)frame->GetHeight();
	currentFrame.timestamp = timestamp;

	obs_source_output_video2(
		static_cast<DeckLinkInput *>(decklink)->GetSource(),
		&currentFrame);
}

void DeckLinkDeviceInstance::HandleCaptionPacket(
	IDeckLinkAncillaryPacket *packet, const uint64_t timestamp)
{
	const void *data;
	uint32_t size;
	packet->GetBytes(bmdAncillaryPacketFormatUInt8, &data, &size);

	auto anc = (uint8_t *)data;
	struct bitstream_reader reader;
	bitstream_reader_init(&reader, anc, size);

	// header1
	bitstream_reader_r8(&reader);
	// header2
	bitstream_reader_r8(&reader);

	// length
	bitstream_reader_r8(&reader);
	// frameRate
	bitstream_reader_read_bits(&reader, 4);
	//reserved
	bitstream_reader_read_bits(&reader, 4);

	auto cdp_timecode_added = bitstream_reader_read_bits(&reader, 1);
	// cdp_data_block_added
	bitstream_reader_read_bits(&reader, 1);
	// cdp_service_info_added
	bitstream_reader_read_bits(&reader, 1);
	// cdp_service_info_start
	bitstream_reader_read_bits(&reader, 1);
	// cdp_service_info_changed
	bitstream_reader_read_bits(&reader, 1);
	// cdp_service_info_end
	bitstream_reader_read_bits(&reader, 1);
	auto cdp_contains_captions = bitstream_reader_read_bits(&reader, 1);
	//reserved
	bitstream_reader_read_bits(&reader, 1);

	// cdp_counter
	bitstream_reader_r8(&reader);
	// cdp_counter2
	bitstream_reader_r8(&reader);

	if (cdp_timecode_added) {
		// timecodeSectionID
		bitstream_reader_r8(&reader);
		//reserved
		bitstream_reader_read_bits(&reader, 2);
		bitstream_reader_read_bits(&reader, 2);
		bitstream_reader_read_bits(&reader, 4);
		// reserved
		bitstream_reader_read_bits(&reader, 1);
		bitstream_reader_read_bits(&reader, 3);
		bitstream_reader_read_bits(&reader, 4);
		bitstream_reader_read_bits(&reader, 1);
		bitstream_reader_read_bits(&reader, 3);
		bitstream_reader_read_bits(&reader, 4);
		bitstream_reader_read_bits(&reader, 1);
		bitstream_reader_read_bits(&reader, 1);
		bitstream_reader_read_bits(&reader, 3);
		bitstream_reader_read_bits(&reader, 4);
	}

	if (cdp_contains_captions) {
		// cdp_data_section
		bitstream_reader_r8(&reader);

		//process_em_data_flag
		bitstream_reader_read_bits(&reader, 1);
		// process_cc_data_flag
		bitstream_reader_read_bits(&reader, 1);
		//additional_data_flag
		bitstream_reader_read_bits(&reader, 1);

		auto cc_count = bitstream_reader_read_bits(&reader, 5);

		auto *outData =
			(uint8_t *)bzalloc(sizeof(uint8_t) * cc_count * 3);
		memcpy(outData, anc + reader.pos, cc_count * 3);

		currentCaptions.data = outData;
		currentCaptions.timestamp = timestamp;
		currentCaptions.packets = cc_count;

		obs_source_output_cea708(
			static_cast<DeckLinkInput *>(decklink)->GetSource(),
			&currentCaptions);
		bfree(outData);
	}
}

void DeckLinkDeviceInstance::FinalizeStream()
{
	input->SetCallback(nullptr);
	input->DisableVideoInput();
	if (channelFormat != SPEAKERS_UNKNOWN)
		input->DisableAudioInput();

	if (audioRepacker != nullptr) {
		delete audioRepacker;
		audioRepacker = nullptr;
	}

	mode = nullptr;
}

//#define LOG_SETUP_VIDEO_FORMAT 1

void DeckLinkDeviceInstance::SetupVideoFormat(DeckLinkDeviceMode *mode_)
{
	if (mode_ == nullptr)
		return;

	currentFrame.format = ConvertPixelFormat(pixelFormat);

	colorSpace = static_cast<DeckLinkInput *>(decklink)->GetColorSpace();
	if (colorSpace == VIDEO_CS_DEFAULT) {
		const BMDDisplayModeFlags flags = mode_->GetDisplayModeFlags();
		if (flags & bmdDisplayModeColorspaceRec709)
			activeColorSpace = VIDEO_CS_709;
		else if (flags & bmdDisplayModeColorspaceRec601)
			activeColorSpace = VIDEO_CS_601;
		else
			activeColorSpace = VIDEO_CS_DEFAULT;
	} else {
		activeColorSpace = colorSpace;
	}

	colorRange = static_cast<DeckLinkInput *>(decklink)->GetColorRange();
	currentFrame.range = colorRange;

	video_format_get_parameters(activeColorSpace, colorRange,
				    currentFrame.color_matrix,
				    currentFrame.color_range_min,
				    currentFrame.color_range_max);

	delete convertFrame;

	BMDPixelFormat convertFormat;
	switch (pixelFormat) {
	case bmdFormat8BitBGRA:
		convertFormat = bmdFormat8BitBGRA;
		break;
	default:
	case bmdFormat10BitYUV:
	case bmdFormat8BitYUV:;
		convertFormat = bmdFormat8BitYUV;
		break;
	}

	convertFrame = new OBSVideoFrame(mode_->GetWidth(), mode_->GetHeight(),
					 convertFormat);

#ifdef LOG_SETUP_VIDEO_FORMAT
	LOG(LOG_INFO, "Setup video format: %s, %s, %s",
	    pixelFormat == bmdFormat8BitYUV ? "YUV" : "RGB",
	    activeColorSpace == VIDEO_CS_601 ? "BT.601" : "BT.709",
	    colorRange == VIDEO_RANGE_FULL ? "full" : "limited");
#endif
}

bool DeckLinkDeviceInstance::StartCapture(DeckLinkDeviceMode *mode_,
					  bool allow10Bit_,
					  BMDVideoConnection bmdVideoConnection,
					  BMDAudioConnection bmdAudioConnection)
{
	if (mode != nullptr)
		return false;
	if (mode_ == nullptr)
		return false;

	LOG(LOG_INFO, "Starting capture...");

	if (!device->GetInput(&input))
		return false;

	IDeckLinkConfiguration *deckLinkConfiguration = NULL;
	HRESULT result = input->QueryInterface(IID_IDeckLinkConfiguration,
					       (void **)&deckLinkConfiguration);
	if (result != S_OK) {
		LOG(LOG_ERROR,
		    "Could not obtain the IDeckLinkConfiguration interface: %08x\n",
		    result);
	} else {
		if (bmdVideoConnection > 0) {
			result = deckLinkConfiguration->SetInt(
				bmdDeckLinkConfigVideoInputConnection,
				bmdVideoConnection);
			if (result != S_OK) {
				LOG(LOG_ERROR,
				    "Couldn't set input video port to %d\n\n",
				    bmdVideoConnection);
			}
		}

		if (bmdAudioConnection > 0) {
			result = deckLinkConfiguration->SetInt(
				bmdDeckLinkConfigAudioInputConnection,
				bmdAudioConnection);
			if (result != S_OK) {
				LOG(LOG_ERROR,
				    "Couldn't set input audio port to %d\n\n",
				    bmdVideoConnection);
			}
		}
	}

	videoConnection = bmdVideoConnection;
	audioConnection = bmdAudioConnection;

	BMDVideoInputFlags flags;

	bool isauto = mode_->GetName() == "Auto";
	if (isauto) {
		displayMode = bmdModeNTSC;
		if (allow10Bit) {
			pixelFormat = bmdFormat10BitYUV;
		} else {
			pixelFormat = bmdFormat8BitYUV;
		}
		flags = bmdVideoInputEnableFormatDetection;
	} else {
		displayMode = mode_->GetDisplayMode();
		pixelFormat =
			static_cast<DeckLinkInput *>(decklink)->GetPixelFormat();
		flags = bmdVideoInputFlagDefault;
	}

	allow10Bit = allow10Bit_;

	const HRESULT videoResult =
		input->EnableVideoInput(displayMode, pixelFormat, flags);
	if (videoResult != S_OK) {
		LOG(LOG_ERROR, "Failed to enable video input");
		return false;
	}

	SetupVideoFormat(mode_);

	channelFormat =
		static_cast<DeckLinkInput *>(decklink)->GetChannelFormat();
	currentPacket.speakers = channelFormat;
	swap = static_cast<DeckLinkInput *>(decklink)->swap;

	int maxdevicechannel = device->GetMaxChannel();

	if (channelFormat != SPEAKERS_UNKNOWN) {
		const int channel = ConvertChannelFormat(channelFormat);
		const HRESULT audioResult = input->EnableAudioInput(
			bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger,
			channel);
		if (audioResult != S_OK)
			LOG(LOG_WARNING,
			    "Failed to enable audio input; continuing...");

		if (channelFormat != SPEAKERS_UNKNOWN &&
		    channelFormat != SPEAKERS_MONO &&
		    channelFormat != SPEAKERS_STEREO &&
		    (channelFormat != SPEAKERS_7POINT1 || swap) &&
		    maxdevicechannel >= 8) {

			const audio_repack_mode_t repack_mode =
				ConvertRepackFormat(channelFormat, swap);
			audioRepacker = new AudioRepacker(repack_mode);
		}
	}

	if (input->SetCallback(this) != S_OK) {
		LOG(LOG_ERROR, "Failed to set callback");
		FinalizeStream();
		return false;
	}

	if (input->StartStreams() != S_OK) {
		LOG(LOG_ERROR, "Failed to start streams");
		FinalizeStream();
		return false;
	}

	mode = mode_;

	return true;
}

bool DeckLinkDeviceInstance::StopCapture(void)
{
	if (mode == nullptr || input == nullptr)
		return false;

	LOG(LOG_INFO, "Stopping capture of '%s'...",
	    GetDevice()->GetDisplayName().c_str());

	input->StopStreams();
	FinalizeStream();

	return true;
}

bool DeckLinkDeviceInstance::StartOutput(DeckLinkDeviceMode *mode_)
{
	if (mode != nullptr)
		return false;
	if (mode_ == nullptr)
		return false;

	LOG(LOG_INFO, "Starting output...");

	if (!device->GetOutput(&output))
		return false;

	const HRESULT videoResult = output->EnableVideoOutput(
		mode_->GetDisplayMode(), bmdVideoOutputFlagDefault);
	if (videoResult != S_OK) {
		LOG(LOG_ERROR, "Failed to enable video output");
		return false;
	}

	const HRESULT audioResult = output->EnableAudioOutput(
		bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2,
		bmdAudioOutputStreamTimestamped);
	if (audioResult != S_OK) {
		LOG(LOG_ERROR, "Failed to enable audio output");
		return false;
	}

	mode = mode_;

	int keyerMode = device->GetKeyerMode();

	IDeckLinkKeyer *deckLinkKeyer = nullptr;
	if (device->GetKeyer(&deckLinkKeyer)) {
		if (keyerMode) {
			deckLinkKeyer->Enable(keyerMode == 1);
			deckLinkKeyer->SetLevel(255);
		} else {
			deckLinkKeyer->Disable();
		}
	}

	auto decklinkOutput = dynamic_cast<DeckLinkOutput *>(decklink);
	if (decklinkOutput == nullptr)
		return false;

	int rowBytes = decklinkOutput->GetWidth() * 2;
	if (decklinkOutput->keyerMode != 0) {
		rowBytes = decklinkOutput->GetWidth() * 4;
	}

	BMDPixelFormat pixelFormat = bmdFormat8BitYUV;
	if (keyerMode != 0) {
		pixelFormat = bmdFormat8BitBGRA;
	}

	HRESULT result;
	result = output->CreateVideoFrame(decklinkOutput->GetWidth(),
					  decklinkOutput->GetHeight(), rowBytes,
					  pixelFormat, bmdFrameFlagDefault,
					  &decklinkOutputFrame);
	if (result != S_OK) {
		blog(LOG_ERROR, "failed to make frame 0x%X", result);
		return false;
	}

	return true;
}

bool DeckLinkDeviceInstance::StopOutput()
{
	if (mode == nullptr || output == nullptr)
		return false;

	LOG(LOG_INFO, "Stopping output of '%s'...",
	    GetDevice()->GetDisplayName().c_str());

	output->DisableVideoOutput();
	output->DisableAudioOutput();

	if (decklinkOutputFrame != nullptr) {
		decklinkOutputFrame->Release();
		decklinkOutputFrame = nullptr;
	}

	return true;
}

void DeckLinkDeviceInstance::DisplayVideoFrame(video_data *frame)
{
	auto decklinkOutput = dynamic_cast<DeckLinkOutput *>(decklink);
	if (decklinkOutput == nullptr)
		return;

	uint8_t *destData;
	decklinkOutputFrame->GetBytes((void **)&destData);

	uint8_t *outData = frame->data[0];

	int rowBytes = decklinkOutput->GetWidth() * 2;
	if (device->GetKeyerMode()) {
		rowBytes = decklinkOutput->GetWidth() * 4;
	}

	std::copy(outData, outData + (decklinkOutput->GetHeight() * rowBytes),
		  destData);

	output->DisplayVideoFrameSync(decklinkOutputFrame);
}

void DeckLinkDeviceInstance::WriteAudio(audio_data *frames)
{
	uint32_t sampleFramesWritten;
	output->WriteAudioSamplesSync(frames->data[0], frames->frames,
				      &sampleFramesWritten);
}

#define TIME_BASE 1000000000

HRESULT STDMETHODCALLTYPE DeckLinkDeviceInstance::VideoInputFrameArrived(
	IDeckLinkVideoInputFrame *videoFrame,
	IDeckLinkAudioInputPacket *audioPacket)
{
	BMDTimeValue videoTS = 0;
	BMDTimeValue videoDur = 0;
	BMDTimeValue audioTS = 0;

	if (videoFrame) {
		videoFrame->GetStreamTime(&videoTS, &videoDur, TIME_BASE);
		lastVideoTS = (uint64_t)videoTS;
	}
	if (audioPacket) {
		BMDTimeValue newAudioTS = 0;
		int64_t diff;

		audioPacket->GetPacketTime(&newAudioTS, TIME_BASE);
		audioTS = newAudioTS + audioOffset;

		diff = (int64_t)audioTS - (int64_t)nextAudioTS;
		if (diff > 10000000LL) {
			audioOffset -= diff;
			audioTS = newAudioTS + audioOffset;

		} else if (diff < -1000000) {
			audioOffset = 0;
			audioTS = newAudioTS;
		}
	}

	if (videoFrame && videoTS >= 0)
		HandleVideoFrame(videoFrame, (uint64_t)videoTS);
	if (audioPacket && audioTS >= 0)
		HandleAudioPacket(audioPacket, (uint64_t)audioTS);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DeckLinkDeviceInstance::VideoInputFormatChanged(
	BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *newMode,
	BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{

	if (events & bmdVideoInputColorspaceChanged) {
		if (detectedSignalFlags & bmdDetectedVideoInputRGB444) {
			pixelFormat = bmdFormat8BitBGRA;
		}
		if (detectedSignalFlags & bmdDetectedVideoInputYCbCr422) {
			if (detectedSignalFlags &
			    bmdDetectedVideoInput10BitDepth) {
				if (allow10Bit) {
					pixelFormat = bmdFormat10BitYUV;
				} else {
					pixelFormat = bmdFormat8BitYUV;
				}
			}
			if (detectedSignalFlags &
			    bmdDetectedVideoInput8BitDepth) {
				pixelFormat = bmdFormat8BitYUV;
			}
		}
	}

	if (events & bmdVideoInputDisplayModeChanged) {
		input->PauseStreams();
		mode->SetMode(newMode);
		displayMode = mode->GetDisplayMode();

		const HRESULT videoResult = input->EnableVideoInput(
			displayMode, pixelFormat,
			bmdVideoInputEnableFormatDetection);
		if (videoResult != S_OK) {
			LOG(LOG_ERROR, "Failed to enable video input");
			input->StopStreams();
			FinalizeStream();

			return E_FAIL;
		}
		SetupVideoFormat(mode);
		input->FlushStreams();
		input->StartStreams();
	}

	return S_OK;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceInstance::AddRef(void)
{
	return os_atomic_inc_long(&refCount);
}

HRESULT STDMETHODCALLTYPE DeckLinkDeviceInstance::QueryInterface(REFIID iid,
								 LPVOID *ppv)
{
	HRESULT result = E_NOINTERFACE;

	*ppv = nullptr;

	CFUUIDBytes unknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &unknown, sizeof(REFIID)) == 0) {
		*ppv = this;
		AddRef();
		result = S_OK;
	} else if (memcmp(&iid, &IID_IDeckLinkNotificationCallback,
			  sizeof(REFIID)) == 0) {
		*ppv = (IDeckLinkNotificationCallback *)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceInstance::Release(void)
{
	const long newRefCount = os_atomic_dec_long(&refCount);
	if (newRefCount == 0) {
		delete this;
		return 0;
	}

	return newRefCount;
}
