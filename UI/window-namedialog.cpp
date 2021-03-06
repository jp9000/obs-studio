/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#include "window-namedialog.hpp"
#include "qt-wrappers.hpp"
#include "obs-app.hpp"

#include <QVBoxLayout>

NameDialog::NameDialog(QWidget *parent) : QDialog(parent)
{
	installEventFilter(CreateShortcutFilter());
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedWidth(555);
	setMinimumHeight(100);
	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	label = new QLabel(this);
	layout->addWidget(label);
	label->setText("Set Text");

	userText = new QLineEdit(this);
	layout->addWidget(userText);

	checkbox = new QCheckBox(this);
	layout->addWidget(checkbox);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttonbox);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

static bool IsWhitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}

static void CleanWhitespace(std::string &str)
{
	while (str.size() && IsWhitespace(str.back()))
		str.erase(str.end() - 1);
	while (str.size() && IsWhitespace(str.front()))
		str.erase(str.begin());
}

bool NameDialog::AskForName(QWidget *parent, const QString &title,
			    const QString &text, std::string &userTextInput,
			    const QString &placeHolder, int maxSize)
{
	if (maxSize <= 0 || maxSize > 32767)
		maxSize = 170;

	NameDialog dialog(parent);
	dialog.setWindowTitle(title);

	dialog.checkbox->setHidden(true);
	dialog.label->setText(text);
	dialog.userText->setMaxLength(maxSize);
	dialog.userText->setText(placeHolder);
	dialog.userText->selectAll();

	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	userTextInput = dialog.userText->text().toUtf8().constData();
	CleanWhitespace(userTextInput);
	return true;
}

bool NameDialog::AskForNameWithOption(QWidget *parent, const QString &title,
				      const QString &text,
				      std::string &userTextInput,
				      const QString &optionLabel,
				      bool &optionChecked,
				      const QString &placeHolder)
{
	NameDialog dialog(parent);
	dialog.setWindowTitle(title);

	dialog.label->setText(text);
	dialog.userText->setMaxLength(170);
	dialog.userText->setText(placeHolder);
	dialog.checkbox->setText(optionLabel);
	dialog.checkbox->setChecked(optionChecked);

	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}

	userTextInput = dialog.userText->text().toUtf8().constData();
	CleanWhitespace(userTextInput);
	optionChecked = dialog.checkbox->isChecked();
	return true;
}
