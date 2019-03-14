/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tunnel_gui.cpp Graphical user interface for tunnel variant construction */

#include "stdafx.h"
#include "error.h"
#include "command_func.h"
#include "rail.h"
#include "strings_func.h"
#include "window_func.h"
#include "sound_func.h"
#include "gfx_func.h"
#include "tunnelbridge.h"
#include "sortlist_type.h"
#include "widgets/dropdown_func.h"
#include "core/geometry_func.hpp"
#include "cmd_helper.h"
#include "tunnelbridge_map.h"
#include "road_gui.h"
#include "newgrf_callbacks.h"
#include "newgrf_railtype.h"

#include "widgets/tunnel_widget.h"

#include "table/strings.h"

#include "safeguards.h"

/** The type of the last built rail tunnel */
static uint _last_railtunnel_type = 0; 

/**
 * Carriage for the data we need if we want to build a tunnel
 */
struct BuildTunnelData {
	uint index;
	SpriteID sprite;
};

typedef GUIList<BuildTunnelData> GUITunnelList; ///< List of tunnels, used in #BuildTunnelWindow.

/**
 * Callback executed after a build Tunnel CMD has been called
 *
 * @param result Whether the build succeeded
 * @param end_tile End tile of the tunnel.
 * @param p1 packed start tile coords (~ dx)
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0- 7) - tunnel type (hi bh)
 * - p2 = (bit  8-13) - rail type or road types.
 * - p2 = (bit 15-16) - transport type.
 */
void CcBuildTunnel(const CommandCost &result, TileIndex end_tile, uint32 p1, uint32 p2)
{
	if (result.Failed()) return;
	if (_settings_client.sound.confirm) SndPlayTileFx(SND_27_BLACKSMITH_ANVIL, end_tile);
}

/** Window class for handling the tunnel-build GUI. */
class BuildTunnelWindow : public Window {
private:
	/* Runtime saved values */
	static Listing last_sorting; ///< Last setting of the sort.

	/* Constants for sorting the tunnels */
	static const StringID sorter_names[];
	static GUITunnelList::SortFunction * const sorter_funcs[];

	/* Internal variables */
	TileIndex start_tile;
	TileIndex end_tile;
	uint32 type;
	GUITunnelList *tunnels;
	int tunneltext_offset; ///< Horizontal offset of the text describing the tunnel properties in #WID_BTS_TUNNEL_LIST relative to the left edge.
	Scrollbar *vscroll;

	/** Sort the tunnels by their index */
	static int CDECL TunnelIndexSorter(const BuildTunnelData *a, const BuildTunnelData *b)
	{
		return a->index - b->index;
	}

	/** Sort the tunnels by their price */
//	static int CDECL TunnelPriceSorter(const BuildTunnelData *a, const BuildTunnelData *b)
//	{
//		return a->cost - b->cost;
//	}

	/** Sort the tunnels by their maximum speed */
//	static int CDECL TunnelSpeedSorter(const BuildTunnelData *a, const BuildTunnelData *b)
//	{
//		return a->spec->speed - b->spec->speed;
//	}

	void BuildTunnel(uint8 i)
	{
		switch ((TransportType)(this->type >> 15)) {
			case TRANSPORT_RAIL: _last_railtunnel_type = this->tunnels->Get(i)->index; break;
			default: break;
		}
		DoCommandP(this->end_tile, this->start_tile, this->type | this->tunnels->Get(i)->index,
					CMD_BUILD_TUNNEL | CMD_MSG(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE), CcBuildTunnel);
	}

