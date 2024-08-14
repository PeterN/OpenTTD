/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge.h Functions related to NewGRF badges. */

#ifndef NEWGRF_BADGE_H
#define NEWGRF_BADGE_H

#include "dropdown_type.h"
#include "newgrf.h"
#include "newgrf_badge_type.h"
#include "newgrf_commons.h"
#include "strings_type.h"
#include "timer/timer_game_calendar.h"

#include <bitset>
#include <unordered_set>

struct Badge {
public:
	BadgeLabel label; ///< Unique label.
	BadgeFlags flags;

	StringID name; ///< Short name.
	StringID description; ///< Long description.

	std::bitset<GSF_END> used; ///< Bitmask of which features use this badge.

	GRFFilePropsBase<GSF_END + 1> grf_prop; ///< Sprite information.

	static std::unordered_map<BadgeLabel, Badge> specs;
	static std::unordered_set<BadgeClass> classes;

	static inline Badge &GetOrCreate(BadgeLabel label)
	{
		return Badge::specs[label];
	}

	static inline Badge *Get(BadgeLabel label)
	{
		auto it = Badge::specs.find(label);
		if (it == std::end(Badge::specs)) return nullptr;
		return &it->second;
	}
};

struct GUIBadgeClass {
	BadgeClass badge_class;
	Dimension size;
	int column_group; ///< Column group in UI. 0 = left, 1 = centre, 2 = right.
};

using GUIBadgeClassList = std::vector<GUIBadgeClass>;

void MarkBadgeSeen(BadgeLabel label, GrfSpecFeature feature);
void AppendCopyableBadgeList(std::vector<BadgeLabel> &dst, std::span<const BadgeLabel> src, GrfSpecFeature feature);

void ResetBadges();
void FinaliseBadges();

SpriteID GetBadgeSprite(BadgeLabel label, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date);
SpriteID GetBadgeSprite(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date);
Dimension GetBadgeNominalDimension(BadgeClass badge_class, GrfSpecFeature feature);
GUIBadgeClassList GetBadgeClassList(GrfSpecFeature feature);

uint32_t GetBadgeVariableResult(std::span<const BadgeLabel> badges, uint32_t parameter);
int DrawBadgeNameList(Rect r, std::span<const BadgeLabel> badges, GrfSpecFeature feature);
void DrawBadgeColumn(int column_group, Rect r, const GUIBadgeClassList &badge_classes, std::span<const BadgeLabel> primary_badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date);

/**
 * Get the badge class of a badge label.
 * @param label Label to get class of.
 * @returns Badge class of label.
 */
inline BadgeClass GetBadgeClass(BadgeLabel label)
{
	return static_cast<BadgeClass>(GB(label, 24, 8));
}

/**
 * Get the descriptor label for a badge class.
 * @param badge_class Badge class.
 * @returns Badge label corresponding to class descriptor label.
 */
inline BadgeLabel GetBadgeClassDescriptorLabel(BadgeClass badge_class)
{
	return static_cast<BadgeLabel>(badge_class << 24);
}

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(std::span<const BadgeLabel> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, StringID str, int value, bool masked = false, bool shaded = false);
std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(std::span<const BadgeLabel> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, const Dimension &dim, SpriteID sprite, PaletteID palette, StringID str, int value, bool masked = false, bool shaded = false);

void FilterByBadge(struct StringFilter &filter, std::span<const BadgeLabel> badges);

#endif /* NEWGRF_BADGE_H */
