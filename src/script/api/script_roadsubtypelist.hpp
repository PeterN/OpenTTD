/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_roadsubtypelist.hpp List all available roadsubtypes. */

#ifndef SCRIPT_ROADSUBTYPELIST_HPP
#define SCRIPT_ROADSUBTYPELIST_HPP

#include "script_list.hpp"
#include "script_road.hpp"

/**
 * Creates a list of all available roadsubtypes.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptRoadSubTypeList : public ScriptList {
public:
	ScriptRoadSubTypeList(ScriptRoad::RoadType road_type);
};

#endif /* SCRIPT_ROADSUBTYPELIST_HPP */
