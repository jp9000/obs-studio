//
//  Stream.mm
//  obs-mac-virtualcam
//
//  Created by John Boiles  on 4/10/20.
//
//  obs-mac-virtualcam is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 2 of the License, or
//  (at your option) any later version.
//
//  obs-mac-virtualcam is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with obs-mac-virtualcam. If not, see <http://www.gnu.org/licenses/>.

#import "OBSDALStream.h"

#import <AppKit/AppKit.h>
#import <mach/mach_time.h>
#include <CoreMediaIO/CMIOSampleBuffer.h>

#import "Logging.h"
#import "CMSampleBufferUtils.h"
#import "OBSDALPlugIn.h"

@interface OBSDALStream () {
	CMSimpleQueueRef _queue;
	CFTypeRef _clock;
	NSImage *_testCardImage;
	dispatch_source_t _frameDispatchSource;
	NSSize _testCardSize;
	Float64 _fps;
}

@property CMIODeviceStreamQueueAlteredProc alteredProc;
@property void *alteredRefCon;
@property (readonly) CMSimpleQueueRef queue;
@property (readonly) CFTypeRef clock;
@property UInt64 sequenceNumber;
@property (readonly) NSImage *testCardImage;
@property (readonly) NSSize testCardSize;
@property (readonly) Float64 fps;

@end

@implementation OBSDALStream

#define DEFAULT_FPS 30.0
#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

