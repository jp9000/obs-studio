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

/* This file exists to prevent ->deleteLater from being called on custom-made
 * item widgets in widgets such as list widgets.  We do this to prevent things
 * such as references to sources/etc from getting stuck in the Qt event queue
 * with no way of controlling when they'll be released. */

class QListWidget;
class QListWidgetItem;

QListWidgetItem *TakeListItem(QListWidget *widget, int row);
void DeleteListItem(QListWidget *widget, QListWidgetItem *item);
void ClearListItems(QListWidget *widget);

template<typename QObjectPtr>
void InsertQObjectByName(std::vector<QObjectPtr> &controls, QObjectPtr control)
{
	QString name = control->objectName();
	auto finder = [name](QObjectPtr elem) {
		return name.localeAwareCompare(elem->objectName()) < 0;
	};
	auto found_at = std::find_if(controls.begin(), controls.end(), finder);

	controls.insert(found_at, control);
}
