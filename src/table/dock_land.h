/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dock_land.h Sprites to use and how to display them for dock tiles. */

#define M(name, size, build_cost_multiplier, clear_cost_multiplier, height, climate, flags) { GRFFilePropsBase<2>(), INVALID_DOCK_CLASS, name, climate, size, build_cost_multiplier, clear_cost_multiplier, 0, MAX_DAY + 1, flags, {0, 0, 0, 0}, 0, height, 1, true }

/* Climates
 * T = Temperate
 * A = Sub-Arctic
 * S = Sub-Tropic
 * Y = Toyland */
#define T 1
#define A 2
#define S 4
#define Y 8
/** Specification of the original object structures. */
extern const DockSpec _original_docks[] = {
	M(STR_DOCK_CLASS_DOCK, 0x12, 1, 1, 5, T|A|S|Y, DOCK_FLAG_SLOPE_NW),
	M(STR_DOCK_CLASS_DOCK, 0x21, 1, 1, 5, T|A|S|Y, DOCK_FLAG_SLOPE_NE),
	M(STR_DOCK_CLASS_DOCK, 0x12, 1, 1, 5, T|A|S|Y, DOCK_FLAG_SLOPE_SE),
	M(STR_DOCK_CLASS_DOCK, 0x21, 1, 1, 5, T|A|S|Y, DOCK_FLAG_SLOPE_SW),
	M(STR_DOCK_CLASS_DOCK, 0x11, 1, 1, 5, T|A|S|Y, DOCK_FLAG_DRAW_WATER | DOCK_FLAG_NOT_ON_LAND),
	M(STR_DOCK_CLASS_DOCK, 0x11, 1, 1, 5, T|A|S|Y, DOCK_FLAG_DRAW_WATER | DOCK_FLAG_NOT_ON_LAND),
};

#undef M
#undef Y
#undef S
#undef A
#undef T
