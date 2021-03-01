/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dock_gui.cpp GUI to create amazing water objects. */

#include "stdafx.h"
#include "terraform_gui.h"
#include "window_gui.h"
#include "station_gui.h"
#include "station_func.h"
#include "command_func.h"
#include "water.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "company_func.h"
#include "slope_func.h"
#include "tilehighlight_func.h"
#include "company_base.h"
#include "hotkeys.h"
#include "gui.h"
#include "zoom_func.h"
#include "strings_func.h"
#include "newgrf_dock.h"

#include "widgets/dock_widget.h"

#include "table/sprites.h"
#include "table/strings.h"

#include "safeguards.h"

static void ShowBuildDockStationPicker(Window *parent);
static void ShowBuildDocksDepotPicker(Window *parent);

static DockClassID _selected_dock_class; ///< the currently visible dock class
static int _selected_dock_index;         ///< the index of the selected dock in the current class, or -1
static uint8 _selected_dock_view;

static Axis _ship_depot_direction;

void CcBuildDocks(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd)
{
	if (result.Failed()) return;

	if (_settings_client.sound.confirm) SndPlayTileFx(SND_02_CONSTRUCTION_WATER, tile);
	if (!_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
}

void CcPlaySound_CONSTRUCTION_WATER(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd)
{
	if (result.Succeeded() && _settings_client.sound.confirm) SndPlayTileFx(SND_02_CONSTRUCTION_WATER, tile);
}

class BuildDockWindow : public Window {
	static const int DOCK_MARGIN = 4;
	int line_height;
	int info_height;
	Scrollbar *vscroll;

	void EnsureSelectedDockClassIsVisible()
	{
		uint pos = 0;
		for (int i = 0; i < _selected_dock_class; i++) {
				if (DockClass::Get((DockClassID)i)->GetUISpecCount() == 0) continue;
				pos++;
		}
		this->vscroll->ScrollTowards(pos);
	}

	bool CanRestoreSelectedDock()
	{
		if (_selected_dock_index == -1) return false;

		DockClass *sel_dockclass = DockClass::Get(_selected_dock_class);
		if ((int)sel_dockclass->GetSpecCount() <= _selected_dock_index) return false;

		return sel_dockclass->GetSpec(_selected_dock_index)->IsAvailable();
	}

	uint GetMatrixColumnCount()
	{
		const NWidgetBase *matrix = this->GetWidget<NWidgetBase>(WID_BD_SELECT_MATRIX);
		return 1 + (matrix->current_x - matrix->smallest_x) / matrix->resize_x;
	}

public:
	BuildDockWindow(WindowDesc *desc, WindowNumber number) : Window(desc), info_height(1)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_BD_SCROLLBAR);
		this->FinishInitNested(number);

		ResetObjectToPlace();

		this->vscroll->SetPosition(0);
		this->vscroll->SetCount(DockClass::GetUIClassCount());

		NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BD_SELECT_MATRIX);
		matrix->SetScrollbar(this->GetScrollbar(WID_BD_SELECT_SCROLL));

		this->SelectOtherClass(_selected_dock_class);
		if (this->CanRestoreSelectedDock()) {
			this->SelectOtherDock(_selected_dock_index);
		} else {
			this->SelectFirstAvailableDock(true);
		}
		assert(DockClass::Get(_selected_dock_class)->GetUISpecCount() > 0);
		this->EnsureSelectedDockClassIsVisible();
		this->GetWidget<NWidgetMatrix>(WID_BD_DOCK_MATRIX)->SetCount(4);
	}

	void SetStringParameters(int widget) const override
	{
		switch (widget) {
			case WID_BD_DOCK_NAME: {
				const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
				SetDParam(0, spec != NULL ? spec->name : STR_EMPTY);
				break;
			}

			case WID_BD_DOCK_SIZE: {
				const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
				int size = spec == NULL ? 0 : spec->size;
				SetDParam(0, GB(size, 0, 4));
				SetDParam(1, GB(size, 4, 4));
				break;
			}

			default: break;
		}
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget)
		{
			case WID_BD_CLASS_LIST: {
				for (uint i = 0; i < DockClass::GetClassCount(); i++) {
					DockClass *dockclass = DockClass::Get((DockClassID)i);
					if (dockclass->GetUISpecCount() == 0) continue;
					size->width = std::max(size->width, GetStringBoundingBox(dockclass->name).width);
				}
				size->width += padding.width;
				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				resize->height = this->line_height;
				size->height = 5 * this->line_height;
				break;
			}

			case WID_BD_DOCK_NAME:
			case WID_BD_DOCK_SIZE:
				/* We do not want the window to resize when selecting objects; better clip texts */
				size->width = 0;
				break;

			case WID_BD_DOCK_MATRIX: {
				/* Get the right amount of buttons based on the current spec. */
				const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
				if (spec != NULL) {
					if (spec->views >= 2) size->width  += resize->width;
					if (spec->views >= 4) size->height += resize->height;
				}
				resize->width = 0;
				resize->height = 0;
				break;
			}

			case WID_BD_DOCK_SPRITE: {
				bool two_wide = false;  // Whether there will be two widgets next to each other in the matrix or not.
				int height[2] = {0, 0}; // The height for the different views; in this case views 1/2 and 4.

				/* Get the height and view information. */
				for (int i = 0; i < NUM_DOCKS; i++) {
					const DockSpec *spec = DockSpec::Get(i);
					if (!spec->IsEverAvailable()) continue;
					two_wide |= spec->views >= 2;
					height[spec->views / 4] = std::max<int>(DockSpec::Get(i)->height, height[spec->views / 4]);
				}

				/* Determine the pixel heights. */
				for (size_t i = 0; i < lengthof(height); i++) {
					height[i] *= ScaleGUITrad(TILE_HEIGHT);
					height[i] += ScaleGUITrad(TILE_PIXELS) + 2 * DOCK_MARGIN;
				}

				/* Now determine the size of the minimum widgets. When there are two columns, then
				 * we want these columns to be slightly less wide. When there are two rows, then
				 * determine the size of the widgets based on the maximum size for a single row
				 * of widgets, or just the twice the widget height of the two row ones. */
				size->height = std::max(height[0], height[1] * 2 + 2);
				if (two_wide) {
					size->width  = (3 * ScaleGUITrad(TILE_PIXELS) + 2 * DOCK_MARGIN) * 2 + 2;
				} else {
					size->width  = 4 * ScaleGUITrad(TILE_PIXELS) + 2 * DOCK_MARGIN;
				}

				/* Get the right size for the single widget based on the current spec. */
				const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
				if (spec != NULL) {
					if (spec->views >= 2) size->width  = size->width  / 2 - 1;
					if (spec->views >= 4) size->height = size->height / 2 - 1;
				}
				break;
			}

			case WID_BD_INFO:
				size->height = this->info_height;
				break;

			case WID_BD_SELECT_MATRIX:
				fill->height = 1;
				resize->height = 1;
				break;

			case WID_BD_SELECT_IMAGE:
				size->width  = ScaleGUITrad(64) + 2;
				size->height = ScaleGUITrad(58) + 2;
				break;

			default: break;
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		switch (GB(widget, 0, 16)) {
			case WID_BD_CLASS_LIST: {
				int y = r.top;
				uint pos = 0;
				for (uint i = 0; i < DockClass::GetClassCount(); i++) {
					DockClass *objclass = DockClass::Get((DockClassID)i);
					if (objclass->GetUISpecCount() == 0) continue;
					if (!this->vscroll->IsVisible(pos++)) continue;
					DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, objclass->name,
							((int)i == _selected_dock_class) ? TC_WHITE : TC_BLACK);
					y += this->line_height;
				}
				break;
			}

			case WID_BD_DOCK_SPRITE: {
				const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
				if (spec == NULL) break;

				/* Height of the selection matrix.
				 * Depending on the number of views, the matrix has a 1x1, 1x2, 2x1 or 2x2 layout. To make the previews
				 * look nice in all layouts, we use the 4x4 layout (smallest previews) as starting point. For the bigger
				 * previews in the layouts with less views we add space homogeneously on all sides, so the 4x4 preview-rectangle
				 * is centered in the 2x1, 1x2 resp. 1x1 buttons. */
				uint matrix_height = this->GetWidget<NWidgetMatrix>(WID_BD_DOCK_MATRIX)->current_y;

				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				if (FillDrawPixelInfo(&tmp_dpi, r.left, r.top, r.right - r.left + 1, r.bottom - r.top + 1)) {
					DrawPixelInfo *old_dpi = _cur_dpi;
					_cur_dpi = &tmp_dpi;
					if (spec->grf_prop.grffile == NULL) {
						const DrawTileSprites *dts = GetStationTileLayout(STATION_DOCK, spec->grf_prop.local_id);
						DrawOrigTileSeqInGUI((r.right - r.left) / 2 - 1, (r.bottom - r.top + matrix_height / 2) / 2 - DOCK_MARGIN - ScaleGUITrad(TILE_PIXELS), dts, PAL_NONE);
					} else {
						DrawNewDockTileInGUI((r.right - r.left) / 2 - 1, (r.bottom - r.top + matrix_height / 2) / 2 - DOCK_MARGIN - ScaleGUITrad(TILE_PIXELS), spec, GB(widget, 16, 16));
					}
					_cur_dpi = old_dpi;
				}
				break;
			}

			case WID_BD_SELECT_IMAGE: {
				DockClass *objclass = DockClass::Get(_selected_dock_class);
				int obj_index = objclass->GetIndexFromUI(GB(widget, 16, 16));
				if (obj_index < 0) break;
				const DockSpec *spec = objclass->GetSpec(obj_index);
				if (spec == NULL) break;

				if (!spec->IsAvailable()) {
					GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, PC_BLACK, FILLRECT_CHECKER);
				}
				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				if (FillDrawPixelInfo(&tmp_dpi, r.left + 1, r.top, (r.right - 1) - (r.left + 1) + 1, r.bottom - r.top + 1)) {
					DrawPixelInfo *old_dpi = _cur_dpi;
					_cur_dpi = &tmp_dpi;
					if (spec->grf_prop.grffile == NULL) {
						const DrawTileSprites *dts = GetStationTileLayout(STATION_DOCK, spec->grf_prop.local_id);
						DrawOrigTileSeqInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - DOCK_MARGIN - ScaleGUITrad(TILE_PIXELS), dts, PAL_NONE);
					} else {
						DrawNewDockTileInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - DOCK_MARGIN - ScaleGUITrad(TILE_PIXELS), spec,
								std::min((uint)_selected_dock_view, spec->views - 1U));
					}
					_cur_dpi = old_dpi;
				}
				break;
			}

			case WID_BD_INFO: {
				const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
				if (spec == NULL) break;

#if 0
				/* Get the extra message for the GUI */
				if (HasBit(spec->callback_mask, CBM_OBJ_FUND_MORE_TEXT)) {
					uint16 callback_res = GetObjectCallback(CBID_DOCK_FUND_MORE_TEXT, 0, 0, spec, NULL, INVALID_TILE, _selected_dock_view);
					if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
						if (callback_res > 0x400) {
							ErrorUnknownCallbackResult(spec->grf_prop.grffile->grfid, CBID_DOCK_FUND_MORE_TEXT, callback_res);
						} else {
							StringID message = GetGRFStringID(spec->grf_prop.grffile->grfid, 0xD000 + callback_res);
							if (message != STR_NULL && message != STR_UNDEFINED) {
								StartTextRefStackUsage(spec->grf_prop.grffile, 6);
								/* Use all the available space left from where we stand up to the
								 * end of the window. We ALSO enlarge the window if needed, so we
								 * can 'go' wild with the bottom of the window. */
								int y = DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, message, TC_ORANGE) - r.top;
								StopTextRefStackUsage();
								if (y > this->info_height) {
									BuildDockWindow *bdw = const_cast<BuildDockWindow *>(this);
									bow->info_height = y + 2;
									bow->ReInit();
								}
							}
						}
					}
				}
