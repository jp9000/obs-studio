/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "obs-app.hpp"
#include "window-basic-interaction.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "display-helpers.hpp"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QScreen>
#include <QWindow>

using namespace std;

OBSBasicInteraction::OBSBasicInteraction(QWidget *parent, OBSSource source_)
	: QDialog(parent),
	  main(qobject_cast<OBSBasic *>(parent)),
	  ui(new Ui::OBSBasicInteraction),
	  source(source_),
	  removedSignal(obs_source_get_signal_handler(source), "remove",
			OBSBasicInteraction::SourceRemoved, this),
	  renamedSignal(obs_source_get_signal_handler(source), "rename",
			OBSBasicInteraction::SourceRenamed, this),
	  eventFilter(BuildEventFilter())
{
	int cx = (int)config_get_int(App()->GlobalConfig(), "InteractionWindow",
				     "cx");
	int cy = (int)config_get_int(App()->GlobalConfig(), "InteractionWindow",
				     "cy");

	Qt::WindowFlags flags = windowFlags();
	Qt::WindowFlags helpFlag = Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags & (~helpFlag));

	ui->setupUi(this);

	ui->preview->setMouseTracking(true);
	ui->preview->setFocusPolicy(Qt::StrongFocus);
	ui->preview->installEventFilter(eventFilter.get());

	if (cx > 400 && cy > 400)
		resize(cx, cy);

	const char *name = obs_source_get_name(source);
	setWindowTitle(QTStr("Basic.InteractionWindow").arg(QT_UTF8(name)));

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(ui->preview->GetDisplay(),
					      OBSBasicInteraction::DrawPreview,
					      this);
	};

	connect(ui->preview, &OBSQTDisplay::DisplayCreated, addDrawCallback);
}

OBSBasicInteraction::~OBSBasicInteraction()
{
	// since QT fakes a mouse movement while destructing a widget
	// remove our event filter
	ui->preview->removeEventFilter(eventFilter.get());
}

OBSEventFilter *OBSBasicInteraction::BuildEventFilter()
{
	return new OBSEventFilter([this](QObject *obj, QEvent *event) {
		UNUSED_PARAMETER(obj);

		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
			return this->HandleMouseClickEvent(
				static_cast<QMouseEvent *>(event));
		case QEvent::MouseMove:
		case QEvent::Enter:
		case QEvent::Leave:
			return this->HandleMouseMoveEvent(
				static_cast<QMouseEvent *>(event));

		case QEvent::Wheel:
			return this->HandleMouseWheelEvent(
				static_cast<QWheelEvent *>(event));
		case QEvent::FocusIn:
		case QEvent::FocusOut:
			return this->HandleFocusEvent(
				static_cast<QFocusEvent *>(event));
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return this->HandleKeyEvent(
				static_cast<QKeyEvent *>(event));
		default:
			return false;
		}
	});
}

void OBSBasicInteraction::SourceRemoved(void *data, calldata_t *params)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicInteraction *>(data),
				  "close");

	UNUSED_PARAMETER(params);
}

void OBSBasicInteraction::SourceRenamed(void *data, calldata_t *params)
{
	const char *name = calldata_string(params, "new_name");
	QString title = QTStr("Basic.InteractionWindow").arg(QT_UTF8(name));

	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data),
				  "setWindowTitle", Q_ARG(QString, title));
}

void OBSBasicInteraction::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	OBSBasicInteraction *window = static_cast<OBSBasicInteraction *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->source), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void OBSBasicInteraction::closeEvent(QCloseEvent *event)
{
	QDialog::closeEvent(event);
	if (!event->isAccepted())
		return;

	config_set_int(App()->GlobalConfig(), "InteractionWindow", "cx",
		       width());
	config_set_int(App()->GlobalConfig(), "InteractionWindow", "cy",
		       height());

	obs_display_remove_draw_callback(ui->preview->GetDisplay(),
					 OBSBasicInteraction::DrawPreview,
					 this);
}

