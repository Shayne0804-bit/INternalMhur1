#include "imgui.h"

namespace c
{
	inline ImVec4 accent = ImColor(250, 73, 73); // HEX - FA4949
	inline ImVec4 accent_depressed = ImColor(30, 40, 45, 255);

	inline ImVec4 shadow = ImColor(5, 5, 5);
	inline ImVec4 shadow_invisible = ImColor(5, 5, 5, 0);


	inline float all_rounding = 5.f;

	namespace bg
	{
		inline ImVec4 background_gradient0 = ImColor(15, 15, 18);
		inline ImVec4 background_gradient1 = ImColor(27, 28, 33);
		inline ImVec2 size = ImVec2(700, 500);
	}

	namespace child
	{
		inline ImVec4 background_gradient0 = ImColor(11, 13, 14);
		inline ImVec4 background_gradient1 = ImColor(28, 28, 35);
		inline ImVec4 line_gradient0 = ImColor(46, 46, 63, 180);
		inline ImVec4 line_gradient1 = ImColor(46, 46, 63, 0);
		inline ImVec4 view_icon = ImColor(46, 46, 63, 255);

	}

	namespace tab
	{
		inline ImVec4 background_inactive = ImColor(74, 71, 91, 255);
		inline ImVec4 background_gradient0 = ImColor(28, 28, 35);
		inline ImVec4 background_gradient1 = ImColor(28, 28, 35, 0);
	}

	namespace checkbox
	{
		inline ImVec4 background_inactive = ImColor(32, 28, 44, 255);
		inline ImVec4 mark_active = ImColor(0, 0, 0, 255);
		inline ImVec4 mark_inactive = ImColor(0, 0, 0, 0);
	}
	
	namespace scroll
	{
		inline ImVec4 background = ImColor(108, 90, 160, 80);
	}

	namespace slider
	{
		inline ImVec4 background_inactive = ImColor(32, 28, 44, 255);
		inline ImVec4 navigator_inactive = ImColor(32, 28, 44, 0);
		inline ImVec4 navigator_text = ImColor(0, 0, 0, 255);
	}

	namespace combo
	{
		inline ImVec4 background_inactive = ImColor(36, 34, 43, 255);
		inline ImVec4 icon_inactive = ImColor(46, 46, 63, 255);
		inline ImVec4 list_inactive = ImColor(32, 31, 37, 255);
	}

	namespace picker
	{
		inline ImVec4 background_inactive = ImColor(15, 15, 18);
	}

	namespace input
	{
		inline ImVec4 background_inactive = ImColor(36, 34, 43, 255);
		inline ImVec4 icon_inactive = ImColor(46, 46, 63, 255);
	}

	namespace keybind
	{
		inline ImVec4 background_inactive = ImColor(36, 34, 43, 255);
	}

	namespace text
	{
		inline ImVec4 text_active = ImColor(255, 255, 255, 255);
		inline ImVec4 text_hov = ImColor(74, 71, 91, 255);
		inline ImVec4 text = ImColor(74, 71, 91, 255);
		inline ImVec4 text_hide = ImColor(52, 48, 64, 0);
	}

}