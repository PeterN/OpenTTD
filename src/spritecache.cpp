/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritecache.cpp Caching of sprites. */

#include "stdafx.h"
#include "random_access_file_type.h"
#include "spriteloader/grf.hpp"
#include "gfx_func.h"
#include "error.h"
#include "error_func.h"
#include "zoom_func.h"
#include "settings_type.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"
#include "core/mem_func.hpp"
#include "video/video_driver.hpp"
#include "spritecache.h"
#include "spritecache_internal.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/palette_convert.h"

#include "safeguards.h"

/* Default of 4MB spritecache */
uint _sprite_cache_size = 4;

static std::vector<SpriteCache> _spritecache;
static size_t _spritecache_bytes_used = 0;
static uint32_t _sprite_lru_counter;
static std::vector<std::unique_ptr<SpriteFile>> _sprite_files;

static inline SpriteCache *GetSpriteCache(size_t index)
{
	return &_spritecache[index];
}

SpriteCache *AllocateSpriteCache(size_t index)
{
	if (index >= _spritecache.size()) {
		_spritecache.resize(index + 1);
	}

	return GetSpriteCache(index);
}

/**
 * Get the cached SpriteFile given the name of the file.
 * @param filename The name of the file at the disk.
 * @return The SpriteFile or \c null.
 */
static SpriteFile *GetCachedSpriteFileByName(const std::string &filename)
{
	for (auto &f : _sprite_files) {
		if (f->GetFilename() == filename) {
			return f.get();
		}
	}
	return nullptr;
}

/**
 * Open/get the SpriteFile that is cached for use in the sprite cache.
 * @param filename      Name of the file at the disk.
 * @param subdir        The sub directory to search this file in.
 * @param palette_remap Whether a palette remap needs to be performed for this file.
 * @return The reference to the SpriteCache.
 */
SpriteFile &OpenCachedSpriteFile(const std::string &filename, Subdirectory subdir, bool palette_remap)
{
	SpriteFile *file = GetCachedSpriteFileByName(filename);
	if (file == nullptr) {
		file = _sprite_files.insert(std::end(_sprite_files), std::make_unique<SpriteFile>(filename, subdir, palette_remap))->get();
	} else {
		file->SeekToBegin();
	}
	return *file;
}

/**
 * Skip the given amount of sprite graphics data.
 * @param type the type of sprite (compressed etc)
 * @param num the amount of sprites to skip
 * @return true if the data could be correctly skipped.
 */
bool SkipSpriteData(SpriteFile &file, byte type, uint16_t num)
{
	if (type & 2) {
		file.SkipBytes(num);
	} else {
		while (num > 0) {
			int8_t i = file.ReadByte();
			if (i >= 0) {
				int size = (i == 0) ? 0x80 : i;
				if (size > num) return false;
				num -= size;
				file.SkipBytes(size);
			} else {
				i = -(i >> 3);
				num -= i;
				file.ReadByte();
			}
		}
	}
	return true;
}

/* Check if the given Sprite ID exists */
bool SpriteExists(SpriteID id)
{
	if (static_cast<size_t>(id) >= _spritecache.size()) return false;

	/* Special case for Sprite ID zero -- its position is also 0... */
	if (id == 0) return true;
	return !(GetSpriteCache(id)->file_pos == 0 && GetSpriteCache(id)->file == nullptr);
}

/**
 * Get the sprite type of a given sprite.
 * @param sprite The sprite to look at.
 * @return the type of sprite.
 */
SpriteType GetSpriteType(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return SpriteType::Invalid;
	return GetSpriteCache(sprite)->type;
}

/**
 * Get the SpriteFile of a given sprite.
 * @param sprite The sprite to look at.
 * @return The SpriteFile.
 */
SpriteFile *GetOriginFile(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return nullptr;
	return GetSpriteCache(sprite)->file;
}

/**
 * Get the GRF-local sprite id of a given sprite.
 * @param sprite The sprite to look at.
 * @return The GRF-local sprite id.
 */
uint32_t GetSpriteLocalID(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return 0;
	return GetSpriteCache(sprite)->id;
}

/**
 * Count the sprites which originate from a specific file in a range of SpriteIDs.
 * @param file The loaded SpriteFile.
 * @param begin First sprite in range.
 * @param end First sprite not in range.
 * @return Number of sprites.
 */