#endif
			}
		}
	}

	/**
	 * Select the specified object class.
	 * @param object_class_index Object class index to select.
	 */
	void SelectOtherClass(DockClassID dock_class_index)
	{
		_selected_dock_class = dock_class_index;
		this->GetWidget<NWidgetMatrix>(WID_BD_SELECT_MATRIX)->SetCount(DockClass::Get(_selected_dock_class)->GetUISpecCount());
	}

	/**
	 * Select the specified object in #_selected_dock_class class.
	 * @param object_index Object index to select, \c -1 means select nothing.
	 */
	void SelectOtherDock(int object_index)
	{
		_selected_dock_index = object_index;
		if (_selected_dock_index != -1) {
			const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
			_selected_dock_view = std::min((uint)_selected_dock_view, spec->views - 1U);
			this->ReInit();
		} else {
			_selected_dock_view = 0;
		}

		if (_selected_dock_index != -1) {
			SetObjectToPlaceWnd(SPR_CURSOR_DOCK, PAL_NONE, HT_SPECIAL, this);
		}

		this->UpdateButtons(_selected_dock_class, _selected_dock_index, _selected_dock_view);
	}

	void UpdateSelectSize()
	{
		if (_selected_dock_index == -1) {
			SetTileSelectSize(1, 1);
		} else {
			const DockSpec *spec = DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index);
			int w = GB(spec->size, HasBit(_selected_dock_view, 0) ? 4 : 0, 4);
			int h = GB(spec->size, HasBit(_selected_dock_view, 0) ? 0 : 4, 4);
			SetTileSelectSize(w, h);
		}
	}

	/**
	 * Update buttons to show the selection to the user.
	 * @param sel_class The class of the selected object.
	 * @param sel_index Index of the object to select, or \c -1 .
	 * @param sel_view View of the object to select.
	 */
	void UpdateButtons(DockClassID sel_class, int sel_index, uint sel_view)
	{
		int view_number, object_number;
		if (sel_index == -1) {
			view_number = -1; // If no object selected, also hide the selected view.
			object_number = -1;
		} else {
			view_number = sel_view;
			object_number = DockClass::Get(sel_class)->GetUIFromIndex(sel_index);
		}

		this->GetWidget<NWidgetMatrix>(WID_BD_DOCK_MATRIX)->SetClicked(view_number);
		this->GetWidget<NWidgetMatrix>(WID_BD_SELECT_MATRIX)->SetClicked(object_number);
		this->UpdateSelectSize();
		this->SetDirty();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BD_CLASS_LIST);
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (GB(widget, 0, 16)) {
			case WID_BD_CLASS_LIST: {
				int num_clicked = this->vscroll->GetPosition() + (pt.y - this->nested_array[widget]->pos_y) / this->line_height;
				if (num_clicked >= (int)DockClass::GetUIClassCount()) break;

				this->SelectOtherClass(DockClass::GetUIClass(num_clicked));
				this->SelectFirstAvailableDock(false);
				break;
			}

			case WID_BD_SELECT_IMAGE: {
				DockClass *objclass = DockClass::Get(_selected_dock_class);
				int num_clicked = objclass->GetIndexFromUI(GB(widget, 16, 16));
				if (num_clicked >= 0 && objclass->GetSpec(num_clicked)->IsAvailable()) this->SelectOtherDock(num_clicked);
				break;
			}

			case WID_BD_DOCK_SPRITE:
				if (_selected_dock_index != -1) {
					_selected_dock_view = GB(widget, 16, 16);
					this->SelectOtherDock(_selected_dock_index); // Re-select the object for a different view.
				}
				break;
		}
	}

	void OnPlaceObject(Point pt, TileIndex tile) override
	{
		uint32 p2 = (uint32)INVALID_STATION << 16; // no station to join
		p2 |= DockClass::Get(_selected_dock_class)->GetSpec(_selected_dock_index)->Index();

		/* tile is always the land tile, so need to evaluate _thd.pos */
		CommandContainer cmdcont = { tile, _ctrl_pressed, p2, CMD_BUILD_DOCK | CMD_MSG(STR_ERROR_CAN_T_BUILD_DOCK_HERE), CcBuildDocks, "" };

		/* Determine the watery part of the dock. */
		DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile));
		TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(tile, ReverseDiagDir(dir)) : tile);

		ShowSelectStationIfNeeded(cmdcont, TileArea(tile, tile_to));
	}

	void OnPlaceObjectAbort() override
	{
		this->UpdateButtons(_selected_dock_class, -1, _selected_dock_view);
	}

	/**
	 * Select the first available object.
	 * @param change_class If true, change the class if no object in the current
	 *   class is available.
	 */
	void SelectFirstAvailableDock(bool change_class)
	{
		/* First try to select an object in the selected class. */
		DockClass *sel_objclass = DockClass::Get(_selected_dock_class);
		for (uint i = 0; i < sel_objclass->GetSpecCount(); i++) {
			const DockSpec *spec = sel_objclass->GetSpec(i);
			if (spec->IsAvailable()) {
				this->SelectOtherDock(i);
				return;
			}
		}
		if (change_class) {
			/* If that fails, select the first available object
			 * from a random class. */
			for (DockClassID j = DOCK_CLASS_BEGIN; j < DOCK_CLASS_MAX; j++) {
				DockClass *objclass = DockClass::Get(j);
				for (uint i = 0; i < objclass->GetSpecCount(); i++) {
					const DockSpec *spec = objclass->GetSpec(i);
					if (spec->IsAvailable()) {
						this->SelectOtherClass(j);
						this->SelectOtherDock(i);
						return;
					}
				}
			}
		}
		/* If all objects are unavailable, select nothing... */
		if (DockClass::Get(_selected_dock_class)->GetUISpecCount() == 0) {
			/* ... but make sure that the class is not empty. */
			for (DockClassID j = DOCK_CLASS_BEGIN; j < DOCK_CLASS_MAX; j++) {
				if (DockClass::Get(j)->GetUISpecCount() > 0) {
					this->SelectOtherClass(j);
					break;
				}
			}
		}
		this->SelectOtherDock(-1);
	}
};

