//
//  Device.mm
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

#import "OBSDALDevice.h"

#import <CoreFoundation/CoreFoundation.h>
#include <IOKit/audio/IOAudioTypes.h>

#import "OBSDALPlugIn.h"
#import "Logging.h"

@interface OBSDALDevice ()
@property BOOL excludeNonDALAccess;
@property pid_t masterPid;
@end

@implementation OBSDALDevice

// Note that the DAL's API calls HasProperty before calling GetPropertyDataSize. This means that it can be assumed that address is valid for the property involved.
- (UInt32)getPropertyDataSizeWithAddress:(CMIOObjectPropertyAddress)address
		       qualifierDataSize:(UInt32)qualifierDataSize
			   qualifierData:(nonnull const void *)qualifierData
{

	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
		return sizeof(CFStringRef);
	case kCMIOObjectPropertyManufacturer:
		return sizeof(CFStringRef);
	case kCMIOObjectPropertyElementCategoryName:
		return sizeof(CFStringRef);
	case kCMIOObjectPropertyElementNumberName:
		return sizeof(CFStringRef);
	case kCMIODevicePropertyPlugIn:
		return sizeof(CMIOObjectID);
	case kCMIODevicePropertyDeviceUID:
		return sizeof(CFStringRef);
	case kCMIODevicePropertyModelUID:
		return sizeof(CFStringRef);
	case kCMIODevicePropertyTransportType:
		return sizeof(UInt32);
	case kCMIODevicePropertyDeviceIsAlive:
		return sizeof(UInt32);
	case kCMIODevicePropertyDeviceHasChanged:
		return sizeof(UInt32);
	case kCMIODevicePropertyDeviceIsRunning:
		return sizeof(UInt32);
	case kCMIODevicePropertyDeviceIsRunningSomewhere:
		return sizeof(UInt32);
	case kCMIODevicePropertyDeviceCanBeDefaultDevice:
		return sizeof(UInt32);
	case kCMIODevicePropertyHogMode:
		return sizeof(pid_t);
	case kCMIODevicePropertyLatency:
		return sizeof(UInt32);
	case kCMIODevicePropertyStreams:
		// Only one stream
		return sizeof(CMIOStreamID) * 1;
	case kCMIODevicePropertyStreamConfiguration:
		// Only one stream
		return sizeof(UInt32) + (sizeof(UInt32) * 1);
	case kCMIODevicePropertyExcludeNonDALAccess:
		return sizeof(UInt32);
	case kCMIODevicePropertyCanProcessAVCCommand:
		return sizeof(Boolean);
	case kCMIODevicePropertyCanProcessRS422Command:
		return sizeof(Boolean);
	case kCMIODevicePropertyLinkedCoreAudioDeviceUID:
		return sizeof(CFStringRef);
	case kCMIODevicePropertyDeviceMaster:
		return sizeof(pid_t);
	default:
		break;
	};

	return 0;
}

// Note that the DAL's API calls HasProperty before calling GetPropertyData. This means that it can be assumed that address is valid for the property involved.
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
	case kCMIOObjectPropertyManufacturer:
		*static_cast<CFStringRef *>(data) = CFSTR("John Boiles");
		*dataUsed = sizeof(CFStringRef);
		break;
	case kCMIOObjectPropertyElementCategoryName:
		*static_cast<CFStringRef *>(data) = CFSTR("Virtual Camera");
		*dataUsed = sizeof(CFStringRef);
		break;
	case kCMIOObjectPropertyElementNumberName:
		*static_cast<CFStringRef *>(data) = CFSTR("0001");
		*dataUsed = sizeof(CFStringRef);
		break;
	case kCMIODevicePropertyPlugIn:
		*static_cast<CMIOObjectID *>(data) = self.pluginId;
		*dataUsed = sizeof(CMIOObjectID);
		break;
	case kCMIODevicePropertyDeviceUID:
		*static_cast<CFStringRef *>(data) =
			CFSTR("obs-virtual-cam-device");
		*dataUsed = sizeof(CFStringRef);
		break;
	case kCMIODevicePropertyModelUID:
		*static_cast<CFStringRef *>(data) =
			CFSTR("obs-virtual-cam-model");
		*dataUsed = sizeof(CFStringRef);
		break;
	case kCMIODevicePropertyTransportType:
		*static_cast<UInt32 *>(data) =
			kIOAudioDeviceTransportTypeBuiltIn;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyDeviceIsAlive:
		*static_cast<UInt32 *>(data) = 1;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyDeviceHasChanged:
		*static_cast<UInt32 *>(data) = 0;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyDeviceIsRunning:
		*static_cast<UInt32 *>(data) = 1;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyDeviceIsRunningSomewhere:
		*static_cast<UInt32 *>(data) = 1;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyDeviceCanBeDefaultDevice:
		*static_cast<UInt32 *>(data) = 1;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyHogMode:
		*static_cast<pid_t *>(data) = -1;
		*dataUsed = sizeof(pid_t);
		break;
	case kCMIODevicePropertyLatency:
		*static_cast<UInt32 *>(data) = 0;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyStreams:
		*static_cast<CMIOObjectID *>(data) = self.streamId;
		*dataUsed = sizeof(CMIOObjectID);
		break;
	case kCMIODevicePropertyStreamConfiguration:
		DLog(@"TODO kCMIODevicePropertyStreamConfiguration");
		break;
	case kCMIODevicePropertyExcludeNonDALAccess:
		*static_cast<UInt32 *>(data) = self.excludeNonDALAccess ? 1 : 0;
		*dataUsed = sizeof(UInt32);
		break;
	case kCMIODevicePropertyCanProcessAVCCommand:
		*static_cast<Boolean *>(data) = false;
		*dataUsed = sizeof(Boolean);
		break;
	case kCMIODevicePropertyCanProcessRS422Command:
		*static_cast<Boolean *>(data) = false;
		*dataUsed = sizeof(Boolean);
		break;
	case kCMIODevicePropertyDeviceMaster:
		*static_cast<pid_t *>(data) = self.masterPid;
		*dataUsed = sizeof(pid_t);
		break;
	default:
		break;
	};
}