	/** Sort the builable tunnels */
	void SortTunnelList()
	{
		this->tunnels->Sort();

		/* Display the current sort variant */
		this->GetWidget<NWidgetCore>(WID_BTS_DROPDOWN_CRITERIA)->widget_data = this->sorter_names[this->tunnels->SortType()];

		/* Set the modified widgets dirty */
		this->SetWidgetDirty(WID_BTS_DROPDOWN_CRITERIA);
		this->SetWidgetDirty(WID_BTS_TUNNEL_LIST);
	}

public:
	BuildTunnelWindow(WindowDesc *desc, TileIndex start, TileIndex end, uint32 br_type, GUITunnelList *bl) : Window(desc),
		start_tile(start),
		end_tile(end),
		type(br_type),
		tunnels(bl)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_BTS_SCROLLBAR);
		/* Change the data, or the caption of the gui. Set it to road or rail, accordingly. */
		this->GetWidget<NWidgetCore>(WID_BTS_CAPTION)->widget_data = STR_SELECT_RAIL_TUNNEL_CAPTION;
		this->FinishInitNested(GB(br_type, 15, 2)); // Initializes 'this->tunneltext_offset'.

		this->parent = FindWindowById(WC_BUILD_TOOLBAR, GB(this->type, 15, 2));
		this->tunnels->SetListing(this->last_sorting);
		this->tunnels->SetSortFuncs(this->sorter_funcs);
		this->tunnels->NeedResort();
		this->SortTunnelList();

		this->vscroll->SetCount(bl->Length());
	}

	~BuildTunnelWindow()
	{
		this->last_sorting = this->tunnels->GetListing();

		delete tunnels;
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_BTS_DROPDOWN_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->widget_data);
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_BTS_DROPDOWN_CRITERIA: {
				Dimension d = {0, 0};
				for (const StringID *str = this->sorter_names; *str != INVALID_STRING_ID; str++) {
					d = maxdim(d, GetStringBoundingBox(*str));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}
			case WID_BTS_TUNNEL_LIST: {
				Dimension sprite_dim = {0, 0}; // Biggest tunnel sprite dimension
				Dimension text_dim   = {0, 0}; // Biggest text dimension
				for (int i = 0; i < (int)this->tunnels->Length(); i++) {
//					const TunnelSpec *b = this->tunnels->Get(i)->spec;
					sprite_dim = maxdim(sprite_dim, GetSpriteSize(this->tunnels->Get(i)->sprite));
//					SetDParam(2, this->tunnels->Get(i)->cost);
//					SetDParam(1, b->speed);
//					SetDParam(0, b->material);
//					text_dim = maxdim(text_dim, GetStringBoundingBox(_game_mode == GM_EDITOR ? STR_SELECT_TUNNEL_SCENEDIT_INFO : STR_SELECT_TUNNEL_INFO));
				}
				sprite_dim.height++; // Sprite is rendered one pixel down in the matrix field.
				text_dim.height++; // Allowing the bottom row pixels to be rendered on the edge of the matrix field.
				resize->height = max(sprite_dim.height, text_dim.height) + 2; // Max of both sizes + account for matrix edges.

				this->tunneltext_offset = WD_MATRIX_LEFT + sprite_dim.width + 1; // Left edge of text, 1 pixel distance from the sprite.
				size->width = this->tunneltext_offset + text_dim.width + WD_MATRIX_RIGHT;
				size->height = 4 * resize->height; // Smallest tunnel gui is 4 entries high in the matrix.
				break;
			}
		}
	}

	virtual Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number)
	{
		/* Position the window so hopefully the first tunnel from the list is under the mouse pointer. */
		NWidgetBase *list = this->GetWidget<NWidgetBase>(WID_BTS_TUNNEL_LIST);
		Point corner; // point of the top left corner of the window.
		corner.y = Clamp(_cursor.pos.y - list->pos_y - 5, GetMainViewTop(), GetMainViewBottom() - sm_height);
		corner.x = Clamp(_cursor.pos.x - list->pos_x - 5, 0, _screen.width - sm_width);
		return corner;
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_BTS_DROPDOWN_ORDER:
				this->DrawSortButtonState(widget, this->tunnels->IsDescSortOrder() ? SBS_DOWN : SBS_UP);
				break;

			case WID_BTS_TUNNEL_LIST: {
				uint y = r.top;
				for (int i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < (int)this->tunnels->Length(); i++) {
//					const TunnelSpec *b = this->tunnels->Get(i)->spec;

//					SetDParam(2, this->tunnels->Get(i)->cost);
//					SetDParam(1, b->speed);
//					SetDParam(0, b->material);
//
					this->tunnels->Get(i)->index;

					SpriteID sprite = this->tunnels->Get(i)->sprite;
					DrawSprite(sprite, PAL_NONE, r.left + WD_MATRIX_LEFT, y + this->resize.step_height - 1 - GetSpriteSize(sprite).height);
//					DrawStringMultiLine(r.left + this->tunneltext_offset, r.right, y + 2, y + this->resize.step_height,
//							_game_mode == GM_EDITOR ? STR_SELECT_TUNNEL_SCENEDIT_INFO : STR_SELECT_TUNNEL_INFO);
					y += this->resize.step_height;
				}
				break;
			}
		}
	}

	virtual EventState OnKeyPress(WChar key, uint16 keycode)
	{
		const uint8 i = keycode - '1';
		if (i < 9 && i < this->tunnels->Length()) {
			/* Build the requested tunnel */
			this->BuildTunnel(i);
			delete this;
			return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			default: break;
			case WID_BTS_TUNNEL_LIST: {
				uint i = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_BTS_TUNNEL_LIST);
				if (i < this->tunnels->Length()) {
					this->BuildTunnel(i);
					delete this;
				}
				break;
			}

			case WID_BTS_DROPDOWN_ORDER:
				this->tunnels->ToggleSortOrder();
				this->SetDirty();
				break;

			case WID_BTS_DROPDOWN_CRITERIA:
				ShowDropDownMenu(this, this->sorter_names, this->tunnels->SortType(), WID_BTS_DROPDOWN_CRITERIA, 0, 0);
				break;
		}
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		if (widget == WID_BTS_DROPDOWN_CRITERIA && this->tunnels->SortType() != index) {
			this->tunnels->SetSortType(index);

			this->SortTunnelList();
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BTS_TUNNEL_LIST);
	}
};

