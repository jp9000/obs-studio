//
//  CMSampleBufferUtils.h
//  dal-plugin
//
//  Created by John Boiles  on 5/8/20.
//

#include <CoreMediaIO/CMIOSampleBuffer.h>

OSStatus CMSampleBufferCreateFromData(NSSize size,
				      CMSampleTimingInfo timingInfo,
				      UInt64 sequenceNumber, NSData *data,
				      CMSampleBufferRef *sampleBuffer);

OSStatus CMSampleBufferCreateFromDataNoCopy(NSSize size,
					    CMSampleTimingInfo timingInfo,
					    UInt64 sequenceNumber, NSData *data,
					    CMSampleBufferRef *sampleBuffer);

CMSampleTimingInfo CMSampleTimingInfoForTimestamp(uint64_t timestampNanos,
						  uint32_t fpsNumerator,
						  uint32_t fpsDenominator);
