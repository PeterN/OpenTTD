/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file palette.cpp Handling of palettes. */

#include "stdafx.h"
#include "blitter/base.hpp"
#include "blitter/factory.hpp"
#include "gfx_type.h"
#include "landscape_type.h"
#include "palette_func.h"
#include "settings_type.h"
#include "spritecache.h"

#include "table/palettes.h"
#include "table/strings.h"

#include <mutex>

#include "safeguards.h"

Palette _cur_palette;

static std::recursive_mutex _palette_mutex; ///< To coordinate access to _cur_palette.

/**
 * PALETTE_BITS reduces the bits-per-channel of 32bpp graphics data to allow faster palette lookups from
 * a smaller lookup table.
 *
 * 6 bpc is chosen as this results in a palette lookup table of 256KiB with adequate fidelity.
 * In contrast, a 5 bpc lookup table would be 32KiB, and 7 bpc would be 2MiB.
 *
 * Values in the table are filled as they are first encountered -- larger lookup table means more colour
 * distance calculations, and is therefore slower.
 */
const uint PALETTE_BITS = 6;
const uint PALETTE_SHIFT = 8 - PALETTE_BITS;
const uint PALETTE_BITS_MASK = ((1U << PALETTE_BITS) - 1) << PALETTE_SHIFT;
const uint PALETTE_BITS_OR = (1U << (PALETTE_SHIFT - 1));

/** @{
 * Palette lookup table. */
using PaletteLookup = std::array<uint8_t, 1U << (PALETTE_BITS * 3)>;
static PaletteLookup _palette_lookup{};
/** @} */

/** @{
 * Reshade lookup table. */
using ReshadeLookup = std::array<uint8_t, 1U << PALETTE_BITS>;
static ReshadeLookup _reshade_lookup{};
/** @} */

/**
 * Reduce bits per channel to PALETTE_BITS, and place value in the middle of the reduced range.
 * This is to counteract the information lost between bright and dark pixels, e.g if PALETTE_BITS was 2:
 *    0 -  63 ->  32
 *   64 - 127 ->  96
 *  128 - 191 -> 160
 *  192 - 255 -> 224
 * @param c 8 bit colour component.
 * @returns Colour component reduced to PALETTE_BITS.
 */
inline uint CrunchColour(uint c)
{
	return (c & PALETTE_BITS_MASK) | PALETTE_BITS_OR;
}

/**
 * Calculate distance between two colours.
 * @param col1 First colour.
 * @param r2 Red component of second colour.
 * @param g2 Green component of second colour.
 * @param b2 Blue component of second colour.
 * @returns Euclidean distance between first and second colour.
 */
static uint CalculateColourDistance(const Colour &col1, int r2, int g2, int b2)
{
	/* Euclidean colour distance for sRGB based on https://en.wikipedia.org/wiki/Color_difference#sRGB */
	int r = (int)col1.r - (int)r2;
	int g = (int)col1.g - (int)g2;
	int b = (int)col1.b - (int)b2;

	int avgr = (col1.r + r2) / 2;
	return ((2 + (avgr / 256.0)) * r * r) + (4 * g * g) + ((2 + ((255 - avgr) / 256.0)) * b * b);
}

/* Palette indexes for conversion. See docs/palettes/palette_key.png */
const uint8_t PALETTE_INDEX_CC_START = 198; ///< Palette index of start of company colour remap area.
const uint8_t PALETTE_INDEX_CC_END = PALETTE_INDEX_CC_START + 8; ///< Palette index of end of company colour remap area.
const uint8_t PALETTE_INDEX_START = 1; ///< Palette index of start of defined palette.
const uint8_t PALETTE_INDEX_END = 215; ///< Palette index of end of defined palette.

/**
 * Find nearest colour palette index for a 32bpp pixel.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @returns palette index of nearest colour.
 */
static uint8_t FindNearestColourIndex(uint8_t r, uint8_t g, uint8_t b)
{
	r = CrunchColour(r);
	g = CrunchColour(g);
	b = CrunchColour(b);

	uint best_index = 0;
	uint best_distance = UINT32_MAX;

	for (uint i = PALETTE_INDEX_START; i < PALETTE_INDEX_CC_START; i++) {
		if (uint distance = CalculateColourDistance(_palette.palette[i], r, g, b); distance < best_distance) {
			best_index = i;
			best_distance = distance;
		}
	}
	/* There's a hole in the palette reserved for company colour remaps. */
	for (uint i = PALETTE_INDEX_CC_END; i < PALETTE_INDEX_END; i++) {
		if (uint distance = CalculateColourDistance(_palette.palette[i], r, g, b); distance < best_distance) {
			best_index = i;
			best_distance = distance;
		}
	}
	return best_index;
}

