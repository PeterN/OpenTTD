/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_roadsubtypelist.cpp Implementation of ScriptRoadSubTypeList and friends. */

#include "../../stdafx.h"
#include "script_roadsubtypelist.hpp"
#include "../../road_func.h"

#include "../../safeguards.h"

ScriptRoadSubTypeList::ScriptRoadSubTypeList(ScriptRoad::RoadType road_type)
{
	RoadTypeIdentifier rtid((::RoadType)road_type, ::INVALID_ROADSUBTYPE);
	for (RoadSubType rst = ROADSUBTYPE_BEGIN; rst != ROADSUBTYPE_END; rst++) {
		rtid.subtype = rst;
		if (ScriptObject::GetCompany() == OWNER_DEITY || ::HasRoadTypeAvail(ScriptObject::GetCompany(), rtid)) this->AddItem(rst);
	}
}