uint GetSpriteCountForFile(const std::string &filename, SpriteID begin, SpriteID end)
{
	SpriteFile *file = GetCachedSpriteFileByName(filename);
	if (file == nullptr) return 0;

	uint count = 0;
	for (SpriteID i = begin; i != end; i++) {
		if (SpriteExists(i)) {
			SpriteCache *sc = GetSpriteCache(i);
			if (sc->file == file) {
				count++;
				Debug(sprite, 4, "Sprite: {}", i);
			}
		}
	}
	return count;
}

/**
 * Get a reasonable (upper bound) estimate of the maximum
 * SpriteID used in OpenTTD; there will be no sprites with
 * a higher SpriteID, although there might be up to roughly
 * a thousand unused SpriteIDs below this number.
 * @note It's actually the number of spritecache items.
 * @return maximum SpriteID
 */
uint GetMaxSpriteID()
{
	return (uint)_spritecache.size();
}

static bool ResizeSpriteIn(SpriteLoader::SpriteCollection &spritecollection, float src, float tgt)
{
	const auto &source = spritecollection[src];
	auto &target = spritecollection[tgt];

	target.width = ceil(source.width * tgt / src);
	target.height = ceil(source.height * tgt / src);
	target.x_offs = ceil(source.x_offs * tgt / src);
	target.y_offs = ceil(source.y_offs * tgt / src);
	target.colours = source.colours;

	/* Check for possible memory overflow. */
	if (target.width > UINT16_MAX || target.height > UINT16_MAX) {
		spritecollection.erase(tgt);
		return false;
	}

	target.AllocateData(tgt, static_cast<size_t>(target.width) * target.height);

	auto *dst = target.data;
	[[maybe_unused]] const auto *src_end = source.data + source.height * source.width;
	for (int y = 0; y < target.height; y++) {
		const auto *src_ln = source.data + static_cast<int>(((y + target.y_offs) * src / tgt) - source.y_offs) * source.width;
		if (src_ln >= src_end) src_ln -= source.width;
		for (int x = 0; x < target.width; x++) {
			auto *src_px = src_ln + static_cast<int>(((x + target.x_offs) * src / tgt) - source.x_offs);
			if (src_px >= src_ln + source.width) --src_px;
			*dst = *src_px;
			dst++;
		}
	}

	return true;
}

static bool ResizeSpriteOut(SpriteLoader::SpriteCollection &spritecollection, float scale)
{
	/* Algorithm based on 32bpp_Optimized::ResizeSprite() */
	auto it = spritecollection.find(scale * 2);
	if (it == std::end(spritecollection)) return false;
	const auto &source = it->second;

	auto &target = spritecollection[scale];
	target.width = ceil(source.width * scale / it->first);
	target.height = ceil(source.height * scale / it->first);
	target.x_offs = ceil(source.x_offs * scale / it->first);
	target.y_offs = ceil(source.y_offs * scale / it->first);
	target.colours = source.colours;

	target.AllocateData(scale, static_cast<size_t>(target.height) * target.width);

	SpriteLoader::CommonPixel *dst = target.data;
	const SpriteLoader::CommonPixel *src = source.data;
	[[maybe_unused]] const SpriteLoader::CommonPixel *src_end = src + source.height * source.width;

	for (uint y = 0; y < target.height; y++) {
		const SpriteLoader::CommonPixel *src_ln = src + source.width;
		assert(src_ln <= src_end);
		for (uint x = 0; x < target.width; x++) {
			assert(src < src_ln);
			if (src + 1 != src_ln && (src + 1)->a != 0) {
				*dst = *(src + 1);
			} else {
				*dst = *src;
			}
			dst++;
			src += 2;
		}
		src = src_ln + source.width;
	}
	assert(dst == target.data + target.width * target.height);
	return true;
}

static bool PadSingleSprite(SpriteLoader::Sprite *sprite, float scale, uint pad_left, uint pad_top, uint pad_right, uint pad_bottom)
{
	uint width  = sprite->width + pad_left + pad_right;
	uint height = sprite->height + pad_top + pad_bottom;

	if (width > UINT16_MAX || height > UINT16_MAX) return false;

	/* Copy source data and reallocate sprite memory. */
	size_t sprite_size = static_cast<size_t>(sprite->width) * sprite->height;
	static std::vector<SpriteLoader::CommonPixel> src_data;
	src_data.resize(sprite_size);
	MemCpyT(src_data.data(), sprite->data, sprite_size);
	sprite->AllocateData(scale, static_cast<size_t>(width) * height);

	/* Copy with padding to destination. */
	SpriteLoader::CommonPixel *src = src_data.data();
	SpriteLoader::CommonPixel *data = sprite->data;
	for (uint y = 0; y < height; y++) {
		if (y < pad_top || pad_bottom + y >= height) {
			/* Top/bottom padding. */
			MemSetT(data, 0, width);
			data += width;
		} else {
			if (pad_left > 0) {
				/* Pad left. */
				MemSetT(data, 0, pad_left);
				data += pad_left;
			}

			/* Copy pixels. */
			MemCpyT(data, src, sprite->width);
			src += sprite->width;
			data += sprite->width;

			if (pad_right > 0) {
				/* Pad right. */
				MemSetT(data, 0, pad_right);
				data += pad_right;
			}
		}
	}

	/* Update sprite size. */
	sprite->width   = width;
	sprite->height  = height;
	sprite->x_offs -= pad_left;
	sprite->y_offs -= pad_top;

	return true;
}

