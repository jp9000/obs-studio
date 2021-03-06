#include "graphics.h"
#include "obsconfig.h"

#define MAGICKCORE_QUANTUM_DEPTH 16
#define MAGICKCORE_HDRI_ENABLE 0

#if LIBOBS_IMAGEMAGICK_DIR_STYLE == LIBOBS_IMAGEMAGICK_DIR_STYLE_6L
#include <magick/MagickCore.h>
#elif LIBOBS_IMAGEMAGICK_DIR_STYLE == LIBOBS_IMAGEMAGICK_DIR_STYLE_7GE
#include <MagickCore/MagickCore.h>
#endif

void gs_init_image_deps()
{
	MagickCoreGenesis(NULL, MagickTrue);
}

void gs_free_image_deps()
{
	MagickCoreTerminus();
}

uint8_t *gs_create_texture_file_data(const char *file,
				     enum gs_color_format *format,
				     uint32_t *cx_out, uint32_t *cy_out)
{
	uint8_t *data = NULL;
	ImageInfo *info;
	ExceptionInfo *exception;
	Image *image;

	if (!file || !*file)
		return NULL;

	info = CloneImageInfo(NULL);
	exception = AcquireExceptionInfo();

	strcpy(info->filename, file);
	image = ReadImage(info, exception);
	if (image) {
		size_t cx = image->magick_columns;
		size_t cy = image->magick_rows;
		data = bmalloc(cx * cy * 4);

		ExportImagePixels(image, 0, 0, cx, cy, "BGRA", CharPixel, data,
				  exception);
		if (exception->severity != UndefinedException) {
			blog(LOG_WARNING,
			     "magickcore warning/error getting "
			     "pixels from file '%s': %s",
			     file, exception->reason);
			bfree(data);
			data = NULL;
		}

		*format = GS_BGRA;
		*cx_out = (uint32_t)cx;
		*cy_out = (uint32_t)cy;
		DestroyImage(image);

	} else if (exception->severity != UndefinedException) {
		blog(LOG_WARNING,
		     "magickcore warning/error reading file "
		     "'%s': %s",
		     file, exception->reason);
	}

	DestroyImageInfo(info);
	DestroyExceptionInfo(exception);

	return data;
}