static const NWidgetPart _nested_build_dock_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_DOCK_BUILD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL), SetPadding(2, 0, 0, 0),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 2, 5),
					NWidget(WWT_MATRIX, COLOUR_GREY, WID_BD_CLASS_LIST), SetFill(1, 0), SetMatrixDataTip(1, 0, STR_DOCK_BUILD_CLASS_TOOLTIP), SetScrollbar(WID_BD_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BD_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 0, 5),
					NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BD_DOCK_MATRIX), SetPIP(0, 2, 0),
						NWidget(WWT_PANEL, COLOUR_GREY, WID_BD_DOCK_SPRITE), SetDataTip(0x0, STR_DOCK_BUILD_PREVIEW_TOOLTIP), EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_TEXT, COLOUR_DARK_GREEN, WID_BD_DOCK_NAME), SetDataTip(STR_ORANGE_STRING, STR_NULL), SetPadding(2, 5, 2, 5),
				NWidget(WWT_TEXT, COLOUR_DARK_GREEN, WID_BD_DOCK_SIZE), SetDataTip(STR_DOCK_BUILD_SIZE, STR_NULL), SetPadding(2, 5, 2, 5),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_BD_SELECT_SCROLL),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BD_SELECT_MATRIX), SetFill(0, 1), SetPIP(0, 2, 0),
						NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BD_SELECT_IMAGE), SetMinimalSize(66, 60), SetDataTip(0x0, STR_DOCK_BUILD_TOOLTIP),
								SetFill(0, 0), SetResize(0, 0), SetScrollbar(WID_BD_SELECT_SCROLL),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BD_SELECT_SCROLL),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BD_INFO), SetPadding(2, 5, 0, 5), SetFill(1, 0), SetResize(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(0, 1), EndContainer(),
				NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_dock_desc(
	WDP_AUTO, "build_object", 0, 0,
	WC_BUILD_OBJECT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_dock_widgets, lengthof(_nested_build_dock_widgets)
);

/** Show our object picker.  */
void ShowBuildDockPicker()
{
	/* Don't show the place object button when there are no objects to place. */
	//if (DockClass::GetUIClassCount() > 0) {
		AllocateWindowDescFront<BuildDockWindow>(&_build_dock_desc, 0);
	//}
}

/**
 * Gets the other end of the aqueduct, if possible.
 * @param      tile_from The begin tile for the aqueduct.
 * @param[out] tile_to   The tile till where to show a selection for the aqueduct.
 * @return The other end of the aqueduct, or otherwise a tile in line with the aqueduct to cause the right error message.
 */
static TileIndex GetOtherAqueductEnd(TileIndex tile_from, TileIndex *tile_to = nullptr)
{
	int z;
	DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile_from, &z));

	/* If the direction isn't right, just return the next tile so the command
	 * complains about the wrong slope instead of the ends not matching up.
	 * Make sure the coordinate is always a valid tile within the map, so we
	 * don't go "off" the map. That would cause the wrong error message. */
	if (!IsValidDiagDirection(dir)) return TILE_ADDXY(tile_from, TileX(tile_from) > 2 ? -1 : 1, 0);

	/* Direction the aqueduct is built to. */
	TileIndexDiff offset = TileOffsByDiagDir(ReverseDiagDir(dir));
	/* The maximum length of the aqueduct. */
	int max_length = std::min<int>(_settings_game.construction.max_bridge_length, DistanceFromEdgeDir(tile_from, ReverseDiagDir(dir)) - 1);

	TileIndex endtile = tile_from;
	for (int length = 0; IsValidTile(endtile) && TileX(endtile) != 0 && TileY(endtile) != 0; length++) {
		endtile = TILE_ADD(endtile, offset);

		if (length > max_length) break;

		if (GetTileMaxZ(endtile) > z) {
			if (tile_to != nullptr) *tile_to = endtile;
			break;
		}
	}

	return endtile;
}

