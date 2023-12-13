/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_fonts_gui.cpp GUI for font settings. */

#include "gfx_func.h"
#include "stdafx.h"
#include "settings_gui.h"
#include "textbuf_gui.h"
#include "strings_func.h"
#include "string_func.h"
#include "core/geometry_func.hpp"
#include "stringfilter_type.h"
#include "querystring_gui.h"
#include "fontcache.h"
#include "zoom_func.h"
#include "widgets/settings_fonts_widget.h"
#include "fontdetection.h"

#include "safeguards.h"


class FontFamilyWindow : public Window {
	FontSize fs;
	int parent_button;

	std::vector<FontFamily> fonts;
	std::vector<std::string> families;
	std::vector<std::string> styles;
	std::vector<uint> sizes;

	std::string selected_family;
	std::string selected_style;
	uint selected_size;
	bool selected_aa;

	std::string sprite_font;

	static inline const uint MIN_FONT_SIZE = 4;
	static inline const uint MAX_FONT_SIZE = 24;
	static inline const uint FILTER_LENGTH = 40;
	static inline const uint FONT_ROWS = 8;

	StringFilter family_filter;
	static inline QueryString family_editbox{FILTER_LENGTH * MAX_CHAR_LENGTH, FILTER_LENGTH};
	static inline QueryString style_editbox{FILTER_LENGTH * MAX_CHAR_LENGTH, FILTER_LENGTH};
	static inline QueryString size_editbox{FILTER_LENGTH * MAX_CHAR_LENGTH, FILTER_LENGTH};

	static int ScaleFontSize(int size)
	{
		return size;
	}

	void FillFonts()
	{
		extern std::vector<FontFamily> ListFonts();
		this->fonts = ListFonts();

		// Presort the font list by family and weight.
		std::sort(std::begin(this->fonts), std::end(this->fonts), [](const FontFamily &first, const FontFamily &second) {
			int r = StrNaturalCompare(first.family, second.family);
			if (r == 0) r = (first.weight - second.weight);
			if (r == 0) r = (first.slant - second.slant);
			if (r == 0) r = StrNaturalCompare(first.style, second.style);
			return r < 0;
		});
	}

	bool FilterByText(const std::string &str)
	{
		if (this->family_filter.IsEmpty()) return true;
		this->family_filter.ResetState();
		this->family_filter.AddLine(str);
		return this->family_filter.GetState();
	}

	void FillFamilies()
	{
		int position = -1;
		this->families.clear();
		if (FilterByText(this->sprite_font)) this->families.emplace_back(this->sprite_font);
		for (auto it = std::begin(this->fonts); it != std::end(this->fonts); ++it) {
			if (!FilterByText(it->family)) continue;
			if (std::find(std::begin(this->families), std::end(this->families), it->family) != std::end(this->families)) continue;
			if (this->selected_family == it->family) position = (int)this->families.size();
			this->families.emplace_back(it->family);
		}

		if (position == -1 && !this->families.empty()) this->selected_family = this->families.front();

		Scrollbar *scroll = this->GetScrollbar(WID_FFW_FAMILIES_SCROLL);
		scroll->SetCount(this->families.size());
		scroll->ScrollTowards(position);

		this->SetWidgetDirty(WID_FFW_FAMILIES);
		this->SetWidgetDirty(WID_FFW_FAMILIES_SCROLL);

		FillStyles(this->selected_family);
	}

	void FillStyles(const std::string &family)
	{
		int position = -1;
		this->styles.clear();
		if (family == this->sprite_font) {
			this->styles.emplace_back(FontSizeToName(FS_NORMAL));
			this->styles.emplace_back(FontSizeToName(FS_SMALL));
			this->styles.emplace_back(FontSizeToName(FS_LARGE));
			this->styles.emplace_back(FontSizeToName(FS_MONO));
		} else if (!family.empty()) {
			for (auto it = std::begin(this->fonts); it != std::end(this->fonts); ++it) {
				if (it->family != family) continue;
				if (std::find(std::begin(this->styles), std::end(this->styles), it->style) != std::end(this->styles)) continue;
				if (this->selected_style == it->style) position = (int)this->styles.size();
				this->styles.emplace_back(it->style);
			}
		}

		if (position == -1 && !this->styles.empty()) this->selected_style = this->styles.front();

		Scrollbar *scroll = this->GetScrollbar(WID_FFW_STYLES_SCROLL);
		scroll->SetCount(this->styles.size());
		scroll->ScrollTowards(position);

		this->SetWidgetDirty(WID_FFW_STYLES);
		this->SetWidgetDirty(WID_FFW_STYLES_SCROLL);

		this->FillSizes();
	}

