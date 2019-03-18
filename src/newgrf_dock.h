/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_dock.h Functions related to NewGRF docks. */

#ifndef NEWGRF_DOCK_H
#define NEWGRF_DOCK_H

#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"
#include "newgrf_town.h"
#include "economy_func.h"
#include "date_type.h"
#include "dock_type.h"
#include "newgrf_animation_type.h"
#include "newgrf_class.h"
#include "newgrf_commons.h"

/** Various dock behaviours. */
enum DockFlags {
	DOCK_FLAG_NONE               =       0, ///< Just nothing.
	DOCK_FLAG_BUILT_ON_WATER     = 1 <<  3, ///< Dock can be built on water (not required).
	DOCK_FLAG_HAS_NO_FOUNDATION  = 1 <<  5, ///< Do not display foundations when on a slope.
	DOCK_FLAG_ANIMATION          = 1 <<  6, ///< Dock has animated tiles.
	DOCK_FLAG_2CC_COLOUR         = 1 <<  8, ///< Dock wants 2CC colour mapping.
	DOCK_FLAG_NOT_ON_LAND        = 1 <<  9, ///< Dock can not be on land, implicitly sets #DOCK_FLAG_BUILT_ON_WATER.
	DOCK_FLAG_DRAW_WATER         = 1 << 10, ///< Dock wants to be drawn on water.
	DOCK_FLAG_ANIM_RANDOM_BITS   = 1 << 12, ///< Dock wants random bits in "next animation frame" callback.
};
DECLARE_ENUM_AS_BIT_SET(DockFlags)

void ResetDocks();

/** Class IDs for docks. */
enum DockClassID {
	DOCK_CLASS_BEGIN   =    0, ///< The lowest valid value
	DOCK_CLASS_MAX     = 0xFF, ///< Maximum number of classes.
	INVALID_DOCK_CLASS = 0xFF, ///< Class for the less fortunate.
};
/** Allow incrementing of DockClassID variables */
DECLARE_POSTFIX_INCREMENT(DockClassID)

/** An dock that isn't use for transport, industries or houses.
 * @note If you change this struct, adopt the initialization of
 * default docks in table/dock_land.h
 */
struct DockSpec {
	/* 2 because of the "normal" and "buy" sprite stacks. */
	GRFFilePropsBase<2> grf_prop; ///< Properties related the the grf file
	DockClassID cls_id;           ///< The class to which this spec belongs.
	StringID name;                ///< The name for this dock.

	uint8 climate;                ///< In which climates is this dock available?
	uint8 size;                   ///< The size of this docks; low nibble for X, high nibble for Y.
	uint8 build_cost_multiplier;  ///< Build cost multiplier per tile.
	uint8 clear_cost_multiplier;  ///< Clear cost multiplier per tile.
	Date introduction_date;       ///< From when can this dock be built.
	Date end_of_life_date;        ///< When can't this dock be built anymore.
	DockFlags flags;              ///< Flags/settings related to the dock.
	AnimationInfo animation;      ///< Information about the animation.
	uint16 callback_mask;         ///< Bitmask of requested/allowed callbacks.
	uint8 height;                 ///< The height of this structure, in heightlevels; max MAX_TILE_HEIGHT.
	uint8 views;                  ///< The number of views.
//	uint8 generate_amount;        ///< Number of docks which are attempted to be generated per 256^2 map during world generation.
	bool enabled;                 ///< Is this spec enabled?

	/**
	 * Get the cost for building a structure of this type.
	 * @return The cost for building.
	 */
	Money GetBuildCost() const { return GetPrice(PR_BUILD_STATION_DOCK, this->build_cost_multiplier, this->grf_prop.grffile, 0); }

	/**
	 * Get the cost for clearing a structure of this type.
	 * @return The cost for clearing.
	 */
	Money GetClearCost() const { return GetPrice(PR_CLEAR_STATION_DOCK, this->clear_cost_multiplier, this->grf_prop.grffile, 0); }

	bool IsEverAvailable() const;
	bool WasEverAvailable() const;
	bool IsAvailable() const;
	uint Index() const;

	static const DockSpec *Get(DockType index);
	static const DockSpec *GetByTile(TileIndex tile);
};

/** Dock scope resolver. */
struct DockScopeResolver : public ScopeResolver {
	struct Station *st; ///< The station the callback is ran for.
	TileIndex tile;     ///< The tile related to the dock.
	uint8 view;         ///< The view of the dock.

	/**
	 * Constructor of an dock scope resolver.
	 * @param ro Surrounding resolver.
	 * @param obj Dock being resolved.
	 * @param tile %Tile of the dock.
	 * @param view View of the dock.
	 */
	DockScopeResolver(ResolverObject &ro, Station *st, TileIndex tile, uint8 view = 0)
		: ScopeResolver(ro), st(st), tile(tile), view(view)
	{
	}

	/* virtual */ uint32 GetRandomBits() const;
	/* virtual */ uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;
};

/** A resolver dock to be used with feature 0F spritegroups. */
struct DockResolverObject : public ResolverObject {
	DockScopeResolver dock_scope;  ///< The dock scope resolver.
	TownScopeResolver *town_scope; ///< The town scope resolver (created on the first call).

	DockResolverObject(const DockSpec *spec, Station *st, TileIndex tile, uint8 view = 0,
			CallbackID callback = CBID_NO_CALLBACK, uint32 param1 = 0, uint32 param2 = 0);
	~DockResolverObject();

	TownScopeResolver *GetTown();

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF:
				return &this->dock_scope;

			case VSG_SCOPE_PARENT: {
			    TownScopeResolver *tsr = this->GetTown();
			    if (tsr != NULL) return tsr;
			    FALLTHROUGH;
			}

			default:
			    return ResolverObject::GetScope(scope, relative);
		}
	}
};

/** Struct containing information relating to dock classes. */
typedef NewGRFClass<DockSpec, DockClassID, DOCK_CLASS_MAX> DockClass;

uint16 GetDockCallback(CallbackID callback, uint32 param1, uint32 param2, const DockSpec *spec, Station *st, TileIndex tile, uint8 view = 0);

void DrawNewDockTile(TileInfo *ti, const DockSpec *spec);
void DrawNewDockTileInGUI(int x, int y, const DockSpec *spec, uint8 view);
void AnimateNewDockTile(TileIndex tile);
void TriggerDockTileAnimation(Station *st, TileIndex tile, DockAnimationTrigger trigger, const DockSpec *spec);
void TriggerDockAnimation(Station *st, DockAnimationTrigger trigger, const DockSpec *spec);

int AllocateSpecToDock(const DockSpec *dockspec, Station *st, bool exec);
void DeallocateSpecFromDock(Station *st, byte specindex);

#endif /* NEWGRF_DOCK_H */