/** Toolbar window for constructing water infrastructure. */
struct BuildDocksToolbarWindow : Window {
	DockToolbarWidgets last_clicked_widget; ///< Contains the last widget that has been clicked on this toolbar.

	BuildDocksToolbarWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->last_clicked_widget = WID_DT_INVALID;
		this->InitNested(window_number);
		this->OnInvalidateData();
		if (_settings_client.gui.link_terraform_toolbar) ShowTerraformToolbar(this);
	}

	~BuildDocksToolbarWindow()
	{
		if (_game_mode == GM_NORMAL && this->IsWidgetLowered(WID_DT_STATION)) SetViewportCatchmentStation(nullptr, true);
		if (_settings_client.gui.link_terraform_toolbar) DeleteWindowById(WC_SCEN_LAND_GEN, 0, false);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;

		bool can_build = CanBuildVehicleInfrastructure(VEH_SHIP);
		this->SetWidgetsDisabledState(!can_build,
			WID_DT_DEPOT,
			WID_DT_STATION,
			WID_DT_BUOY,
			WIDGET_LIST_END);
		if (!can_build) {
			DeleteWindowById(WC_BUILD_STATION, TRANSPORT_WATER);
			DeleteWindowById(WC_BUILD_DEPOT, TRANSPORT_WATER);
		}

		if (_game_mode != GM_EDITOR) {
			if (!can_build) {
				/* Show in the tooltip why this button is disabled. */
				this->GetWidget<NWidgetCore>(WID_DT_DEPOT)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_DT_STATION)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
				this->GetWidget<NWidgetCore>(WID_DT_BUOY)->SetToolTip(STR_TOOLBAR_DISABLED_NO_VEHICLE_AVAILABLE);
			} else {
				this->GetWidget<NWidgetCore>(WID_DT_DEPOT)->SetToolTip(STR_WATERWAYS_TOOLBAR_BUILD_DEPOT_TOOLTIP);
				this->GetWidget<NWidgetCore>(WID_DT_STATION)->SetToolTip(STR_WATERWAYS_TOOLBAR_BUILD_DOCK_TOOLTIP);
				this->GetWidget<NWidgetCore>(WID_DT_BUOY)->SetToolTip(STR_WATERWAYS_TOOLBAR_BUOY_TOOLTIP);
			}
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_DT_CANAL: // Build canal button
				HandlePlacePushButton(this, WID_DT_CANAL, SPR_CURSOR_CANAL, HT_RECT);
				break;

			case WID_DT_LOCK: // Build lock button
				HandlePlacePushButton(this, WID_DT_LOCK, SPR_CURSOR_LOCK, HT_SPECIAL);
				break;

			case WID_DT_DEMOLISH: // Demolish aka dynamite button
				HandlePlacePushButton(this, WID_DT_DEMOLISH, ANIMCURSOR_DEMOLISH, HT_RECT | HT_DIAGONAL);
				break;

			case WID_DT_DEPOT: // Build depot button
				if (HandlePlacePushButton(this, WID_DT_DEPOT, SPR_CURSOR_SHIP_DEPOT, HT_RECT)) ShowBuildDocksDepotPicker(this);
				break;

			case WID_DT_STATION: // Build station button
				if (HandlePlacePushButton(this, WID_DT_STATION, SPR_CURSOR_DOCK, HT_SPECIAL)) ShowBuildDockStationPicker(this);
				break;

			case WID_DT_BUOY: // Build buoy button
				HandlePlacePushButton(this, WID_DT_BUOY, SPR_CURSOR_BUOY, HT_RECT);
				break;

			case WID_DT_RIVER: // Build river button (in scenario editor)
				if (_game_mode != GM_EDITOR) return;
				HandlePlacePushButton(this, WID_DT_RIVER, SPR_CURSOR_RIVER, HT_RECT);
				break;

			case WID_DT_BUILD_AQUEDUCT: // Build aqueduct button
				HandlePlacePushButton(this, WID_DT_BUILD_AQUEDUCT, SPR_CURSOR_AQUEDUCT, HT_SPECIAL);
				break;

			default: return;
		}
		this->last_clicked_widget = (DockToolbarWidgets)widget;
	}

	void OnPlaceObject(Point pt, TileIndex tile) override
	{
		switch (this->last_clicked_widget) {
			case WID_DT_CANAL: // Build canal button
				VpStartPlaceSizing(tile, (_game_mode == GM_EDITOR) ? VPM_X_AND_Y : VPM_X_OR_Y, DDSP_CREATE_WATER);
				break;

			case WID_DT_LOCK: // Build lock button
				DoCommandP(tile, 0, 0, CMD_BUILD_LOCK | CMD_MSG(STR_ERROR_CAN_T_BUILD_LOCKS), CcBuildDocks);
				break;

			case WID_DT_DEMOLISH: // Demolish aka dynamite button
				PlaceProc_DemolishArea(tile);
				break;

			case WID_DT_DEPOT: // Build depot button
				DoCommandP(tile, _ship_depot_direction, 0, CMD_BUILD_SHIP_DEPOT | CMD_MSG(STR_ERROR_CAN_T_BUILD_SHIP_DEPOT), CcBuildDocks);
				break;

			case WID_DT_STATION: { // Build station button
				uint32 p2 = (uint32)INVALID_STATION << 16; // no station to join

				/* tile is always the land tile, so need to evaluate _thd.pos */
				CommandContainer cmdcont = { tile, _ctrl_pressed, p2, CMD_BUILD_DOCK | CMD_MSG(STR_ERROR_CAN_T_BUILD_DOCK_HERE), CcBuildDocks, "" };

				/* Determine the watery part of the dock. */
				DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile));
				TileIndex tile_to = (dir != INVALID_DIAGDIR ? TileAddByDiagDir(tile, ReverseDiagDir(dir)) : tile);

				ShowSelectStationIfNeeded(cmdcont, TileArea(tile, tile_to));
				break;
			}

			case WID_DT_BUOY: // Build buoy button
				DoCommandP(tile, 0, 0, CMD_BUILD_BUOY | CMD_MSG(STR_ERROR_CAN_T_POSITION_BUOY_HERE), CcBuildDocks);
				break;

			case WID_DT_RIVER: // Build river button (in scenario editor)
				VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CREATE_RIVER);
				break;

			case WID_DT_BUILD_AQUEDUCT: // Build aqueduct button
				DoCommandP(tile, GetOtherAqueductEnd(tile), TRANSPORT_WATER << 15, CMD_BUILD_BRIDGE | CMD_MSG(STR_ERROR_CAN_T_BUILD_AQUEDUCT_HERE), CcBuildBridge);
				break;

			default: NOT_REACHED();
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt) override
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	void OnPlaceMouseUp(ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x != -1) {
			switch (select_proc) {
				case DDSP_DEMOLISH_AREA:
					GUIPlaceProcDragXY(select_proc, start_tile, end_tile);
					break;
				case DDSP_CREATE_WATER:
					DoCommandP(end_tile, start_tile, (_game_mode == GM_EDITOR && _ctrl_pressed) ? WATER_CLASS_SEA : WATER_CLASS_CANAL, CMD_BUILD_CANAL | CMD_MSG(STR_ERROR_CAN_T_BUILD_CANALS), CcPlaySound_CONSTRUCTION_WATER);
					break;
				case DDSP_CREATE_RIVER:
					DoCommandP(end_tile, start_tile, WATER_CLASS_RIVER, CMD_BUILD_CANAL | CMD_MSG(STR_ERROR_CAN_T_PLACE_RIVERS), CcPlaySound_CONSTRUCTION_WATER);
					break;

				default: break;
			}
		}
	}

	void OnPlaceObjectAbort() override
	{
		if (_game_mode != GM_EDITOR && this->IsWidgetLowered(WID_DT_STATION)) SetViewportCatchmentStation(nullptr, true);

		this->RaiseButtons();

		DeleteWindowById(WC_BUILD_STATION, TRANSPORT_WATER);
		DeleteWindowById(WC_BUILD_DEPOT, TRANSPORT_WATER);
		DeleteWindowById(WC_SELECT_STATION, 0);
		DeleteWindowByClass(WC_BUILD_BRIDGE);
	}

	void OnPlacePresize(Point pt, TileIndex tile_from) override
	{
		TileIndex tile_to = tile_from;

		if (this->last_clicked_widget == WID_DT_BUILD_AQUEDUCT) {
			GetOtherAqueductEnd(tile_from, &tile_to);
		} else {
			DiagDirection dir = GetInclinedSlopeDirection(GetTileSlope(tile_from));
			if (IsValidDiagDirection(dir)) {
				/* Locks and docks always select the tile "down" the slope. */
				tile_to = TileAddByDiagDir(tile_from, ReverseDiagDir(dir));
				/* Locks also select the tile "up" the slope. */
				if (this->last_clicked_widget == WID_DT_LOCK) tile_from = TileAddByDiagDir(tile_from, dir);
			}
		}

		VpSetPresizeRange(tile_from, tile_to);
	}

	static HotkeyList hotkeys;
};