	void FillSizes()
	{
		this->sizes.clear();

		if (this->selected_family == this->sprite_font) {
			this->sizes.push_back(FontCache::GetDefaultFontHeight(this->fs));
		} else {
			for (uint i = this->MIN_FONT_SIZE; i <= this->MAX_FONT_SIZE; i++) {
				this->sizes.push_back(i);
			}
		}

		int position = std::clamp(this->selected_size - this->MIN_FONT_SIZE, 0U, (uint)this->sizes.size());

		Scrollbar *scroll = this->GetScrollbar(WID_FFW_SIZES_SCROLL);
		scroll->SetCount(this->sizes.size());
		scroll->ScrollTowards(position);

		this->SetWidgetDirty(WID_FFW_SIZES);
		this->SetWidgetDirty(WID_FFW_SIZES_SCROLL);
	}

	void UpdateSelections()
	{
		this->sprite_font = GetString(STR_GAME_OPTIONS_FONT_SPRITE);

		FontCacheSubSetting *setting = GetFontCacheSubSetting(this->fs);
		auto [font_family, font_style] = SplitFontFamilyAndStyle(FontCache::Get(this->fs)->GetFontName());
		this->selected_family = font_family;
		this->selected_style = font_style;
		this->selected_size = setting->size;
		this->selected_aa = setting->aa;
		this->SetWidgetLoweredState(WID_FFW_ANTIALIAS_BUTTON, this->selected_aa);

		this->style_editbox.text.Assign(this->selected_style);
		SetDParam(0, this->ScaleFontSize(this->selected_size));
		this->size_editbox.text.Assign(GetString(STR_JUST_COMMA));
	}

	void ChangeFont(FontSize fs)
	{
		if (this->selected_family == this->sprite_font) {
			SetFont(fs, "", this->selected_size, this->selected_aa);
		} else if (!this->selected_family.empty() && !this->selected_style.empty()) {
			std::string fontname = fmt::format("{}, {}", this->selected_family, this->selected_style);
			SetFont(fs, fontname, this->selected_size, this->selected_aa);
		}

		this->style_editbox.text.Assign(this->selected_style);
		SetDParam(0, this->ScaleFontSize(this->selected_size));
		this->size_editbox.text.Assign(GetString(STR_JUST_COMMA));
	}

public:
	FontFamilyWindow(Window *parent, int button, FontSize fs, WindowDesc *desc) : Window(desc)
	{
		this->parent = parent;
		this->parent_button = button;
		this->fs = fs;

		this->CreateNestedTree();
		this->UpdateSelections();
		this->FillFonts();
		this->FinishInitNested(WN_GAME_OPTIONS_FONT);

		this->querystrings[WID_FFW_FAMILY_FILTER] = &this->family_editbox;
		this->querystrings[WID_FFW_STYLE_FILTER] = &this->style_editbox;
		this->querystrings[WID_FFW_SIZE_FILTER] = &this->size_editbox;
		this->family_editbox.cancel_button = QueryString::ACTION_CLEAR;
		this->style_editbox.cancel_button = QueryString::ACTION_CLEAR;
		this->size_editbox.cancel_button = QueryString::ACTION_CLEAR;
		this->family_filter.SetFilterTerm(this->family_editbox.text.buf);

		this->FillFamilies();

		this->ChangeFont(FS_PREVIEW);
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *, Dimension *resize) override
	{
		switch (widget) {
			case WID_FFW_FAMILIES:
				size->width = 0;
				for (const auto &ff : this->fonts) *size = maxdim(*size, GetStringBoundingBox(ff.family));
				size->width += WidgetDimensions::scaled.hsep_wide + padding.width; // hsep_wide to avoid cramped list
				resize->height = GetCharacterHeight(FS_NORMAL) + padding.height;
				size->height = this->FONT_ROWS * resize->height;
				break;

			case WID_FFW_STYLES:
				size->width = 0;
				for (const auto &ff : this->fonts) *size = maxdim(*size, GetStringBoundingBox(ff.style));
				size->width += WidgetDimensions::scaled.hsep_wide + padding.width; // hsep_wide to avoid cramped list
				resize->height = GetCharacterHeight(FS_NORMAL) + padding.height;
				size->height = this->FONT_ROWS * resize->height;
				break;

			case WID_FFW_SIZES:
				SetDParamMaxDigits(0, 3);
				*size = GetStringBoundingBox(STR_JUST_COMMA);
				size->width += WidgetDimensions::scaled.hsep_wide + padding.width; // hsep_wide to avoid cramped list
				resize->height = GetCharacterHeight(FS_NORMAL) + padding.height;
				size->height = this->FONT_ROWS * resize->height;
				break;
		}
	}

