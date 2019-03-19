/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_dock.cpp Handling of dock NewGRFs. */

#include "stdafx.h"
#include "company_base.h"
#include "company_func.h"
#include "debug.h"
#include "genworld.h"
#include "newgrf_class_func.h"
#include "newgrf_dock.h"
#include "newgrf_station.h"
#include "newgrf_sound.h"
#include "newgrf_cargo.h"
#include "station_base.h"
#include "station_map.h"
#include "tile_cmd.h"
#include "town.h"
#include "water.h"
#include "newgrf_animation_base.h"

#include "safeguards.h"

/** The override manager for our docks. */
DockOverrideManager _dock_mngr(NEW_DOCK_OFFSET, NUM_DOCKS, INVALID_DOCK_TYPE);

extern const DockSpec _original_docks[NEW_DOCK_OFFSET];
/** All the dock specifications. */
DockSpec _dock_specs[NUM_DOCKS];

/**
 * Get the specification associated with a specific DockType.
 * @param index The dock type to fetch.
 * @return The specification.
 */
/* static */ const DockSpec *DockSpec::Get(DockType index)
{
	assert(index < NUM_DOCKS);
	return &_dock_specs[index];
}

/**
 * Get the specification associated with a tile.
 * @param tile The tile to fetch the data for.
 * @return The specification.
 */
/* static */ const DockSpec *DockSpec::GetByTile(TileIndex tile)
{
	return DockSpec::Get((DockType)GetStationGfx(tile));
}

/**
 * Check whether the dock might be available at some point in this game with the current game mode.
 * @return true if it might be available.
 */
bool DockSpec::IsEverAvailable() const
{
	return this->enabled && HasBit(this->climate, _settings_game.game_creation.landscape);
}

/**
 * Check whether the dock was available at some point in the past or present in this game with the current game mode.
 * @return true if it was ever or is available.
 */
bool DockSpec::WasEverAvailable() const
{
	return this->IsEverAvailable() && _date > this->introduction_date;
}

/**
 * Check whether the dock is available at this time.
 * @return true if it is available.
 */
bool DockSpec::IsAvailable() const
{
	return this->WasEverAvailable() &&
			(_date < this->end_of_life_date || this->end_of_life_date < this->introduction_date + 365);
}

/**
 * Gets the index of this spec.
 * @return The index.
 */
uint DockSpec::Index() const
{
	return this - _dock_specs;
}

/** This function initialize the spec arrays of docks. */
void ResetDocks()
{
	/* Clean the pool. */
	for (uint16 i = 0; i < NUM_DOCKS; i++) {
		_dock_specs[i] = {};
	}

	/* And add our originals. */
	MemCpyT(_dock_specs, _original_docks, lengthof(_original_docks));

	for (uint16 i = 0; i < lengthof(_original_docks); i++) {
		_dock_specs[i].grf_prop.local_id = i;
	}
}

/**
 * Method to install the new dock data in its proper slot
 * The slot assignment is internal of this method, since it requires
 * checking what is available
 * @param spec DockSpec that comes from the grf decoding process
 */
void DockOverrideManager::SetEntitySpec(DockSpec *spec)
{
	/* First step : We need to find if this dock is already specified in the savegame data. */
	DockType type = this->GetID(spec->grf_prop.local_id, spec->grf_prop.grffile->grfid);

	if (type == this->invalid_ID) {
		/* Not found.
		 * Or it has already been overridden, so you've lost your place old boy.
		 * Or it is a simple substitute.
		 * We need to find a free available slot */
		type = this->AddEntityID(spec->grf_prop.local_id, spec->grf_prop.grffile->grfid, DOCK_ORIGINAL);
	}

	if (type == this->invalid_ID) {
		grfmsg(1, "Dock.SetEntitySpec: Too many docks allocated. Ignoring.");
		return;
	}

	/* Now that we know we can use the given id, copy the spec to its final destination. */
	memcpy(&_dock_specs[type], spec, sizeof(*spec));
	DockClass::Assign(&_dock_specs[type]);
}

template <typename Tspec, typename Tid, Tid Tmax>
/* static */ void NewGRFClass<Tspec, Tid, Tmax>::InsertDefaults()
{
	DockClassID cls = DockClass::Allocate('DOCK');
	DockClass::Get(cls)->name = STR_DOCK_CLASS_DOCK;

	for (uint i = 0; i < lengthof(_original_docks); i++) {
		_dock_specs[i].cls_id = cls;
		DockClass::Assign(&_dock_specs[i]);
	}
}

