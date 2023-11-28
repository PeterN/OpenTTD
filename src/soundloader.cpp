/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundloader.cpp Handling of loading sounds. */

#include "stdafx.h"
#include "core/endian_func.hpp"
#include "core/math_func.hpp"
#include "debug.h"
#include "sound_type.h"
#include "soundloader_type.h"
#include "soundloader_func.h"
#include "mixer.h"
#include "newgrf_sound.h"
#include "random_access_file_type.h"

#ifdef WITH_SOXR
#include <soxr.h>
#include <thread>
#endif /* WITH_SOXR */

#include "safeguards.h"

/**
 * Reinterpret a span from one type to another type.
 * @tparam Ttarget Target type to convert to.
 * @tparam Tsource Source type to convert from (deduced from span.)
 * @param s A span of Tsource type.
 * @return A span of Ttarget type.
 * @pre Span must be aligned to the target type's alignment.
 */
template <typename Ttarget, typename Tsource>
std::span<Ttarget> ReinterpretSpan(std::span<Tsource> s)
{
	assert(reinterpret_cast<uintptr_t>((s.data())) % std::alignment_of_v<Ttarget> == 0);
	assert(s.size_bytes() == Align(s.size_bytes(), std::alignment_of_v<Ttarget>));
	return std::span(reinterpret_cast<Ttarget *>(&*std::begin(s)), reinterpret_cast<Ttarget *>(&*std::end(s)));
}

/**
 * Construct the sound loader, and register it.
 * @param name The name of the sound loader.
 * @param priority The load order priority of the sound loader.
 */
SoundLoader::SoundLoader(const std::string &name, int priority) : name(name), priority(priority)
{
	SoundLoaders &sound_loaders = GetSoundLoaders();
	/* Insert sound loader according to priority. */
	auto it = std::find_if(std::begin(sound_loaders), std::end(sound_loaders), [priority](const SoundLoader *loader) { return loader->priority > priority; });
	sound_loaders.insert(it, this);
}

SoundLoader::~SoundLoader()
{
	SoundLoaders &sound_loaders = GetSoundLoaders();
	sound_loaders.erase(std::find(std::begin(sound_loaders), std::end(sound_loaders), this));
	if (sound_loaders.empty()) delete &sound_loaders;
}

/**
 * Get the currently known sound loaders.
 * @return The known sound loaders.
 */
/* static */ SoundLoader::SoundLoaders &SoundLoader::GetSoundLoaders()
{
	static SoundLoaders &s_sound_loaders = *new SoundLoaders();
	return s_sound_loaders;
}

/**
 * Convert unsigned 8-bit samples to signed 8-bit samples.
 * @param buffer Buffer of samples to convert.
 */
/* static */ void SoundLoader::NormaliseInt8(std::span<uint8_t> buffer)
{
	/* Convert 8-bit samples from unsigned to signed. */
	for (auto &sample : buffer) {
		sample = sample - 128;
	}
}

/**
 * Convert signed 16-bit samples from little endian to host endian.
 * @param buffer Buffer of samples to convert.
 */
/* static */ void SoundLoader::NormaliseInt16(std::span<uint8_t> buffer)
{
	/* Convert samples from little endian. On a LE system this will do nothing. */
	auto s16buffer = ReinterpretSpan<int16_t>(buffer);
	for (auto &sample : s16buffer) {
		sample = FROM_LE16(sample);
	}
}

#ifdef WITH_SOXR