	void OnResize() override
	{
		this->GetScrollbar(WID_FFW_FAMILIES_SCROLL)->SetCapacityFromWidget(this, WID_FFW_FAMILIES);
		this->GetScrollbar(WID_FFW_STYLES_SCROLL)->SetCapacityFromWidget(this, WID_FFW_STYLES);
		this->GetScrollbar(WID_FFW_SIZES_SCROLL)->SetCapacityFromWidget(this, WID_FFW_SIZES);
	}

	void OnInvalidateData(int data = 0, bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (data == 1) {
			this->FillFonts();
			this->FillFamilies();
		}
		this->UpdateSelections();
	}

	void OnEditboxChanged(int widget) override
	{
		switch (widget) {
			case WID_FFW_FAMILY_FILTER:
				this->family_filter.SetFilterTerm(this->family_editbox.text.buf);
				this->FillFamilies();
				break;

			case WID_FFW_STYLE_FILTER:
				if (this->style_editbox.text.buf[0] == '\0') return;
				for (const auto &style : this->styles) {
					if (StrStartsWithIgnoreCase(style, this->style_editbox.text.buf)) {
						// Change selected style if existing selection doesn't start with the text.
						if (!StrStartsWithIgnoreCase(this->selected_style, this->style_editbox.text.buf)) {
							this->selected_style = style;
							this->SetWidgetDirty(WID_FFW_STYLES);
						}
						break;
					}
				}
				break;

			case WID_FFW_SIZE_FILTER:
				if (this->size_editbox.text.buf[0] == '\0') return;
				for (const auto &size : this->sizes) {
					SetDParam(0, this->ScaleFontSize(size));
					if (StrStartsWithIgnoreCase(GetString(STR_JUST_COMMA), this->size_editbox.text.buf)) {
						// Change selected size if existing selection doesn't start with the text.
						SetDParam(0, this->ScaleFontSize(this->selected_size));
						if (!StrStartsWithIgnoreCase(GetString(STR_JUST_COMMA), this->size_editbox.text.buf)) {
							this->selected_size = size;
							this->SetWidgetDirty(WID_FFW_SIZES);
						}
						break;
					}
				}
				break;
		}
	}

	void OnClick(Point pt, int widget, int) override
	{
		switch (widget) {
			case WID_FFW_FAMILIES: {
				const auto it = this->GetScrollbar(WID_FFW_FAMILIES_SCROLL)->GetScrolledItemFromWidget(this->families, pt.y, this, widget);
				if (it != std::end(this->families)) this->selected_family = *it;
				this->FillStyles(this->selected_family);
				this->ChangeFont(FS_PREVIEW);
				this->SetDirty();
				break;
			}

			case WID_FFW_STYLES: {
				const auto it = this->GetScrollbar(WID_FFW_STYLES_SCROLL)->GetScrolledItemFromWidget(this->styles, pt.y, this, widget);
				if (it != std::end(this->styles)) this->selected_style = *it;
				this->ChangeFont(FS_PREVIEW);
				this->SetDirty();
				break;
			}

			case WID_FFW_SIZES: {
				const auto it = this->GetScrollbar(WID_FFW_SIZES_SCROLL)->GetScrolledItemFromWidget(this->sizes, pt.y, this, widget);
				if (it != std::end(this->sizes)) this->selected_size = *it;
				this->ChangeFont(FS_PREVIEW);
				this->SetDirty();
				break;
			}

			case WID_FFW_ANTIALIAS_BUTTON: {
				this->selected_aa = !this->selected_aa;
				this->SetWidgetLoweredState(WID_FFW_ANTIALIAS_BUTTON, this->selected_aa);
				this->ChangeFont(FS_PREVIEW);
				this->SetDirty();
				break;
			}

			case WID_FFW_CANCEL:
				this->Close();
				break;

			case WID_FFW_OK:
				this->ChangeFont(this->fs);
				this->Close();
				break;
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		switch (widget) {
			case WID_FFW_FAMILIES: {
				const Scrollbar *scroll = this->GetScrollbar(WID_FFW_FAMILIES_SCROLL);
				Rect ir = r.Shrink(WidgetDimensions::scaled.matrix);
				auto begin = std::begin(this->families) + scroll->GetPosition();
				auto end = std::begin(this->families) + std::min<size_t>(scroll->GetPosition() + scroll->GetCapacity(), scroll->GetCount());
				for (auto it = begin; it != end; ++it) {
					DrawString(ir, *it, *it == this->selected_family ? TC_WHITE : TC_BLACK);
					ir.top += this->resize.step_height;
				}
				break;
			}

			case WID_FFW_STYLES: {
				const Scrollbar *scroll = this->GetScrollbar(WID_FFW_STYLES_SCROLL);
				Rect ir = r.Shrink(WidgetDimensions::scaled.matrix);
				auto begin = std::begin(this->styles) + scroll->GetPosition();
				auto end = std::begin(this->styles) + std::min<size_t>(scroll->GetPosition() + scroll->GetCapacity(), scroll->GetCount());
				for (auto it = begin; it != end; ++it) {
					DrawString(ir, *it, *it == this->selected_style ? TC_WHITE : TC_BLACK);
					ir.top += this->resize.step_height;
				}
				break;
			}

			case WID_FFW_SIZES: {
				const Scrollbar *scroll = this->GetScrollbar(WID_FFW_SIZES_SCROLL);
				Rect ir = r.Shrink(WidgetDimensions::scaled.matrix);
				auto begin = std::begin(this->sizes) + scroll->GetPosition();
				auto end = std::begin(this->sizes) + std::min<size_t>(scroll->GetPosition() + scroll->GetCapacity(), scroll->GetCount());
				for (auto it = begin; it != end; ++it) {
					SetDParam(0, this->ScaleFontSize(*it));
					DrawString(ir, STR_JUST_COMMA, *it == this->selected_size ? TC_WHITE : TC_BLACK, SA_RIGHT);
					ir.top += this->resize.step_height;
				}
				break;
			}

			case WID_FFW_PREVIEW:
				DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.fullbevel), STR_GAME_OPTIONS_FONT_PANGRAM, TC_BLACK, SA_CENTER, false, FS_PREVIEW);
				break;
		}
	}

	EventState OnKeyPress(char32_t, uint16_t keycode) override
	{
		if (keycode == WKC_RETURN) {
			this->ChangeFont(FS_PREVIEW);
			return ES_HANDLED;
		}

		return ES_NOT_HANDLED;
	}
};

