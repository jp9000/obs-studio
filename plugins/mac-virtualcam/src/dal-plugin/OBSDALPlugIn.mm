//
//  PlugIn.mm
//  obs-mac-virtualcam
//
//  Created by John Boiles  on 4/9/20.
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

#import "OBSDALPlugIn.h"

#import <CoreMediaIO/CMIOHardwarePlugin.h>

#import "Logging.h"

typedef enum {
	PlugInStateNotStarted = 0,
	PlugInStateWaitingForServer,
	PlugInStateReceivingFrames,
} OBSDALPlugInState;

@interface OBSDALPlugin () <MachClientDelegate> {
	//! Serial queue for all state changes that need to be concerned with thread safety
	dispatch_queue_t _stateQueue;

	//! Repeated timer for driving the mach server re-connection
	dispatch_source_t _machConnectTimer;

	//! Timeout timer when we haven't received frames for 5s
	dispatch_source_t _timeoutTimer;
}
@property OBSDALPlugInState state;
@property OBSDALMachClient *machClient;

@end

@implementation OBSDALPlugin

+ (OBSDALPlugin *)SharedPlugIn
{
	static OBSDALPlugin *sPlugIn = nil;
	static dispatch_once_t sOnceToken;
	dispatch_once(&sOnceToken, ^{
		sPlugIn = [[self alloc] init];
	});
	return sPlugIn;
}

- (instancetype)init
{
	if (self = [super init]) {
		_stateQueue = dispatch_queue_create(
			"com.obsproject.obs-mac-virtualcam.dal.state",
			DISPATCH_QUEUE_SERIAL);

		_timeoutTimer = dispatch_source_create(
			DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _stateQueue);
		__weak __typeof(self) weakSelf = self;
		dispatch_source_set_event_handler(_timeoutTimer, ^{
			if (weakSelf.state == PlugInStateReceivingFrames) {
				DLog(@"No frames received for 5s, restarting connection");
				[self stopStream];
				[self startStream];
			}
		});

		_machClient = [[OBSDALMachClient alloc] init];
		_machClient.delegate = self;

		_machConnectTimer = dispatch_source_create(
			DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _stateQueue);
		dispatch_time_t startTime = dispatch_time(DISPATCH_TIME_NOW, 0);
		uint64_t intervalTime = (int64_t)(1 * NSEC_PER_SEC);
		dispatch_source_set_timer(_machConnectTimer, startTime,
					  intervalTime, 0);
		dispatch_source_set_event_handler(_machConnectTimer, ^{
			if (![[weakSelf machClient] isServerAvailable]) {
				DLog(@"Server is not available");
			} else if (weakSelf.state ==
				   PlugInStateWaitingForServer) {
				DLog(@"Attempting connection");
				[[weakSelf machClient] connectToServer];
			}
		});
	}
	return self;
}

- (void)startStream
{
	DLogFunc(@"");
	dispatch_async(_stateQueue, ^{
		if (_state == PlugInStateNotStarted) {
			dispatch_resume(_machConnectTimer);
			[self.stream startServingDefaultFrames];
			_state = PlugInStateWaitingForServer;
		}
	});
}

- (void)stopStream
{
	DLogFunc(@"");
	dispatch_async(_stateQueue, ^{
		if (_state == PlugInStateWaitingForServer) {
			dispatch_suspend(_machConnectTimer);
			[self.stream stopServingDefaultFrames];
		} else if (_state == PlugInStateReceivingFrames) {
			// TODO: Disconnect from the mach server?
			dispatch_suspend(_timeoutTimer);
		}
		_state = PlugInStateNotStarted;
	});
}

- (void)initialize
{
}

- (void)teardown
{
}

#pragma mark - CMIOObject

- (BOOL)hasPropertyWithAddress:(CMIOObjectPropertyAddress)address
{
	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
		return true;
	default:
		DLog(@"PlugIn unhandled hasPropertyWithAddress for %@",
		     [OBSDALObjectStore
			     StringFromPropertySelector:address.mSelector]);
		return false;
	};
}

- (BOOL)isPropertySettableWithAddress:(CMIOObjectPropertyAddress)address
{
	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
		return false;
	default:
		DLog(@"PlugIn unhandled isPropertySettableWithAddress for %@",
		     [OBSDALObjectStore
			     StringFromPropertySelector:address.mSelector]);
		return false;
	};
}

- (UInt32)getPropertyDataSizeWithAddress:(CMIOObjectPropertyAddress)address
		       qualifierDataSize:(UInt32)qualifierDataSize
			   qualifierData:(const void *)qualifierData
{
	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
		return sizeof(CFStringRef);
	default:
		DLog(@"PlugIn unhandled getPropertyDataSizeWithAddress for %@",
		     [OBSDALObjectStore
			     StringFromPropertySelector:address.mSelector]);
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
		*static_cast<CFStringRef *>(data) =
			CFSTR("OBS Virtual Camera Plugin");
		*dataUsed = sizeof(CFStringRef);
		return;
	default:
		DLog(@"PlugIn unhandled getPropertyDataWithAddress for %@",
		     [OBSDALObjectStore
			     StringFromPropertySelector:address.mSelector]);
		return;
	};
}

- (void)setPropertyDataWithAddress:(CMIOObjectPropertyAddress)address
		 qualifierDataSize:(UInt32)qualifierDataSize
		     qualifierData:(nonnull const void *)qualifierData
			  dataSize:(UInt32)dataSize
			      data:(nonnull const void *)data
{
	DLog(@"PlugIn unhandled setPropertyDataWithAddress for %@",
	     [OBSDALObjectStore StringFromPropertySelector:address.mSelector]);
}

#pragma mark - MachClientDelegate

- (void)receivedFrameWithSize:(NSSize)size
		    timestamp:(uint64_t)timestamp
		 fpsNumerator:(uint32_t)fpsNumerator
	       fpsDenominator:(uint32_t)fpsDenominator
		    frameData:(NSData *)frameData
{
	dispatch_sync(_stateQueue, ^{
		if (_state == PlugInStateWaitingForServer) {
			NSUserDefaults *defaults =
				[NSUserDefaults standardUserDefaults];
			[defaults setInteger:size.width
				      forKey:kTestCardWidthKey];
			[defaults setInteger:size.height
				      forKey:kTestCardHeightKey];
			[defaults setDouble:(double)fpsNumerator /
					    (double)fpsDenominator
				     forKey:kTestCardFPSKey];

			dispatch_suspend(_machConnectTimer);
			[self.stream stopServingDefaultFrames];
			dispatch_resume(_timeoutTimer);
			_state = PlugInStateReceivingFrames;
		}
	});

	// Add 5 more seconds onto the timeout timer
	dispatch_source_set_timer(
		_timeoutTimer,
		dispatch_time(DISPATCH_TIME_NOW, 5.0 * NSEC_PER_SEC),
		5.0 * NSEC_PER_SEC, (1ull * NSEC_PER_SEC) / 10);

	[self.stream queueFrameWithSize:size
			      timestamp:timestamp
			   fpsNumerator:fpsNumerator
			 fpsDenominator:fpsDenominator
			      frameData:frameData];
}

- (void)receivedStop
{
	DLogFunc(@"Restarting connection");
	[self stopStream];
	[self startStream];
}

@end