template <typename Tspec, typename Tid, Tid Tmax>
bool NewGRFClass<Tspec, Tid, Tmax>::IsUIAvailable(uint index) const
{
	return this->GetSpec(index)->IsEverAvailable();
}

INSTANTIATE_NEWGRF_CLASS_METHODS(DockClass, DockSpec, DockClassID, DOCK_CLASS_MAX)

/* virtual */ uint32 DockScopeResolver::GetRandomBits() const
{
	return (this->st == NULL ? 0 : this->st->random_bits) | (this->tile == INVALID_TILE ? 0 : GetStationTileRandomBits(this->tile) << 16);
}

/**
 * Make an analysis of a tile and get the dock type.
 * @param tile TileIndex of the tile to query
 * @param cur_grfid GRFID of the current callback chain
 * @return value encoded as per NFO specs
 */
static uint32 GetDockIDAtOffset(TileIndex tile, uint32 cur_grfid)
{
	if (!IsDockTile(tile)) {
		return 0xFFFF;
	}

	const DockSpec *spec = DockSpec::GetByTile(tile);

	/* Default docks have no associated NewGRF file */
	if (spec->grf_prop.grffile == NULL) {
		return 0xFFFE; // Defined in another grf file
	}

	if (spec->grf_prop.grffile->grfid == cur_grfid) { // same dock, same grf ?
		return spec->grf_prop.local_id;// | o->view << 16;
	}

	return 0xFFFE; // Defined in another grf file
}

/**
 * Based on newhouses equivalent, but adapted for newdocks
 * @param parameter from callback.  It's in fact a pair of coordinates
 * @param tile TileIndex from which the callback was initiated
 * @param index of the dock been queried for
 * @param grf_version8 True, if we are dealing with a new NewGRF which uses GRF version >= 8.
 * @return a construction of bits obeying the newgrf format
 */

static uint32 GetNearbyDockTileInformation(byte parameter, TileIndex tile, StationID index, bool grf_version8)
{
	if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required
	bool is_same_dock = (IsDockTile(tile) && GetStationIndex(tile) == index);

	return GetNearbyTileInformation(tile, grf_version8) | (is_same_dock ? 1 : 0) << 8;
}

/**
 * Get the closest dock of a given type.
 * @param tile    The tile to start searching from.
 * @param type    The type of the dock to search for.
 * @param current The current dock (to ignore).
 * @return The distance to the closest dock.
 */
/*
static uint32 GetClosestDock(TileIndex tile, DockType type, const Dock *current)
{
	uint32 best_dist = UINT32_MAX;
	const Dock *o;
	FOR_ALL_DOCKS(o) {
		if (o->type != type || o == current) continue;

		best_dist = min(best_dist, DistanceManhattan(tile, o->location.tile));
	}

	return best_dist;
}*/

/**
 * Implementation of var 65
 * @param local_id Parameter given to the callback, which is the set id, or the local id, in our terminology.
 * @param grfid    The dock's GRFID.
 * @param tile     The tile to look from.
 * @param current  Dock for which the inquiry is made
 * @return The formatted answer to the callback : rr(reserved) cc(count) dddd(manhattan distance of closest sister)
 */
#if 0
static uint32 GetCountAndDistanceOfClosestInstance(byte local_id, uint32 grfid, TileIndex tile, const Dock *current)
{
	uint32 grf_id = GetRegister(0x100);  // Get the GRFID of the definition to look for in register 100h
	uint32 idx;

	/* Determine what will be the dock type to look for */
	switch (grf_id) {
		case 0:  // this is a default dock type
			idx = local_id;
			break;

		case 0xFFFFFFFF: // current grf
			grf_id = grfid;
			FALLTHROUGH;

		default: // use the grfid specified in register 100h
			idx = _dock_mngr.GetID(local_id, grf_id);
			break;
	}

	/* If the dock type is invalid, there is none and the closest is far away. */
	if (idx >= NUM_DOCKS) return 0 | 0xFFFF;

	return Dock::GetTypeCount(idx) << 16 | min(GetClosestDock(tile, idx, current), 0xFFFF);
}
#endif

