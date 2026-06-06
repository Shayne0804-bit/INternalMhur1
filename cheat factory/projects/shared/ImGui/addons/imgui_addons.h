//============ Copyright KiwiHax, All rights reserved ============//
//
//  Purpose: 
//
//================================================================//

#pragma once

#include <vector>

#include "../imgui.h"

#define IMADD_ANIMATIONS_SPEED			0.07f
#define IMADD_ITEM_WIDTH_MULTIPILIER	0.65f

struct ImGuiWindow;

namespace ImAdd
{
	// Helpers
	ImVec4  HexToColorVec4(unsigned int hex_color, float alpha = 1.0f);

	// Separators
	void    SeparatorText(const char* label, float thickness = 1.0f);
	void    VSeparator(float margin = 0.0f, float thickness = 1.0f);

	// Tabs
	bool	TabIcon(ImTextureRef icon_texture, const char* label, int* v, int tab_id, bool expandable = false, const ImVec2& size_arg = ImVec2(0, 0));

	// Buttons
	bool    ButtonXMark(const char* str_id, const ImVec2& size_arg = ImVec2(0, 0));
	bool	Button(ImTextureRef icon_texture, const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags button_flags = 0, ImDrawFlags draw_flags = 0);
	bool	Button(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiButtonFlags button_flags = 0, ImDrawFlags draw_flags = 0);

	// Groups
	void	ScrollBar(const char* str_id, ImGuiWindow* window, const ImVec2& size_arg = ImVec2(0, 0));
	bool	BeginChild(const char* str_id, const ImVec2& size_arg = ImVec2(0, 0), bool child_style = true);
	void	EndChild();

	// Checkables
	bool	ToggleButton(const char* label, bool* v);
	bool	CheckBox(const char* label, bool* checked);

	// Sliders
	bool	SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format = NULL);
	bool	SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f");
	bool	SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d");

	// Colors
	bool	SliderAlpha(const char* str_id, ImVec4& col);
	bool	SliderHue(const char* str_id, ImVec4& col);
	bool	ColorPalette(const char* str_id, ImVec4& col, const ImVec2& size_arg = ImVec2(0, 0));
	bool	ColorButton(const char* desc_id, const ImVec4& col, bool has_alpha, float square_sz);
	bool	ColorPicker4(const char* label, float col[4], bool has_alpha);
	bool    ColorEdit4(const char* name, float col[4]);

	// Others
	bool	SelectableLabel(const char* label, bool selected, bool centered = false, const ImVec2& size_arg = ImVec2(0, 0));
	bool	Combo(const char* label, int* selected_index, std::vector<const char*> items);
	bool	KeyBind(const char* label, ImGuiKey* k);

	// Render
	void	RenderArrow(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz, ImGuiDir direction);
	void	RenderCheckMark(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz);
}
