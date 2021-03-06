/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include "util/c99defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *   @file
 *
 *   Modules can specify custom user-interface-specific exports.  UI functions
 * can be within the same library as the actual core logic, or separated in to
 * different modules to split up UI logic and core module logic.
 *
 *   The reasoning for this is to allow for custom user interface of differing
 * toolkits or for automatically generated user interface, or to simply allow
 * separation of UI code from core code (which may often be in differing
 * languages).
 */

/** Modal UI definition structure */
struct obs_modal_ui {
	const char *id;     /**< Identifier associated with this UI */
	const char *task;   /**< Task of the UI */
	const char *target; /**< UI target (UI toolkit or program name) */

	/**
	 * Callback to execute modal interface.
	 *
	 * The @b object variable points to the input/output/encoder/etc.  The
	 * @b ui_data variable points to the UI parent or UI-specific data to
	 * be used with the custom user interface.
	 *
	 * What @b ui_data points to differs depending on the target, and you
	 * should use discretion and consistency when using this variable to
	 * relay information to the UI function.  For example, it would be
	 * ideal to have @b ui_data point to a parent, QWidget for Qt, or a
	 * wxWindow for wxWidgets, etc., though it's up to the discretion of
	 * the developer to define that value.  Because of the nature of void
	 * pointers, discretion and consistency is advised.
	 *
	 * @param  object   Pointer/handle to the data associated with this
	 *                  call.
	 * @param  ui_data  UI data to pass associated with this specific
	 *                  target, if any.
	 * @return          @b true if user completed the task, or
	 *                  @b false if user cancelled the task.
	 */
	bool (*exec)(void *object, void *ui_data);

	void *type_data;
	void (*free_type_data)(void *type_data);
};

/**
 * Registers a modal UI definition to the current obs context.  This should be
 * used in obs_module_load.
 *
 * @param  info  Pointer to the modal definition structure
 */
EXPORT void obs_register_modal_ui(const struct obs_modal_ui *info);

/* ------------------------------------------------------------------------- */

/** Modeless UI definition structure */
struct obs_modeless_ui {
	const char *id;     /**< Identifier associated with this UI */
	const char *task;   /**< Task of the UI */
	const char *target; /**< UI target (UI toolkit or program name) */

	/**
	 * Callback to create modeless interface.
	 *
	 * This function is almost identical to the modal exec function,
	 * except modeless UI calls return immediately, and typically are
	 * supposed to return a pointer or handle to the specific UI object
	 * that was created.  For example, a Qt object would ideally return a
	 * pointer to a QWidget.  Again, discretion and consistency is advised
	 * for the return value.
	 *
	 * @param   object  Pointer/handle to the data associated with this
	 *                  call.
	 * @param  ui_data  UI data to pass associated with this specific
	 *                  target, if any.
	 * @return          Pointer/handle to the modeless UI associated with
	 *                  the specific target.
	 */
	void *(*create)(void *object, void *ui_data);

	void *type_data;
	void (*free_type_data)(void *type_data);
};

/**
 * Registers a modeless UI definition to the current obs context.  This should
 * be used in obs_module_load.
 *
 * @param  info  Pointer to the modal definition structure
 */
EXPORT void obs_register_modeless_ui(const struct obs_modeless_ui *info);

/* ------------------------------------------------------------------------- */

#define OBS_UI_SUCCESS 0
#define OBS_UI_CANCEL -1
#define OBS_UI_NOTFOUND -2

/**
 * Requests modal UI to be displayed.  Returns when user is complete.
 *
 * @param    name  Name of the input/output/etc type that UI was requested for
 * @param    task  Task of the user interface (usually "config")
 * @param  target  Desired target (i.e. "qt", "wx", "gtk3", "win32", etc)
 * @param    data  Pointer to the obs input/output/etc
 * @param ui_data  UI-specific data, usually a parent pointer/handle (if any)
 *
 * @return         OBS_UI_SUCCESS if the UI was successful,
 *                 OBS_UI_CANCEL if the UI was cancelled by the user, or
 *                 OBS_UI_NOTFOUND if the UI callback was not found
 */
EXPORT int obs_exec_ui(const char *id, const char *task, const char *target,
		       void *data, void *ui_data);

/**
 * Requests modeless UI to be created.  Returns immediately.
 *
 * @param    name  Name of the input/output/etc type that UI was requested for
 * @param    task  Task of the user interface
 * @param  target  Desired target (i.e. "qt", "wx", "gtk3", "win32", etc)
 * @param    data  Pointer to the obs input/output/etc
 * @param ui_data  UI-specific data, usually a parent pointer/handle (if any)
 *
 * @return         Pointer/handle to the target-specific modeless object, or
 *                 NULL if not found or failed.
 */
EXPORT void *obs_create_ui(const char *id, const char *task, const char *target,
			   void *data, void *ui_data);

#ifdef __cplusplus
}
#endif
