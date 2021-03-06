/******************************************************************************
    Copyright (C) 2015 by Ruwen Hahn <palana@stunned.de>

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

#include "source-label.hpp"

void OBSSourceLabel::SourceRenamed(void *data, calldata_t *params)
{
	auto &label = *static_cast<OBSSourceLabel *>(data);

	const char *name = calldata_string(params, "new_name");
	label.setText(name);

	emit label.Renamed(name);
}

void OBSSourceLabel::SourceRemoved(void *data, calldata_t *)
{
	auto &label = *static_cast<OBSSourceLabel *>(data);
	emit label.Removed();
}

void OBSSourceLabel::SourceDestroyed(void *data, calldata_t *)
{
	auto &label = *static_cast<OBSSourceLabel *>(data);
	emit label.Destroyed();

	label.destroyedSignal.Disconnect();
	label.removedSignal.Disconnect();
	label.renamedSignal.Disconnect();
}