static int TranslateQtKeyboardEventModifiers(QInputEvent *event,
					     bool mouseEvent)
{
	int obsModifiers = INTERACT_NONE;

	if (event->modifiers().testFlag(Qt::ShiftModifier))
		obsModifiers |= INTERACT_SHIFT_KEY;
	if (event->modifiers().testFlag(Qt::AltModifier))
		obsModifiers |= INTERACT_ALT_KEY;
#ifdef __APPLE__
	// Mac: Meta = Control, Control = Command
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_COMMAND_KEY;
	if (event->modifiers().testFlag(Qt::MetaModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#else
	// Handle windows key? Can a browser even trap that key?
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#endif

	if (!mouseEvent) {
		if (event->modifiers().testFlag(Qt::KeypadModifier))
			obsModifiers |= INTERACT_IS_KEY_PAD;
	}

	return obsModifiers;
}

static int TranslateQtMouseEventModifiers(QMouseEvent *event)
{
	int modifiers = TranslateQtKeyboardEventModifiers(event, true);

	if (event->buttons().testFlag(Qt::LeftButton))
		modifiers |= INTERACT_MOUSE_LEFT;
	if (event->buttons().testFlag(Qt::MiddleButton))
		modifiers |= INTERACT_MOUSE_MIDDLE;
	if (event->buttons().testFlag(Qt::RightButton))
		modifiers |= INTERACT_MOUSE_RIGHT;

	return modifiers;
}

bool OBSBasicInteraction::GetSourceRelativeXY(int mouseX, int mouseY, int &relX,
					      int &relY)
{
#ifdef SUPPORTS_FRACTIONAL_SCALING
	float pixelRatio = devicePixelRatioF();
#else
	float pixelRatio = devicePixelRatio();
#endif
	int mouseXscaled = (int)roundf(mouseX * pixelRatio);
	int mouseYscaled = (int)roundf(mouseY * pixelRatio);

	QSize size = GetPixelSize(ui->preview);

	uint32_t sourceCX = max(obs_source_get_width(source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(source), 1u);

	int x, y;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, size.width(), size.height(), x,
			     y, scale);

	if (x > 0) {
		relX = int(float(mouseXscaled - x) / scale);
		relY = int(float(mouseYscaled / scale));
	} else {
		relX = int(float(mouseXscaled / scale));
		relY = int(float(mouseYscaled - y) / scale);
	}

	// Confirm mouse is inside the source
	if (relX < 0 || relX > int(sourceCX))
		return false;
	if (relY < 0 || relY > int(sourceCY))
		return false;

	return true;
}

bool OBSBasicInteraction::HandleMouseClickEvent(QMouseEvent *event)
{
	bool mouseUp = event->type() == QEvent::MouseButtonRelease;
	int clickCount = 1;
	if (event->type() == QEvent::MouseButtonDblClick)
		clickCount = 2;

	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);

	int32_t button = 0;

	switch (event->button()) {
	case Qt::LeftButton:
		button = MOUSE_LEFT;
		break;
	case Qt::MiddleButton:
		button = MOUSE_MIDDLE;
		break;
	case Qt::RightButton:
		button = MOUSE_RIGHT;
		break;
	default:
		blog(LOG_WARNING, "unknown button type %d", event->button());
		return false;
	}

	// Why doesn't this work?
	//if (event->flags().testFlag(Qt::MouseEventCreatedDoubleClick))
	//	clickCount = 2;

	bool insideSource = GetSourceRelativeXY(event->x(), event->y(),
						mouseEvent.x, mouseEvent.y);

	if (mouseUp || insideSource)
		obs_source_send_mouse_click(source, &mouseEvent, button,
					    mouseUp, clickCount);

	return true;
}

bool OBSBasicInteraction::HandleMouseMoveEvent(QMouseEvent *event)
{
	struct obs_mouse_event mouseEvent = {};

	bool mouseLeave = event->type() == QEvent::Leave;

	if (!mouseLeave) {
		mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);
		mouseLeave = !GetSourceRelativeXY(event->x(), event->y(),
						  mouseEvent.x, mouseEvent.y);
	}

	obs_source_send_mouse_move(source, &mouseEvent, mouseLeave);

	return true;
}

bool OBSBasicInteraction::HandleMouseWheelEvent(QWheelEvent *event)
{
	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtKeyboardEventModifiers(event, true);

	int xDelta = 0;
	int yDelta = 0;

	const QPoint angleDelta = event->angleDelta();
	if (!event->pixelDelta().isNull()) {
		if (angleDelta.x())
			xDelta = event->pixelDelta().x();
		else
			yDelta = event->pixelDelta().y();
	} else {
		if (angleDelta.x())
			xDelta = angleDelta.x();
		else
			yDelta = angleDelta.y();
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	const QPointF position = event->position();
	const int x = position.x();
	const int y = position.y();
#else
	const int x = event->x();
	const int y = event->y();
#endif

	if (GetSourceRelativeXY(x, y, mouseEvent.x, mouseEvent.y)) {
		obs_source_send_mouse_wheel(source, &mouseEvent, xDelta,
					    yDelta);
	}

	return true;
}

bool OBSBasicInteraction::HandleFocusEvent(QFocusEvent *event)
{
	bool focus = event->type() == QEvent::FocusIn;

	obs_source_send_focus(source, focus);

	return true;
}

bool OBSBasicInteraction::HandleKeyEvent(QKeyEvent *event)
{
	struct obs_key_event keyEvent;

	QByteArray text = event->text().toUtf8();
	keyEvent.modifiers = TranslateQtKeyboardEventModifiers(event, false);
	keyEvent.text = text.data();
	keyEvent.native_modifiers = event->nativeModifiers();
	keyEvent.native_scancode = event->nativeScanCode();
	keyEvent.native_vkey = event->nativeVirtualKey();

	bool keyUp = event->type() == QEvent::KeyRelease;

	obs_source_send_key_click(source, &keyEvent, keyUp);

	return true;
}

void OBSBasicInteraction::Init()
{
	show();
}