static bool PadSprites(SpriteLoader::SpriteCollection &spritecollection, SpriteEncoder *encoder)
{
	/* Get minimum top left corner coordinates. */
	int min_xoffs = INT32_MAX;
	int min_yoffs = INT32_MAX;
	for (auto &pair : spritecollection) {
		auto &s = pair.second;
		min_xoffs = std::min(min_xoffs, ScaleFraction(pair.first, s.x_offs));
		min_yoffs = std::min(min_yoffs, ScaleFraction(pair.first, s.y_offs));
	}

	/* Get maximum dimensions taking necessary padding at the top left into account. */
	int max_width  = INT32_MIN;
	int max_height = INT32_MIN;
	for (auto &pair : spritecollection) {
		auto &s = pair.second;
		max_width  = std::max(max_width, ScaleFraction(pair.first, s.width + s.x_offs - UnScaleFraction(pair.first, min_xoffs)));
		max_height = std::max(max_height, ScaleFraction(pair.first, s.height + s.y_offs - UnScaleFraction(pair.first, min_yoffs)));
	}

	/* Align height and width if required to match the needs of the sprite encoder. */
	uint align = encoder->GetSpriteAlignment();
	if (align != 0) {
		max_width  = Align(max_width,  align);
		max_height = Align(max_height, align);
	}

	/* Pad sprites where needed. */
	for (auto &pair : spritecollection) {
		auto &s = pair.second;
		/* Scaling the sprite dimensions in the blitter is done with rounding up,
		 * so a negative padding here is not an error. */
		int pad_left   = std::max(0, s.x_offs - UnScaleFraction(pair.first, min_xoffs));
		int pad_top    = std::max(0, s.y_offs - UnScaleFraction(pair.first, min_yoffs));
		int pad_right  = std::max(0, UnScaleFraction(pair.first, max_width) - s.width - pad_left);
		int pad_bottom = std::max(0, UnScaleFraction(pair.first, max_height) - s.height - pad_top);

		if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0) {
			if (!PadSingleSprite(&s, pair.first, pad_left, pad_top, pad_right, pad_bottom)) return false;
		}
	}

	return true;
}

static bool ResizeSprites(SpriteLoader::SpriteCollection &spritecollection, SpriteEncoder *encoder)
{
	auto first = spritecollection.begin();
	/* Create a fully zoomed image if it does not exist */
	float first_avail = first->first;
	if (first_avail < ZoomLevelToFraction(ZOOM_LVL_NORMAL)) {
	 	if (!ResizeSpriteIn(spritecollection, first_avail, ZoomLevelToFraction(ZOOM_LVL_NORMAL))) return false;
	}

	first = spritecollection.begin();

	/* Pad sprites to make sizes match. */
	if (!PadSprites(spritecollection, encoder)) return false;

	/* Create other missing zoom levels */
	for (ZoomLevel zoom = ZOOM_LVL_OUT_2X; zoom != ZOOM_LVL_END; zoom++) {
		float scale = ZoomLevelToFraction(zoom);
		auto it = spritecollection.find(scale);
		if (it != spritecollection.end()) {
			/* Check that size and offsets match the fully zoomed image. */
			assert(it->second.width  == int(UnScaleFraction(it->first, first->second.width)));
			assert(it->second.height == int(UnScaleFraction(it->first, first->second.height)));
			assert(it->second.x_offs == int(UnScaleFraction(it->first, first->second.x_offs)));
			assert(it->second.y_offs == int(UnScaleFraction(it->first, first->second.y_offs)));
		} else {
			/* Zoom level is not available, or unusable, so create it */
			ResizeSpriteOut(spritecollection, scale);
		}
	}

	/* Upscale to desired sprite_min_zoom if provided sprite only had zoomed in versions. */
	if (first_avail > ZoomLevelToFraction(_settings_client.gui.sprite_zoom_min)) {
		if (_settings_client.gui.sprite_zoom_min >= ZOOM_LVL_OUT_4X) ResizeSpriteIn(spritecollection, ZoomLevelToFraction(ZOOM_LVL_OUT_4X), ZoomLevelToFraction(ZOOM_LVL_OUT_2X));
		if (_settings_client.gui.sprite_zoom_min >= ZOOM_LVL_OUT_2X) ResizeSpriteIn(spritecollection, ZoomLevelToFraction(ZOOM_LVL_OUT_2X), ZoomLevelToFraction(ZOOM_LVL_NORMAL));
	}

	/* Remove zoom levels outside of configured min/max. The slots still need to exist. */
	// float zoom_min = ZoomLevelToFraction(_settings_client.gui.zoom_min);
	// float zoom_max = ZoomLevelToFraction(_settings_client.gui.zoom_max);
	// for (auto &sprite : spritecollection) {
	// 	if (sprite.first < zoom_min || sprite.first > zoom_max) {
	// 		sprite.second.width = 0;
	// 		sprite.second.height = 0;
	// 		sprite.second.x_offs = 0;
	// 		sprite.second.y_offs = 0;
	// 	}
	// }

	return true;
}

