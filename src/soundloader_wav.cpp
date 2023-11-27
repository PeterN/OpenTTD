/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundloader_wav.cpp Loading of wav sounds. */

#include "stdafx.h"
#include "core/bitmath_func.hpp"
#include "core/endian_func.hpp"
#include "random_access_file_type.h"
#include "sound_type.h"
#include "soundloader_type.h"

#include "safeguards.h"

/** Wav file (RIFF/WAVE) sound louder. */
class SoundLoader_Wav : public SoundLoader {
public:
	SoundLoader_Wav() : SoundLoader("wav", "Wav sound loader", 0) {}

	static constexpr uint16_t DEFAULT_SAMPLE_RATE = 11025;

	bool Load(SoundEntry &sound, bool new_format, std::vector<uint8_t> &data) override
	{
		RandomAccessFile &file = *sound.file;

		/* Check RIFF/WAVE header. */
		if (file.ReadDword() != BSWAP32('RIFF')) return false;
		file.ReadDword(); // Skip data size
		if (file.ReadDword() != BSWAP32('WAVE')) return false;

		/* Read riff tags */
		for (;;) {
			uint32_t tag = file.ReadDword();
			uint32_t size = file.ReadDword();

			if (tag == BSWAP32('fmt ')) {
				uint16_t format = file.ReadWord();
				if (format != 1) return false; // File must be uncompressed PCM

				sound.channels = file.ReadWord();
				if (sound.channels != 1) return false; // File must be mono.

				sound.rate = file.ReadDword();
				if (!new_format) sound.rate = DEFAULT_SAMPLE_RATE; // All old samples should be played at 11025 Hz.

				file.ReadDword(); // avg bytes per second
				file.ReadWord();  // alignment

				sound.bits_per_sample = file.ReadWord();
				if (sound.bits_per_sample != 8 && sound.bits_per_sample != 16) return false; // File must be 8 or 16 BPS.
			} else if (tag == BSWAP32('data')) {
				uint align = sound.channels * sound.bits_per_sample / 8;
				if (Align(size, align) != size) return false; // Ensure length is aligned correctly for channels and BPS.

				sound.file_size = size;
				if (size == 0) return true; // No need to continue.

				/* Allocate an extra sample to ensure the runtime resampler doesn't go out of bounds. */
				data.reserve(sound.file_size + align);
				data.resize(sound.file_size);
				file.ReadBlock(std::data(data), std::size(data));

				switch (sound.bits_per_sample) {
					case 8:
						/* Convert 8-bit samples from unsigned to signed. */
						for (auto &sample : data) {
							sample = sample - 128;
						}
						break;

					case 16:
						/* Convert samples from little endian. On a little-endian system this will do nothing. */
						for (auto &sample : ReinterpretSpan<int16_t>(std::span(data))) {
							sample = FROM_LE16(sample);
						}
						break;

					default: NOT_REACHED();
				}

				return true;
			} else {
				sound.file_size = 0;
				break;
			}
		}

		return false;
	}
};

static SoundLoader_Wav s_sound_loader_wav;
