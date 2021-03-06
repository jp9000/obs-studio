//
//  MachClient.h
//  dal-plugin
//
//  Created by John Boiles  on 5/5/20.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MachClientDelegate

- (void)receivedFrameWithSize:(NSSize)size
		    timestamp:(uint64_t)timestamp
		 fpsNumerator:(uint32_t)fpsNumerator
	       fpsDenominator:(uint32_t)fpsDenominator
		    frameData:(NSData *)frameData;
- (void)receivedStop;

@end

@interface OBSDALMachClient : NSObject

@property (nullable, weak) id<MachClientDelegate> delegate;

- (BOOL)isServerAvailable;

- (BOOL)connectToServer;

@end

NS_ASSUME_NONNULL_END
