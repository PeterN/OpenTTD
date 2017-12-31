/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 40bpp_optimized.hpp Optimized 40 bpp blitter. */

#ifndef BLITTER_40BPP_OPTIMIZED_HPP
#define BLITTER_40BPP_OPTIMIZED_HPP

#ifdef WITH_OPENGL

#include "32bpp_optimized.hpp"
#include "../video/video_driver.hpp"

/** The optimized 40 bpp blitter (for OpenGL video driver). */
class Blitter_40bppAnim : public Blitter_32bppOptimized {
public:

	///* virtual */ void *MoveTo(void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 colour);
	/* virtual */ void DrawRect(void *video, int width, int height, uint8 colour);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height);
	/* virtual */ void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y);
	/* virtual */ void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom);
	/* virtual */ void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal);
	/* virtual */ Sprite *Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator);
	/* virtual */ int BufferSize(int width, int height);
	/* virtual */ Blitter::PaletteAnimation UsePaletteAnimation();
	/* virtual */ bool NeedsAnimationBuffer();

	/* virtual */ const char *GetName() { return "40bpp-anim"; }
	/* virtual */ int GetBytesPerPixel() { return 5; }

	template <BlitterMode mode> void Draw(const Blitter::BlitterParams *bp, ZoomLevel zoom);

protected:
	static inline Colour RealizeBlendedColour(uint8 anim, Colour c)
	{
		return anim != 0 ? AdjustBrightness(LookupColourInPalette(anim), GetColourBrightness(c)) : c;
	}

};

/** Factory for the 40 bpp animated blitter (for OpenGL). */
class FBlitter_40bppAnim : public BlitterFactory {
protected:
	/* virtual */ bool IsUsable() const
	{
		return VideoDriver::GetInstance() == NULL || VideoDriver::GetInstance()->HasAnimBuffer();
	}

public:
	FBlitter_40bppAnim() : BlitterFactory("40bpp-anim", "40bpp Animation Blitter (OpenGL)") {}
	/* virtual */ Blitter *CreateInstance() { return new Blitter_40bppAnim(); }
};

#endif /* WITH_OPENGL */

#endif /* BLITTER_40BPP_OPTIMIZED_HPP */
