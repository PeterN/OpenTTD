/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dock_type.h Types related to dock tiles. */

#ifndef DOCK_TYPE_H
#define DOCK_TYPE_H

/** Types of docks. */
typedef uint16 DockType;

static const DockType DOCK_ORIGINAL     =   0;    ///< The original dock

static const DockType NUM_DOCKS_PER_GRF = 255;    ///< Number of supported docks per NewGRF; limited to 255 to allow extending Action3 with an extended byte later on.

static const DockType NEW_DOCK_OFFSET   =   1;    ///< Offset for new docks
static const DockType NUM_DOCKS         = 64000;  ///< Number of supported docks overall
static const DockType INVALID_DOCK_TYPE = 0xFFFF; ///< An invalid dock

struct DockSpec;

#endif /* DOCK_TYPE_H */
