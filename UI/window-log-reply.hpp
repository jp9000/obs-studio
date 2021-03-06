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

#pragma once

#include <memory>
#include "ui_OBSLogReply.h"

class OBSLogReply : public QDialog {
	Q_OBJECT

private:
	std::unique_ptr<Ui::OBSLogReply> ui;

public:
	OBSLogReply(QWidget *parent, const QString &url, const bool crash);

private slots:
	void on_copyURL_clicked();
	void on_analyzeURL_clicked();
};
