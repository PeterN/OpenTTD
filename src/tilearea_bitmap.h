/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_bitmap.h Classes for storing and iterating a tile area. */

#ifndef BITMAP_TYPE_HPP
#define BITMAP_TYPE_HPP

#include "tilearea_orthogonal.h"

class BitmapTileIterator;

/** Represents a tile area containing containing individually set tiles.
 * Each tile must be contained within the preallocated area.
 * A std::vector<bool> is used to mark which tiles are contained.
 */
class BitmapTileArea {
private:
	std::vector<bool> data; ///< Storage of tiles contained within the tile area.
	OrthogonalTileArea ta; ///< Underlying tile area.

	/**
	 * Get the internal index for the given tile.
	 * @param tile The tile.
	 * @return The internal index.
	 */
	inline uint Index(TileIndex tile) const
	{
		const auto [x, y] = TileIndexToTileIndexDiffC(tile, this->ta.tile);
		return y * this->ta.w + x;
	}

public:
	using iterator = BitmapTileIterator; ///< The tile iterator type for this tile area type.

	BitmapTileArea() : ta(INVALID_TILE, 0, 0) {}

	explicit BitmapTileArea(const OrthogonalTileArea &ta);

	/**
	 * Reset and clear the BitmapTileArea.
	 */
	void Clear()
	{
		this->ta.Clear();
		this->data.clear();
	}

	bool IsEmpty() const;
	void Initialize(const OrthogonalTileArea &ta);

	/**
	 * Add a tile as part of the tile area.
	 * @param tile Tile to add.
	 */
	inline void Add(TileIndex tile)
	{
		assert(this->ta.Contains(tile));
		this->data[this->Index(tile)] = true;
	}

	/**
	 * Remove a tile from the tile area.
	 * @param tile Tile to clear
	 */
	inline void Remove(TileIndex tile)
	{
		assert(this->ta.Contains(tile));
		this->data[this->Index(tile)] = false;
	}

	/**
	 * Test if a tile is part of the tile area.
	 * @param tile Tile to check.
	 * @return \c true iff the tile is in this area.
	 */
	inline bool Contains(TileIndex tile) const
	{
		return this->ta.Contains(tile) && this->data[this->Index(tile)];
	}

	BitmapTileIterator begin() const;

	/**
	 * Returns an iterator to the end of the tile area.
	 * @return The iterator.
	 */
	std::default_sentinel_t end() const
	{
		return std::default_sentinel_t();
	}

	friend BitmapTileIterator;
};

/** Iterator to iterate over all tiles belonging to a bitmaptilearea. */
class BitmapTileIterator {
private:
	const BitmapTileArea *bitmap; ///< The bitmap tile area being iterated.
	OrthogonalTileIterator iter; ///< The underlying tile area iterator.

public:
	using value_type = TileIndex; ///< value_type iterator trait
	using difference_type = std::ptrdiff_t; ///< difference_type iterator trait
	using iterator_category = std::forward_iterator_tag; ///< iterator_category iterator trait
	using pointer = void; ///< pointer iterator trait
	using reference = void; ///< reference iterator trait

	/**
	 * Construct the iterator.
	 * @param bitmap BitmapTileArea to iterate.
	 */
	BitmapTileIterator(const BitmapTileArea &bitmap) : bitmap(&bitmap), iter(bitmap.ta)
	{
		if (*this->iter != INVALID_TILE && !this->bitmap->Contains(*this->iter)) ++this->iter;
	}

	bool operator==(const BitmapTileIterator &other) const
	{
		return this->bitmap == other.bitmap && this->iter == other.iter;
	}

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
		return *this->iter;
	}

	/**
	 * Increment the iterator and set it to the next position.
	 * @return This iterator after incrementing.
	 */
	inline BitmapTileIterator &operator++()
	{
		do {
			++this->iter;
		} while (*this->iter != INVALID_TILE && !this->bitmap->Contains(*this->iter));
		return *this;
	}
};

/**
 * Returns an iterator to the start of the tile area.
 * @return The BitmapTileIterator.
 */
inline BitmapTileIterator BitmapTileArea::begin() const
{
	return BitmapTileIterator{*this};
}

#endif /* BITMAP_TYPE_HPP */