/**
 * Find nearest company colour palette index for a brightness level.
 * @param b Pixel-colour to find.
 * @returns palette index of nearest colour.
 */
static uint8_t FindNearestColourReshadeIndex(uint8_t b)
{
	b = CrunchColour(b);

	uint best_index = 0;
	uint best_distance = UINT32_MAX;

	for (uint i = PALETTE_INDEX_CC_START; i < PALETTE_INDEX_CC_END; i++) {
		if (uint distance = CalculateColourDistance(_palette.palette[i], b, b, b); distance < best_distance) {
			best_index = i;
			best_distance = distance;
		}
	}
	return best_index;
}

/**
 * Get nearest colour palette index from an RGB colour.
 * A search is performed if this colour is not already in the lookup table.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @returns nearest colour palette index.
 */
uint8_t GetNearestColourIndex(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t key = (r >> PALETTE_SHIFT) | (g >> PALETTE_SHIFT) << PALETTE_BITS | (b >> PALETTE_SHIFT) << (PALETTE_BITS * 2);
	if (_palette_lookup[key] == 0) _palette_lookup[key] = FindNearestColourIndex(r, g, b);
	return _palette_lookup[key];
}

/**
 * Get nearest colour palette index from a brightness level.
 * A search is performed if this brightness level is not already in the lookup table.
 * @param b Brightness component.
 * @returns nearest colour palette index.
 */
uint8_t GetNearestColourReshadeIndex(uint8_t b)
{
	uint32_t key = (b >> PALETTE_SHIFT);
	if (_reshade_lookup[key] == 0) _reshade_lookup[key] = FindNearestColourReshadeIndex(b);
	return _reshade_lookup[key];
}

/**
 * Adjust brightness of colour.
 * @param colour Colour to adjust.
 * @param brightness Brightness to apply to colour.
 * @returns Adjusted colour.
 */
Colour ReallyAdjustBrightness(Colour colour, int brightness)
{
	if (brightness == DEFAULT_BRIGHTNESS) return colour;

	uint64_t combined = (static_cast<uint64_t>(colour.r) << 32) | (static_cast<uint64_t>(colour.g) << 16) | static_cast<uint64_t>(colour.b);
	combined *= brightness;

	uint16_t r = GB(combined, 39, 9);
	uint16_t g = GB(combined, 23, 9);
	uint16_t b = GB(combined, 7, 9);

	if ((combined & 0x800080008000L) == 0L) {
		return Colour(r, g, b, colour.a);
	}

	uint16_t ob = 0;
	/* Sum overbright */
	if (r > 255) ob += r - 255;
	if (g > 255) ob += g - 255;
	if (b > 255) ob += b - 255;

	/* Reduce overbright strength */
	ob /= 2;
	return Colour(
		r >= 255 ? 255 : std::min(r + ob * (255 - r) / 256, 255),
		g >= 255 ? 255 : std::min(g + ob * (255 - g) / 256, 255),
		b >= 255 ? 255 : std::min(b + ob * (255 - b) / 256, 255),
		colour.a);
}

void DoPaletteAnimations();

void GfxInitPalettes()
{
	ResetColourSpecs();

	std::lock_guard<std::recursive_mutex> lock(_palette_mutex);
	_cur_palette = _palette;
	DoPaletteAnimations();
}

/**
 * Copy the current palette if the palette was updated.
 * Used by video-driver to get a current up-to-date version of the palette,
 * to avoid two threads accessing the same piece of memory (with a good chance
 * one is already updating the palette while the other is drawing based on it).
 * @param local_palette The location to copy the palette to.
 * @param force_copy Whether to ignore if there is an update for the palette.
 * @return True iff a copy was done.
 */
bool CopyPalette(Palette &local_palette, bool force_copy)
{
	std::lock_guard<std::recursive_mutex> lock(_palette_mutex);

	if (!force_copy && _cur_palette.count_dirty == 0) return false;

	local_palette = _cur_palette;
	_cur_palette.count_dirty = 0;

	if (force_copy) {
		local_palette.first_dirty = 0;
		local_palette.count_dirty = 256;
	}

	return true;
}