- (instancetype _Nonnull)init
{
	self = [super init];
	if (self) {
		_frameDispatchSource = dispatch_source_create(
			DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
			dispatch_get_global_queue(
				DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
		__weak __typeof(self) wself = self;
		dispatch_source_set_event_handler(_frameDispatchSource, ^{
			[wself fillFrame];
		});
	}
	return self;
}

- (void)dealloc
{
	DLog(@"Stream Dealloc");
	CMIOStreamClockInvalidate(_clock);
	CFRelease(_clock);
	_clock = NULL;
	CFRelease(_queue);
	_queue = NULL;
	dispatch_suspend(_frameDispatchSource);
}

- (void)startServingDefaultFrames
{
	DLogFunc(@"");
	_testCardImage = nil;
	_testCardSize = NSZeroSize;
	_fps = 0;
	dispatch_time_t startTime = dispatch_time(DISPATCH_TIME_NOW, 0);
	uint64_t intervalTime = (int64_t)(NSEC_PER_SEC / self.fps);
	dispatch_source_set_timer(_frameDispatchSource, startTime, intervalTime,
				  0);
	dispatch_resume(_frameDispatchSource);
}

- (void)stopServingDefaultFrames
{
	DLogFunc(@"");
	dispatch_suspend(_frameDispatchSource);
}

- (CMSimpleQueueRef)queue
{
	if (_queue == NULL) {
		// Allocate a one-second long queue, which we can use our FPS constant for.
		OSStatus err = CMSimpleQueueCreate(kCFAllocatorDefault,
						   self.fps, &_queue);
		if (err != noErr) {
			DLog(@"Err %d in CMSimpleQueueCreate", err);
		}
	}
	return _queue;
}

- (CFTypeRef)clock
{
	if (_clock == NULL) {
		OSStatus err = CMIOStreamClockCreate(
			kCFAllocatorDefault,
			CFSTR("obs-mac-virtualcam::Stream::clock"),
			(__bridge void *)self, CMTimeMake(1, 10), 100, 10,
			&_clock);
		if (err != noErr) {
			DLog(@"Error %d from CMIOStreamClockCreate", err);
		}
	}
	return _clock;
}

- (NSSize)testCardSize
{
	if (NSEqualSizes(_testCardSize, NSZeroSize)) {
		NSUserDefaults *defaults =
			[NSUserDefaults standardUserDefaults];
		int width = [[defaults objectForKey:kTestCardWidthKey]
			integerValue];
		int height = [[defaults objectForKey:kTestCardHeightKey]
			integerValue];
		if (width == 0 || height == 0) {
			_testCardSize =
				NSMakeSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
		} else {
			_testCardSize = NSMakeSize(width, height);
		}
	}
	return _testCardSize;
}

- (Float64)fps
{
	if (_fps == 0) {
		NSUserDefaults *defaults =
			[NSUserDefaults standardUserDefaults];
		double fps =
			[[defaults objectForKey:kTestCardFPSKey] doubleValue];
		if (fps == 0) {
			_fps = DEFAULT_FPS;
		} else {
			_fps = fps;
		}
	}
	return _fps;
}

- (NSImage *)testCardImage
{
	if (_testCardImage == nil) {
		NSString *bundlePath = [[NSBundle
			bundleForClass:[OBSDALStream class]] bundlePath];
		NSString *placeHolderPath = [bundlePath
			stringByAppendingString:
				@"/Contents/Resources/placeholder.png"];
		NSFileManager *fileManager = [NSFileManager defaultManager];
		NSURL *homeUrl = [fileManager homeDirectoryForCurrentUser];
		NSURL *customUrl = [homeUrl
			URLByAppendingPathComponent:
				@"Library/Application Support/obs-studio/plugin_config/mac-virtualcam/placeholder.png"];
		NSString *customPlaceHolder = customUrl.path;
		if ([fileManager isReadableFileAtPath:customPlaceHolder])
			placeHolderPath = customPlaceHolder;
		DLog(@"PlaceHolder:%@", placeHolderPath);
		NSImage *placeholderImage = [[NSImage alloc]
			initWithContentsOfFile:placeHolderPath];

		NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
			initWithBitmapDataPlanes:NULL
				      pixelsWide:self.testCardSize.width
				      pixelsHigh:self.testCardSize.height
				   bitsPerSample:8
				 samplesPerPixel:4
					hasAlpha:YES
					isPlanar:NO
				  colorSpaceName:NSCalibratedRGBColorSpace
				     bytesPerRow:0
				    bitsPerPixel:0];
		rep.size = self.testCardSize;

		float hScale =
			placeholderImage.size.width / self.testCardSize.width;
		float vScale =
			placeholderImage.size.height / self.testCardSize.height;

		float scaling = fmax(hScale, vScale);

		float newWidth = placeholderImage.size.width / scaling;
		float newHeight = placeholderImage.size.height / scaling;

		float leftOffset = (self.testCardSize.width - newWidth) / 2;
		float topOffset = (self.testCardSize.height - newHeight) / 2;

		[NSGraphicsContext saveGraphicsState];
		[NSGraphicsContext
			setCurrentContext:
				[NSGraphicsContext
					graphicsContextWithBitmapImageRep:rep]];

		NSColor *backgroundColor = [NSColor blackColor];
		[backgroundColor set];
		NSRectFill(NSMakeRect(0, 0, self.testCardSize.width,
				      self.testCardSize.height));

		[placeholderImage drawInRect:NSMakeRect(leftOffset, topOffset,
							newWidth, newHeight)
				    fromRect:NSZeroRect
				   operation:NSCompositingOperationCopy
				    fraction:1.0];
		[NSGraphicsContext restoreGraphicsState];

		NSImage *testCardImage =
			[[NSImage alloc] initWithSize:self.testCardSize];
		[testCardImage addRepresentation:rep];

		_testCardImage = testCardImage;
	}
	return _testCardImage;
}

- (CMSimpleQueueRef)copyBufferQueueWithAlteredProc:
			    (CMIODeviceStreamQueueAlteredProc)alteredProc
				     alteredRefCon:(void *)alteredRefCon
{
	self.alteredProc = alteredProc;
	self.alteredRefCon = alteredRefCon;

	// Retain this since it's a copy operation
	CFRetain(self.queue);

	return self.queue;
}

- (CVPixelBufferRef)createPixelBufferWithTestAnimation
{
	int width = self.testCardSize.width;
	int height = self.testCardSize.height;

	NSDictionary *options = [NSDictionary
		dictionaryWithObjectsAndKeys:
			[NSNumber numberWithBool:YES],
			kCVPixelBufferCGImageCompatibilityKey,
			[NSNumber numberWithBool:YES],
			kCVPixelBufferCGBitmapContextCompatibilityKey, nil];
	CVPixelBufferRef pxbuffer = NULL;
	CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, width,
					      height, kCVPixelFormatType_32ARGB,
					      (__bridge CFDictionaryRef)options,
					      &pxbuffer);

	NSParameterAssert(status == kCVReturnSuccess && pxbuffer != NULL);

	CVPixelBufferLockBaseAddress(pxbuffer, 0);
	void *pxdata = CVPixelBufferGetBaseAddressOfPlane(pxbuffer, 0);
	NSParameterAssert(pxdata != NULL);

	CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
	CGContextRef context = CGBitmapContextCreate(
		pxdata, width, height, 8,
		CVPixelBufferGetBytesPerRowOfPlane(pxbuffer, 0), rgbColorSpace,
		kCGImageAlphaPremultipliedFirst | kCGImageByteOrder32Big);
	NSParameterAssert(context);

	NSGraphicsContext *nsContext = [NSGraphicsContext
		graphicsContextWithCGContext:context
				     flipped:NO];
	[NSGraphicsContext setCurrentContext:nsContext];

	NSRect rect = NSMakeRect(0, 0, self.testCardImage.size.width,
				 self.testCardImage.size.height);
	CGImageRef image = [self.testCardImage CGImageForProposedRect:&rect
							      context:nsContext
								hints:nil];
	CGContextDrawImage(context,
			   CGRectMake(0, 0, CGImageGetWidth(image),
				      CGImageGetHeight(image)),
			   image);

	//	DrawDialWithFrame(
	//		NSMakeRect(0, 0, width, height),
	//		(int(self.fps) - self.sequenceNumber % int(self.fps)) * 360 /
	//			int(self.fps));

	CGContextRelease(context);

	CVPixelBufferUnlockBaseAddress(pxbuffer, 0);

	return pxbuffer;
}

