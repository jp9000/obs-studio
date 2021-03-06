#include "double-slider.hpp"

#include <cmath>

DoubleSlider::DoubleSlider(QWidget *parent) : SliderIgnoreScroll(parent)
{
	connect(this, SIGNAL(valueChanged(int)), this,
		SLOT(intValChanged(int)));
}

void DoubleSlider::setDoubleConstraints(double newMin, double newMax,
					double newStep, double val)
{
	minVal = newMin;
	maxVal = newMax;
	minStep = newStep;

	double total = maxVal - minVal;
	int intMax = int(total / minStep);

	setMinimum(0);
	setMaximum(intMax);
	setSingleStep(1);
	setDoubleVal(val);
}

void DoubleSlider::intValChanged(int val)
{
	emit doubleValChanged((minVal / minStep + val) * minStep);
}

void DoubleSlider::setDoubleVal(double val)
{
	setValue(lround((val - minVal) / minStep));
}
