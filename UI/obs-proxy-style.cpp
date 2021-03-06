#include "obs-proxy-style.hpp"
#include <QStyleOptionButton>

static inline uint qt_intensity(uint r, uint g, uint b)
{
	/* 30% red, 59% green, 11% blue */
	return (77 * r + 150 * g + 28 * b) / 255;
}

/* The constants in the default QT styles don't dim the icons enough in
 * disabled mode
 *
 * https://code.woboq.org/qt5/qtbase/src/widgets/styles/qcommonstyle.cpp.html#6429
 */
QPixmap OBSProxyStyle::generatedIconPixmap(QIcon::Mode iconMode,
					   const QPixmap &pixmap,
					   const QStyleOption *option) const
{
	if (iconMode == QIcon::Disabled) {
		QImage im =
			pixmap.toImage().convertToFormat(QImage::Format_ARGB32);

		/* Create a colortable based on the background
		 * (black -> bg -> white) */

		QColor bg = option->palette.color(QPalette::Disabled,
						  QPalette::Window);
		int red = bg.red();
		int green = bg.green();
		int blue = bg.blue();
		uchar reds[256], greens[256], blues[256];

		for (int i = 0; i < 128; ++i) {
			reds[i] = uchar((red * (i << 1)) >> 8);
			greens[i] = uchar((green * (i << 1)) >> 8);
			blues[i] = uchar((blue * (i << 1)) >> 8);
		}
		for (int i = 0; i < 128; ++i) {
			reds[i + 128] = uchar(qMin(red + (i << 1), 255));
			greens[i + 128] = uchar(qMin(green + (i << 1), 255));
			blues[i + 128] = uchar(qMin(blue + (i << 1), 255));
		}

		/* High intensity colors needs dark shifting in the color
		 * table, while low intensity colors needs light shifting. This
		 * is to increase the perceived contrast. */

		int intensity = qt_intensity(red, green, blue);
		const int factor = 191;

		if ((red - factor > green && red - factor > blue) ||
		    (green - factor > red && green - factor > blue) ||
		    (blue - factor > red && blue - factor > green))
			qMin(255, intensity + 20);
		else if (intensity <= 128)
			intensity += 100;

		for (int y = 0; y < im.height(); ++y) {
			QRgb *scanLine = (QRgb *)im.scanLine(y);
			for (int x = 0; x < im.width(); ++x) {
				QRgb pixel = *scanLine;
				/* Calculate color table index, taking
				 * intensity adjustment and a magic offset into
				 * account. */
				uint ci = uint(qGray(pixel) / 3 +
					       (130 - intensity / 3));
				*scanLine = qRgba(reds[ci], greens[ci],
						  blues[ci], qAlpha(pixel));
				++scanLine;
			}
		}

		return QPixmap::fromImage(im);
	}

	return QProxyStyle::generatedIconPixmap(iconMode, pixmap, option);
}
