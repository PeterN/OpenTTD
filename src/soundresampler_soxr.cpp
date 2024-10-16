/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundresampler_soxr.cpp SOXR sound resampler. */

#include "stdafx.h"
#include "core/math_func.hpp"
#include "debug.h"
#include "sound_type.h"
#include "soundloader_type.h"

#include <soxr.h>
#include <thread>

#include "safeguards.h"

class SoundResampler_Soxr : public SoundResampler {
public:
	SoundResampler_Soxr() : SoundResampler("soxr", "SOXR sound resampler", 0) {}

	static constexpr int BITS_PER_BYTE = 8;
	static constexpr int SOXR_BITS_PER_SAMPLE = 16;

	bool Resample(SoundEntry &sound, uint32_t play_rate) override
	{
		/* We always convert with the same configuration, these are static so they only need to be set up once. */
		static const soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I); // 16-bit input and output.
		static const soxr_quality_spec_t quality = soxr_quality_spec(SOXR_VHQ, 0); // Use 'Very high quality'.
		static const soxr_runtime_spec_t runtime = soxr_runtime_spec(std::thread::hardware_concurrency()); // Enable multi-threading.

		/* The sound data to work on. */
		std::vector<uint8_t> &data = *sound.data;

		std::vector<uint8_t> tmp;
		if (sound.bits_per_sample == SOXR_BITS_PER_SAMPLE) {
			/* No conversion necessary so just move from sound data to temporary buffer. */
			data.swap(tmp);
		} else {
			/* SoxR cannot resample 8-bit audio, so convert from 8-bit to 16-bit into temporary buffer. */
			tmp.resize(std::size(data) * sizeof(int16_t));

			auto in = ReinterpretSpan<int8_t>(std::span(data));
			auto out = ReinterpretSpan<int16_t>(std::span(tmp));
			std::transform(std::begin(in), std::end(in), std::begin(out), [](const auto &a) { return a * 256; });

			sound.bits_per_sample = SOXR_BITS_PER_SAMPLE;
		}

		/* Resize buffer ensuring it is correctly aligned. */
		uint align = sound.channels * sound.bits_per_sample / BITS_PER_BYTE;
		data.resize(Align(std::size(tmp) * play_rate / sound.rate, align));

		soxr_error_t error = soxr_oneshot(sound.rate, play_rate, sound.channels,
			std::data(tmp), std::size(tmp) / align, nullptr,
			std::data(data), std::size(data) / align, nullptr,
			&io, &quality, &runtime);

		if (error != nullptr) {
			/* Could not resample, try using the original data as-is without resampling instead. */
			Debug(misc, 0, "Failed to resample: {}", soxr_strerror(error));
			data.swap(tmp);
		} else {
			sound.rate = play_rate;
		}

		return true;
	}
};

static SoundResampler_Soxr s_sound_resampler_soxr;
