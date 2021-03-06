#include "combobox-ignorewheel.hpp"

ComboBoxIgnoreScroll::ComboBoxIgnoreScroll(QWidget *parent) : QComboBox(parent)
{
	setFocusPolicy(Qt::StrongFocus);
}

void ComboBoxIgnoreScroll::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QComboBox::wheelEvent(event);
}