#define EXTR(p, q) (((uint16_t)(palette_animation_counter * (p)) * (q)) >> 16)
#define EXTR2(p, q) (((uint16_t)(~palette_animation_counter * (p)) * (q)) >> 16)

void DoPaletteAnimations()
{
	std::lock_guard<std::recursive_mutex> lock(_palette_mutex);

	/* Animation counter for the palette animation. */
	static int palette_animation_counter = 0;
	palette_animation_counter += 8;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	const Colour *s;
	const ExtraPaletteValues *ev = &_extra_palette_values;
	const uint old_tc = palette_animation_counter;
	uint j;

	if (blitter != nullptr && blitter->UsePaletteAnimation() == Blitter::PaletteAnimation::None) {
		palette_animation_counter = 0;
	}

	std::span<Colour> current_palette{&_cur_palette.palette[PALETTE_ANIM_START], PALETTE_ANIM_SIZE};
	/* Makes a copy of the current animation palette in old_val,
	 * so the work on the current palette could be compared, see if there has been any changes */
	std::array<Colour, PALETTE_ANIM_SIZE> original_palette;
	std::ranges::copy(current_palette, original_palette.begin());
	auto palette_pos = current_palette.begin(); // Points to where animations are taking place on the palette

	/* Fizzy Drink bubbles animation */
	s = ev->fizzy_drink;
	j = EXTR2(512, EPV_CYCLES_FIZZY_DRINK);
	for (uint i = 0; i != EPV_CYCLES_FIZZY_DRINK; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_FIZZY_DRINK) j = 0;
	}

	/* Oil refinery fire animation */
	s = ev->oil_refinery;
	j = EXTR2(512, EPV_CYCLES_OIL_REFINERY);
	for (uint i = 0; i != EPV_CYCLES_OIL_REFINERY; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_OIL_REFINERY) j = 0;
	}

	/* Radio tower blinking */
	{
		uint8_t i = (palette_animation_counter >> 1) & 0x7F;
		uint8_t v;

		if (i < 0x3f) {
			v = 255;
		} else if (i < 0x4A || i >= 0x75) {
			v = 128;
		} else {
			v = 20;
		}
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;

		i ^= 0x40;
		if (i < 0x3f) {
			v = 255;
		} else if (i < 0x4A || i >= 0x75) {
			v = 128;
		} else {
			v = 20;
		}
		palette_pos->r = v;
		palette_pos->g = 0;
		palette_pos->b = 0;
		palette_pos++;
	}

	/* Handle lighthouse and stadium animation */
	s = ev->lighthouse;
	j = EXTR(256, EPV_CYCLES_LIGHTHOUSE);
	for (uint i = 0; i != EPV_CYCLES_LIGHTHOUSE; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_LIGHTHOUSE) j = 0;
	}

	/* Dark blue water */
	s = (_settings_game.game_creation.landscape == LandscapeType::Toyland) ? ev->dark_water_toyland : ev->dark_water;
	j = EXTR(320, EPV_CYCLES_DARK_WATER);
	for (uint i = 0; i != EPV_CYCLES_DARK_WATER; i++) {
		*palette_pos++ = s[j];
		j++;
		if (j == EPV_CYCLES_DARK_WATER) j = 0;
	}

	/* Glittery water */
	s = (_settings_game.game_creation.landscape == LandscapeType::Toyland) ? ev->glitter_water_toyland : ev->glitter_water;
	j = EXTR(128, EPV_CYCLES_GLITTER_WATER);
	for (uint i = 0; i != EPV_CYCLES_GLITTER_WATER / 3; i++) {
		*palette_pos++ = s[j];
		j += 3;
		if (j >= EPV_CYCLES_GLITTER_WATER) j -= EPV_CYCLES_GLITTER_WATER;
	}

	if (blitter != nullptr && blitter->UsePaletteAnimation() == Blitter::PaletteAnimation::None) {
		palette_animation_counter = old_tc;
	} else if (_cur_palette.count_dirty == 0 && !std::ranges::equal(current_palette, original_palette)) {
		/* Did we changed anything on the palette? Seems so.  Mark it as dirty */
		_cur_palette.first_dirty = PALETTE_ANIM_START;
		_cur_palette.count_dirty = PALETTE_ANIM_SIZE;
	}
}

