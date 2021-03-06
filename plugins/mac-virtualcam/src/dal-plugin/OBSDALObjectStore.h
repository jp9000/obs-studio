//
//  ObjectStore.h
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
#import <CoreMediaIO/CMIOHardwarePlugIn.h>

NS_ASSUME_NONNULL_BEGIN

@protocol CMIOObject

- (BOOL)hasPropertyWithAddress:(CMIOObjectPropertyAddress)address;
- (BOOL)isPropertySettableWithAddress:(CMIOObjectPropertyAddress)address;
- (UInt32)getPropertyDataSizeWithAddress:(CMIOObjectPropertyAddress)address
		       qualifierDataSize:(UInt32)qualifierDataSize
			   qualifierData:(const void *)qualifierData;
- (void)getPropertyDataWithAddress:(CMIOObjectPropertyAddress)address
		 qualifierDataSize:(UInt32)qualifierDataSize
		     qualifierData:(const void *)qualifierData
			  dataSize:(UInt32)dataSize
			  dataUsed:(UInt32 *)dataUsed
			      data:(void *)data;
- (void)setPropertyDataWithAddress:(CMIOObjectPropertyAddress)address
		 qualifierDataSize:(UInt32)qualifierDataSize
		     qualifierData:(const void *)qualifierData
			  dataSize:(UInt32)dataSize
			      data:(const void *)data;

@end

@interface OBSDALObjectStore : NSObject

+ (OBSDALObjectStore *)SharedObjectStore;

+ (NSObject<CMIOObject> *)GetObjectWithId:(CMIOObjectID)objectId;

+ (NSString *)StringFromPropertySelector:(CMIOObjectPropertySelector)selector;

+ (BOOL)IsBridgedTypeForSelector:(CMIOObjectPropertySelector)selector;

- (NSObject<CMIOObject> *)getObject:(CMIOObjectID)objectID;

- (void)setObject:(id<CMIOObject>)object forObjectId:(CMIOObjectID)objectId;

@end

NS_ASSUME_NONNULL_END