- (void)fillFrame
{
	if (CMSimpleQueueGetFullness(self.queue) >= 1.0) {
		return;
	}

	CVPixelBufferRef pixelBuffer =
		[self createPixelBufferWithTestAnimation];

	uint64_t hostTime = mach_absolute_time();
	CMSampleTimingInfo timingInfo =
		CMSampleTimingInfoForTimestamp(hostTime, self.fps, 1);

	OSStatus err = CMIOStreamClockPostTimingEvent(
		timingInfo.presentationTimeStamp, hostTime, true, self.clock);
	if (err != noErr) {
		DLog(@"CMIOStreamClockPostTimingEvent err %d", err);
	}

	CMFormatDescriptionRef format;
	CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault,
						     pixelBuffer, &format);

	self.sequenceNumber = CMIOGetNextSequenceNumber(self.sequenceNumber);

	CMSampleBufferRef buffer;
	err = CMIOSampleBufferCreateForImageBuffer(
		kCFAllocatorDefault, pixelBuffer, format, &timingInfo,
		self.sequenceNumber, kCMIOSampleBufferNoDiscontinuities,
		&buffer);
	CFRelease(pixelBuffer);
	CFRelease(format);
	if (err != noErr) {
		DLog(@"CMIOSampleBufferCreateForImageBuffer err %d", err);
	}

	CMSimpleQueueEnqueue(self.queue, buffer);

	// Inform the clients that the queue has been altered
	if (self.alteredProc != NULL) {
		(self.alteredProc)(self.objectId, buffer, self.alteredRefCon);
	}
}

- (void)queueFrameWithSize:(NSSize)size
		 timestamp:(uint64_t)timestamp
	      fpsNumerator:(uint32_t)fpsNumerator
	    fpsDenominator:(uint32_t)fpsDenominator
		 frameData:(NSData *)frameData
{
	if (CMSimpleQueueGetFullness(self.queue) >= 1.0) {
		DLog(@"Queue is full, bailing out");
		return;
	}
	OSStatus err = noErr;

	CMSampleTimingInfo timingInfo = CMSampleTimingInfoForTimestamp(
		timestamp, fpsNumerator, fpsDenominator);

	err = CMIOStreamClockPostTimingEvent(timingInfo.presentationTimeStamp,
					     mach_absolute_time(), true,
					     self.clock);
	if (err != noErr) {
		DLog(@"CMIOStreamClockPostTimingEvent err %d", err);
	}

	self.sequenceNumber = CMIOGetNextSequenceNumber(self.sequenceNumber);

	CMSampleBufferRef sampleBuffer;
	CMSampleBufferCreateFromData(size, timingInfo, self.sequenceNumber,
				     frameData, &sampleBuffer);
	CMSimpleQueueEnqueue(self.queue, sampleBuffer);

	// Inform the clients that the queue has been altered
	if (self.alteredProc != NULL) {
		(self.alteredProc)(self.objectId, sampleBuffer,
				   self.alteredRefCon);
	}
}