/**
 * Load a recolour sprite into memory.
 * @param file GRF we're reading from.
 * @param num Size of the sprite in the GRF.
 * @return Sprite data.
 */
static void *ReadRecolourSprite(SpriteFile &file, uint num, SpriteAllocator &allocator)
{
	/* "Normal" recolour sprites are ALWAYS 257 bytes. Then there is a small
	 * number of recolour sprites that are 17 bytes that only exist in DOS
	 * GRFs which are the same as 257 byte recolour sprites, but with the last
	 * 240 bytes zeroed.  */
	static const uint RECOLOUR_SPRITE_SIZE = 257;
	byte *dest = (byte *)allocator.Allocate(std::max(RECOLOUR_SPRITE_SIZE, num));

	if (file.NeedsPaletteRemap()) {
		byte *dest_tmp = new byte[std::max(RECOLOUR_SPRITE_SIZE, num)];

		/* Only a few recolour sprites are less than 257 bytes */
		if (num < RECOLOUR_SPRITE_SIZE) memset(dest_tmp, 0, RECOLOUR_SPRITE_SIZE);
		file.ReadBlock(dest_tmp, num);

		/* The data of index 0 is never used; "literal 00" according to the (New)GRF specs. */
		for (uint i = 1; i < RECOLOUR_SPRITE_SIZE; i++) {
			dest[i] = _palmap_w2d[dest_tmp[_palmap_d2w[i - 1] + 1]];
		}
		delete[] dest_tmp;
	} else {
		file.ReadBlock(dest, num);
	}

	return dest;
}

/**
 * Read a sprite from disk.
 * @param sc          Location of sprite.
 * @param id          Sprite number.
 * @param sprite_type Type of sprite.
 * @param allocator   Allocator function to use.
 * @param encoder     Sprite encoder to use.
 * @return Read sprite data.
 */