TownScopeResolver *DockResolverObject::GetTown()
{
	if (this->town_scope == NULL) {
		Town *t = NULL;
		if (this->dock_scope.st != NULL) {
			t = this->dock_scope.st->town;
		} else if (this->dock_scope.tile != INVALID_TILE) {
			t = ClosestTownFromTile(this->dock_scope.tile, UINT_MAX);
		}
		if (t == NULL) return NULL;
		this->town_scope = new TownScopeResolver(*this, t, this->dock_scope.st == NULL);
	}
	return this->town_scope;
}

/** Used by the resolver to get values for feature 0F deterministic spritegroups. */
/* virtual */ uint32 DockScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	/* We get the town from the dock, or we calculate the closest
	 * town if we need to when there's no dock. */
	const Town *t = NULL;

	if (this->st == NULL) {
		switch (variable) {
			/* Allow these when there's no dock. */
			case 0x41:
				switch (this->view) {
					case 0: return SLOPE_SE << 8;
					case 1: return SLOPE_SW << 8;
					case 2: return SLOPE_NE << 8;
					case 3: return SLOPE_NW << 8;
					default: return 0;
				}
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x64:
				break;

			/* Allow these, but find the closest town. */
			case 0x45:
			case 0x46:
				if (!IsValidTile(this->tile)) goto unhandled;
				t = ClosestTownFromTile(this->tile, UINT_MAX);
				break;

			/* Construction date */
			case 0x42: return _date;

			/* Dock founder information */
			case 0x44: return _current_company;

			/* Dock view */
			case 0x48: return this->view;

			/*
			 * Disallow the rest:
			 * 0x40: Relative position is passed as parameter during construction.
			 * 0x43: Animation counter is only for actual tiles.
			 * 0x47: Dock colour is only valid when its built.
			 * 0x63: Animation counter of nearby tile, see above.
			 */
			default:
				goto unhandled;
		}

		/* If there's an invalid tile, then we don't have enough information at all. */
		if (!IsValidTile(this->tile)) goto unhandled;
	} else {
		t = this->st->town;
	}

	switch (variable) {
		/* Relative position. */
		case 0x40: {
			uint x = TileX(this->tile);
			uint y = TileY(this->tile);
			uint min_x = x;
			uint min_y = y;
			while (HasBit(GetDockAdjacency(TileXY(min_x, y)), 0)) { min_x--; }
			while (HasBit(GetDockAdjacency(TileXY(x, min_y)), 1)) { min_y--; }
			uint offset_x = x - min_x;
			uint offset_y = y - min_y;
			return offset_y << 20 | offset_x << 16 | offset_y << 8 | offset_x;
		}

		/* Tile information. */
		case 0x41: return GetTileSlope(this->tile) << 8 | GetTerrainType(this->tile);

		/* Construction date */
		case 0x42: return this->st->build_date;

		/* Animation counter */
		case 0x43: return GetAnimationFrame(this->tile);

		/* Dock founder information */
		case 0x44: return GetTileOwner(this->tile);

		/* Get town zone and Manhattan distance of closest town */
		case 0x45: return GetTownRadiusGroup(t, this->tile) << 16 | min(DistanceManhattan(this->tile, t->xy), 0xFFFF);

		/* Get square of Euclidian distance of closes town */
		case 0x46: return GetTownRadiusGroup(t, this->tile) << 16 | min(DistanceSquare(this->tile, t->xy), 0xFFFF);

		/* Dock colour */
//		case 0x47: return this->st->owner;

		/* Dock view */
//		case 0x48: return this->obj->view;

		/* Get dock ID at offset param */
		case 0x60: return GetDockIDAtOffset(GetNearbyTile(parameter, this->tile), this->ro.grffile->grfid);

		/* Get random tile bits at offset param */
		case 0x61: {
			TileIndex tile = GetNearbyTile(parameter, this->tile);
			return (IsDockTile(tile) && GetStationIndex(tile) == this->st->index) ? GetStationTileRandomBits(tile) : 0;
		}

		/* Land info of nearby tiles */
		case 0x62: return GetNearbyDockTileInformation(parameter, this->tile, this->st == NULL ? INVALID_STATION : this->st->index, this->ro.grffile->grf_version >= 8);

		/* Animation counter of nearby tile */
		case 0x63: {
			TileIndex tile = GetNearbyTile(parameter, this->tile);
			return (IsDockTile(tile) && GetStationIndex(tile) == this->st->index) ? GetAnimationFrame(tile) : 0;
		}

		/* Count of dock, distance of closest instance */
//		case 0x64: return GetCountAndDistanceOfClosestInstance(parameter, this->ro.grffile->grfid, this->tile, this->obj);
	}