- (BOOL)hasPropertyWithAddress:(CMIOObjectPropertyAddress)address
{
	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
	case kCMIOObjectPropertyManufacturer:
	case kCMIOObjectPropertyElementCategoryName:
	case kCMIOObjectPropertyElementNumberName:
	case kCMIODevicePropertyPlugIn:
	case kCMIODevicePropertyDeviceUID:
	case kCMIODevicePropertyModelUID:
	case kCMIODevicePropertyTransportType:
	case kCMIODevicePropertyDeviceIsAlive:
	case kCMIODevicePropertyDeviceHasChanged:
	case kCMIODevicePropertyDeviceIsRunning:
	case kCMIODevicePropertyDeviceIsRunningSomewhere:
	case kCMIODevicePropertyDeviceCanBeDefaultDevice:
	case kCMIODevicePropertyHogMode:
	case kCMIODevicePropertyLatency:
	case kCMIODevicePropertyStreams:
	case kCMIODevicePropertyExcludeNonDALAccess:
	case kCMIODevicePropertyCanProcessAVCCommand:
	case kCMIODevicePropertyCanProcessRS422Command:
	case kCMIODevicePropertyDeviceMaster:
		return true;
	case kCMIODevicePropertyStreamConfiguration:
	case kCMIODevicePropertyLinkedCoreAudioDeviceUID:
		return false;
	default:
		return false;
	};
}

- (BOOL)isPropertySettableWithAddress:(CMIOObjectPropertyAddress)address
{
	switch (address.mSelector) {
	case kCMIOObjectPropertyName:
	case kCMIOObjectPropertyManufacturer:
	case kCMIOObjectPropertyElementCategoryName:
	case kCMIOObjectPropertyElementNumberName:
	case kCMIODevicePropertyPlugIn:
	case kCMIODevicePropertyDeviceUID:
	case kCMIODevicePropertyModelUID:
	case kCMIODevicePropertyTransportType:
	case kCMIODevicePropertyDeviceIsAlive:
	case kCMIODevicePropertyDeviceHasChanged:
	case kCMIODevicePropertyDeviceIsRunning:
	case kCMIODevicePropertyDeviceIsRunningSomewhere:
	case kCMIODevicePropertyDeviceCanBeDefaultDevice:
	case kCMIODevicePropertyHogMode:
	case kCMIODevicePropertyLatency:
	case kCMIODevicePropertyStreams:
	case kCMIODevicePropertyStreamConfiguration:
	case kCMIODevicePropertyCanProcessAVCCommand:
	case kCMIODevicePropertyCanProcessRS422Command:
	case kCMIODevicePropertyLinkedCoreAudioDeviceUID:
		return false;
	case kCMIODevicePropertyExcludeNonDALAccess:
	case kCMIODevicePropertyDeviceMaster:
		return true;
	default:
		return false;
	};
}

- (void)setPropertyDataWithAddress:(CMIOObjectPropertyAddress)address
		 qualifierDataSize:(UInt32)qualifierDataSize
		     qualifierData:(nonnull const void *)qualifierData
			  dataSize:(UInt32)dataSize
			      data:(nonnull const void *)data
{

	switch (address.mSelector) {
	case kCMIODevicePropertyExcludeNonDALAccess:
		self.excludeNonDALAccess =
			(*static_cast<const UInt32 *>(data) != 0);
		break;
	case kCMIODevicePropertyDeviceMaster:
		self.masterPid = *static_cast<const pid_t *>(data);
		break;
	default:
		break;
	};
}

@end
