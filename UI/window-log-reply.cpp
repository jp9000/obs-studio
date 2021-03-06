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

#include <QClipboard>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include "window-log-reply.hpp"
#include "obs-app.hpp"

OBSLogReply::OBSLogReply(QWidget *parent, const QString &url, const bool crash)
	: QDialog(parent), ui(new Ui::OBSLogReply)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui->setupUi(this);
	ui->urlEdit->setText(url);
	if (crash) {
		ui->analyzeURL->hide();
		ui->description->setText(
			Str("LogReturnDialog.Description.Crash"));
	}

	installEventFilter(CreateShortcutFilter());
}

void OBSLogReply::on_copyURL_clicked()
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(ui->urlEdit->text());
}

void OBSLogReply::on_analyzeURL_clicked()
{
	QUrlQuery param;
	param.addQueryItem("log_url",
			   QUrl::toPercentEncoding(ui->urlEdit->text()));
	QUrl url("https://obsproject.com/tools/analyzer", QUrl::TolerantMode);
	url.setQuery(param);
	QDesktopServices::openUrl(url);
}