/**
 * Handler for global hotkeys of the BuildDocksToolbarWindow.
 * @param hotkey Hotkey
 * @return ES_HANDLED if hotkey was accepted.
 */
static EventState DockToolbarGlobalHotkeys(int hotkey)
{
	if (_game_mode != GM_NORMAL) return ES_NOT_HANDLED;
	Window *w = ShowBuildDocksToolbar();
	if (w == nullptr) return ES_NOT_HANDLED;
	return w->OnHotkey(hotkey);
}

const uint16 _dockstoolbar_aqueduct_keys[] = {'B', '8', 0};

static Hotkey dockstoolbar_hotkeys[] = {
	Hotkey('1', "canal", WID_DT_CANAL),
	Hotkey('2', "lock", WID_DT_LOCK),
	Hotkey('3', "demolish", WID_DT_DEMOLISH),
	Hotkey('4', "depot", WID_DT_DEPOT),
	Hotkey('5', "dock", WID_DT_STATION),
	Hotkey('6', "buoy", WID_DT_BUOY),
	Hotkey('7', "river", WID_DT_RIVER),
	Hotkey(_dockstoolbar_aqueduct_keys, "aqueduct", WID_DT_BUILD_AQUEDUCT),
	HOTKEY_LIST_END
};
HotkeyList BuildDocksToolbarWindow::hotkeys("dockstoolbar", dockstoolbar_hotkeys, DockToolbarGlobalHotkeys);

