//
//  Device.h
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

#import <Foundation/Foundation.h>

#import "OBSDALObjectStore.h"

NS_ASSUME_NONNULL_BEGIN

@interface OBSDALDevice : NSObject <CMIOObject>

@property CMIOObjectID objectId;
@property CMIOObjectID pluginId;
@property CMIOObjectID streamId;

@end

NS_ASSUME_NONNULL_END
