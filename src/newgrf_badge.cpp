/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge.cpp Functionality for NewGRF badges. */

#include "stdafx.h"
#include "newgrf_spritegroup.h"
#include "newgrf.h"
#include "newgrf_badge.h"
#include "newgrf_badge_type.h"
#include "stringfilter_type.h"
#include "strings_func.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_tick.h"
#include "zoom_func.h"
#include "window_gui.h"
#include "dropdown_type.h"
#include "dropdown_common_type.h"

#include "table/strings.h"

/* static */ std::unordered_map<BadgeLabel, Badge> Badge::specs;
/* static */ std::unordered_set<BadgeClass> Badge::classes;

static std::unordered_map<BadgeLabel, std::bitset<GSF_END>> _seen_badges;

/** Resolver for a badge scope. */
struct BadgeScopeResolver : public ScopeResolver {
	const Badge &badge;
	const std::optional<TimerGameCalendar::Date> introduction_date;

	/**
	 * Scope resolver of a badge.
	 * @param ro Surrounding resolver.
	 * @param badge Badge to resolve.
	 * @param introduction_date Introduction date of entity.
	 */
	BadgeScopeResolver(ResolverObject &ro, const Badge &badge, const std::optional<TimerGameCalendar::Date> introduction_date)
		: ScopeResolver(ro), badge(badge), introduction_date(introduction_date) { }

	uint32_t GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const override;
};

/* virtual */ uint32_t BadgeScopeResolver::GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const
{
	switch (variable) {
		case 0x40:
			if (this->introduction_date.has_value()) return this->introduction_date->base();
			return TimerGameCalendar::date.base();

		default: break;
	}

	available = false;
	return UINT_MAX;
}

/** Resolver of badges. */
struct BadgeResolverObject : public ResolverObject {
	BadgeScopeResolver self_scope;