/**
 * Determine a contrasty text colour for a coloured background.
 * @param background Background colour.
 * @param threshold Background colour brightness threshold below which the background is considered dark and TextColour::White is returned, range: 0 - 255, default 128.
 * @return TextColour::Black or TextColour::White depending on what gives a better contrast.
 */
TextColour GetContrastColour(PixelColour background, uint8_t threshold)
{
	Colour c = _cur_palette.palette[background.p];
	/* Compute brightness according to http://www.w3.org/TR/AERT#color-contrast.
	 * The following formula computes 1000 * brightness^2, with brightness being in range 0 to 255. */
	uint sq1000_brightness = c.r * c.r * 299 + c.g * c.g * 587 + c.b * c.b * 114;
	/* Compare with threshold brightness which defaults to 128 (50%) */
	return sq1000_brightness < ((uint) threshold) * ((uint) threshold) * 1000 ? TextColour::White : TextColour::Black;
}

static constexpr ColourSpec _original_colour_specs[] = {
	{STR_COLOUR_DARK_BLUE,  2, {PixelColour{0xC6}, PixelColour{0xC7}, PixelColour{0xC8}, PixelColour{0xC9}, PixelColour{0xCA}, PixelColour{0xCB}, PixelColour{0xCC}, PixelColour{0xCD}}, {Colours::Blue, Colours::LightBlue}},
	{STR_COLOUR_PALE_GREEN, 2, {PixelColour{0x60}, PixelColour{0x61}, PixelColour{0x62}, PixelColour{0x63}, PixelColour{0x64}, PixelColour{0x65}, PixelColour{0x66}, PixelColour{0x67}}, {Colours::Green, Colours::DarkGreen}},
	{STR_COLOUR_PINK,       3, {PixelColour{0x2A}, PixelColour{0x2B}, PixelColour{0x2C}, PixelColour{0x2D}, PixelColour{0x2E}, PixelColour{0x2F}, PixelColour{0x30}, PixelColour{0x31}}, {}},
	{STR_COLOUR_YELLOW,     2, {PixelColour{0x3E}, PixelColour{0x3F}, PixelColour{0x40}, PixelColour{0x41}, PixelColour{0x42}, PixelColour{0x43}, PixelColour{0x44}, PixelColour{0x45}}, {Colours::Orange}},
	{STR_COLOUR_RED,        3, {PixelColour{0xB3}, PixelColour{0xB4}, PixelColour{0xB5}, PixelColour{0xB6}, PixelColour{0xB7}, PixelColour{0xA4}, PixelColour{0xA5}, PixelColour{0xA6}}, {}},
	{STR_COLOUR_LIGHT_BLUE, 2, {PixelColour{0x9A}, PixelColour{0x9B}, PixelColour{0x9C}, PixelColour{0x9D}, PixelColour{0x9E}, PixelColour{0x9F}, PixelColour{0xA0}, PixelColour{0xA1}}, {Colours::DarkBlue, Colours::Blue}},
	{STR_COLOUR_GREEN,      3, {PixelColour{0x52}, PixelColour{0x53}, PixelColour{0x54}, PixelColour{0x55}, PixelColour{0xCE}, PixelColour{0xCF}, PixelColour{0xD0}, PixelColour{0xD1}}, {Colours::PaleGreen, Colours::DarkGreen}},
	{STR_COLOUR_DARK_GREEN, 2, {PixelColour{0x58}, PixelColour{0x59}, PixelColour{0x5A}, PixelColour{0x5B}, PixelColour{0x5C}, PixelColour{0x5D}, PixelColour{0x5E}, PixelColour{0x5F}}, {Colours::PaleGreen, Colours::Green}},
	{STR_COLOUR_BLUE,       3, {PixelColour{0x92}, PixelColour{0x93}, PixelColour{0x94}, PixelColour{0x95}, PixelColour{0x96}, PixelColour{0x97}, PixelColour{0x98}, PixelColour{0x99}}, {Colours::DarkBlue, Colours::LightBlue}},
	{STR_COLOUR_CREAM,      2, {PixelColour{0x72}, PixelColour{0x73}, PixelColour{0x74}, PixelColour{0x75}, PixelColour{0x76}, PixelColour{0x77}, PixelColour{0x78}, PixelColour{0x79}}, {Colours::Brown, Colours::Orange}},
	{STR_COLOUR_MAUVE,      2, {PixelColour{0x80}, PixelColour{0x81}, PixelColour{0x82}, PixelColour{0x83}, PixelColour{0x84}, PixelColour{0x85}, PixelColour{0x86}, PixelColour{0x87}}, {Colours::Purple}},
	{STR_COLOUR_PURPLE,     2, {PixelColour{0x88}, PixelColour{0x89}, PixelColour{0x8A}, PixelColour{0x8B}, PixelColour{0x8C}, PixelColour{0x8D}, PixelColour{0x8E}, PixelColour{0x8F}}, {Colours::Mauve}},
	{STR_COLOUR_ORANGE,     3, {PixelColour{0x40}, PixelColour{0xC0}, PixelColour{0xC1}, PixelColour{0xC2}, PixelColour{0xC3}, PixelColour{0xC4}, PixelColour{0xC5}, PixelColour{0x27}}, {Colours::Yellow, Colours::Cream}},
	{STR_COLOUR_BROWN,      1, {PixelColour{0x20}, PixelColour{0x21}, PixelColour{0x22}, PixelColour{0x23}, PixelColour{0x24}, PixelColour{0x25}, PixelColour{0x26}, PixelColour{0x27}}, {Colours::Cream}},
	{STR_COLOUR_GREY,       1, {PixelColour{0x04}, PixelColour{0x05}, PixelColour{0x06}, PixelColour{0x07}, PixelColour{0x08}, PixelColour{0x09}, PixelColour{0x0A}, PixelColour{0x0B}}, {Colours::White}},
	{STR_COLOUR_WHITE,      1, {PixelColour{0x08}, PixelColour{0x09}, PixelColour{0x0A}, PixelColour{0x0B}, PixelColour{0x0C}, PixelColour{0x0D}, PixelColour{0x0E}, PixelColour{0x0F}}, {Colours::Grey}},
};

