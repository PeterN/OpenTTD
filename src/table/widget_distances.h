/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file window_gui.h Functions, definitions and such used only by the GUI. */

#ifndef WIDGET_DISTANCES_H
#define WIDGET_DISTANCES_H

/** Distances used in drawing widgets. */
const byte _widget_distances[WDD_END] = {

	/* WWT_IMGBTN(_2) */
	/* WD_IMGBTN_LEFT    = */ 1,      ///< Left offset of the image in the button.
	/* WD_IMGBTN_RIGHT   = */ 2,      ///< Right offset of the image in the button.
	/* WD_IMGBTN_TOP     = */ 1,      ///< Top offset of image in the button.
	/* WD_IMGBTN_BOTTOM  = */ 2,      ///< Bottom offset of image in the button.

	/* WWT_INSET */
	/* WD_INSET_LEFT  = */ 2,         ///< Left offset of string.
	/* WD_INSET_RIGHT = */ 2,         ///< Right offset of string.
	/* WD_INSET_TOP   = */ 1,         ///< Top offset of string.

	/* WD_SCROLLBAR_LEFT   = */ 2,    ///< Left offset of scrollbar.
	/* WD_SCROLLBAR_RIGHT  = */ 2,    ///< Right offset of scrollbar.
	/* WD_SCROLLBAR_TOP    = */ 2,    ///< Top offset of scrollbar.
	/* WD_SCROLLBAR_BOTTOM = */ 2,    ///< Bottom offset of scrollbar.

	/* Size of the pure frame bevel without any padding. */
	/* WD_BEVEL_LEFT       = */ 1,    ///< Width of left bevel border.
	/* WD_BEVEL_RIGHT      = */ 1,    ///< Width of right bevel border.
	/* WD_BEVEL_TOP        = */ 1,    ///< Height of top bevel border.
	/* WD_BEVEL_BOTTOM     = */ 1,    ///< Height of bottom bevel border.

	/* FrameRect widgets, all text buttons, panel, editbox */
	/* WD_FRAMERECT_LEFT   = */ 2,    ///< Offset at left to draw the frame rectangular area
	/* WD_FRAMERECT_RIGHT  = */ 2,    ///< Offset at right to draw the frame rectangular area
	/* WD_FRAMERECT_TOP    = */ 1,    ///< Offset at top to draw the frame rectangular area
	/* WD_FRAMERECT_BOTTOM = */ 1,    ///< Offset at bottom to draw the frame rectangular area

	/* Extra space at top/bottom of text panels */
	/* WD_TEXTPANEL_TOP    = */ 6,    ///< Offset at top to draw above the text
	/* WD_TEXTPANEL_BOTTOM = */ 6,    ///< Offset at bottom to draw below the text

	/* WWT_FRAME */
	/* WD_FRAMETEXT_LEFT   = */ 6,    ///< Left offset of the text of the frame.
	/* WD_FRAMETEXT_RIGHT  = */ 6,    ///< Right offset of the text of the frame.
	/* WD_FRAMETEXT_TOP    = */ 6,    ///< Top offset of the text of the frame
	/* WD_FRAMETEXT_BOTTOM = */ 6,    ///< Bottom offset of the text of the frame

	/* WWT_MATRIX */
	/* WD_MATRIX_LEFT   = */ 2,       ///< Offset at left of a matrix cell.
	/* WD_MATRIX_RIGHT  = */ 2,       ///< Offset at right of a matrix cell.
	/* WD_MATRIX_TOP    = */ 3,       ///< Offset at top of a matrix cell.
	/* WD_MATRIX_BOTTOM = */ 1,       ///< Offset at bottom of a matrix cell.

	/* WWT_SHADEBOX */
	/* WD_SHADEBOX_WIDTH  = */ 12,    ///< Width of a standard shade box widget.
	/* WD_SHADEBOX_LEFT   = */ 2,     ///< Left offset of shade sprite.
	/* WD_SHADEBOX_RIGHT  = */ 2,     ///< Right offset of shade sprite.
	/* WD_SHADEBOX_TOP    = */ 3,     ///< Top offset of shade sprite.
	/* WD_SHADEBOX_BOTTOM = */ 3,     ///< Bottom offset of shade sprite.

	/* WWT_STICKYBOX */
	/* WD_STICKYBOX_WIDTH  = */ 12,   ///< Width of a standard sticky box widget.
	/* WD_STICKYBOX_LEFT   = */ 2,    ///< Left offset of sticky sprite.
	/* WD_STICKYBOX_RIGHT  = */ 2,    ///< Right offset of sticky sprite.
	/* WD_STICKYBOX_TOP    = */ 3,    ///< Top offset of sticky sprite.
	/* WD_STICKYBOX_BOTTOM = */ 3,    ///< Bottom offset of sticky sprite.

	/* WWT_DEBUGBOX */
	/* WD_DEBUGBOX_WIDTH  = */ 12,    ///< Width of a standard debug box widget.
	/* WD_DEBUGBOX_LEFT   = */ 2,     ///< Left offset of debug sprite.
	/* WD_DEBUGBOX_RIGHT  = */ 2,     ///< Right offset of debug sprite.
	/* WD_DEBUGBOX_TOP    = */ 3,     ///< Top offset of debug sprite.
	/* WD_DEBUGBOX_BOTTOM = */ 3,     ///< Bottom offset of debug sprite.

	/* WWT_DEFSIZEBOX */
	/* WD_DEFSIZEBOX_WIDTH  = */ 12,  ///< Width of a standard defsize box widget.
	/* WD_DEFSIZEBOX_LEFT   = */ 2,   ///< Left offset of defsize sprite.
	/* WD_DEFSIZEBOX_RIGHT  = */ 2,   ///< Right offset of defsize sprite.
	/* WD_DEFSIZEBOX_TOP    = */ 3,   ///< Top offset of defsize sprite.
	/* WD_DEFSIZEBOX_BOTTOM = */ 3,   ///< Bottom offset of defsize sprite.

	/* WWT_RESIZEBOX */
	/* WD_RESIZEBOX_WIDTH  = */ 12,   ///< Width of a resize box widget.
	/* WD_RESIZEBOX_LEFT   = */ 3,    ///< Left offset of resize sprite.
	/* WD_RESIZEBOX_RIGHT  = */ 2,    ///< Right offset of resize sprite.
	/* WD_RESIZEBOX_TOP    = */ 3,    ///< Top offset of resize sprite.
	/* WD_RESIZEBOX_BOTTOM = */ 2,    ///< Bottom offset of resize sprite.

	/* WWT_CLOSEBOX */
	/* WD_CLOSEBOX_WIDTH  = */ 11,    ///< Width of a close box widget.
	/* WD_CLOSEBOX_LEFT   = */ 2,     ///< Left offset of closebox string.
	/* WD_CLOSEBOX_RIGHT  = */ 1,     ///< Right offset of closebox string.
	/* WD_CLOSEBOX_TOP    = */ 2,     ///< Top offset of closebox string.
	/* WD_CLOSEBOX_BOTTOM = */ 2,     ///< Bottom offset of closebox string.

	/* WWT_CAPTION */
	/* WD_CAPTION_HEIGHT     = */ 14, ///< Height of a title bar.
	/* WD_CAPTIONTEXT_LEFT   = */ 2,  ///< Offset of the caption text at the left.
	/* WD_CAPTIONTEXT_RIGHT  = */ 2,  ///< Offset of the caption text at the right.
	/* WD_CAPTIONTEXT_TOP    = */ 2,  ///< Offset of the caption text at the top.
	/* WD_CAPTIONTEXT_BOTTOM = */ 2,  ///< Offset of the caption text at the bottom.

	/* Dropdown widget. */
	/* WD_DROPDOWN_HEIGHT     = */ 12, ///< Height of a drop down widget.
	/* WD_DROPDOWNTEXT_LEFT   = */ 2,  ///< Left offset of the dropdown widget string.
	/* WD_DROPDOWNTEXT_RIGHT  = */ 2,  ///< Right offset of the dropdown widget string.
	/* WD_DROPDOWNTEXT_TOP    = */ 1,  ///< Top offset of the dropdown widget string.
	/* WD_DROPDOWNTEXT_BOTTOM = */ 1,  ///< Bottom offset of the dropdown widget string.

	/* WD_PAR_VSEP_NORMAL = */ 2,      ///< Normal amount of vertical space between two paragraphs of text.
	/* WD_PAR_VSEP_WIDE   = */ 8,      ///< Large amount of vertical space between two paragraphs of text.
};

#endif /* WIDGET_DISTANCES_H */