static void ConvertSampleRate(SoundEntry &sound, uint32_t play_rate)
{
	/* As we always convert with the same configuration, these are static so they only need to be set up once.
	 * Always 16 bit input and 16 bit output.
	 * Highest quality.
	 * Multi-threaded. */
	static const soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
	static const soxr_quality_spec_t quality = soxr_quality_spec(SOXR_VHQ, 0);
	static const soxr_runtime_spec_t runtime = soxr_runtime_spec(std::thread::hardware_concurrency());

	std::vector<uint8_t> tmp;
	if (sound.bits_per_sample == 16) {
		/* Move from sound data to temporary buffer. */
		sound.data->swap(tmp);
	} else {
		/* SoxR cannot resample 8-bit audio, so convert from 8-bit to 16-bit into temporary buffer. */
		tmp.resize(sound.data->size() * sizeof(int16_t));
		auto out = ReinterpretSpan<int16_t>(std::span(tmp)).begin();
		for (auto in = sound.data->begin(); in != sound.data->end(); ++in, ++out) {
			*out = *in * 256;
		}
		sound.bits_per_sample = 16;
	}

	/* Resize buffer ensuring it is correctly aligned. */
	uint align = sound.channels * sound.bits_per_sample / 8;
	sound.data->resize(Align(tmp.size() * play_rate / sound.rate, align));

	soxr_error_t error = soxr_oneshot(sound.rate, play_rate, sound.channels,
		tmp.data(), tmp.size() / align, nullptr,
		sound.data->data(), sound.data->size() / align, nullptr,
		&io, &quality, &runtime);

	if (error != nullptr) {
		/* Could not resample, try using the original data as-is instead. */
		Debug(misc, 0, "Failed to resample: {}", soxr_strerror(error));
		sound.data->swap(tmp);
	} else {
		sound.rate = play_rate;
	}
}

#endif /* WITH_SOXR */

bool LoadSoundData(SoundEntry &sound, bool new_format, SoundID sound_id, const std::string &name)
{
	/* Check for valid sound size. */
	if (sound.file_size == 0 || sound.file_size > SIZE_MAX - 2) return false;

	size_t pos = sound.file->GetPos();
	sound.data = std::make_shared<std::vector<uint8_t>>();
	for (auto &sound_loader : SoundLoader::GetSoundLoaders()) {
		sound.file->SeekTo(pos, SEEK_SET);
		if (sound_loader->Load(sound, new_format, *sound.data)) break;
	}

	if (sound.data->empty()) {
		Debug(grf, 0, "LoadSound [{}]: Failed to load sound '{}' for slot {}", sound.file->GetSimplifiedFilename(), name, sound_id);
		return false;
	}

	assert(sound.bits_per_sample == 8 || sound.bits_per_sample == 16);
	assert(sound.channels == 1);
	assert(sound.rate != 0);

	Debug(grf, 2, "LoadSound [{}]: channels {}, sample rate {}, bits per sample {}, length {}", sound.file->GetSimplifiedFilename(), sound.channels, sound.rate, sound.bits_per_sample, sound.file_size);

#ifdef WITH_SOXR

	/* Convert sample rate? */
	uint32_t play_rate = MxGetRate();
	if (play_rate != sound.rate) ConvertSampleRate(sound, play_rate);

#endif /* WITH_SOXR */

	/* Mixer always requires an extra sample at the end for the built-in linear resampler. */
	sound.data->resize(sound.data->size() + sound.channels * sound.bits_per_sample / 8);
	sound.data->shrink_to_fit();

	return true;
}

static bool LoadBasesetSound(SoundEntry &sound, bool new_format, SoundID sound_id)
{
	sound.file->SeekTo(sound.file_offset, SEEK_SET);

	/* Read name of sound for diagnostics. */
	size_t name_len = sound.file->ReadByte();
	std::string name(name_len, '\0');
	sound.file->ReadBlock(name.data(), name_len);

	return LoadSoundData(sound, new_format, sound_id, name);
}

bool LoadSound(SoundEntry &sound, SoundID sound_id)
{
	switch (sound.source) {
		case SoundSource::BasesetOldFormat: return LoadBasesetSound(sound, false, sound_id);
		case SoundSource::BasesetNewFormat: return LoadBasesetSound(sound, true, sound_id);
		case SoundSource::NewGRF: return LoadNewGRFSound(sound, sound_id);
		default: NOT_REACHED();
	}
}