static void *ReadSprite(const SpriteCache *sc, SpriteID id, SpriteType sprite_type, std::optional<float> scale, SpriteAllocator &allocator, SpriteEncoder *encoder)
{
	/* Use current blitter if no other sprite encoder is given. */
	if (encoder == nullptr) encoder = BlitterFactory::GetCurrentBlitter();

	SpriteFile &file = *sc->file;
	size_t file_pos = sc->file_pos;

	assert(sprite_type != SpriteType::Recolour);
	assert(IsMapgenSpriteID(id) == (sprite_type == SpriteType::MapGen));
	assert(sc->type == sprite_type);

	Debug(sprite, 9, "Load sprite {}", id);

	SpriteLoader::SpriteCollection spritecollection;
	bool sprite_avail = false;

	if (sprite_type == SpriteType::Font && !scale.has_value()) scale = ZoomLevelToFraction(_font_zoom);

	SpriteLoaderGrf sprite_loader(file.GetContainerVersion());
	if (sprite_type != SpriteType::MapGen && encoder->Is32BppSupported()) {
		/* Try for 32bpp sprites first. */
		sprite_avail = sprite_loader.LoadSprite(spritecollection, file, file_pos, sprite_type, true, sc->control_flags);
	}
	if (!sprite_avail) {
		sprite_avail = sprite_loader.LoadSprite(spritecollection, file, file_pos, sprite_type, false, sc->control_flags);
	}

	if (!sprite_avail) {
		if (sprite_type == SpriteType::MapGen) return nullptr;
		if (id == SPR_IMG_QUERY) UserError("Okay... something went horribly wrong. I couldn't load the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, scale, &allocator, encoder);
	}

	if (sprite_type == SpriteType::MapGen) {
		/* Ugly hack to work around the problem that the old landscape
		 *  generator assumes that those sprites are stored uncompressed in
		 *  the memory, and they are only read directly by the code, never
		 *  send to the blitter. So do not send it to the blitter (which will
		 *  result in a data array in the format the blitter likes most), but
		 *  extract the data directly and store that as sprite.
		 * Ugly: yes. Other solution: no. Blame the original author or
		 *  something ;) The image should really have been a data-stream
		 *  (so type = 0xFF basically). */
		auto &sprite = spritecollection.begin()->second;
		uint num = sprite.width * sprite.height;

		Sprite *s = (Sprite *)allocator.Allocate(sizeof(*s) + num);
		s->width  = sprite.width;
		s->height = sprite.height;
		s->x_offs = sprite.x_offs;
		s->y_offs = sprite.y_offs;

		SpriteLoader::CommonPixel *src = sprite.data;
		byte *dest = s->data;
		while (num-- > 0) {
			*dest++ = src->m;
			src++;
		}

		return s;
	}

	if (scale.has_value()) {
		/* Perform fractional scaling to single provided scale. */
		float desired_scale = *scale;
		if (spritecollection.count(desired_scale) == 0) ResizeSpriteIn(spritecollection, spritecollection.begin()->first, desired_scale);
		std::erase_if(spritecollection, [desired_scale](const auto &pair) { return pair.first != desired_scale; });
	} else if (!ResizeSprites(spritecollection, encoder)) {
		if (id == SPR_IMG_QUERY) UserError("Okay... something went horribly wrong. I couldn't resize the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, {}, &allocator, encoder);
	}

	return encoder->Encode(spritecollection, allocator);
}

struct GrfSpriteOffset {
	size_t file_pos;
	byte control_flags;
};

/** Map from sprite numbers to position in the GRF file. */
static std::map<uint32_t, GrfSpriteOffset> _grf_sprite_offsets;

/**
 * Get the file offset for a specific sprite in the sprite section of a GRF.
 * @param id ID of the sprite to look up.
 * @return Position of the sprite in the sprite section or SIZE_MAX if no such sprite is present.
 */
size_t GetGRFSpriteOffset(uint32_t id)
{
	return _grf_sprite_offsets.find(id) != _grf_sprite_offsets.end() ? _grf_sprite_offsets[id].file_pos : SIZE_MAX;
}

/**
 * Parse the sprite section of GRFs.
 * @param container_version Container version of the GRF we're currently processing.
 */
void ReadGRFSpriteOffsets(SpriteFile &file)
{
	_grf_sprite_offsets.clear();

	if (file.GetContainerVersion() >= 2) {
		/* Seek to sprite section of the GRF. */
		size_t data_offset = file.ReadDword();
		size_t old_pos = file.GetPos();
		file.SeekTo(data_offset, SEEK_CUR);

		GrfSpriteOffset offset = { 0, 0 };

		/* Loop over all sprite section entries and store the file
		 * offset for each newly encountered ID. */
		uint32_t id, prev_id = 0;
		while ((id = file.ReadDword()) != 0) {
			if (id != prev_id) {
				_grf_sprite_offsets[prev_id] = offset;
				offset.file_pos = file.GetPos() - 4;
				offset.control_flags = 0;
			}
			prev_id = id;
			uint length = file.ReadDword();
			if (length > 0) {
				byte colour = file.ReadByte() & SCC_MASK;
				length--;
				if (length > 0) {
					byte zoom = file.ReadByte();
					length--;
					if (colour != 0 && zoom == 0) { // ZOOM_LVL_OUT_4X (normal zoom)
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_1X_32BPP : SCCF_ALLOW_ZOOM_MIN_1X_PAL);
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_2X_32BPP : SCCF_ALLOW_ZOOM_MIN_2X_PAL);
					}
					if (colour != 0 && zoom == 2) { // ZOOM_LVL_OUT_2X (2x zoomed in)
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_2X_32BPP : SCCF_ALLOW_ZOOM_MIN_2X_PAL);
					}
				}
			}
			file.SkipBytes(length);
		}
		if (prev_id != 0) _grf_sprite_offsets[prev_id] = offset;

		/* Continue processing the data section. */
		file.SeekTo(old_pos, SEEK_SET);
	}
}


