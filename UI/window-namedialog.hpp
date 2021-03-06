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

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <string>
#include <memory>

class NameDialog : public QDialog {
	Q_OBJECT

public:
	NameDialog(QWidget *parent);

	// Returns true if user clicks OK, false otherwise
	// userTextInput returns string that user typed into dialog
	static bool AskForName(QWidget *parent, const QString &title,
			       const QString &text, std::string &userTextInput,
			       const QString &placeHolder = QString(""),
			       int maxSize = 170);

	// Returns true if user clicks OK, false otherwise
	// userTextInput returns string that user typed into dialog
	// userOptionReturn the checkbox was ticked user accepted
	static bool
	AskForNameWithOption(QWidget *parent, const QString &title,
			     const QString &text, std::string &userTextInput,
			     const QString &optionLabel, bool &optionChecked,
			     const QString &placeHolder = QString(""));

private:
	QLabel *label;
	QLineEdit *userText;
	QCheckBox *checkbox;
};
