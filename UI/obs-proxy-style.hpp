#pragma once

#include <QProxyStyle>

class OBSProxyStyle : public QProxyStyle {
public:
	QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap,
				    const QStyleOption *option) const override;
};