static const NWidgetPart _nested_font_family_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_GAME_OPTIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.sparse), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					NWidget(WWT_EDITBOX, COLOUR_GREY, WID_FFW_FAMILY_FILTER), SetFill(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_MATRIX, COLOUR_GREY, WID_FFW_FAMILIES), SetFill(1, 0), SetScrollbar(WID_FFW_FAMILIES_SCROLL), SetMatrixDataTip(1, 0, STR_NULL),
						NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_FFW_FAMILIES_SCROLL),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					NWidget(WWT_EDITBOX, COLOUR_GREY, WID_FFW_STYLE_FILTER), SetFill(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_MATRIX, COLOUR_GREY, WID_FFW_STYLES), SetFill(1, 0), SetScrollbar(WID_FFW_STYLES_SCROLL), SetMatrixDataTip(1, 0, STR_NULL),
						NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_FFW_STYLES_SCROLL),
					EndContainer(),
				EndContainer(),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
					NWidget(WWT_EDITBOX, COLOUR_GREY, WID_FFW_SIZE_FILTER), SetFill(1, 0), SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_MATRIX, COLOUR_GREY, WID_FFW_SIZES), SetFill(1, 0), SetScrollbar(WID_FFW_SIZES_SCROLL), SetMatrixDataTip(1, 0, STR_NULL),
						NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_FFW_SIZES_SCROLL),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_TEXT, COLOUR_GREY), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_GAME_OPTIONS_FONT_ANTIALIAS, STR_NULL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_FFW_ANTIALIAS_BUTTON), SetMinimalSize(21, 9), SetDataTip(STR_EMPTY, STR_GAME_OPTIONS_FONT_ANTIALIAS_TOOLTIP),
			EndContainer(),
			NWidget(WWT_INSET, COLOUR_GREY, WID_FFW_PREVIEW), SetFill(1, 0), SetMinimalSize(100, 50),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_FFW_CANCEL), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_BUTTON_CANCEL, STR_NULL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_FFW_DEFAULT), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_BUTTON_DEFAULT, STR_NULL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_FFW_OK), SetFill(1, 0), SetResize(1, 0), SetDataTip(STR_BUTTON_OK, STR_NULL),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _font_family_desc(__FILE__, __LINE__,
	WDP_AUTO, nullptr, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	0,
	std::begin(_nested_font_family_widgets), std::end(_nested_font_family_widgets)
);

void ShowFontFamilyWindow(Window *parent, int button, FontSize fs)
{
	new FontFamilyWindow(parent, button, fs, &_font_family_desc);
}
