#include "record-button.hpp"
#include "window-basic-main.hpp"

void RecordButton::resizeEvent(QResizeEvent *event)
{
	OBSBasic *main = OBSBasic::Get();
	if (!main->pause)
		return;

	QSize pauseSize = main->pause->size();
	int height = main->ui->recordButton->size().height();

	if (pauseSize.height() != height || pauseSize.width() != height) {
		main->pause->setMinimumSize(height, height);
		main->pause->setMaximumSize(height, height);
	}

	event->accept();
}

void ReplayBufferButton::resizeEvent(QResizeEvent *event)
{
	OBSBasic *main = OBSBasic::Get();
	if (!main->replay)
		return;

	QSize replaySize = main->replay->size();
	int height = main->ui->recordButton->size().height();

	if (replaySize.height() != height || replaySize.width() != height) {
		main->replay->setMinimumSize(height, height);
		main->replay->setMaximumSize(height, height);
	}

	event->accept();
}