/**
 * Load a real or recolour sprite.
 * @param load_index Global sprite index.
 * @param file GRF to load from.
 * @param file_sprite_id Sprite number in the GRF.
 * @param container_version Container version of the GRF.
 * @return True if a valid sprite was loaded, false on any error.
 */
bool LoadNextSprite(uint load_index, SpriteFile &file, uint file_sprite_id)
{
	size_t file_pos = file.GetPos();

	/* Read sprite header. */
	uint32_t num = file.GetContainerVersion() >= 2 ? file.ReadDword() : file.ReadWord();
	if (num == 0) return false;
	byte grf_type = file.ReadByte();

	SpriteType type;
	byte control_flags = 0;
	if (grf_type == 0xFF) {
		/* Some NewGRF files have "empty" pseudo-sprites which are 1
		 * byte long. Catch these so the sprites won't be displayed. */
		if (num == 1) {
			file.ReadByte();
			return false;
		}
		type = SpriteType::Recolour;
	} else if (file.GetContainerVersion() >= 2 && grf_type == 0xFD) {
		if (num != 4) {
			/* Invalid sprite section include, ignore. */
			file.SkipBytes(num);
			return false;
		}
		/* It is not an error if no sprite with the provided ID is found in the sprite section. */
		auto iter = _grf_sprite_offsets.find(file.ReadDword());
		if (iter != _grf_sprite_offsets.end()) {
			file_pos = iter->second.file_pos;
			control_flags = iter->second.control_flags;
		} else {
			file_pos = SIZE_MAX;
		}
		type = SpriteType::Normal;
	} else {
		file.SkipBytes(7);
		type = SkipSpriteData(file, grf_type, num - 8) ? SpriteType::Normal : SpriteType::Invalid;
		/* Inline sprites are not supported for container version >= 2. */
		if (file.GetContainerVersion() >= 2) return false;
	}

	if (type == SpriteType::Invalid) return false;

	if (load_index >= MAX_SPRITES) {
		UserError("Tried to load too many sprites (#{}; max {})", load_index, MAX_SPRITES);
	}

	bool is_mapgen = IsMapgenSpriteID(load_index);

	if (is_mapgen) {
		if (type != SpriteType::Normal) UserError("Uhm, would you be so kind not to load a NewGRF that changes the type of the map generator sprites?");
		type = SpriteType::MapGen;
	}

	SpriteCache *sc = AllocateSpriteCache(load_index);
	sc->file = &file;
	sc->file_pos = file_pos;
	CacheSpriteAllocator allocator(sc->data);
	if (type == SpriteType::Recolour) ReadRecolourSprite(file, num, allocator);
	sc->lru = 0;
	sc->id = file_sprite_id;
	sc->type = type;
	sc->warned = false;
	sc->control_flags = control_flags;

	return true;
}

void DupSprite(SpriteID old_spr, SpriteID new_spr)
{
	SpriteCache *scnew = AllocateSpriteCache(new_spr); // may reallocate: so put it first
	SpriteCache *scold = GetSpriteCache(old_spr);

	scnew->file = scold->file;
	scnew->file_pos = scold->file_pos;
	scnew->ClearSpriteData();
	scnew->ClearSpriteFractionalData();
	scnew->id = scold->id;
	scnew->type = scold->type;
	scnew->warned = false;
	scnew->control_flags = scold->control_flags;
}

/**
 * Delete entries from the sprite cache to remove the requested number of bytes.
 * Sprite data is removed in order of LRU values.
 * The total number of bytes removed may be larger than the number requested.
 * @param to_remove Requested number of bytes to remove.
 */
