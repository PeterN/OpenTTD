/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge_type.h Types related to NewGRF badges. */

#ifndef NEWGRF_BADGE_TYPE_H
#define NEWGRF_BADGE_TYPE_H

#include "core/enum_type.hpp"

using BadgeLabel = uint32_t;
using BadgeClass = uint8_t;

enum class BadgeFlags : uint8_t {
    None = 0,
    Copy = (1U << 0), ///< Copy badge to related things.
};

DECLARE_ENUM_AS_BIT_SET(BadgeFlags);

#endif /* NEWGRF_BADGE_TYPE_H */