/** Set the default sorting for the tunnels */
Listing BuildTunnelWindow::last_sorting = {true, 0};

/** Available tunnel sorting functions. */
GUITunnelList::SortFunction * const BuildTunnelWindow::sorter_funcs[] = {
	&TunnelIndexSorter,
//	&TunnelPriceSorter,
//	&TunnelSpeedSorter
};

/** Names of the sorting functions. */
const StringID BuildTunnelWindow::sorter_names[] = {
	STR_SORT_BY_NUMBER,
//	STR_SORT_BY_COST,
//	STR_SORT_BY_MAX_SPEED,
	INVALID_STRING_ID
};

/** Widgets of the tunnel gui. */
static const NWidgetPart _nested_build_tunnel_widgets[] = {
	/* Header */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WID_BTS_CAPTION), SetDataTip(STR_SELECT_RAIL_TUNNEL_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),

	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			/* Sort order + criteria buttons */
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WID_BTS_DROPDOWN_ORDER), SetFill(1, 0), SetDataTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_DARK_GREEN, WID_BTS_DROPDOWN_CRITERIA), SetFill(1, 0), SetDataTip(0x0, STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			/* Matrix. */
			NWidget(WWT_MATRIX, COLOUR_DARK_GREEN, WID_BTS_TUNNEL_LIST), SetFill(1, 0), SetResize(0, 22), SetMatrixDataTip(1, 0, STR_SELECT_TUNNEL_SELECTION_TOOLTIP), SetScrollbar(WID_BTS_SCROLLBAR),
		EndContainer(),

		/* scrollbar + resize button */
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BTS_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
};

/** Window definition for the rail tunnel selection window. */
static WindowDesc _build_tunnel_desc(
	WDP_AUTO, "build_tunnel", 200, 114,
	WC_BUILD_BRIDGE, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_tunnel_widgets, lengthof(_nested_build_tunnel_widgets)
);

/**
 * Prepare the data for the build a tunnel window.
 *  If we can't build a tunnel under the given conditions
 *  show an error message.
 *
 * @param start The start tile of the tunnel
 * @param end The end tile of the tunnel
 * @param transport_type The transport type
 * @param road_rail_type The road/rail type
 */
