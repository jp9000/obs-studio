#include <util/darray.h>
#include <util/crc32.h>
#include "find-font.h"
#include "text-freetype2.h"

#import <Foundation/Foundation.h>

extern void save_font_list(void);

static inline void add_path_font(const char *path)
{
	FT_Face face;
	FT_Long idx = 0;
	FT_Long max_faces = 1;

	while (idx < max_faces) {
		if (FT_New_Face(ft2_lib, path, idx, &face) != 0)
			break;

		build_font_path_info(face, idx++, path);
		max_faces = face->num_faces;
		FT_Done_Face(face);
	}
}

static void add_path_fonts(NSFileManager *file_manager, NSString *path)
{
	NSArray *files = NULL;

	files = [file_manager contentsOfDirectoryAtPath:path error:nil];

	for (NSString *file in files) {
		NSString *full_path =
			[path stringByAppendingPathComponent:file];

		BOOL is_dir = FALSE;
		bool folder_exists = [file_manager fileExistsAtPath:full_path
							isDirectory:&is_dir];

		if (folder_exists && is_dir) {
			add_path_fonts(file_manager, full_path);
		} else {
			add_path_font(full_path.fileSystemRepresentation);
		}
	}
}

void load_os_font_list(void)
{
	@autoreleasepool {
		BOOL is_dir;
		NSArray *paths = NSSearchPathForDirectoriesInDomains(
			NSLibraryDirectory, NSAllDomainsMask, true);

		for (NSString *path in paths) {
			NSFileManager *file_manager =
				[NSFileManager defaultManager];
			NSString *font_path =
				[path stringByAppendingPathComponent:@"Fonts"];

			bool folder_exists = [file_manager
				fileExistsAtPath:font_path
				     isDirectory:&is_dir];

			if (folder_exists && is_dir)
				add_path_fonts(file_manager, font_path);
		}

		save_font_list();
	}
}

static uint32_t add_font_checksum(uint32_t checksum, const char *path)
{
	if (path && *path)
		checksum = calc_crc32(checksum, path, strlen(path));
	return checksum;
}

static uint32_t add_font_checksum_path(uint32_t checksum,
				       NSFileManager *file_manager,
				       NSString *path)
{
	NSArray *files = NULL;

	files = [file_manager contentsOfDirectoryAtPath:path error:nil];

	for (NSString *file in files) {
		NSString *full_path =
			[path stringByAppendingPathComponent:file];

		checksum = add_font_checksum(
			checksum, full_path.fileSystemRepresentation);
	}

	return checksum;
}

uint32_t get_font_checksum(void)
{
	uint32_t checksum = 0;

	@autoreleasepool {
		BOOL is_dir;
		NSArray *paths = NSSearchPathForDirectoriesInDomains(
			NSLibraryDirectory, NSAllDomainsMask, true);

		for (NSString *path in paths) {
			NSFileManager *file_manager =
				[NSFileManager defaultManager];
			NSString *font_path =
				[path stringByAppendingPathComponent:@"Fonts"];

			bool folder_exists = [file_manager
				fileExistsAtPath:font_path
				     isDirectory:&is_dir];

			if (folder_exists && is_dir)
				checksum = add_font_checksum_path(
					checksum, file_manager, font_path);
		}
	}

	return checksum;
}
