#pragma once

#include <QMouseEvent>
#include "slider-ignorewheel.hpp"

class MediaSlider : public SliderIgnoreScroll {
	Q_OBJECT

public:
	MediaSlider(QWidget *parent = nullptr);

signals:
	void mediaSliderHovered(int value);

protected:
	virtual void mouseMoveEvent(QMouseEvent *event) override;
};