void ShowBuildTunnelWindow(TileIndex start, TileIndex end, TransportType transport_type, byte road_rail_type)
{
	DeleteWindowByClass(WC_BUILD_BRIDGE);

	/* Data type for the tunnel.
	 * Bit 16,15 = transport type,
	 *     14..8 = road/rail types,
	 *      7..0 = type of tunnel */
	uint32 type = (transport_type << 15) | (road_rail_type << 8);

	/* The tunnel length without ramps. */
//	const uint tunnel_len = GetTunnelBridgeLength(start, end);

	/* If Ctrl is being pressed, check whether the last tunnel built is available
	 * If so, return this tunnel type. Otherwise continue normally.
	 * We store tunnel types for each transport type, so we have to check for
	 * the transport type beforehand.
	 */
	uint last_tunnel_type = 0;
	switch (transport_type) {
		case TRANSPORT_RAIL: last_tunnel_type = _last_railtunnel_type; break;
		default: break; // water ways and air routes don't have tunnel types
	}

	const RailtypeInfo *rti = GetRailTypeInfo((RailType)road_rail_type);
	uint16 available_tunnels = GetRailTypeCallback(CBID_TUNNEL_AVAILABLE_VARIANTS, 0, 0, rti, INVALID_TILE, RTSG_CURSORS);
	if (available_tunnels == CALLBACK_FAILED) available_tunnels = 1;

	if (_ctrl_pressed && HasBit(available_tunnels, last_tunnel_type)) {
		DoCommandP(end, start, type | last_tunnel_type, CMD_BUILD_TUNNEL | CMD_MSG(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE), CcBuildTunnel);
		return;
	}

	/* only query tunnel building possibility once, result is the same for all tunnels!
	 * returns CMD_ERROR on failure, and price on success */
//	StringID errmsg = INVALID_STRING_ID;
//	CommandCost ret = DoCommand(end, start, type, CommandFlagsToDCFlags(GetCommandFlags(CMD_BUILD_TUNNEL)) | DC_QUERY_COST, CMD_BUILD_TUNNEL);

	GUITunnelList *tl = NULL;
	if (rti->UsesOverlay()) {
//	if (ret.Failed()) {
//		errmsg = ret.GetErrorMessage();
//	} else {
		/* check which tunnels can be built */
		//const uint tot_tunneldata_len = CalcTunnelLenCostFactor(tunnel_len + 2);

		tl = new GUITunnelList();

		//Money infra_cost = 0;
		switch (transport_type) {
//			case TRANSPORT_ROAD:
//				infra_cost = (tunnel_len + 2) * _price[PR_BUILD_ROAD] * 2;
//				/* In case we add a new road type as well, we must be aware of those costs. */
//				if (IsTunnelTile(start)) infra_cost *= CountBits(GetRoadTypes(start) | (RoadTypes)road_rail_type);
//				break;
//			case TRANSPORT_RAIL: infra_cost = (tunnel_len + 2) * RailBuildCost((RailType)road_rail_type); break;
			default: break;
		}


		/* loop for all tunnel variants */
		for (uint i = 0; i < 15; i++) {
			if (HasBit(available_tunnels, i)) {
				/* tunnel is accepted, add to list */
				BuildTunnelData *item = tl->Append();
				item->index = i;
				item->sprite = GetCustomRailSprite(rti, INVALID_TILE, RTSG_CURSORS, TCX_NORMAL, NULL, i, 0);
//				item->spec = GetTunnelSpec(brd_type);
				/* Add to terraforming & bulldozing costs the cost of the
				 * tunnel itself (not computed with DC_QUERY_COST) */
//				item->cost = ret.GetCost() + (((int64)tot_tunneldata_len * _price[PR_BUILD_TUNNEL] * item->spec->price) >> 8) + infra_cost;
			}
		}
	}

	if (tl != NULL && tl->Length() != 0) {
		new BuildTunnelWindow(&_build_tunnel_desc, start, end, type, tl);
	} else {
		delete tl;
//		ShowErrorMessage(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE, errmsg, WL_INFO, TileX(end) * TILE_SIZE, TileY(end) * TILE_SIZE);
	}
}
