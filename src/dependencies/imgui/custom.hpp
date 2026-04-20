#pragma once

#define  IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <unordered_map>

using namespace std;

#define to_vec4( r, g, b, a ) ImColor( r / 255.f, g / 255.f, b / 255.f, a )

namespace ImSpinner {
    using float_ptr = float*;
    constexpr float PI_DIV_4 = IM_PI / 4.f;
    constexpr float PI_DIV_2 = IM_PI / 2.f;
    constexpr float PI_2 = IM_PI * 2.f;
    inline ImColor color_alpha(ImColor c, float alpha) { c.Value.w *= alpha * ImGui::GetStyle().Alpha; return c; }

    namespace detail {
        // SpinnerBegin is a function that starts a spinner widget, used to display an animation indicating that
        // a task is in progress. It returns true if the widget is visible and can be used, or false if it should be skipped.
        inline bool SpinnerBegin(const char* label, float radius, ImVec2& pos, ImVec2& size, ImVec2& centre, int& num_segments) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);

            pos = window->DC.CursorPos;
            // The size of the spinner is set to twice the radius, plus some padding based on the style
            size = ImVec2((radius) * 2, (radius + style.FramePadding.y) * 2);

            const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
            ImGui::ItemSize(bb, style.FramePadding.y);

            num_segments = window->DrawList->_CalcCircleAutoSegmentCount(radius);

            centre = bb.GetCenter();
            // If the item cannot be added to the window, return false
            if (!ImGui::ItemAdd(bb, id))
                return false;

            return true;
        }
    }
}

#define SPINNER_HEADER(pos, size, centre, num_segments) ImVec2 pos, size, centre; int num_segments; if (!ImSpinner::detail::SpinnerBegin(label, radius, pos, size, centre, num_segments)) { return; }; ImGuiWindow *window = ImGui::GetCurrentWindow(); \
    auto circle = [&] (auto point_func, auto dbc, auto dth) { window->DrawList->PathClear(); for (int i = 0; i < num_segments; i++) { ImVec2 p = point_func(i); window->DrawList->PathLineTo(ImVec2(centre.x + p.x, centre.y + p.y)); } window->DrawList->PathStroke(dbc, false, dth); }

class c_custom {
public:

    float color_buf[4] = { 0.f, 21.f, 255.f, 1.f };

	ImVec2 window_size = ImVec2( 1050, 500 );

    float m_anim = 0.f;
	int m_tab = -1;
    vector < const char* > m_tab_list = { "Combat", "Visuals", "Self", "Vehicle", "Online", "Misc"/*,*//* "Lua Executer", "Resources"*/ };

    int m_combat_subtab = 0;
    vector < const char* > m_combat_subtab_list = { "Aim", "Weapon" };
    int m_visual_subtab = 0;
    vector < const char* > m_visual_subtab_list = { "Visuals", "Vehicle Visuals" };
        int m_self_subtab = 0;
    vector < const char* > m_self_subtab_list = { "Player", "Teleport"};
            int m_vehicle_subtab = 0;
    vector < const char* > m_vehicle_subtab_list = { "Vehicle", "Vehicle Spawner"};
        int m_player_subtab = 0;
    vector < const char* > m_player_subtab_list = { "Online"};
    //    int m_config_subtab = 0;
    //vector < const char* > m_player_subtab_list = { "Config" };
        int m_misc_subtab = 0;
    vector < const char* > m_misc_subtab_list = { "Misc"};
    //int m_lua_subtab = 0;
    //vector < const char* > m_lua_subtab_list = { "Executer", "" };

    void render_rect_for_horizontal_bar( ImVec2 pos, ImColor color, float alpha );

    void spinner( const char *label, float radius1, float radius2, float thickness, float b_thickness, const ImColor &ball = ImColor( 1.f, 1.f, 1.f, 1.f ), const ImColor& bg = ImColor( 1.f, 1.f, 1.f, 0.5f ), float speed = 2.8f, size_t balls = 2 );

    bool subtab( const char* name, bool current, int size );
    bool tab( const char* name, bool current );

    void group_box( const char* name, ImVec2 size_arg );
    void end_group_box( );

    void custom_child(ImFont* font, const char* name, const char* title, const ImVec2 pos, const ImVec2 size );
    void end_custom_child( );

    float accent_c[4] = { 186 / 8.f, 54 / 150.f, 193 / 255.f, 1.f };
	ImColor get_accent_color( float a = 1.f ) {
		return ImColor( accent_c[0], accent_c[1], accent_c[2], a );
	}
    ImColor get_accent_colorparticles( float a = 1.f ) {
		return ImColor( accent_c[0], accent_c[1], accent_c[2], 0.10f );
	}
};

inline c_custom custom;