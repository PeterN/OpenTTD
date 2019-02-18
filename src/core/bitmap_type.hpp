/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bitmap_type.hpp Bitmap functions. */

#ifndef BITMAP_TYPE_HPP
#define BITMAP_TYPE_HPP

#include "smallvec_type.hpp"

class Bitmap : public SmallVector<uint8, 8> {
public:
	inline void Resize(uint size)
	{
		this->SmallVector::Resize((size + 7) / 8);
		MemSetT(this->data, 0, this->capacity);
	}

	inline void SetBit(uint index)
	{
		::SetBit(this->data[index / 8], index & 7);
	}

	inline void ClrBit(uint index)
	{
		::ClrBit(this->data[index / 8], index & 7);
	}

	inline bool HasBit(uint index) const
	{
		return ::HasBit(this->data[index / 8], index & 7);
	}
};

class BitmapTileArea : Bitmap, public TileArea {
protected:
	inline uint Index(uint x, uint y) const { return y * this->w + x; }

	inline uint Index(TileIndex tile) const { return Index(TileX(tile) - TileX(this->tile), TileY(tile) - TileY(this->tile)); }

public:
	BitmapTileArea()
	{
		this->tile = INVALID_TILE;
		this->w = 0;
		this->h = 0;
	}

	void Reset()
	{
		this->tile = INVALID_TILE;
		this->w = 0;
		this->h = 0;
		this->Bitmap::Reset();
	}

	void Initialize(Rect r)
	{
		this->tile = TileXY(r.left, r.top);
		this->w = r.right - r.left + 1;
		this->h = r.bottom - r.top + 1;
		this->Resize(Index(w, h));
	}

	inline bool Contains(TileIndex tile) const
	{
		return this->TileArea::Contains(tile);
	}

	inline void SetTile(TileIndex tile)
	{
		assert(this->Contains(tile));
		this->SetBit(Index(tile));
	}

	inline void ClrTile(TileIndex tile)
	{
		assert(this->Contains(tile));
		this->ClrBit(Index(tile));
	}

	inline bool HasTile(TileIndex tile) const
	{
		return this->Contains(tile) && this->HasBit(Index(tile));
	}
};

/** Iterator to iterate over all tiles belonging to a bitmaptilearea. */
class BitmapTileIterator : public OrthogonalTileIterator {
protected:
	const BitmapTileArea *bitmap;
public:
	BitmapTileIterator(const BitmapTileArea &bitmap) : OrthogonalTileIterator(bitmap), bitmap(&bitmap)
	{
		if (!this->bitmap->HasTile(TileIndex(this->tile))) ++(*this);
	}

	inline TileIterator& operator ++()
	{
		(*this).OrthogonalTileIterator::operator++();
		while (this->tile != INVALID_TILE && !this->bitmap->HasTile(TileIndex(this->tile))) {
			(*this).OrthogonalTileIterator::operator++();
		}
		return *this;
	}

	virtual TileIterator *Clone() const
	{
		return new BitmapTileIterator(*this);
	}
};

#endif /* BITMAP_TYPE_HPP */
