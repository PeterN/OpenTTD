/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundloader_type.h Types related to sound loaders. */

#ifndef SOUNDLOADER_TYPE_H
#define SOUNDLOADER_TYPE_H

#include "core/math_func.hpp"
#include "provider_manager.h"
#include "sound_type.h"

/** Base interface for a SoundLoader implementation. */
class SoundLoader : public PriorityBaseProvider<SoundLoader> {
public:
	SoundLoader(std::string_view name, std::string_view description, int priority) : PriorityBaseProvider<SoundLoader>(name, description, priority)
	{
		ProviderManager<SoundLoader>::Register(*this);
	}

	virtual ~SoundLoader()
	{
		ProviderManager<SoundLoader>::Unregister(*this);
	}

	virtual bool Load(SoundEntry &sound, bool new_format, std::vector<uint8_t> &data) = 0;
};

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

#endif /* SOUNDLOADER_TYPE_H */
