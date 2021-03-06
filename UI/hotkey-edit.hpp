/******************************************************************************
    Copyright (C) 2014-2015 by Ruwen Hahn <palana@stunned.de>

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

#pragma once

#include <QLineEdit>
#include <QKeyEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QPointer>
#include <QLabel>

#include <obs.hpp>

class OBSHotkeyWidget;

class OBSHotkeyLabel : public QLabel {
	Q_OBJECT

public:
	QPointer<OBSHotkeyLabel> pairPartner;
	QPointer<OBSHotkeyWidget> widget;
	void highlightPair(bool highlight);
	void enterEvent(QEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void setToolTip(const QString &toolTip);
};

class OBSHotkeyEdit : public QLineEdit {
	Q_OBJECT;

public:
	OBSHotkeyEdit(obs_key_combination_t original, QWidget *parent = nullptr)
		: QLineEdit(parent), original(original)
	{
#ifdef __APPLE__
		// disable the input cursor on OSX, focus should be clear
		// enough with the default focus frame
		setReadOnly(true);
#endif
		setAttribute(Qt::WA_InputMethodEnabled, false);
		setAttribute(Qt::WA_MacShowFocusRect, true);
		InitSignalHandler();
		ResetKey();
	}

	obs_key_combination_t original;
	obs_key_combination_t key;
	bool changed = false;

protected:
	OBSSignal layoutChanged;

	void InitSignalHandler();

	void keyPressEvent(QKeyEvent *event) override;
#ifdef __APPLE__
	void keyReleaseEvent(QKeyEvent *event) override;
#endif
	void mousePressEvent(QMouseEvent *event) override;

	void HandleNewKey(obs_key_combination_t new_key);
	void RenderKey();

public slots:
	void ReloadKeyLayout();
	void ResetKey();
	void ClearKey();

signals:
	void KeyChanged(obs_key_combination_t);
};

class OBSHotkeyWidget : public QWidget {
	Q_OBJECT;

public:
	OBSHotkeyWidget(obs_hotkey_id id, std::string name,
			const std::vector<obs_key_combination_t> &combos = {},
			QWidget *parent = nullptr)
		: QWidget(parent),
		  id(id),
		  name(name),
		  bindingsChanged(obs_get_signal_handler(),
				  "hotkey_bindings_changed",
				  &OBSHotkeyWidget::BindingsChanged, this)
	{
		auto layout = new QVBoxLayout;
		layout->setSpacing(0);
		layout->setContentsMargins(0, 0, 0, 0);
		setLayout(layout);

		SetKeyCombinations(combos);
	}

	void SetKeyCombinations(const std::vector<obs_key_combination_t> &);

	obs_hotkey_id id;
	std::string name;

	bool changed = false;
	bool Changed() const;

	QPointer<OBSHotkeyLabel> label;

	QString toolTip;
	void setToolTip(const QString &toolTip_)
	{
		toolTip = toolTip_;
		for (auto &edit : edits)
			edit->setToolTip(toolTip_);
	}

	void Apply();
	void GetCombinations(std::vector<obs_key_combination_t> &) const;
	void Save();
	void Save(std::vector<obs_key_combination_t> &combinations);

	void enterEvent(QEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	void AddEdit(obs_key_combination combo, int idx = -1);
	void RemoveEdit(size_t idx, bool signal = true);

	static void BindingsChanged(void *data, calldata_t *param);

	std::vector<QPointer<OBSHotkeyEdit>> edits;
	std::vector<QPointer<QPushButton>> removeButtons;
	std::vector<QPointer<QPushButton>> revertButtons;
	OBSSignal bindingsChanged;
	bool ignoreChangedBindings = false;

	QVBoxLayout *layout() const
	{
		return dynamic_cast<QVBoxLayout *>(QWidget::layout());
	}

private slots:
	void HandleChangedBindings(obs_hotkey_id id_);

signals:
	void KeyChanged();
};
