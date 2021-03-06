#pragma once

#include <QProxyStyle>

class SliderAbsoluteSetStyle : public QProxyStyle {
public:
	SliderAbsoluteSetStyle(const QString &baseStyle);
	SliderAbsoluteSetStyle(QStyle *baseStyle = Q_NULLPTR);
	int styleHint(QStyle::StyleHint hint, const QStyleOption *option,
		      const QWidget *widget,
		      QStyleHintReturn *returnData) const;
};