/**
 * Nested widget parts of docks toolbar, game version.
 * Position of #WID_DT_RIVER widget has changed.
 */
static const NWidgetPart _nested_build_docks_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_WATERWAYS_TOOLBAR_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL_LTR),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_CANAL), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_CANAL, STR_WATERWAYS_TOOLBAR_BUILD_CANALS_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_LOCK), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_LOCK, STR_WATERWAYS_TOOLBAR_BUILD_LOCKS_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetMinimalSize(5, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEMOLISH), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEPOT), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SHIP_DEPOT, STR_WATERWAYS_TOOLBAR_BUILD_DEPOT_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_STATION), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_SHIP_DOCK, STR_WATERWAYS_TOOLBAR_BUILD_DOCK_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUOY), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUOY, STR_WATERWAYS_TOOLBAR_BUOY_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUILD_AQUEDUCT), SetMinimalSize(23, 22), SetFill(0, 1), SetDataTip(SPR_IMG_AQUEDUCT, STR_WATERWAYS_TOOLBAR_BUILD_AQUEDUCT_TOOLTIP),
	EndContainer(),
};

static WindowDesc _build_docks_toolbar_desc(
	WDP_ALIGN_TOOLBAR, "toolbar_water", 0, 0,
	WC_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_docks_toolbar_widgets, lengthof(_nested_build_docks_toolbar_widgets),
	&BuildDocksToolbarWindow::hotkeys
);

