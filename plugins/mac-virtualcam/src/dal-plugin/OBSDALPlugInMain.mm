//
//  PlugInMain.mm
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

#import <CoreMediaIO/CMIOHardwarePlugin.h>

#import "OBSDALPlugInInterface.h"
#import "Logging.h"
#import "Defines.h"

//! PlugInMain is the entrypoint for the plugin
extern "C" {
void *PlugInMain(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID)
{
	UNUSED_PARAMETER(allocator);

	DLogFunc(@"version=%@", PLUGIN_VERSION);
	if (!CFEqual(requestedTypeUUID, kCMIOHardwarePlugInTypeID)) {
		return 0;
	}

	return OBSDALPlugInRef();
}
}
