/******************************************************************************
    Copyright (C) 2019-2020 by Dillon Pentz <dillon@vodbox.io>

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

#include "obs.hpp"
#include "json11.hpp"
#include <util/platform.h>
#include <util/util.hpp>
#include <string>
#include <vector>

enum obs_importer_responses {
	IMPORTER_SUCCESS,
	IMPORTER_FILE_NOT_FOUND,
	IMPORTER_FILE_NOT_RECOGNISED,
	IMPORTER_FILE_WONT_OPEN,
	IMPORTER_ERROR_DURING_CONVERSION,
	IMPORTER_UNKNOWN_ERROR,
	IMPORTER_NOT_FOUND
};

typedef std::vector<std::string> OBSImporterFiles;

class Importer {
public:
	virtual ~Importer() {}
	virtual std::string Prog() { return "Null"; };
	virtual int ImportScenes(const std::string &path, std::string &name,
				 json11::Json &res) = 0;
	virtual bool Check(const std::string &path) = 0;
	virtual std::string Name(const std::string &path) = 0;
	virtual OBSImporterFiles FindFiles()
	{
		OBSImporterFiles f;
		return f;
	};
};

class ClassicImporter : public Importer {
public:
	std::string Prog() { return "OBSClassic"; };
	int ImportScenes(const std::string &path, std::string &name,
			 json11::Json &res);
	bool Check(const std::string &path);
	std::string Name(const std::string &path);
	OBSImporterFiles FindFiles();
};

class StudioImporter : public Importer {
public:
	std::string Prog() { return "OBSStudio"; };
	int ImportScenes(const std::string &path, std::string &name,
			 json11::Json &res);
	bool Check(const std::string &path);
	std::string Name(const std::string &path);
};

class SLImporter : public Importer {
public:
	std::string Prog() { return "Streamlabs"; };
	int ImportScenes(const std::string &path, std::string &name,
			 json11::Json &res);
	bool Check(const std::string &path);
	std::string Name(const std::string &path);
	OBSImporterFiles FindFiles();
};

class XSplitImporter : public Importer {
public:
	std::string Prog() { return "XSplitBroadcaster"; };
	int ImportScenes(const std::string &path, std::string &name,
			 json11::Json &res);
	bool Check(const std::string &path);
	std::string Name(const std::string &path)
	{
		return "XSplit Import";
		UNUSED_PARAMETER(path);
	};
	OBSImporterFiles FindFiles();
};

void ImportersInit();

std::string DetectProgram(const std::string &path);
std::string GetSCName(const std::string &path, const std::string &prog);

int ImportSCFromProg(const std::string &path, std::string &name,
		     const std::string &program, json11::Json &res);
int ImportSC(const std::string &path, std::string &name, json11::Json &res);

OBSImporterFiles ImportersFindFiles();

void TranslateOSStudio(json11::Json &data);

static inline std::string GetFilenameFromPath(const std::string &path)
{
#ifdef _WIN32
	size_t pos = path.find_last_of('\\');
	if (pos == -1 || pos < path.find_last_of('/'))
		pos = path.find_last_of('/');
#else
	size_t pos = path.find_last_of('/');
#endif
	size_t ext = path.find_last_of('.');

	if (ext < pos) {
		return path.substr(pos + 1);
	} else {
		return path.substr(pos + 1, ext - pos - 1);
	}
}

static inline std::string GetFolderFromPath(const std::string &path)
{
#ifdef _WIN32
	size_t pos = path.find_last_of('\\');
	if (pos == -1 || pos < path.find_last_of('/'))
		pos = path.find_last_of('/');
#else
	size_t pos = path.find_last_of('/');
#endif
	return path.substr(0, pos + 1);
}

static inline std::string StringReplace(const std::string &in,
					const std::string &search,
					const std::string &rep)
{
	std::string res = in;
	size_t pos;

	while ((pos = res.find(search)) != std::string::npos) {
		res.replace(pos, search.length(), rep);
	}

	return res;
}

static inline std::string ReadLine(std::string &str)
{
	str = StringReplace(str, "\r\n", "\n");

	size_t pos = str.find('\n');

	if (pos == std::string::npos)
		pos = str.find(EOF);

	if (pos == std::string::npos)
		pos = str.find('\0');

	if (pos == std::string::npos)
		return "";

	std::string res = str.substr(0, pos);
	str = str.substr(pos + 1);

	return res;
}
