#pragma once

#include <QSlider>
#include "slider-ignorewheel.hpp"

class DoubleSlider : public SliderIgnoreScroll {
	Q_OBJECT

	double minVal, maxVal, minStep;

public:
	DoubleSlider(QWidget *parent = nullptr);

	void setDoubleConstraints(double newMin, double newMax, double newStep,
				  double val);

signals:
	void doubleValChanged(double val);

public slots:
	void intValChanged(int val);
	void setDoubleVal(double val);
};
