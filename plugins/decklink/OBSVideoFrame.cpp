#include "OBSVideoFrame.h"

OBSVideoFrame::OBSVideoFrame(long width, long height,
			     BMDPixelFormat pixelFormat)
{
	int bpp = 2;
	this->width = width;
	this->height = height;
	this->rowBytes = width * bpp;
	this->data = new unsigned char[width * height * bpp + 1];
	this->pixelFormat = pixelFormat;
}

HRESULT OBSVideoFrame::SetFlags(BMDFrameFlags newFlags)
{
	flags = newFlags;
	return S_OK;
}

HRESULT OBSVideoFrame::SetTimecode(BMDTimecodeFormat format,
				   IDeckLinkTimecode *timecode)
{
	UNUSED_PARAMETER(format);
	UNUSED_PARAMETER(timecode);
	return 0;
}

HRESULT
OBSVideoFrame::SetTimecodeFromComponents(BMDTimecodeFormat format,
					 uint8_t hours, uint8_t minutes,
					 uint8_t seconds, uint8_t frames,
					 BMDTimecodeFlags flags)
{
	UNUSED_PARAMETER(format);
	UNUSED_PARAMETER(hours);
	UNUSED_PARAMETER(minutes);
	UNUSED_PARAMETER(seconds);
	UNUSED_PARAMETER(frames);
	UNUSED_PARAMETER(flags);
	return 0;
}

HRESULT OBSVideoFrame::SetAncillaryData(IDeckLinkVideoFrameAncillary *ancillary)
{
	UNUSED_PARAMETER(ancillary);
	return 0;
}

HRESULT OBSVideoFrame::SetTimecodeUserBits(BMDTimecodeFormat format,
					   BMDTimecodeUserBits userBits)
{
	UNUSED_PARAMETER(format);
	UNUSED_PARAMETER(userBits);
	return 0;
}

long OBSVideoFrame::GetWidth()
{
	return width;
}

long OBSVideoFrame::GetHeight()
{
	return height;
}

long OBSVideoFrame::GetRowBytes()
{
	return rowBytes;
}

BMDPixelFormat OBSVideoFrame::GetPixelFormat()
{
	return pixelFormat;
}

BMDFrameFlags OBSVideoFrame::GetFlags()
{
	return flags;
}

HRESULT OBSVideoFrame::GetBytes(void **buffer)
{
	*buffer = this->data;
	return S_OK;
}
