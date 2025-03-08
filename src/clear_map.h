/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file clear_map.h Map accessors for 'clear' tiles */

#ifndef CLEAR_MAP_H
#define CLEAR_MAP_H

#include "bridge_map.h"
#include "industry_type.h"

enum class GroundType : uint8_t {
	Rough = 0,
	Rocks = 1,
	Fields = 2,
	Snow = 3,
	Desert = 4,
	End,
};

using GroundTypes = EnumBitSet<GroundType, uint8_t, GroundType::End>;

/**
 * Get the ground types of clear tile.
 * @param t the tile to get the clear ground types of
 * @pre IsTileType(t, MP_CLEAR)
 * @return the ground types
 */
inline GroundTypes GetClearGroundTypes(Tile t)
{
	assert(IsTileType(t, MP_CLEAR));
	return static_cast<GroundTypes>(t.m7());
}

inline void SetClearGroundType(Tile t, GroundType groundtype, bool set)
{
	t.m7() = static_cast<GroundTypes>(t.m7()).Set(groundtype, set).base();
}

/**
 * Test if a tile is covered with snow.
 * @param t the tile to check
 * @pre IsTileType(t, MP_CLEAR)
 * @return whether the tile is covered with snow.
 */
 inline bool IsSnowTile(Tile t)
 {
	 return GetClearGroundTypes(t).Test(GroundType::Snow);
 }

/**
 * Get the density of a non-field clear tile.
 * @param t the tile to get the density of
 * @pre IsTileType(t, MP_CLEAR)
 * @return the density
 */
inline uint GetClearDensity(Tile t)
{
	assert(IsTileType(t, MP_CLEAR));
	return GB(t.m5(), 0, 2);
}

/**
 * Increment the density of a non-field clear tile.
 * @param t the tile to increment the density of
 * @param d the amount to increment the density with
 * @pre IsTileType(t, MP_CLEAR)
 */
inline void AddClearDensity(Tile t, int d)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	t.m5() += d;
}

/**
 * Set the density of a non-field clear tile.
 * @param t the tile to set the density of
 * @param d the new density
 * @pre IsTileType(t, MP_CLEAR)
 */
inline void SetClearDensity(Tile t, uint d)
{
	assert(IsTileType(t, MP_CLEAR));
	SB(t.m5(), 0, 2, d);
}


/**
 * Get the counter used to advance to the next clear density/field type.
 * @param t the tile to get the counter of
 * @pre IsTileType(t, MP_CLEAR)
 * @return the value of the counter
 */
inline uint GetClearCounter(Tile t)
{
	assert(IsTileType(t, MP_CLEAR));
	return GB(t.m5(), 5, 3);
}

/**
 * Increments the counter used to advance to the next clear density/field type.
 * @param t the tile to increment the counter of
 * @param c the amount to increment the counter with
 * @pre IsTileType(t, MP_CLEAR)
 */
inline void AddClearCounter(Tile t, int c)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	t.m5() += c << 5;
}

/**
 * Sets the counter used to advance to the next clear density/field type.
 * @param t the tile to set the counter of
 * @param c the amount to set the counter to
 * @pre IsTileType(t, MP_CLEAR)
 */
inline void SetClearCounter(Tile t, uint c)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	SB(t.m5(), 5, 3, c);
}

/**
 * Get the field type (production stage) of the field
 * @param t the field to get the type of
 * @pre GetClearGround(t) == CLEAR_FIELDS
 * @return the field type
 */
inline uint GetFieldType(Tile t)
{
	assert(GetClearGroundTypes(t).Test(GroundType::Fields));
	return GB(t.m3(), 0, 4);
}

/**
 * Set the field type (production stage) of the field
 * @param t the field to get the type of
 * @param f the field type
 * @pre GetClearGround(t) == CLEAR_FIELDS
 */
inline void SetFieldType(Tile t, uint f)
{
	assert(GetClearGroundTypes(t).Test(GroundType::Fields));
	SB(t.m3(), 0, 4, f);
}

/**
 * Get the industry (farm) that made the field
 * @param t the field to get creating industry of
 * @pre GetClearGround(t) == CLEAR_FIELDS
 * @return the industry that made the field
 */
