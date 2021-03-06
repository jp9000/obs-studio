//
//  CMSampleBufferUtils.m
//  dal-plugin
//
//  Created by John Boiles  on 5/8/20.
//

#import "CMSampleBufferUtils.h"

#include "Logging.h"

/*!
CMSampleBufferCreateFromData

Creates a CMSampleBuffer by copying bytes from NSData into a CVPixelBuffer.
*/
OSStatus CMSampleBufferCreateFromData(NSSize size,
				      CMSampleTimingInfo timingInfo,
				      UInt64 sequenceNumber, NSData *data,
				      CMSampleBufferRef *sampleBuffer)
{
	OSStatus err = noErr;

	// Create an empty pixel buffer
	CVPixelBufferRef pixelBuffer;
	err = CVPixelBufferCreate(kCFAllocatorDefault, size.width, size.height,
				  kCVPixelFormatType_422YpCbCr8, nil,
				  &pixelBuffer);
	if (err != noErr) {
		DLog(@"CVPixelBufferCreate err %d", err);
		return err;
	}

	// Generate the video format description from that pixel buffer
	CMFormatDescriptionRef format;
	err = CMVideoFormatDescriptionCreateForImageBuffer(NULL, pixelBuffer,
							   &format);
	if (err != noErr) {
		DLog(@"CMVideoFormatDescriptionCreateForImageBuffer err %d",
		     err);
		return err;
	}

	// Copy memory into the pixel buffer
	CVPixelBufferLockBaseAddress(pixelBuffer, 0);
	uint8_t *dest =
		(uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
	uint8_t *src = (uint8_t *)data.bytes;

	size_t destBytesPerRow =
		CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
	size_t srcBytesPerRow = size.width * 2;

	// Sometimes CVPixelBufferCreate will create a pixelbuffer that's a different
	// size than necessary to hold the frame (probably for some optimization reason).
	// If that is the case this will do a row-by-row copy into the buffer.
	if (destBytesPerRow == srcBytesPerRow) {
		memcpy(dest, src, data.length);
	} else {
		for (int line = 0; line < size.height; line++) {
			memcpy(dest, src, srcBytesPerRow);
			src += srcBytesPerRow;
			dest += destBytesPerRow;
		}
	}

	CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

	err = CMIOSampleBufferCreateForImageBuffer(kCFAllocatorDefault,
						   pixelBuffer, format,
						   &timingInfo, sequenceNumber,
						   0, sampleBuffer);
	CFRelease(format);
	CFRelease(pixelBuffer);

	if (err != noErr) {
		DLog(@"CMIOSampleBufferCreateForImageBuffer err %d", err);
		return err;
	}

	return noErr;
}

static void releaseNSData(void *o, void *block, size_t size)
{
	UNUSED_PARAMETER(block);
	UNUSED_PARAMETER(size);

	NSData *data = (__bridge_transfer NSData *)o;
	data = nil; // Assuming ARC is enabled
}

// From https://stackoverflow.com/questions/26158253/how-to-create-a-cmblockbufferref-from-nsdata
OSStatus createReadonlyBlockBuffer(CMBlockBufferRef *result, NSData *data)
{
	CMBlockBufferCustomBlockSource blockSource = {
		.version = kCMBlockBufferCustomBlockSourceVersion,
		.AllocateBlock = NULL,
		.FreeBlock = &releaseNSData,
		.refCon = (__bridge_retained void *)data,
	};
	return CMBlockBufferCreateWithMemoryBlock(NULL, (void *)data.bytes,
						  data.length, NULL,
						  &blockSource, 0, data.length,
						  0, result);
}

/*!
 CMSampleBufferCreateFromDataNoCopy

 Creates a CMSampleBuffer by using the bytes directly from NSData (without copying them).
 Seems to mostly work but does not work at full resolution in OBS for some reason (which prevents loopback testing).
 */
OSStatus CMSampleBufferCreateFromDataNoCopy(NSSize size,
					    CMSampleTimingInfo timingInfo,
					    UInt64 sequenceNumber, NSData *data,
					    CMSampleBufferRef *sampleBuffer)
{
	OSStatus err = noErr;

	CMBlockBufferRef dataBuffer;
	createReadonlyBlockBuffer(&dataBuffer, data);

	// Magic format properties snagged from https://github.com/lvsti/CoreMediaIO-DAL-Example/blob/0392cbf27ed33425a1a5bd9f495b2ccec8f20501/Sources/Extras/CoreMediaIO/DeviceAbstractionLayer/Devices/Sample/PlugIn/CMIO_DP_Sample_Stream.cpp#L830
	NSDictionary *extensions = @{
		@"com.apple.cmio.format_extension.video.only_has_i_frames":
			@YES,
		(__bridge NSString *)
		kCMFormatDescriptionExtension_FieldCount: @1,
		(__bridge NSString *)
		kCMFormatDescriptionExtension_ColorPrimaries:
			(__bridge NSString *)
				kCMFormatDescriptionColorPrimaries_SMPTE_C,
		(__bridge NSString *)
		kCMFormatDescriptionExtension_TransferFunction: (
			__bridge NSString *)
			kCMFormatDescriptionTransferFunction_ITU_R_709_2,
		(__bridge NSString *)
		kCMFormatDescriptionExtension_YCbCrMatrix: (__bridge NSString *)
			kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4,
		(__bridge NSString *)
		kCMFormatDescriptionExtension_BytesPerRow: @(size.width * 2),
		(__bridge NSString *)kCMFormatDescriptionExtension_FormatName:
			@"Component Video - CCIR-601 uyvy",
		(__bridge NSString *)kCMFormatDescriptionExtension_Version: @2,
	};

	CMFormatDescriptionRef format;
	err = CMVideoFormatDescriptionCreate(
		NULL, kCMVideoCodecType_422YpCbCr8, size.width, size.height,
		(__bridge CFDictionaryRef)extensions, &format);
	if (err != noErr) {
		DLog(@"CMVideoFormatDescriptionCreate err %d", err);
		return err;
	}

	size_t dataSize = data.length;
	err = CMIOSampleBufferCreate(kCFAllocatorDefault, dataBuffer, format, 1,
				     1, &timingInfo, 1, &dataSize,
				     sequenceNumber, 0, sampleBuffer);
	CFRelease(format);
	CFRelease(dataBuffer);

	if (err != noErr) {
		DLog(@"CMIOSampleBufferCreate err %d", err);
		return err;
	}

	return noErr;
}

CMSampleTimingInfo CMSampleTimingInfoForTimestamp(uint64_t timestampNanos,
						  uint32_t fpsNumerator,
						  uint32_t fpsDenominator)
{
	// The timing here is quite important. For frames to be delivered correctly and successfully be recorded by apps
	// like QuickTime Player, we need to be accurate in both our timestamps _and_ have a sensible scale. Using large
	// timestamps and scales like mach_absolute_time() and NSEC_PER_SEC will work for display, but will error out
	// when trying to record.
	//
	// 600 is a commmon default in Apple's docs https://developer.apple.com/documentation/avfoundation/avmutablemovie/1390622-timescale
	CMTimeScale scale = 600;
	CMSampleTimingInfo timing;
	timing.duration =
		CMTimeMake(fpsDenominator * scale, fpsNumerator * scale);
	timing.presentationTimeStamp = CMTimeMake(
		(timestampNanos / (double)NSEC_PER_SEC) * scale, scale);
	timing.decodeTimeStamp = kCMTimeInvalid;
	return timing;
}
