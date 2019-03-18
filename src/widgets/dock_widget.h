/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dock_widget.h Types related to the dock widgets. */

#ifndef WIDGETS_DOCK_WIDGET_H
#define WIDGETS_DOCK_WIDGET_H

/** Widgets of the #BuildDocksDepotWindow class. */
enum BuildDockDepotWidgets {
	WID_BDD_BACKGROUND, ///< Background of the window.
	WID_BDD_X,          ///< X-direction button.
	WID_BDD_Y,          ///< Y-direction button.
};

/** Widgets of the #BuildDocksToolbarWindow class. */
enum DockToolbarWidgets {
	WID_DT_CANAL,          ///< Build canal button.
	WID_DT_LOCK,           ///< Build lock button.
	WID_DT_DEMOLISH,       ///< Demolish aka dynamite button.
	WID_DT_DEPOT,          ///< Build depot button.
	WID_DT_STATION,        ///< Build station button.
	WID_DT_BUOY,           ///< Build buoy button.
	WID_DT_RIVER,          ///< Build river button (in scenario editor).
	WID_DT_BUILD_AQUEDUCT, ///< Build aqueduct button.

	WID_DT_INVALID,        ///< Used to initialize a variable.
};

/** Widgets of the #BuildDockWindow class. */
enum BuildDockWidgets {
	WID_BD_CLASS_LIST,     ///< The list with classes.
	WID_BD_SCROLLBAR,      ///< The scrollbar associated with the list.
	WID_BD_DOCK_MATRIX,    ///< The matrix with preview sprites.
	WID_BD_DOCK_SPRITE,    ///< A preview sprite of the dock.
	WID_BD_DOCK_NAME,      ///< The name of the selected dock.
	WID_BD_DOCK_SIZE,      ///< The size of the selected dock.
	WID_BD_INFO,           ///< Other information about the dock (from the NewGRF).

	WID_BD_SELECT_MATRIX,  ///< Selection preview matrix of docks of a given class.
	WID_BD_SELECT_IMAGE,   ///< Preview image in the #WID_BD_SELECT_MATRIX.
	WID_BD_SELECT_SCROLL,  ///< Scrollbar next to the #WID_BD_SELECT_MATRIX.
};

#endif /* WIDGETS_DOCK_WIDGET_H */
