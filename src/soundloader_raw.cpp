/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundloader_raw.cpp Loading of raw sounds. */

#include "stdafx.h"
#include "random_access_file_type.h"
#include "sound_type.h"
#include "soundloader_type.h"

#include "safeguards.h"

/** Raw PCM sound loader, used as a fallback if other sound loaders fail. */
class SoundLoader_Raw : public SoundLoader {
public:
	SoundLoader_Raw() : SoundLoader("raw", INT_MAX) {}

	bool Load(SoundEntry &sound, bool new_format, std::vector<uint8_t> &data) override
	{
		/* No raw sounds are permitted with a new format file. */
		if (new_format) return false;

		/* Special case for the jackhammer sound (name in Windows sample.cat is "Corrupt sound")
		 * It's no RIFF file, but raw PCM data. */
		sound.channels = 1;
		sound.rate = 11025;
		sound.bits_per_sample = 8;

		/* Allocate an extra sample to ensure the runtime resampler doesn't go out of bounds.*/
		data.reserve(sound.file_size + 1);
		data.resize(sound.file_size);
		sound.file->ReadBlock(data.data(), data.size());

		NormaliseInt8(data);

		return true;
	}
};

static SoundLoader_Raw _sound_loader_raw;