inline IndustryID GetIndustryIndexOfField(Tile t)
{
	assert(GetClearGroundTypes(t).Test(GroundType::Fields));
	return(IndustryID) t.m2();
}

/**
 * Set the industry (farm) that made the field
 * @param t the field to get creating industry of
 * @param i the industry that made the field
 * @pre GetClearGround(t) == CLEAR_FIELDS
 */
inline void SetIndustryIndexOfField(Tile t, IndustryID i)
{
	assert(GetClearGroundTypes(t).Test(GroundType::Fields));
	t.m2() = i.base();
}


/**
 * Is there a fence at the given border?
 * @param t the tile to check for fences
 * @param side the border to check
 * @pre IsClearGround(t, CLEAR_FIELDS)
 * @return 0 if there is no fence, otherwise the fence type
 */
inline uint GetFence(Tile t, DiagDirection side)
{
	assert(GetClearGroundTypes(t).Test(GroundType::Fields));
	switch (side) {
		default: NOT_REACHED();
		case DIAGDIR_SE: return GB(t.m4(), 2, 3);
		case DIAGDIR_SW: return GB(t.m4(), 5, 3);
		case DIAGDIR_NE: return GB(t.m3(), 5, 3);
		case DIAGDIR_NW: return GB(t.m6(), 2, 3);
	}
}

/**
 * Sets the type of fence (and whether there is one) for the given border.
 * @param t the tile to check for fences
 * @param side the border to check
 * @param h 0 if there is no fence, otherwise the fence type
 * @pre IsClearGround(t, CLEAR_FIELDS)
 */
inline void SetFence(Tile t, DiagDirection side, uint h)
{
	assert(GetClearGroundTypes(t).Test(GroundType::Fields));
	switch (side) {
		default: NOT_REACHED();
		case DIAGDIR_SE: SB(t.m4(), 2, 3, h); break;
		case DIAGDIR_SW: SB(t.m4(), 5, 3, h); break;
		case DIAGDIR_NE: SB(t.m3(), 5, 3, h); break;
		case DIAGDIR_NW: SB(t.m6(), 2, 3, h); break;
	}
}


/**
 * Make a clear tile.
 * @param t       the tile to make a clear tile
 * @param g       the type of ground
 * @param density the density of the grass/snow/desert etc
 */
inline void MakeClear(Tile t, GroundTypes g, uint density)
{
	SetTileType(t, MP_CLEAR);
	t.m1() = 0;
	SetTileOwner(t, OWNER_NONE);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0 << 5 | 0 << 2;
	SetClearDensity(t, density);
	t.m6() = 0;
	t.m7() = g.base();
	t.m8() = 0;
}


/**
 * Make a (farm) field tile.
 * @param t          the tile to make a farm field
 * @param field_type the 'growth' level of the field
 * @param industry   the industry this tile belongs to
 */
inline void MakeField(Tile t, uint field_type, IndustryID industry)
{
	SetTileType(t, MP_CLEAR);
	t.m1() = 0;
	SetTileOwner(t, OWNER_NONE);
	t.m2() = industry.base();
	t.m3() = field_type;
	t.m4() = 0 << 5 | 0 << 2;
	SetClearDensity(t, 3);
	SB(t.m6(), 2, 4, 0);
	t.m7() = GroundTypes{GroundType::Fields}.base();
	t.m8() = 0;
}

/**
 * Make a snow tile.
 * @param t the tile to make snowy
 * @param density The density of snowiness.
 * @pre !IsSnowTile(t)
 */
inline void MakeSnow(Tile t, uint density = 0)
{
	assert(!IsSnowTile(t));
	static_cast<GroundTypes>(t.m7()).Set(GroundType::Snow);
	SetClearDensity(t, density);
}

/**
 * Clear the snow from a tile and return it to its previous type.
 * @param t the tile to clear of snow
 * @pre IsSnowTile(t)
 */
inline void ClearSnow(Tile t)
{
	assert(IsSnowTile(t));
	static_cast<GroundTypes>(t.m7()).Reset(GroundType::Snow);
	SetClearDensity(t, 3);
}

#endif /* CLEAR_MAP_H */
