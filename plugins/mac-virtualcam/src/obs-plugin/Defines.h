//
//  Defines.h
//  obs-mac-virtualcam
//
//  Created by John Boiles  on 5/27/20.
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

#define PLUGIN_NAME "mac-virtualcam"
#define PLUGIN_VERSION "1.3.0"

#define blog(level, msg, ...) \
	blog(level, "[" PLUGIN_NAME "] " msg, ##__VA_ARGS__)