EnumClassIndexContainer<std::vector<ColourSpec>, Colours> _colour_specs;
std::map<std::pair<Colours, Colours>, PaletteID> _colour_palettes;

void ResetColourSpecs()
{
	_colour_specs.clear();
	_colour_specs.assign(std::begin(_original_colour_specs), std::end(_original_colour_specs));

	_colour_palettes.clear();
}

/**
 * Get colour gradient palette index.
 * @param colour The colour.
 * @param shade The shade.
 * @returns palette index of colour.
 */
PixelColour GetColourGradient(Colours colour, Shade shade)
{
	assert(colour <= static_cast<Colours>(_colour_specs.size()));
	assert(shade <= Shade::End);
	return _colour_specs[colour].pc[shade];
}

static void ApplyRecolourToPalette(const ColourSpec &cs, uint8_t *p)
{
	for (Shade shade : EnumRange(Shade::End)) {
		*p++ = cs.pc[shade].p;
	}
}

PaletteID CreateRecolourPalette(Colours colour1, Colours colour2)
{
	PaletteID pal = AllocateDynamicSprite();

	InjectSprite(SpriteType::Recolour, pal, [colour1, colour2](SpriteAllocator &allocator) {
		static constexpr uint8_t PALETTE_INDEX_CC1_START = 0xC6;
		static constexpr uint8_t PALETTE_INDEX_CC2_START = 0x50;

		uint8_t *palette = allocator.Allocate<uint8_t>(257) + 1;
		std::iota(palette, palette + 256, 0);

		if (colour1 != Colours::Invalid) ApplyRecolourToPalette(_colour_specs[colour1], &palette[PALETTE_INDEX_CC1_START]);
		if (colour2 != Colours::Invalid) ApplyRecolourToPalette(_colour_specs[colour2], &palette[PALETTE_INDEX_CC2_START]);
	});

	return pal;
}

/**
 * Get recolour palette for a colour.
 * @param colour1 First colour.
 * @param colour2 Second colour.
 * @return Recolour palette.
 */
PaletteID GetColourPalette(Colours colour1, Colours colour2)
{
	std::pair<Colours, Colours> key{colour1, colour2};
	auto it = _colour_palettes.find(key);
	if (it != _colour_palettes.end()) return it->second;

	return _colour_palettes[key] = CreateRecolourPalette(colour1, colour2);
}

/**
 * Get name for a colour.
 * @param colour Colour.
 * @return Name of colour.
 */
StringID GetColourName(Colours colour)
{
	assert(colour <= static_cast<Colours>(_colour_specs.size()));
	return _colour_specs[colour].name;
}