- (CMVideoFormatDescriptionRef)getFormatDescription
{
	CMVideoFormatDescriptionRef formatDescription;
	OSStatus err = CMVideoFormatDescriptionCreate(
		kCFAllocatorDefault, kCMVideoCodecType_422YpCbCr8,
		self.testCardSize.width, self.testCardSize.height, NULL,
		&formatDescription);
	if (err != noErr) {
		DLog(@"Error %d from CMVideoFormatDescriptionCreate", err);
	}
	return formatDescription;
}

#pragma mark - CMIOObject

- (UInt32)getPropertyDataSizeWithAddress:(CMIOObjectPropertyAddress)address
		       qualifierDataSize:(UInt32)qualifierDataSize
			   qualifierData:(nonnull const void *)qualifierData
{
	switch (address.mSelector) {
	case kCMIOStreamPropertyInitialPresentationTimeStampForLinkedAndSyncedAudio:
		return sizeof(CMTime);
	case kCMIOStreamPropertyOutputBuffersNeededForThrottledPlayback:
		return sizeof(UInt32);
	case kCMIOObjectPropertyName:
		return sizeof(CFStringRef);
	case kCMIOObjectPropertyManufacturer:
		return sizeof(CFStringRef);
	case kCMIOObjectPropertyElementName:
		return sizeof(CFStringRef);
	case kCMIOObjectPropertyElementCategoryName:
		return sizeof(CFStringRef);
	case kCMIOObjectPropertyElementNumberName:
		return sizeof(CFStringRef);
	case kCMIOStreamPropertyDirection:
		return sizeof(UInt32);
	case kCMIOStreamPropertyTerminalType:
		return sizeof(UInt32);
	case kCMIOStreamPropertyStartingChannel:
		return sizeof(UInt32);
	case kCMIOStreamPropertyLatency:
		return sizeof(UInt32);
	case kCMIOStreamPropertyFormatDescriptions:
		return sizeof(CFArrayRef);
	case kCMIOStreamPropertyFormatDescription:
		return sizeof(CMFormatDescriptionRef);
	case kCMIOStreamPropertyFrameRateRanges:
		return sizeof(AudioValueRange);
	case kCMIOStreamPropertyFrameRate:
	case kCMIOStreamPropertyFrameRates:
		return sizeof(Float64);
	case kCMIOStreamPropertyMinimumFrameRate:
		return sizeof(Float64);
	case kCMIOStreamPropertyClock:
		return sizeof(CFTypeRef);
	default:
		return 0;
	};
}