	BadgeResolverObject(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, uint8_t relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->self_scope;
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

GrfSpecFeature BadgeResolverObject::GetFeature() const
{
	return GSF_BADGES;
}

uint32_t BadgeResolverObject::GetDebugID() const
{
	return this->self_scope.badge.label;
}

/**
 * Constructor of the badge resolver.
 * @param badge Badge being resolved.
 * @param feature GRF feature being used.
 * @param introduction_date Optional introduction date of entity.
 * @param callback Callback ID.
 * @param callback_param1 First parameter (var 10) of the callback.
 * @param callback_param2 Second parameter (var 18) of the callback.
 */
BadgeResolverObject::BadgeResolverObject(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, CallbackID callback, uint32_t callback_param1, uint32_t callback_param2)
		: ResolverObject(badge.grf_prop.grffile, callback, callback_param1, callback_param2), self_scope(*this, badge, introduction_date)
{
	assert(feature <= GSF_END);
	if (this->self_scope.badge.grf_prop.spritegroup[feature] == nullptr) feature = GSF_END;
	this->root_spritegroup = badge.grf_prop.spritegroup[feature];
}

uint32_t GetBadgeVariableResult(std::span<const BadgeLabel> badges, uint32_t parameter)
{
	parameter = BSWAP32(parameter);
	for (const auto &label : badges) {
		if (label == parameter) return 0x01;
	}
	return 0x00;
}

/**
 * Reset badges to the default state.
 */
void ResetBadges()
{
	Badge::specs.clear();
	Badge::classes.clear();
	_seen_badges.clear();
}

void FinaliseBadges()
{
	/* Apply used information to defined badges. */
	for (const auto &pair : _seen_badges) {
		Badge *badge = Badge::Get(pair.first);
		if (badge == nullptr) continue;
		badge->used = pair.second;
	}
}

void MarkBadgeSeen(BadgeLabel label, GrfSpecFeature feature)
{
	_seen_badges[label].set(feature);
}

/**
 * Append copyable badges from a list onto another.
 * Badges must exist and be marked with the Copy flag.
 * @param dst Destination badge list.
 * @param src Source badge list.
 * @param feature Feature of list.
 */
void AppendCopyableBadgeList(std::vector<BadgeLabel> &dst, std::span<const BadgeLabel> src, GrfSpecFeature feature)
{
	for (const auto &label : src) {
		/* Is badge already present? */
		if (std::find(std::begin(dst), std::end(dst), label) != std::end(dst)) continue;

		/* Is badge copyable? */
		const Badge *badge = Badge::Get(label);
		if (badge == nullptr) continue;
		if ((badge->flags & BadgeFlags::Copy) == BadgeFlags::None) continue;

		dst.push_back(label);
		MarkBadgeSeen(label, feature);
	}
}

class UsedBadgeClasses {
public:
	/**
	 * Create a set of present badge labels for a feature.
	 * @param feature GRF feature being used.
	 */
	UsedBadgeClasses(GrfSpecFeature feature)
	{
		/* Get set of badge classes for this feature */
		for (const auto &pair : Badge::specs) {
			if (!pair.second.used.test(feature)) continue;

			BadgeClass badge_class = GetBadgeClass(pair.first);
			SetBit(this->classes[badge_class / BITMAP_SIZE], badge_class % BITMAP_SIZE);
		}
	}

	/**
	 * Iterate the set of present badge labels.
	 * @param func Function to call for each present badge label.
	 */
	template <typename Func>
	void Iterate(Func func)
	{
		size_t offs = 0;
		for (const auto &bitmap : classes) {
			for (uint8_t idx : SetBitIterator(bitmap)) {
				func(static_cast<BadgeClass>(idx + offs));
			}
			offs += BITMAP_SIZE;
		}
	}

private:
	/* Storage for bitmap of used classes. */
	using BitmapStorage = size_t;
	static constexpr size_t BITMAP_SIZE = std::numeric_limits<BitmapStorage>::digits;
	static constexpr size_t BADGE_CLASSES = std::numeric_limits<BadgeClass>::max() + 1;

	std::array<BitmapStorage, BADGE_CLASSES / BITMAP_SIZE> classes{};
};

GUIBadgeClassList GetBadgeClassList(GrfSpecFeature feature)
{
	GUIBadgeClassList list;

	/* Set up storage for a bitmap of used classes. */
	UsedBadgeClasses used(feature);

	used.Iterate([&list](BadgeClass badge_class) {
		Dimension size = GetBadgeNominalDimension(badge_class, GSF_TRAINS);
		if (size.width == 0) return;

		list.push_back({badge_class, size, 0});
	});

	return list;
}

void FilterByBadge(StringFilter &filter, std::span<const BadgeLabel> badges)
{
	for (const auto &badge_label : badges) {
		const Badge *badge = Badge::Get(badge_label);
		if (badge == nullptr) continue;
		if (badge->name == 0) continue;

		filter.AddLine(GetString(badge->name));
	}
}

/**
 * Get sprite for the given badge.
 * @param badge Badge being queried.
 * @param feature GRF feature being used.
 * @param introduction_date Introduction date of the item, if it has one.
 * @returns Custom sprite to draw, or \c 0 if not available.
 */
SpriteID GetBadgeSprite(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date)
{
	BadgeResolverObject object(badge, feature, introduction_date);
	const SpriteGroup *group = object.Resolve();
	if (group == nullptr) return 0;

	return group->GetResult() + (TimerGameTick::counter % group->GetNumResults());
}

/**
 * Get sprite for the given badge label.
 * @param badge Badge label being queried.
 * @param feature GRF feature being used.
 * @param introduction_date Introduction date of the item, if it has one.
 * @returns Custom sprite to draw, or \c 0 if not available.
 */
SpriteID GetBadgeSprite(BadgeLabel label, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date)
{
	const Badge *badge = Badge::Get(label);
	if (badge == nullptr) return 0;

	return GetBadgeSprite(*badge, feature, introduction_date);
}

static constexpr uint MAX_BADGE_HEIGHT = 12; ///< Maximmal height of a badge sprite.
static constexpr uint MAX_BADGE_WIDTH = MAX_BADGE_HEIGHT * 2; ///< Maximal width.

/**
 * Get the largest badge size (within limits) for a badge class.
 * @param badge_class Badge class.
 * @param feature Feature being used.
 * @returns Largest base size of the badge class for the feature.
 */
Dimension GetBadgeNominalDimension(BadgeClass badge_class, GrfSpecFeature feature)
{
	Dimension d = { 0, MAX_BADGE_HEIGHT };

	for (const auto &pair : Badge::specs) {
		if (GetBadgeClass(pair.first) != badge_class) continue;

		SpriteID sprite = GetBadgeSprite(pair.second, feature, std::nullopt);
		if (sprite == 0) continue;

		d.width = std::max(d.width, GetSpriteSize(sprite, nullptr, ZOOM_LVL_NORMAL).width);
		if (d.width > MAX_BADGE_WIDTH) break;
	}

	d.width = std::min(d.width, MAX_BADGE_WIDTH);
	return d;
}

/**
 * Draw names for a list of badge labels.
 * @param r Rect to draw in.
 * @param badges List of badges.
 * @param feature GRF feature being used.
 * @returns Vertical position after drawing is complete.
 */
int DrawBadgeNameList(Rect r, std::span<const BadgeLabel> badges, GrfSpecFeature)
{
	for (const auto &label : badges) {
		const Badge *badge = Badge::Get(label);
		if (badge == nullptr) continue;

		const Badge *class_badge = Badge::Get(GetBadgeClassDescriptorLabel(GetBadgeClass(label)));
		if (class_badge == nullptr) continue;

		SetDParam(0, class_badge->name);
		SetDParam(1, badge->name);
		r.top = DrawStringMultiLine(r, STR_BADGE_NAME, TC_BLACK);
	}

	return r.top;
}

void DrawBadgeColumn(int column_group, Rect r, const GUIBadgeClassList &badge_classes, std::span<const BadgeLabel> primary_badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date)
{
	bool rtl = _current_text_dir == TD_RTL;
	for (const auto &badge_class : badge_classes) {
		if (badge_class.column_group != column_group) continue;

		int width = ScaleGUITrad(badge_class.size.width);
		for (const auto &badge_label : primary_badges) {
			if (GetBadgeClass(badge_label) != badge_class.badge_class) continue;

			SpriteID badge_sprite = GetBadgeSprite(badge_label, feature, introduction_date);
			if (badge_sprite == 0) continue;

			DrawSpriteIgnorePadding(badge_sprite, PAL_NONE, r.WithWidth(width, rtl), SA_CENTER);
			break;
		}

		r = r.Indent(width + WidgetDimensions::scaled.hsep_normal, rtl);
	}
}

/** Drop down element that draws a list of badges. */
template <class TBase, bool TEnd = true, FontSize TFs = FS_NORMAL>
class DropDownBadges : public TBase {
	const std::span<const BadgeLabel> badges;
	const GrfSpecFeature feature;
	const std::optional<TimerGameCalendar::Date> introduction_date;
	Dimension dim{};
public:
	template <typename... Args>
	explicit DropDownBadges(std::span<const BadgeLabel> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, Args&&... args)
		: TBase(std::forward<Args>(args)...), badges(badges), feature(feature), introduction_date(introduction_date)
	{
		for (const auto &badge_label : badges) {
			SpriteID sprite = GetBadgeSprite(badge_label, feature, std::nullopt);
			if (sprite == 0) continue;

			Dimension d = GetSpriteSize(sprite);
			dim.width += d.width + WidgetDimensions::scaled.hsep_normal;
			dim.height = MAX_BADGE_HEIGHT;
		}
	}

	uint Height() const override { return std::max<uint>(this->dim.height, this->TBase::Height()); }
	uint Width() const override { return this->dim.width + WidgetDimensions::scaled.hsep_wide + this->TBase::Width(); }

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);

		Rect ir = r;
		for (const auto &badge_label : this->badges) {
			SpriteID sprite = GetBadgeSprite(badge_label, this->feature, this->introduction_date);
			if (sprite == 0) continue;

			Dimension d = GetSpriteSize(sprite);
			DrawSpriteIgnorePadding(sprite, PAL_NONE, ir.WithWidth(d.width, rtl), SA_CENTER);

			ir = ir.Indent(d.width + WidgetDimensions::scaled.hsep_normal, rtl);
		}

		this->TBase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), sel, bg_colour);
	}
};

using DropDownListBadgeItem = DropDownBadges<DropDownListStringItem>;
using DropDownListBadgeIconItem = DropDownBadges<DropDownListIconItem>;

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(std::span<const BadgeLabel> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, StringID str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeItem>(badges, feature, introduction_date, str, value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(std::span<const BadgeLabel> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, const Dimension &dim, SpriteID sprite, PaletteID palette, StringID str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeIconItem>(badges, feature, introduction_date, dim, sprite, palette, str, value, masked, shaded);
}
