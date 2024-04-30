/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file soundloader_type.h Types related to sound loaders. */

#ifndef SOUNDLOADER_TYPE_H
#define SOUNDLOADER_TYPE_H

class SoundLoader {
public:
	SoundLoader(const std::string &name, int priority);
	virtual ~SoundLoader();

	virtual bool Load(SoundEntry &sound, bool new_format, std::vector<uint8_t> &data) = 0;

	using SoundLoaders = std::vector<SoundLoader *>;
	static SoundLoaders &GetSoundLoaders();

protected:
	static void NormaliseInt8(std::span<uint8_t> buffer);
	static void NormaliseInt16(std::span<uint8_t> buffer);

private:
	std::string name;
	int priority;
};

#endif /* SOUNDLOADER_TYPE_H */