static void DeleteEntriesFromSpriteCache(size_t to_remove)
{
	const size_t initial_in_use = _spritecache_bytes_used;

	struct SpriteInfo {
		uint32_t lru;
		SpriteID id;
		size_t size;

		bool operator<(const SpriteInfo &other) const
		{
			return this->lru < other.lru;
		}
	};

	std::vector<SpriteInfo> candidates; // max heap, ordered by LRU
	size_t candidate_bytes = 0;         // total bytes that would be released when clearing all sprites in candidates

	auto push = [&](SpriteInfo info) {
		candidates.push_back(info);
		std::push_heap(candidates.begin(), candidates.end());
		candidate_bytes += info.size;
	};

	auto pop = [&]() {
		candidate_bytes -= candidates.front().size;
		std::pop_heap(candidates.begin(), candidates.end());
		candidates.pop_back();
	};

	SpriteID i = 0;
	for (; i != (SpriteID)_spritecache.size() && candidate_bytes < to_remove; i++) {
		SpriteCache *sc = GetSpriteCache(i);
		if (sc->type != SpriteType::Recolour && !sc->data.empty()) {
			push({ sc->lru, i, sc->data.size() });
			if (candidate_bytes >= to_remove) break;
		}
	}
	/* candidates now contains enough bytes to meet to_remove.
	 * only sprites with LRU values <= the maximum (i.e. the top of the heap) need to be considered */
	for (; i != (SpriteID)_spritecache.size(); i++) {
		SpriteCache *sc = GetSpriteCache(i);
		if (sc->type != SpriteType::Recolour && !sc->data.empty() && sc->lru <= candidates.front().lru) {
			push({ sc->lru, i, sc->data.size() });
			while (!candidates.empty() && candidate_bytes - candidates.front().size >= to_remove) {
				pop();
			}
		}
	}

	for (auto &it : candidates) {
		GetSpriteCache(it.id)->ClearSpriteData();
	}

	Debug(sprite, 3, "DeleteEntriesFromSpriteCache, deleted: {}, freed: {}, in use: {} --> {}, requested: {}",
			candidates.size(), candidate_bytes, initial_in_use, _spritecache_bytes_used, to_remove);
}

void IncreaseSpriteLRU()
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	uint target_size = (bpp > 0 ? _sprite_cache_size * bpp / 8 : 1) * 1024 * 1024;
	if (_spritecache_bytes_used > target_size) {
		DeleteEntriesFromSpriteCache(_spritecache_bytes_used - target_size + 512 * 1024);
	}

	if (_sprite_lru_counter >= 0xC0000000) {
		Debug(sprite, 3, "Fixing lru {}, inuse={}", _sprite_lru_counter, _spritecache_bytes_used);

		for (SpriteCache &sc : _spritecache) {
			if (!sc.data.empty()) {
				if (sc.lru > 0x80000000) {
					sc.lru -= 0x80000000;
				} else {
					sc.lru = 0;
				}
			}
		}
		_sprite_lru_counter -= 0x80000000;
	}
}

void SpriteCache::ClearSpriteData()
{
	_spritecache_bytes_used -= this->data.size();
	this->data.clear();
	this->data.shrink_to_fit();
}

void SpriteCache::ClearSpriteFractionalData()
{
	_spritecache_bytes_used -= this->fractional_data.size();
	this->fractional_data.clear();
	this->fractional_data.shrink_to_fit();
}

void *CacheSpriteAllocator::Allocate(size_t size)
{
	_spritecache_bytes_used -= this->data.size();
	this->data.resize(size);
	_spritecache_bytes_used += this->data.size();
	return this->data.data();
}

/**
 * Sprite allocator simply using malloc.
 */
void *SimpleSpriteAllocator::Allocate(size_t size)
{
	return MallocT<byte>(size);
}

/**
 * Handles the case when a sprite of different type is requested than is present in the SpriteCache.
 * For SpriteType::Font sprites, it is normal. In other cases, default sprite is loaded instead.
 * @param sprite ID of loaded sprite
 * @param requested requested sprite type
 * @param sc the currently known sprite cache for the requested sprite
 * @return fallback sprite
 * @note this function will do UserError() in the case the fallback sprite isn't available
 */
static void *HandleInvalidSpriteRequest(SpriteID sprite, SpriteType requested, SpriteCache *sc, SpriteAllocator *allocator)
{
	static const char * const sprite_types[] = {
		"normal",        // SpriteType::Normal
		"map generator", // SpriteType::MapGen
		"character",     // SpriteType::Font
		"recolour",      // SpriteType::Recolour
	};

	SpriteType available = sc->type;
	if (requested == SpriteType::Font && available == SpriteType::Normal) {
		if (sc->data.empty()) sc->type = SpriteType::Font;
		return GetRawSprite(sprite, sc->type, {}, allocator);
	}

	byte warning_level = sc->warned ? 6 : 0;
	sc->warned = true;
	Debug(sprite, warning_level, "Tried to load {} sprite #{} as a {} sprite. Probable cause: NewGRF interference", sprite_types[static_cast<byte>(available)], sprite, sprite_types[static_cast<byte>(requested)]);

	switch (requested) {
		case SpriteType::Normal:
			if (sprite == SPR_IMG_QUERY) UserError("Uhm, would you be so kind not to load a NewGRF that makes the 'query' sprite a non-normal sprite?");
			[[fallthrough]];
		case SpriteType::Font:
			return GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, {}, allocator);
		case SpriteType::Recolour:
			if (sprite == PALETTE_TO_DARK_BLUE) UserError("Uhm, would you be so kind not to load a NewGRF that makes the 'PALETTE_TO_DARK_BLUE' sprite a non-remap sprite?");
			return GetRawSprite(PALETTE_TO_DARK_BLUE, SpriteType::Recolour, {}, allocator);
		case SpriteType::MapGen:
			/* this shouldn't happen, overriding of SpriteType::MapGen sprites is checked in LoadNextSprite()
			 * (the only case the check fails is when these sprites weren't even loaded...) */
		default:
			NOT_REACHED();
	}
}