unhandled:
	DEBUG(grf, 1, "Unhandled dock variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

/**
 * Constructor of the dock resolver.
 * @param obj Dock being resolved.
 * @param tile %Tile of the dock.
 * @param view View of the dock.
 * @param callback Callback ID.
 * @param param1 First parameter (var 10) of the callback.
 * @param param2 Second parameter (var 18) of the callback.
 */
DockResolverObject::DockResolverObject(const DockSpec *spec, Station *st, TileIndex tile, uint8 view,
		CallbackID callback, uint32 param1, uint32 param2)
	: ResolverObject(spec->grf_prop.grffile, callback, param1, param2), dock_scope(*this, st, tile, view)
{
	this->town_scope = NULL;
	this->root_spritegroup = (st == NULL && spec->grf_prop.spritegroup[1] != NULL) ?
			spec->grf_prop.spritegroup[1] : spec->grf_prop.spritegroup[0];
}

DockResolverObject::~DockResolverObject()
{
	delete this->town_scope;
}

/**
 * Perform a callback for an dock.
 * @param callback The callback to perform.
 * @param param1   The first parameter to pass to the NewGRF.
 * @param param2   The second parameter to pass to the NewGRF.
 * @param spec     The specification of the dock / the entry point.
 * @param o        The dock to call the callback for.
 * @param tile     The tile the callback is called for.
 * @param view     The view of the dock (only used when o == NULL).
 * @return The result of the callback.
 */
uint16 GetDockCallback(CallbackID callback, uint32 param1, uint32 param2, const DockSpec *spec, Station *st, TileIndex tile, uint8 view)
{
	DockResolverObject dock(spec, st, tile, view, callback, param1, param2);
	return dock.ResolveCallback();
}

/**
 * Draw an group of sprites on the map.
 * @param ti    Information about the tile to draw on.
 * @param group The group of sprites to draw.
 * @param spec  Dock spec to draw.
 */
static void DrawTileLayout(const TileInfo *ti, const TileLayoutSpriteGroup *group, const Station *st, const DockSpec *spec)
{
	const DrawTileSprites *dts = group->ProcessRegisters(NULL);
	PaletteID palette = (spec->flags & DOCK_FLAG_2CC_COLOUR) ? SPR_2CCMAP_BASE : PALETTE_RECOLOUR_START;

	const Livery *livery = &Company::Get(st->owner)->livery[LS_DEFAULT];
	palette += livery->colour1;
	if (spec->flags & DOCK_FLAG_2CC_COLOUR) palette += livery->colour2 * 16;

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		/* If the ground sprite is the default flat water sprite, draw also canal/river borders
		 * Do not do this if the tile's WaterClass is 'land'. */
		if ((image == SPR_FLAT_WATER_TILE || spec->flags & DOCK_FLAG_DRAW_WATER) && IsTileOnWater(ti->tile)) {
			DrawWaterClassGround(ti);
		} else {
			DrawGroundSprite(image, GroundSpritePaletteTransform(image, pal, palette));
		}
	}

	DrawNewGRFTileSeq(ti, dts, TO_STRUCTURES, 0, palette);
}

/**
 * Draw an dock on the map.
 * @param ti   Information about the tile to draw on.
 * @param spec Dock spec to draw.
 */
void DrawNewDockTile(TileInfo *ti, const DockSpec *spec)
{
	Station *st = Station::GetByTile(ti->tile);
	DockResolverObject dock(spec, st, ti->tile);

	const SpriteGroup *group = dock.Resolve();
	if (group == NULL || group->type != SGT_TILELAYOUT) return;

	DrawTileLayout(ti, (const TileLayoutSpriteGroup *)group, st, spec);
}

/**
 * Draw representation of an dock (tile) for GUI purposes.
 * @param x    Position x of image.
 * @param y    Position y of image.
 * @param spec Dock spec to draw.
 * @param view The dock's view.
 */
