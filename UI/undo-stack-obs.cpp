#include "undo-stack-obs.hpp"
#include "qt-wrappers.hpp"

#include <util/util.hpp>

#define MAX_STACK_SIZE 5000

undo_stack::undo_stack(ui_ptr ui) : ui(ui)
{
	QObject::connect(&repeat_reset_timer, &QTimer::timeout, this,
			 &undo_stack::reset_repeatable_state);
	repeat_reset_timer.setSingleShot(true);
	repeat_reset_timer.setInterval(3000);
}

void undo_stack::reset_repeatable_state()
{
	last_is_repeatable = false;
}

static inline void setHistoryRow(QListWidget *stack, int index)
{
	stack->blockSignals(true);
	stack->setCurrentRow(index);
	stack->blockSignals(false);
}

static inline void styleHistoryRow(QListWidget *stack, int index)
{
	QListWidgetItem *item = stack->item(index);
	item->setForeground(stack->palette().windowText());

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	QFontMetrics font(item->font());
	if (font.horizontalAdvance(item->text()) > 220)
		item->setToolTip(item->text());
#endif
}

void undo_stack::clear()
{
	undo_items.clear();
	redo_items.clear();
	last_is_repeatable = false;

	ui->actionMainUndo->setText(QTStr("Undo.Undo"));
	ui->actionMainRedo->setText(QTStr("Undo.Redo"));

	ui->actionMainUndo->setDisabled(true);
	ui->actionMainRedo->setDisabled(true);

	ui->undoStack->clear();

	QString name(config_get_string(App()->GlobalConfig(), "Basic",
				       "SceneCollection"));
	ui->undoStack->addItem(QTStr("Undo.LoadSceneCollection").arg(name));

	styleHistoryRow(ui->undoStack, 0);
	setHistoryRow(ui->undoStack, 0);
}

void undo_stack::add_action(const QString &name, undo_redo_cb undo,
			    undo_redo_cb redo, std::string undo_data,
			    std::string redo_data, bool repeatable)
{
	if (!is_enabled())
		return;

	while (undo_items.size() >= MAX_STACK_SIZE) {
		undo_redo_t item = undo_items.back();
		undo_items.pop_back();
		if (ui->undoStack->count() > 1)
			ui->undoStack->takeItem(1);
	}

	undo_redo_t n = {name, undo_data, redo_data, undo, redo};

	if (repeatable) {
		repeat_reset_timer.start();
	}

	if (last_is_repeatable && repeatable && name == undo_items[0].name) {
		undo_items[0].redo = redo;
		undo_items[0].redo_data = redo_data;
		return;
	}

	last_is_repeatable = repeatable;
	undo_items.push_front(n);
	clear_redo();

	int index = (int)undo_items.size();
	while (ui->undoStack->count() > index)
		ui->undoStack->takeItem(ui->undoStack->count() - 1);

	ui->undoStack->insertItem(index, name);
	styleHistoryRow(ui->undoStack, index);
	setHistoryRow(ui->undoStack, index);

	ui->actionMainUndo->setText(QTStr("Undo.Item.Undo").arg(name));
	ui->actionMainUndo->setEnabled(true);

	ui->actionMainRedo->setText(QTStr("Undo.Redo"));
	ui->actionMainRedo->setDisabled(true);
}

void undo_stack::undo()
{
	if (undo_items.size() == 0 || !is_enabled())
		return;

	last_is_repeatable = false;

	undo_redo_t temp = undo_items.front();
	temp.undo(temp.undo_data);
	redo_items.push_front(temp);
	undo_items.pop_front();

	ui->actionMainRedo->setText(QTStr("Undo.Item.Redo").arg(temp.name));
	ui->actionMainRedo->setEnabled(true);

	int index = (int)undo_items.size();
	QListWidgetItem *redoItem = ui->undoStack->item(index + 1);
	redoItem->setForeground(ui->undoStack->palette().light());

	setHistoryRow(ui->undoStack, index);

	if (undo_items.size() == 0) {
		ui->actionMainUndo->setDisabled(true);
		ui->actionMainUndo->setText(QTStr("Undo.Undo"));
	} else {
		ui->actionMainUndo->setText(
			QTStr("Undo.Item.Undo").arg(undo_items.front().name));
	}
}

void undo_stack::redo()
{
	if (redo_items.size() == 0 || !is_enabled())
		return;

	last_is_repeatable = false;

	undo_redo_t temp = redo_items.front();
	temp.redo(temp.redo_data);
	undo_items.push_front(temp);
	redo_items.pop_front();

	int index = (int)undo_items.size();
	if (index < 0)
		index = 0;
	QListWidgetItem *redoItem = ui->undoStack->item(index);
	redoItem->setForeground(ui->undoStack->palette().windowText());

	setHistoryRow(ui->undoStack, index);

	ui->actionMainUndo->setText(QTStr("Undo.Item.Undo").arg(temp.name));
	ui->actionMainUndo->setEnabled(true);

	if (redo_items.size() == 0) {
		ui->actionMainRedo->setDisabled(true);
		ui->actionMainRedo->setText(QTStr("Undo.Redo"));
	} else {
		ui->actionMainRedo->setText(
			QTStr("Undo.Item.Redo").arg(redo_items.front().name));
	}
}

void undo_stack::enable_internal()
{
	last_is_repeatable = false;

	ui->actionMainUndo->setDisabled(false);
	if (redo_items.size() > 0)
		ui->actionMainRedo->setDisabled(false);
}

void undo_stack::disable_internal()
{
	last_is_repeatable = false;

	ui->actionMainUndo->setDisabled(true);
	ui->actionMainRedo->setDisabled(true);
}

void undo_stack::enable()
{
	enabled = true;
	if (is_enabled())
		enable_internal();
}

void undo_stack::disable()
{
	if (is_enabled())
		disable_internal();
	enabled = false;
}

void undo_stack::push_disabled()
{
	if (is_enabled())
		disable_internal();
	disable_refs++;
}

void undo_stack::pop_disabled()
{
	disable_refs--;
	if (is_enabled())
		enable_internal();
}

void undo_stack::clear_redo()
{
	redo_items.clear();
}
