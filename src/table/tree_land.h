/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tree_land.h Sprites to use and how to display them for tree tiles. */

#ifndef TREE_LAND_H
#define TREE_LAND_H

struct TreePos {
	uint8_t x;
	uint8_t y;
};

static const TreePos _tree_layout_xy[][4] = {
	{ { 9, 3 }, { 1, 8 }, { 0, 0 }, { 8, 9 } },
	{ { 4, 4 }, { 9, 1 }, { 6, 9 }, { 0, 9 } },
	{ { 9, 1 }, { 0, 9 }, { 6, 6 }, { 3, 0 } },
	{ { 3, 9 }, { 8, 2 }, { 9, 9 }, { 1, 5 } }
};

static constexpr uint8_t TREE_GROWTH_COUNT = 7; ///< Number of tree growth stages.


/**
 * Make a TreeBase for a default tree.
 * @param index Sprite offset index for normal tree.
 * @return TreeBase for default tree.
 */
static constexpr TreeInfo MakeDefaultTree(uint8_t index)
{
	return {SPR_TREE_BASE + (TREE_GROWTH_COUNT * index), 0};
}

/**
 * Make a TreeBase for a default snowy tree.
 * @param normal_index Sprite offset index for normal tree.
 * @param snowy_index Sprite offset index for snowy tree.
 * @return TreeBase for default snowy tree.
 */
static constexpr TreeInfo MakeDefaultSnowyTree(uint8_t normal_index, uint8_t snowy_index)
{
	return {SPR_TREE_BASE + (TREE_GROWTH_COUNT * normal_index), SPR_TREE_BASE + (TREE_GROWTH_COUNT * snowy_index)};
}

/** Sprite IDs of original trees, normal and snowy variants. */
static constexpr TreeInfo _original_tree_info[] = {
	/* Temperate */
	MakeDefaultTree(0),
	MakeDefaultTree(1),
	MakeDefaultTree(2),
	MakeDefaultTree(3),
	MakeDefaultTree(4),
	MakeDefaultTree(5),
	MakeDefaultTree(6),
	MakeDefaultTree(7),
	MakeDefaultTree(8),
	MakeDefaultTree(9),
	MakeDefaultTree(10),
	MakeDefaultTree(11),
	MakeDefaultTree(12),
	MakeDefaultTree(13),
	MakeDefaultTree(14),
	MakeDefaultTree(15),
	MakeDefaultTree(16),
	MakeDefaultTree(17),
	MakeDefaultTree(18),

	/* Arctic */
	MakeDefaultSnowyTree(19, 27),
	MakeDefaultSnowyTree(20, 28),
	MakeDefaultSnowyTree(21, 29),
	MakeDefaultSnowyTree(22, 30),
	MakeDefaultSnowyTree(23, 31),
	MakeDefaultSnowyTree(24, 32),
	MakeDefaultSnowyTree(25, 33),
	MakeDefaultSnowyTree(26, 34),

	/* Sub-tropic */
	MakeDefaultTree(35),
	MakeDefaultTree(36),
	MakeDefaultTree(37),
	MakeDefaultTree(38),
	MakeDefaultTree(39),
	MakeDefaultTree(40),
	MakeDefaultTree(41),
	MakeDefaultTree(42),
	MakeDefaultTree(43),
	MakeDefaultTree(44),
	MakeDefaultTree(45),
	MakeDefaultTree(46),
	MakeDefaultTree(47),
	MakeDefaultTree(48),
	MakeDefaultTree(49),
	MakeDefaultTree(50),
	MakeDefaultTree(51),
	MakeDefaultTree(52),

	/* Toyland */
	MakeDefaultTree(53),
	MakeDefaultTree(54),
	MakeDefaultTree(55),
	MakeDefaultTree(56),
	MakeDefaultTree(57),
	MakeDefaultTree(58),
	MakeDefaultTree(59),
	MakeDefaultTree(60),
	MakeDefaultTree(61),
};