void DrawNewDockTileInGUI(int x, int y, const DockSpec *spec, uint8 view)
{
	DockResolverObject dock(spec, NULL, INVALID_TILE, view);
	const SpriteGroup *group = dock.Resolve();
	if (group == NULL || group->type != SGT_TILELAYOUT) return;

	const DrawTileSprites *dts = ((const TileLayoutSpriteGroup *)group)->ProcessRegisters(NULL);

	PaletteID palette;
	if (Company::IsValidID(_local_company)) {
		/* Get the colours of our company! */
		if (spec->flags & DOCK_FLAG_2CC_COLOUR) {
			const Livery *l = Company::Get(_local_company)->livery;
			palette = SPR_2CCMAP_BASE + l->colour1 + l->colour2 * 16;
		} else {
			palette = COMPANY_SPRITE_COLOUR(_local_company);
		}
	} else {
		/* There's no company, so just take the base palette. */
		palette = (spec->flags & DOCK_FLAG_2CC_COLOUR) ? SPR_2CCMAP_BASE : PALETTE_RECOLOUR_START;
	}

	SpriteID image = dts->ground.sprite;
	PaletteID pal  = dts->ground.pal;

	if (GB(image, 0, SPRITE_WIDTH) != 0) {
		DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);
	}

	DrawNewGRFTileSeqInGUI(x, y, dts, 0, palette);
}

/**
 * Perform a callback for an dock.
 * @param callback The callback to perform.
 * @param param1   The first parameter to pass to the NewGRF.
 * @param param2   The second parameter to pass to the NewGRF.
 * @param spec     The specification of the dock / the entry point.
 * @param o        The dock to call the callback for.
 * @param tile     The tile the callback is called for.
 * @param extra_data Ignored.
 * @return The result of the callback.
 */
uint16 StubGetDockCallback(CallbackID callback, uint32 param1, uint32 param2, const DockSpec *spec, Station *st, TileIndex tile, int extra_data)
{
	return GetDockCallback(callback, param1, param2, spec, st, tile);
}

/** Helper class for animation control. */
struct DockAnimationBase : public AnimationBase<DockAnimationBase, DockSpec, Station, int, StubGetDockCallback> {
	static const CallbackID cb_animation_speed      = CBID_DOCK_ANIM_SPEED;
	static const CallbackID cb_animation_next_frame = CBID_DOCK_ANIM_NEXT_FRAME;

	static const DockCallbackMask cbm_animation_speed      = CBM_DOCK_ANIM_SPEED;
	static const DockCallbackMask cbm_animation_next_frame = CBM_DOCK_ANIM_NEXT_FRAME;
};

/**
 * Handle the animation of the dock tile.
 * @param tile The tile to animate.
 */
void AnimateNewDockTile(TileIndex tile)
{
	const DockSpec *spec = DockSpec::GetByTile(tile);
	if (spec == NULL || !(spec->flags & DOCK_FLAG_ANIMATION)) return;

	DockAnimationBase::AnimateTile(spec, Station::GetByTile(tile), tile, (spec->flags & DOCK_FLAG_ANIM_RANDOM_BITS) != 0);
}

/**
 * Trigger the update of animation on a single tile.
 * @param st      The station of the dock that got triggered.
 * @param tile    The location of the triggered tile.
 * @param trigger The trigger that is triggered.
 * @param spec    The spec associated with the dock.
 */
void TriggerDockTileAnimation(Station *st, TileIndex tile, DockAnimationTrigger trigger, const DockSpec *spec)
{
	if (!HasBit(spec->animation.triggers, trigger)) return;

	DockAnimationBase::ChangeAnimationFrame(CBID_DOCK_ANIM_START_STOP, spec, st, tile, Random(), trigger);
}

/**
 * Trigger the update of animation on a whole dock.
 * @param st      The station of the dock that got triggered.
 * @param trigger The trigger that is triggered.
 * @param spec    The spec associated with the dock.
 */
void TriggerDockAnimation(Station *st, DockAnimationTrigger trigger, const DockSpec *spec)
{
	if (!HasBit(spec->animation.triggers, trigger)) return;

	TILE_AREA_LOOP(tile, st->ship_station) {
		TriggerDockTileAnimation(st, tile, trigger, spec);
	}
}
