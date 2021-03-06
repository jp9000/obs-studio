#include "slider-ignorewheel.hpp"
#include "volume-control.hpp"

SliderIgnoreScroll::SliderIgnoreScroll(QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
}

SliderIgnoreScroll::SliderIgnoreScroll(Qt::Orientation orientation,
				       QWidget *parent)
	: QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setOrientation(orientation);
}

void SliderIgnoreScroll::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QSlider::wheelEvent(event);
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, QWidget *parent)
	: SliderIgnoreScroll(parent)
{
	fad = fader;
}

VolumeSlider::VolumeSlider(obs_fader_t *fader, Qt::Orientation orientation,
			   QWidget *parent)
	: SliderIgnoreScroll(orientation, parent)
{
	fad = fader;
}

VolumeAccessibleInterface::VolumeAccessibleInterface(QWidget *w)
	: QAccessibleWidget(w)
{
}

VolumeSlider *VolumeAccessibleInterface::slider() const
{
	return qobject_cast<VolumeSlider *>(object());
}

QString VolumeAccessibleInterface::text(QAccessible::Text t) const
{
	if (slider()->isVisible()) {
		switch (t) {
		case QAccessible::Text::Value:
			return currentValue().toString();
		default:
			break;
		}
	}
	return QAccessibleWidget::text(t);
}

QVariant VolumeAccessibleInterface::currentValue() const
{
	QString text;
	float db = obs_fader_get_db(slider()->fad);

	if (db < -96.0f)
		text = "-inf dB";
	else
		text = QString::number(db, 'f', 1).append(" dB");

	return text;
}

void VolumeAccessibleInterface::setCurrentValue(const QVariant &value)
{
	slider()->setValue(value.toInt());
}

QVariant VolumeAccessibleInterface::maximumValue() const
{
	return slider()->maximum();
}

QVariant VolumeAccessibleInterface::minimumValue() const
{
	return slider()->minimum();
}

QVariant VolumeAccessibleInterface::minimumStepSize() const
{
	return slider()->singleStep();
}

QAccessible::Role VolumeAccessibleInterface::role() const
{
	return QAccessible::Role::Slider;
}