- (void)getPropertyDataWithAddress:(CMIOObjectPropertyAddress)address
		 qualifierDataSize:(UInt32)qualifierDataSize
		     qualifierData:(nonnull const void *)qualifierData
			  dataSize:(UInt32)dataSize
			  dataUsed:(nonnull UInt32 *)dataUsed
			      data:(nonnull void *)data
{
	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
		*static_cast<CFStringRef *>(data) = CFSTR("OBS Virtual Camera");
		*dataUsed = sizeof(CFStringRef);
		break;
	case kCMIOObjectPropertyElementName:
		*static_cast<CFStringRef *>(data) =
			CFSTR("OBS Virtual Camera Stream Element");
		*dataUsed = sizeof(CFStringRef);
		break;
	case kCMIOObjectPropertyManufacturer:
	case kCMIOObjectPropertyElementCategoryName:
	case kCMIOObjectPropertyElementNumberName:
	case kCMIOStreamPropertyTerminalType:
	case kCMIOStreamPropertyStartingChannel:
	case kCMIOStreamPropertyLatency:
	case kCMIOStreamPropertyInitialPresentationTimeStampForLinkedAndSyncedAudio:
	case kCMIOStreamPropertyOutputBuffersNeededForThrottledPlayback:
		break;
	case kCMIOStreamPropertyDirection:
		*static_cast<UInt32 *>(data) = 1;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIOStreamPropertyFormatDescriptions:
		*static_cast<CFArrayRef *>(
			data) = (__bridge_retained CFArrayRef)[NSArray
			arrayWithObject:(__bridge_transfer NSObject *)
						[self getFormatDescription]];
		*dataUsed = sizeof(CFArrayRef);
		break;
	case kCMIOStreamPropertyFormatDescription:
		*static_cast<CMVideoFormatDescriptionRef *>(data) =
			[self getFormatDescription];
		*dataUsed = sizeof(CMVideoFormatDescriptionRef);
		break;
	case kCMIOStreamPropertyFrameRateRanges:
		AudioValueRange range;
		range.mMinimum = self.fps;
		range.mMaximum = self.fps;
		*static_cast<AudioValueRange *>(data) = range;
		*dataUsed = sizeof(AudioValueRange);
		break;
	case kCMIOStreamPropertyFrameRate:
	case kCMIOStreamPropertyFrameRates:
		*static_cast<Float64 *>(data) = self.fps;
		*dataUsed = sizeof(Float64);
		break;
	case kCMIOStreamPropertyMinimumFrameRate:
		*static_cast<Float64 *>(data) = self.fps;
		*dataUsed = sizeof(Float64);
		break;
	case kCMIOStreamPropertyClock:
		*static_cast<CFTypeRef *>(data) = self.clock;
		// This one was incredibly tricky and cost me many hours to find. It seems that DAL expects
		// the clock to be retained when returned. It's unclear why, and that seems inconsistent
		// with other properties that don't have the same behavior. But this is what Apple's sample
		// code does.
		// https://github.com/lvsti/CoreMediaIO-DAL-Example/blob/0392cb/Sources/Extras/CoreMediaIO/DeviceAbstractionLayer/Devices/DP/Properties/CMIO_DP_Property_Clock.cpp#L75
		CFRetain(*static_cast<CFTypeRef *>(data));
		*dataUsed = sizeof(CFTypeRef);
		break;
	default:
		*dataUsed = 0;
	};
}

- (BOOL)hasPropertyWithAddress:(CMIOObjectPropertyAddress)address
{
	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
	case kCMIOObjectPropertyElementName:
	case kCMIOStreamPropertyFormatDescriptions:
	case kCMIOStreamPropertyFormatDescription:
	case kCMIOStreamPropertyFrameRateRanges:
	case kCMIOStreamPropertyFrameRate:
	case kCMIOStreamPropertyFrameRates:
	case kCMIOStreamPropertyMinimumFrameRate:
	case kCMIOStreamPropertyClock:
		return true;
	case kCMIOObjectPropertyManufacturer:
	case kCMIOObjectPropertyElementCategoryName:
	case kCMIOObjectPropertyElementNumberName:
	case kCMIOStreamPropertyDirection:
	case kCMIOStreamPropertyTerminalType:
	case kCMIOStreamPropertyStartingChannel:
	case kCMIOStreamPropertyLatency:
	case kCMIOStreamPropertyInitialPresentationTimeStampForLinkedAndSyncedAudio:
	case kCMIOStreamPropertyOutputBuffersNeededForThrottledPlayback:
		DLog(@"TODO: %@",
		     [OBSDALObjectStore
			     StringFromPropertySelector:address.mSelector]);
		return false;
	default:
		return false;
	};
}

- (BOOL)isPropertySettableWithAddress:(CMIOObjectPropertyAddress)address
{
	return false;
}

- (void)setPropertyDataWithAddress:(CMIOObjectPropertyAddress)address
		 qualifierDataSize:(UInt32)qualifierDataSize
		     qualifierData:(nonnull const void *)qualifierData
			  dataSize:(UInt32)dataSize
			      data:(nonnull const void *)data
{
}

@end