/**
 * Open the build water toolbar window
 *
 * If the terraform toolbar is linked to the toolbar, that window is also opened.
 *
 * @return newly opened water toolbar, or nullptr if the toolbar could not be opened.
 */
Window *ShowBuildDocksToolbar()
{
	if (!Company::IsValidID(_local_company)) return nullptr;

	DeleteWindowByClass(WC_BUILD_TOOLBAR);
	return AllocateWindowDescFront<BuildDocksToolbarWindow>(&_build_docks_toolbar_desc, TRANSPORT_WATER);
}

/**
 * Nested widget parts of docks toolbar, scenario editor version.
 * Positions of #WID_DT_DEPOT, #WID_DT_STATION, and #WID_DT_BUOY widgets have changed.
 */
static const NWidgetPart _nested_build_docks_scen_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_WATERWAYS_TOOLBAR_CAPTION_SE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_CANAL), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_CANAL, STR_WATERWAYS_TOOLBAR_CREATE_LAKE_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_LOCK), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_LOCK, STR_WATERWAYS_TOOLBAR_BUILD_LOCKS_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetMinimalSize(5, 22), SetFill(1, 1), EndContainer(),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_DEMOLISH), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_DYNAMITE, STR_TOOLTIP_DEMOLISH_BUILDINGS_ETC),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_RIVER), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_BUILD_RIVER, STR_WATERWAYS_TOOLBAR_CREATE_RIVER_TOOLTIP),
		NWidget(WWT_IMGBTN, COLOUR_DARK_GREEN, WID_DT_BUILD_AQUEDUCT), SetMinimalSize(22, 22), SetFill(0, 1), SetDataTip(SPR_IMG_AQUEDUCT, STR_WATERWAYS_TOOLBAR_BUILD_AQUEDUCT_TOOLTIP),
	EndContainer(),
};

/** Window definition for the build docks in scenario editor window. */
static WindowDesc _build_docks_scen_toolbar_desc(
	WDP_AUTO, "toolbar_water_scen", 0, 0,
	WC_SCEN_BUILD_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_build_docks_scen_toolbar_widgets, lengthof(_nested_build_docks_scen_toolbar_widgets)
);

/**
 * Open the build water toolbar window for the scenario editor.
 *
 * @return newly opened water toolbar, or nullptr if the toolbar could not be opened.
 */
Window *ShowBuildDocksScenToolbar()
{
	return AllocateWindowDescFront<BuildDocksToolbarWindow>(&_build_docks_scen_toolbar_desc, TRANSPORT_WATER);
}

/** Widget numbers of the build-dock GUI. */
enum BuildDockStationWidgets {
	BDSW_BACKGROUND, ///< Background panel.
	BDSW_LT_OFF,     ///< 'Off' button of coverage high light.
	BDSW_LT_ON,      ///< 'On' button of coverage high light.
	BDSW_INFO,       ///< 'Coverage highlight' label.
};

struct BuildDocksStationWindow : public PickerWindowBase {
public:
	BuildDocksStationWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_WATER);
		this->LowerWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
	}

	~BuildDocksStationWindow()
	{
		DeleteWindowById(WC_SELECT_STATION, 0);
	}

	void OnPaint() override
	{
		int rad = (_settings_game.station.modified_catchment) ? CA_DOCK : CA_UNMODIFIED;

		this->DrawWidgets();

		if (_settings_client.gui.station_show_coverage) {
			SetTileSelectBigSize(-rad, -rad, 2 * rad, 2 * rad);
		} else {
			SetTileSelectSize(1, 1);
		}

		/* strings such as 'Size' and 'Coverage Area' */
		int top = this->GetWidget<NWidgetBase>(BDSW_LT_OFF)->pos_y + this->GetWidget<NWidgetBase>(BDSW_LT_OFF)->current_y + WD_PAR_VSEP_NORMAL;
		NWidgetBase *back_nwi = this->GetWidget<NWidgetBase>(BDSW_BACKGROUND);
		int right  = back_nwi->pos_x + back_nwi->current_x;
		int bottom = back_nwi->pos_y + back_nwi->current_y;
		top = DrawStationCoverageAreaText(back_nwi->pos_x + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, SCT_ALL, rad, false) + WD_PAR_VSEP_NORMAL;
		top = DrawStationCoverageAreaText(back_nwi->pos_x + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, SCT_ALL, rad, true) + WD_PAR_VSEP_NORMAL;
		/* Resize background if the window is too small.
		 * Never make the window smaller to avoid oscillating if the size change affects the acceptance.
		 * (This is the case, if making the window bigger moves the mouse into the window.) */
		if (top > bottom) {
			ResizeWindow(this, 0, top - bottom, false);
		}
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case BDSW_LT_OFF:
			case BDSW_LT_ON:
				this->RaiseWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
				_settings_client.gui.station_show_coverage = (widget != BDSW_LT_OFF);
				this->LowerWidget(_settings_client.gui.station_show_coverage + BDSW_LT_OFF);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				this->SetDirty();
				SetViewportCatchmentStation(nullptr, true);
				break;
		}
	}

	void OnRealtimeTick(uint delta_ms) override
	{
		CheckRedrawStationCoverage(this);
	}
};