/**
 * Reads a sprite (from disk or sprite cache).
 * If the sprite is not available or of wrong type, a fallback sprite is returned.
 * @param sprite Sprite to read.
 * @param type Expected sprite type.
 * @param allocator Allocator function to use. Set to nullptr to use the usual sprite cache.
 * @param encoder Sprite encoder to use. Set to nullptr to use the currently active blitter.
 * @return Sprite raw data
 */
void *GetRawSprite(SpriteID sprite, SpriteType type, std::optional<float> scale, SpriteAllocator *allocator, SpriteEncoder *encoder)
{
	assert(type != SpriteType::MapGen || IsMapgenSpriteID(sprite));
	assert(type < SpriteType::Invalid);

	if (!SpriteExists(sprite)) {
		Debug(sprite, 1, "Tried to load non-existing sprite #{}. Probable cause: Wrong/missing NewGRFs", sprite);

		/* SPR_IMG_QUERY is a BIG FAT RED ? */
		sprite = SPR_IMG_QUERY;
	}

	SpriteCache *sc = GetSpriteCache(sprite);

	if (sc->type != type) return HandleInvalidSpriteRequest(sprite, type, sc, allocator);

	if (allocator == nullptr && encoder == nullptr) {
		/* Load sprite into/from spritecache */

		/* Update LRU */
		sc->lru = ++_sprite_lru_counter;

		/* Load the sprite, if it is not loaded, yet */
		if (scale.has_value()) {
			if (sc->fractional_data.empty()) {
				CacheSpriteAllocator cache_allocator(sc->fractional_data);
				ReadSprite(sc, sprite, type, scale, cache_allocator, nullptr);
			}
			return sc->fractional_data.data();
		}

		if (sc->data.empty()) {
			CacheSpriteAllocator cache_allocator(sc->data);
			ReadSprite(sc, sprite, type, scale, cache_allocator, nullptr);
		}

		return static_cast<void *>(sc->data.data());
	} else {
		/* Do not use the spritecache, but a different allocator. */
		return ReadSprite(sc, sprite, type, scale, *allocator, encoder);
	}
}

void GfxInitSpriteMem()
{
	/* Reset the spritecache 'pool' */
	_spritecache.clear();
	_sprite_files.clear();
	_spritecache_bytes_used = 0;
}

/**
 * Remove all encoded sprites from the sprite cache without
 * discarding sprite location information.
 */
void GfxClearSpriteCache()
{
	/* Clear sprite ptr for all cached items */
	for (SpriteCache &sc : _spritecache) {
		if (sc.type != SpriteType::Recolour && !sc.data.empty()) sc.ClearSpriteData();
	}

	VideoDriver::GetInstance()->ClearSystemSprites();
}

void GfxClearFractionalSpriteCache()
{
	for (SpriteCache &sc : _spritecache) {
		if (sc.type != SpriteType::Recolour && !sc.fractional_data.empty()) sc.ClearSpriteFractionalData();
	}
}

/**
 * Remove all encoded font sprites from the sprite cache without
 * discarding sprite location information.
 */
void GfxClearFontSpriteCache()
{
	/* Clear sprite ptr for all cached font items */
	for (SpriteCache &sc : _spritecache) {
		if (sc.type == SpriteType::Font && !sc.data.empty()) sc.ClearSpriteData();
	}
}

/**
 * Shrink to fit the sprite cache index.
 */
void GfxShrinkToFitSpriteCacheIndex()
{
	_spritecache.shrink_to_fit();
}

/* static */ std::map<float, ReusableBuffer<SpriteLoader::CommonPixel>, std::greater<float>> SpriteLoader::Sprite::buffer;