static constexpr TreeID TREEBASE_T = 0; ///< Base tree index for default temperate tree sprites.
static constexpr TreeID TREEBASE_A = 19; ///< Base tree index for default arctic tree sprites.
static constexpr TreeID TREEBASE_S = 27; ///< Base tree index for default subtropic tree sprites.
static constexpr TreeID TREEBASE_Y = 45; ///< Base tree index for default toyland tree sprites.

/** Tree tile information for original trees. */
static const TreeTileInfo _original_tree_tile_info[] = {
	// Temperate
	{
		.trees = {{
			{TREEBASE_T + 6, TREEBASE_T + 7, TREEBASE_T + 8, TREEBASE_T + 9},
			{TREEBASE_T + 6, TREEBASE_T + 9, TREEBASE_T + 10, TREEBASE_T + 11},
			{TREEBASE_T + 6, TREEBASE_T + 10, TREEBASE_T + 7, TREEBASE_T + 11},
			{TREEBASE_T + 6, TREEBASE_T + 6, TREEBASE_T + 8, TREEBASE_T + 10},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 8, TREEBASE_T + 9, TREEBASE_T + 7, TREEBASE_T + 6},
			{TREEBASE_T + 8, TREEBASE_T + 11, TREEBASE_T + 8, TREEBASE_T + 8},
			{TREEBASE_T + 8, TREEBASE_T + 6, TREEBASE_T + 6, TREEBASE_T + 10},
			{TREEBASE_T + 8, TREEBASE_T + 11, TREEBASE_T + 9, TREEBASE_T + 7},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 11, TREEBASE_T + 8, TREEBASE_T + 11, TREEBASE_T + 11},
			{TREEBASE_T + 11, TREEBASE_T + 7, TREEBASE_T + 6, TREEBASE_T + 6},
			{TREEBASE_T + 11, TREEBASE_T + 10, TREEBASE_T + 6, TREEBASE_T + 6},
			{TREEBASE_T + 11, TREEBASE_T + 9, TREEBASE_T + 7, TREEBASE_T + 9},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 0, TREEBASE_T + 6, TREEBASE_T + 8, TREEBASE_T + 1},
			{TREEBASE_T + 0, TREEBASE_T + 2, TREEBASE_T + 11, TREEBASE_T + 4},
			{TREEBASE_T + 0, TREEBASE_T + 6, TREEBASE_T + 3, TREEBASE_T + 10},
			{TREEBASE_T + 0, TREEBASE_T + 9, TREEBASE_T + 4, TREEBASE_T + 6},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 4, TREEBASE_T + 7, TREEBASE_T + 8, TREEBASE_T + 0},
			{TREEBASE_T + 4, TREEBASE_T + 5, TREEBASE_T + 7, TREEBASE_T + 2},
			{TREEBASE_T + 4, TREEBASE_T + 11, TREEBASE_T + 6, TREEBASE_T + 3},
			{TREEBASE_T + 4, TREEBASE_T + 3, TREEBASE_T + 10, TREEBASE_T + 6},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 2, TREEBASE_T + 2, TREEBASE_T + 0, TREEBASE_T + 2},
			{TREEBASE_T + 2, TREEBASE_T + 3, TREEBASE_T + 2, TREEBASE_T + 2},
			{TREEBASE_T + 2, TREEBASE_T + 5, TREEBASE_T + 2, TREEBASE_T + 2},
			{TREEBASE_T + 2, TREEBASE_T + 2, TREEBASE_T + 2, TREEBASE_T + 2},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 5, TREEBASE_T + 0, TREEBASE_T + 1, TREEBASE_T + 2},
			{TREEBASE_T + 5, TREEBASE_T + 3, TREEBASE_T + 4, TREEBASE_T + 2},
			{TREEBASE_T + 5, TREEBASE_T + 2, TREEBASE_T + 3, TREEBASE_T + 0},
			{TREEBASE_T + 5, TREEBASE_T + 5, TREEBASE_T + 2, TREEBASE_T + 3},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 1, TREEBASE_T + 4, TREEBASE_T + 4, TREEBASE_T + 2},
			{TREEBASE_T + 1, TREEBASE_T + 1, TREEBASE_T + 2, TREEBASE_T + 0},
			{TREEBASE_T + 1, TREEBASE_T + 5, TREEBASE_T + 2, TREEBASE_T + 2},
			{TREEBASE_T + 1, TREEBASE_T + 2, TREEBASE_T + 1, TREEBASE_T + 2},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 12, TREEBASE_T + 11, TREEBASE_T + 13, TREEBASE_T + 12},
			{TREEBASE_T + 12, TREEBASE_T + 17, TREEBASE_T + 12, TREEBASE_T + 7},
			{TREEBASE_T + 12, TREEBASE_T + 12, TREEBASE_T + 12, TREEBASE_T + 18},
			{TREEBASE_T + 12, TREEBASE_T + 15, TREEBASE_T + 10, TREEBASE_T + 14},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 14, TREEBASE_T + 14, TREEBASE_T + 16, TREEBASE_T + 14},
			{TREEBASE_T + 14, TREEBASE_T + 16, TREEBASE_T + 13, TREEBASE_T + 14},
			{TREEBASE_T + 14, TREEBASE_T + 12, TREEBASE_T + 15, TREEBASE_T + 14},
			{TREEBASE_T + 14, TREEBASE_T + 13, TREEBASE_T + 18, TREEBASE_T + 17},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 16, TREEBASE_T + 14, TREEBASE_T + 16, TREEBASE_T + 6},
			{TREEBASE_T + 16, TREEBASE_T + 16, TREEBASE_T + 8, TREEBASE_T + 9},
			{TREEBASE_T + 16, TREEBASE_T + 12, TREEBASE_T + 18, TREEBASE_T + 16},
			{TREEBASE_T + 16, TREEBASE_T + 16, TREEBASE_T + 16, TREEBASE_T + 15},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	{
		.trees = {{
			{TREEBASE_T + 18, TREEBASE_T + 18, TREEBASE_T + 12, TREEBASE_T + 8},
			{TREEBASE_T + 18, TREEBASE_T + 17, TREEBASE_T + 18, TREEBASE_T + 6},
			{TREEBASE_T + 18, TREEBASE_T + 12, TREEBASE_T + 18, TREEBASE_T + 15},
			{TREEBASE_T + 18, TREEBASE_T + 15, TREEBASE_T + 17, TREEBASE_T + 18},
		}},
		.landscapes = LandscapeType::Temperate,
	},
	// Arctic
	{
		.trees = {{
			{TREEBASE_A + 0, TREEBASE_A + 0, TREEBASE_A + 0, TREEBASE_A + 0},
			{TREEBASE_A + 0, TREEBASE_A + 0, TREEBASE_A + 3, TREEBASE_A + 5},
			{TREEBASE_A + 0, TREEBASE_A + 6, TREEBASE_A + 0, TREEBASE_A + 0},
			{TREEBASE_A + 0, TREEBASE_A + 5, TREEBASE_A + 4, TREEBASE_A + 0},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREEBASE_A + 5, TREEBASE_A + 5, TREEBASE_A + 5, TREEBASE_A + 0},
			{TREEBASE_A + 5, TREEBASE_A + 0, TREEBASE_A + 6, TREEBASE_A + 4},
			{TREEBASE_A + 5, TREEBASE_A + 6, TREEBASE_A + 5, TREEBASE_A + 3},
			{TREEBASE_A + 5, TREEBASE_A + 5, TREEBASE_A + 5, TREEBASE_A + 0},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREEBASE_A + 6, TREEBASE_A + 6, TREEBASE_A + 6, TREEBASE_A + 6},
			{TREEBASE_A + 6, TREEBASE_A + 6, TREEBASE_A + 0, TREEBASE_A + 0},
			{TREEBASE_A + 6, TREEBASE_A + 5, TREEBASE_A + 6, TREEBASE_A + 0},
			{TREEBASE_A + 6, TREEBASE_A + 6, TREEBASE_A + 5, TREEBASE_A + 0},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREEBASE_A + 3, TREEBASE_A + 5, TREEBASE_A + 4, TREEBASE_A + 3},
			{TREEBASE_A + 3, TREEBASE_A + 4, TREEBASE_A + 3, TREEBASE_A + 0},
			{TREEBASE_A + 3, TREEBASE_A + 3, TREEBASE_A + 3, TREEBASE_A + 0},
			{TREEBASE_A + 3, TREEBASE_A + 3, TREEBASE_A + 3, TREEBASE_A + 4},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREEBASE_A + 4, TREEBASE_A + 5, TREEBASE_A + 1, TREEBASE_A + 3},
			{TREEBASE_A + 4, TREEBASE_A + 2, TREEBASE_A + 7, TREEBASE_A + 6},
			{TREEBASE_A + 4, TREEBASE_A + 3, TREEBASE_A + 2, TREEBASE_A + 1},
			{TREEBASE_A + 4, TREEBASE_A + 2, TREEBASE_A + 3, TREEBASE_A + 7},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREEBASE_A + 1, TREEBASE_A + 1, TREEBASE_A + 7, TREEBASE_A + 4},
			{TREEBASE_A + 1, TREEBASE_A + 2, TREEBASE_A + 2, TREEBASE_A + 0},
			{TREEBASE_A + 1, TREEBASE_A + 7, TREEBASE_A + 2, TREEBASE_A + 1},
			{TREEBASE_A + 1, TREEBASE_A + 0, TREEBASE_A + 3, TREEBASE_A + 7},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREEBASE_A + 2, TREEBASE_A + 5, TREEBASE_A + 7, TREEBASE_A + 3},
			{TREEBASE_A + 2, TREEBASE_A + 1, TREEBASE_A + 2, TREEBASE_A + 6},
			{TREEBASE_A + 2, TREEBASE_A + 7, TREEBASE_A + 2, TREEBASE_A + 1},
			{TREEBASE_A + 2, TREEBASE_A + 4, TREEBASE_A + 3, TREEBASE_A + 7},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	{
		.trees = {{
			{TREEBASE_A + 7, TREEBASE_A + 6, TREEBASE_A + 7, TREEBASE_A + 3},
			{TREEBASE_A + 7, TREEBASE_A + 2, TREEBASE_A + 7, TREEBASE_A + 5},
			{TREEBASE_A + 7, TREEBASE_A + 7, TREEBASE_A + 2, TREEBASE_A + 1},
			{TREEBASE_A + 7, TREEBASE_A + 4, TREEBASE_A + 3, TREEBASE_A + 7},
		}},
		.landscapes = LandscapeType::Arctic,
	},
	// Sub-tropic, rainforest
	{
		.trees = {{
			{TREEBASE_S + 2, TREEBASE_S + 3, TREEBASE_S + 2, TREEBASE_S + 4},
			{TREEBASE_S + 2, TREEBASE_S + 6, TREEBASE_S + 8, TREEBASE_S + 2},
			{TREEBASE_S + 2, TREEBASE_S + 2, TREEBASE_S + 11, TREEBASE_S + 15},
			{TREEBASE_S + 2, TREEBASE_S + 7, TREEBASE_S + 2, TREEBASE_S + 2},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREEBASE_S + 3, TREEBASE_S + 3, TREEBASE_S + 2, TREEBASE_S + 4},
			{TREEBASE_S + 3, TREEBASE_S + 6, TREEBASE_S + 3, TREEBASE_S + 3},
			{TREEBASE_S + 3, TREEBASE_S + 3, TREEBASE_S + 8, TREEBASE_S + 17},
			{TREEBASE_S + 3, TREEBASE_S + 7, TREEBASE_S + 3, TREEBASE_S + 16},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREEBASE_S + 6, TREEBASE_S + 3, TREEBASE_S + 6, TREEBASE_S + 5},
			{TREEBASE_S + 6, TREEBASE_S + 6, TREEBASE_S + 3, TREEBASE_S + 11},
			{TREEBASE_S + 6, TREEBASE_S + 2, TREEBASE_S + 8, TREEBASE_S + 6},
			{TREEBASE_S + 6, TREEBASE_S + 15, TREEBASE_S + 3, TREEBASE_S + 6},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREEBASE_S + 7, TREEBASE_S + 7, TREEBASE_S + 2, TREEBASE_S + 17},
			{TREEBASE_S + 7, TREEBASE_S + 8, TREEBASE_S + 3, TREEBASE_S + 7},
			{TREEBASE_S + 7, TREEBASE_S + 2, TREEBASE_S + 15, TREEBASE_S + 6},
			{TREEBASE_S + 7, TREEBASE_S + 7, TREEBASE_S + 3, TREEBASE_S + 17},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREEBASE_S + 11, TREEBASE_S + 11, TREEBASE_S + 7, TREEBASE_S + 7},
			{TREEBASE_S + 11, TREEBASE_S + 17, TREEBASE_S + 3, TREEBASE_S + 11},
			{TREEBASE_S + 11, TREEBASE_S + 3, TREEBASE_S + 15, TREEBASE_S + 11},
			{TREEBASE_S + 11, TREEBASE_S + 15, TREEBASE_S + 3, TREEBASE_S + 16},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREEBASE_S + 16, TREEBASE_S + 16, TREEBASE_S + 7, TREEBASE_S + 17},
			{TREEBASE_S + 16, TREEBASE_S + 3, TREEBASE_S + 4, TREEBASE_S + 6},
			{TREEBASE_S + 16, TREEBASE_S + 3, TREEBASE_S + 15, TREEBASE_S + 11},
			{TREEBASE_S + 16, TREEBASE_S + 15, TREEBASE_S + 16, TREEBASE_S + 17},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	{
		.trees = {{
			{TREEBASE_S + 15, TREEBASE_S + 15, TREEBASE_S + 5, TREEBASE_S + 3},
			{TREEBASE_S + 15, TREEBASE_S + 15, TREEBASE_S + 2, TREEBASE_S + 3},
			{TREEBASE_S + 15, TREEBASE_S + 3, TREEBASE_S + 15, TREEBASE_S + 15},
			{TREEBASE_S + 15, TREEBASE_S + 15, TREEBASE_S + 16, TREEBASE_S + 17},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Rainforest,
	},
	// Sub-tropic, cactus
	{
		.trees = {{
			{TREEBASE_S + 13, TREEBASE_S + 13, TREEBASE_S + 14, TREEBASE_S + 13},
			{TREEBASE_S + 13, TREEBASE_S + 14, TREEBASE_S + 13, TREEBASE_S + 14},
			{TREEBASE_S + 13, TREEBASE_S + 14, TREEBASE_S + 14, TREEBASE_S + 13},
			{TREEBASE_S + 13, TREEBASE_S + 13, TREEBASE_S + 13, TREEBASE_S + 14},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Desert,
		.probability = 12,
	},
	// Sub-tropic, non-tropical
	{
		.trees = {{
			{TREEBASE_S + 9, TREEBASE_S + 0, TREEBASE_S + 9, TREEBASE_S + 1},
			{TREEBASE_S + 9, TREEBASE_S + 2, TREEBASE_S + 9, TREEBASE_S + 10},
			{TREEBASE_S + 9, TREEBASE_S + 9, TREEBASE_S + 12, TREEBASE_S + 0},
			{TREEBASE_S + 9, TREEBASE_S + 12, TREEBASE_S + 9, TREEBASE_S + 9},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	{
		.trees = {{
			{TREEBASE_S + 12, TREEBASE_S + 12, TREEBASE_S + 9, TREEBASE_S + 0},
			{TREEBASE_S + 12, TREEBASE_S + 6, TREEBASE_S + 9, TREEBASE_S + 12},
			{TREEBASE_S + 12, TREEBASE_S + 9, TREEBASE_S + 12, TREEBASE_S + 1},
			{TREEBASE_S + 12, TREEBASE_S + 12, TREEBASE_S + 9, TREEBASE_S + 10},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	{
		.trees = {{
			{TREEBASE_S + 0, TREEBASE_S + 0, TREEBASE_S + 12, TREEBASE_S + 1},
			{TREEBASE_S + 0, TREEBASE_S + 7, TREEBASE_S + 10, TREEBASE_S + 0},
			{TREEBASE_S + 0, TREEBASE_S + 1, TREEBASE_S + 17, TREEBASE_S + 0},
			{TREEBASE_S + 0, TREEBASE_S + 0, TREEBASE_S + 9, TREEBASE_S + 16},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	{
		.trees = {{
			{TREEBASE_S + 17, TREEBASE_S + 1, TREEBASE_S + 9, TREEBASE_S + 17},
			{TREEBASE_S + 17, TREEBASE_S + 17, TREEBASE_S + 9, TREEBASE_S + 0},
			{TREEBASE_S + 17, TREEBASE_S + 1, TREEBASE_S + 17, TREEBASE_S + 0},
			{TREEBASE_S + 17, TREEBASE_S + 17, TREEBASE_S + 12, TREEBASE_S + 16},
		}},
		.landscapes = LandscapeType::Tropic,
		.tropiczones = TropicZone::Normal,
	},
	// Toyland
	{
		.trees = {{
			{TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0},
			{TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0},
			{TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0},
			{TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0, TREEBASE_Y + 0},
		}},
		.palettes = {{
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_MAUVE, PALETTE_TO_PURPLE},
			{PAL_NONE,          PALETTE_TO_GREY,       PALETTE_TO_GREEN, PALETTE_TO_WHITE},
			{PALETTE_TO_GREEN,  PALETTE_TO_ORANGE,     PALETTE_TO_PINK,  PAL_NONE},
			{PALETTE_TO_YELLOW, PALETTE_TO_RED,        PALETTE_TO_CREAM, PALETTE_TO_RED},
		}},
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1},
			{TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1},
			{TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1},
			{TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1, TREEBASE_Y + 1},
		}},
		.palettes = {{
			{PAL_NONE,          PALETTE_TO_RED,        PALETTE_TO_PINK,   PALETTE_TO_PURPLE},
			{PALETTE_TO_MAUVE,  PALETTE_TO_GREEN,      PALETTE_TO_PINK,   PALETTE_TO_GREY},
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_YELLOW, PALETTE_TO_WHITE},
			{PALETTE_TO_ORANGE, PALETTE_TO_MAUVE,      PALETTE_TO_CREAM,  PALETTE_TO_BROWN},
		}},
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2},
			{TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2},
			{TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2},
			{TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2, TREEBASE_Y + 2},
		}},
		.palettes = {{
			{PALETTE_TO_RED,    PAL_NONE,         PALETTE_TO_ORANGE,     PALETTE_TO_GREY},
			{PALETTE_TO_ORANGE, PALETTE_TO_GREEN, PALETTE_TO_PALE_GREEN, PALETTE_TO_MAUVE},
			{PALETTE_TO_PINK,   PALETTE_TO_RED,   PALETTE_TO_GREEN,      PALETTE_TO_BROWN},
			{PALETTE_TO_GREEN,  PAL_NONE,         PALETTE_TO_RED,        PALETTE_TO_CREAM},
		}},
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3},
			{TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3},
			{TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3},
			{TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3, TREEBASE_Y + 3},
		}},
		/* No palettes */
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4},
			{TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4},
			{TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4},
			{TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4, TREEBASE_Y + 4},
		}},
		.palettes = {{
			{PALETTE_TO_PINK,  PALETTE_TO_RED,        PALETTE_TO_ORANGE, PALETTE_TO_MAUVE},
			{PALETTE_TO_RED,   PAL_NONE,              PALETTE_TO_GREY,   PALETTE_TO_CREAM},
			{PALETTE_TO_GREEN, PALETTE_TO_BROWN,      PALETTE_TO_PINK,   PALETTE_TO_RED},
			{PAL_NONE,         PALETTE_TO_PALE_GREEN, PALETTE_TO_ORANGE, PALETTE_TO_RED},

		}},
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5},
			{TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5},
			{TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5},
			{TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5, TREEBASE_Y + 5},
		}},
		.palettes = {{
			{PALETTE_TO_RED,   PALETTE_TO_PINK,  PALETTE_TO_GREEN,      PAL_NONE},
			{PALETTE_TO_GREEN, PALETTE_TO_BROWN, PALETTE_TO_PURPLE,     PALETTE_TO_GREY},
			{PALETTE_TO_MAUVE, PALETTE_TO_CREAM, PALETTE_TO_ORANGE,     PALETTE_TO_RED},
			{PAL_NONE,         PALETTE_TO_RED,   PALETTE_TO_PALE_GREEN, PALETTE_TO_PINK},
		}},
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6},
			{TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6},
			{TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6},
			{TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6, TREEBASE_Y + 6},
		}},
		.palettes = {{
			{PALETTE_TO_YELLOW, PALETTE_TO_RED,        PALETTE_TO_WHITE, PALETTE_TO_CREAM},
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_BROWN, PALETTE_TO_YELLOW},
			{PAL_NONE,          PALETTE_TO_PURPLE,     PALETTE_TO_GREEN, PALETTE_TO_YELLOW},
			{PALETTE_TO_PINK,   PALETTE_TO_CREAM,      PAL_NONE,         PALETTE_TO_GREY},
		}},
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7},
			{TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7},
			{TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7},
			{TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7, TREEBASE_Y + 7},
		}},
		.palettes = {{
			{PALETTE_TO_YELLOW, PALETTE_TO_GREY,       PALETTE_TO_PURPLE, PALETTE_TO_BROWN},
			{PALETTE_TO_GREEN,  PAL_NONE,              PALETTE_TO_CREAM,  PALETTE_TO_WHITE},
			{PALETTE_TO_RED,    PALETTE_TO_PALE_GREEN, PALETTE_TO_MAUVE,  PALETTE_TO_RED},
			{PALETTE_TO_PINK,   PALETTE_TO_ORANGE,     PALETTE_TO_GREEN,  PALETTE_TO_YELLOW},
		}},
		.landscapes = LandscapeType::Toyland,
	},
	{
		.trees = {{
			{TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8},
			{TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8},
			{TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8},
			{TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8, TREEBASE_Y + 8},
		}},
		.palettes = {{
			{PALETTE_TO_RED,    PALETTE_TO_PINK,       PALETTE_TO_BROWN, PALETTE_TO_WHITE},
			{PALETTE_TO_GREEN,  PALETTE_TO_ORANGE,     PALETTE_TO_GREY,  PALETTE_TO_MAUVE},
			{PALETTE_TO_YELLOW, PALETTE_TO_PALE_GREEN, PAL_NONE,         PALETTE_TO_CREAM},
			{PALETTE_TO_GREY,   PALETTE_TO_RED,        PALETTE_TO_WHITE, PAL_NONE},
		}},
		.landscapes = LandscapeType::Toyland,
	},
};

#endif /* TREE_LAND_H */