/** Nested widget parts of a build dock station window. */
static const NWidgetPart _nested_build_dock_station_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_DOCK_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BDSW_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN, BDSW_INFO), SetMinimalSize(148, 14), SetDataTip(STR_STATION_BUILD_COVERAGE_AREA_TITLE, STR_NULL),
		NWidget(NWID_HORIZONTAL), SetPIP(14, 0, 14),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BDSW_LT_OFF), SetMinimalSize(40, 12), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_COVERAGE_OFF, STR_STATION_BUILD_COVERAGE_AREA_OFF_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, BDSW_LT_ON), SetMinimalSize(40, 12), SetFill(1, 0), SetDataTip(STR_STATION_BUILD_COVERAGE_ON, STR_STATION_BUILD_COVERAGE_AREA_ON_TOOLTIP),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 20), SetResize(0, 1),
	EndContainer(),
};

static WindowDesc _build_dock_station_desc(
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_STATION, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_dock_station_widgets, lengthof(_nested_build_dock_station_widgets)
);

static void ShowBuildDockStationPicker(Window *parent)
{
	//new BuildDocksStationWindow(&_build_dock_station_desc, parent);
	ShowBuildDockPicker();
}

struct BuildDocksDepotWindow : public PickerWindowBase {
private:
	static void UpdateDocksDirection()
	{
		if (_ship_depot_direction != AXIS_X) {
			SetTileSelectSize(1, 2);
		} else {
			SetTileSelectSize(2, 1);
		}
	}

public:
	BuildDocksDepotWindow(WindowDesc *desc, Window *parent) : PickerWindowBase(desc, parent)
	{
		this->InitNested(TRANSPORT_WATER);
		this->LowerWidget(_ship_depot_direction + WID_BDD_X);
		UpdateDocksDirection();
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
	{
		switch (widget) {
			case WID_BDD_X:
			case WID_BDD_Y:
				size->width  = ScaleGUITrad(96) + 2;
				size->height = ScaleGUITrad(64) + 2;
				break;
		}
	}

	void OnPaint() override
	{
		this->DrawWidgets();

		int x1 = ScaleGUITrad(63) + 1;
		int x2 = ScaleGUITrad(31) + 1;
		int y1 = ScaleGUITrad(17) + 1;
		int y2 = ScaleGUITrad(33) + 1;

		DrawShipDepotSprite(this->GetWidget<NWidgetBase>(WID_BDD_X)->pos_x + x1, this->GetWidget<NWidgetBase>(WID_BDD_X)->pos_y + y1, AXIS_X, DEPOT_PART_NORTH);
		DrawShipDepotSprite(this->GetWidget<NWidgetBase>(WID_BDD_X)->pos_x + x2, this->GetWidget<NWidgetBase>(WID_BDD_X)->pos_y + y2, AXIS_X, DEPOT_PART_SOUTH);
		DrawShipDepotSprite(this->GetWidget<NWidgetBase>(WID_BDD_Y)->pos_x + x2, this->GetWidget<NWidgetBase>(WID_BDD_Y)->pos_y + y1, AXIS_Y, DEPOT_PART_NORTH);
		DrawShipDepotSprite(this->GetWidget<NWidgetBase>(WID_BDD_Y)->pos_x + x1, this->GetWidget<NWidgetBase>(WID_BDD_Y)->pos_y + y2, AXIS_Y, DEPOT_PART_SOUTH);
	}

	void OnClick(Point pt, int widget, int click_count) override
	{
		switch (widget) {
			case WID_BDD_X:
			case WID_BDD_Y:
				this->RaiseWidget(_ship_depot_direction + WID_BDD_X);
				_ship_depot_direction = (widget == WID_BDD_X ? AXIS_X : AXIS_Y);
				this->LowerWidget(_ship_depot_direction + WID_BDD_X);
				if (_settings_client.sound.click_beep) SndPlayFx(SND_15_BEEP);
				UpdateDocksDirection();
				this->SetDirty();
				break;
		}
	}
};

static const NWidgetPart _nested_build_docks_depot_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_DEPOT_BUILD_SHIP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BDD_BACKGROUND),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL_LTR),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_BDD_X), SetMinimalSize(98, 66), SetDataTip(0x0, STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(2, 0),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_BDD_Y), SetMinimalSize(98, 66), SetDataTip(0x0, STR_DEPOT_BUILD_SHIP_ORIENTATION_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(3, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
	EndContainer(),
};

static WindowDesc _build_docks_depot_desc(
	WDP_AUTO, nullptr, 0, 0,
	WC_BUILD_DEPOT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_docks_depot_widgets, lengthof(_nested_build_docks_depot_widgets)
);


static void ShowBuildDocksDepotPicker(Window *parent)
{
	new BuildDocksDepotWindow(&_build_docks_depot_desc, parent);
}


void InitializeDockGui()
{
	_ship_depot_direction = AXIS_X;
}
