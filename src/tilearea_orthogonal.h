/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_orthogonal.h Type for storing and iterating orthogonal tile areas. */

#ifndef TILEAREA_ORTHOGONAL_H
#define TILEAREA_ORTHOGONAL_H

#include "map_func.h"

class OrthogonalTileIterator;

/** Represents the covered area of e.g. a rail station */
class OrthogonalTileArea {
public:
	using iterator = OrthogonalTileIterator; ///< The tile iterator type for this tile area type.

	TileIndex tile; ///< The base tile of the area
	uint16_t w; ///< The width of the area
	uint16_t h; ///< The height of the area

	/**
	 * Construct this tile area with some set values
	 * @param tile the base tile
	 * @param w the width
	 * @param h the height
	 */
	OrthogonalTileArea(TileIndex tile = INVALID_TILE, uint16_t w = 0, uint16_t h = 0) : tile(tile), w(w), h(h) {}

	OrthogonalTileArea(TileIndex start, TileIndex end);

	/**
	 * Test if this tile area is empty.
	 * @return \c true iff the tile area is empty.
	 */
	inline bool IsEmpty() const { return this->tile == INVALID_TILE; }

	void Add(TileIndex to_add);

	/**
	 * Add another tile area to this tile area.
	 * @param area The tile area to add.
	 */
	inline void Add(const OrthogonalTileArea &area)
	{
		/* Only the top and bottom corners need to be added. */
		this->Add(area.tile);
		this->Add(TileAddXY(area.tile, area.w - 1, area.h - 1));
	}

	/**
	 * Clears the 'tile area', i.e. make the tile invalid.
	 */
	void Clear()
	{
		this->tile = INVALID_TILE;
		this->w = 0;
		this->h = 0;
	}

	bool Intersects(const OrthogonalTileArea &ta) const;
	bool Contains(TileIndex tile) const;
	OrthogonalTileArea &Expand(int rad);
	void ClampToMap();

	/**
	 * Get the centre tile.
	 * @return The tile at the centre, or just north of it.
	 */
	TileIndex GetCentreTile() const
	{
		return TileAddXY(this->tile, this->w / 2, this->h / 2);
	}

	OrthogonalTileIterator begin() const;

	/**
	 * Returns an iterator to the end of the tile area.
	 * @return The iterator.
	 */
	std::default_sentinel_t end() const
	{
		return std::default_sentinel_t();
	}
};

/** Iterator to iterate over a tile area (rectangle) of the map. */
class OrthogonalTileIterator {
private:
	TileIndex tile; ///< The current tile.
	int w; ///< The width of the iterated area.
	int x; ///< The current 'x' position in the rectangle.
	int y; ///< The current 'y' position in the rectangle.

public:
	using value_type = TileIndex; ///< value_type iterator trait
	using difference_type = std::ptrdiff_t; ///< difference_type iterator trait
	using iterator_category = std::forward_iterator_tag; ///< iterator_category iterator trait
	using pointer = void; ///< pointer iterator trait
	using reference = void; ///< reference iterator trait

	/**
	 * Construct the iterator.
	 * @param ta Area, i.e. begin point and width/height of to-be-iterated area.
	 */
	explicit OrthogonalTileIterator(const OrthogonalTileArea &ta) : tile(ta.w == 0 || ta.h == 0 ? INVALID_TILE : ta.tile), w(ta.w), x(ta.w), y(ta.h) {}

	bool operator==(const OrthogonalTileIterator &) const = default;

	bool operator==(const std::default_sentinel_t &) const
	{
		return **this == INVALID_TILE;
	}

	/**
	 * Get the current tile.
	 * @return The current tile, or INVALID_TILE when complete.
	 */
	TileIndex operator*() const
	{
		return this->tile;
	}

	/**
	 * Increment the iterator and set it to the next position.
	 * @return This iterator after incrementing.
	 */
	inline OrthogonalTileIterator &operator++()
	{
		assert(this->tile != INVALID_TILE);

		if (--this->x > 0) {
			this->tile++;
		} else if (--this->y > 0) {
			this->x = this->w;
			this->tile += TileDiffXY(1 - this->w, 1);
		} else {
			this->tile = INVALID_TILE;
		}
		return *this;
	}
};

/**
 * Returns an iterator to the start of the tile area.
 * @return The OrthogonalTileIterator.
 */
inline OrthogonalTileIterator OrthogonalTileArea::begin() const
{
	return OrthogonalTileIterator{*this};
}

/** Shorthand for the much more common orthogonal tile area. */
using TileArea = OrthogonalTileArea;

#endif /* TILEAREA_ORTHOGONAL_H */
