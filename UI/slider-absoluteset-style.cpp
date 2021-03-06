#include "slider-absoluteset-style.hpp"

SliderAbsoluteSetStyle::SliderAbsoluteSetStyle(const QString &baseStyle)
	: QProxyStyle(baseStyle)
{
}
SliderAbsoluteSetStyle::SliderAbsoluteSetStyle(QStyle *baseStyle)
	: QProxyStyle(baseStyle)
{
}

int SliderAbsoluteSetStyle::styleHint(QStyle::StyleHint hint,
				      const QStyleOption *option = 0,
				      const QWidget *widget = 0,
				      QStyleHintReturn *returnData = 0) const
{
	if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
		return (Qt::LeftButton | Qt::MiddleButton);
	return QProxyStyle::styleHint(hint, option, widget, returnData);
}
