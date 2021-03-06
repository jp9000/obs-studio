/******************************************************************************
    Copyright (C) 2015 by Hugh Bailey <obs.jim@gmail.com>

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

#include <QThread>
#include <vector>
#include <string>

class RemoteTextThread : public QThread {
	Q_OBJECT

	std::string url;
	std::string contentType;
	std::string postData;

	std::vector<std::string> extraHeaders;

	int timeoutSec = 0;

	void run() override;

signals:
	void Result(const QString &text, const QString &error);

public:
	inline RemoteTextThread(std::string url_,
				std::string contentType_ = std::string(),
				std::string postData_ = std::string(),
				int timeoutSec_ = 0)
		: url(url_),
		  contentType(contentType_),
		  postData(postData_),
		  timeoutSec(timeoutSec_)
	{
	}

	inline RemoteTextThread(std::string url_,
				std::vector<std::string> &&extraHeaders_,
				std::string contentType_ = std::string(),
				std::string postData_ = std::string(),
				int timeoutSec_ = 0)
		: url(url_),
		  contentType(contentType_),
		  postData(postData_),
		  extraHeaders(std::move(extraHeaders_)),
		  timeoutSec(timeoutSec_)
	{
	}
};

bool GetRemoteFile(
	const char *url, std::string &str, std::string &error,
	long *responseCode = nullptr, const char *contentType = nullptr,
	const char *postData = nullptr,
	std::vector<std::string> extraHeaders = std::vector<std::string>(),
	std::string *signature = nullptr, int timeoutSec = 0);
