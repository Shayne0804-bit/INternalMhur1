#pragma once

#include <imgui.h>
#include <map>
#include <string>

namespace ImGuiHelper
{
    static std::map<std::string, float> g_ToggleAnimations;
    static constexpr float ANIMATION_SPEED = 0.15f;

    /**
     * @brief Custom toggle switch with animation
     * Adapted for RUGIR-INTERNAL color scheme (Magenta/Purple accent)
     * 
     * @param label Display label
     * @param v Reference to boolean value
     * @param colorOn Color when toggle is ON (accent magenta)
     * @param colorOff Color when toggle is OFF (background medium)
     * @return true if value changed
     */
    inline bool ToggleSwitch(const char* label, bool* v, ImVec4 colorOn = ImVec4(0.65f, 0.35f, 0.95f, 1.0f), ImVec4 colorOff = ImVec4(0.12f, 0.10f, 0.18f, 1.0f))
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Get current cursor position
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImVec2(32.0f, 16.0f);  // Toggle switch size (reduced)
        float radius = size.y * 0.5f;

        // Create unique animation key for this toggle
        std::string animKey = std::string(label) + "##toggle";
        if (g_ToggleAnimations.find(animKey) == g_ToggleAnimations.end())
        {
            g_ToggleAnimations[animKey] = *v ? 1.0f : 0.0f;
        }

        // Interactive area
        ImGui::InvisibleButton(label, size);
        bool toggled = false;
        if (ImGui::IsItemClicked())
        {
            *v = !(*v);
            toggled = true;
        }

        // Smooth animation
        float targetPos = *v ? 1.0f : 0.0f;
        g_ToggleAnimations[animKey] += (targetPos - g_ToggleAnimations[animKey]) * ANIMATION_SPEED;

        // Draw background rectangle
        ImVec2 p_min = pos;
        ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);
        ImU32 bgColor = ImGui::GetColorU32(*v ? colorOn : colorOff);
        drawList->AddRectFilled(p_min, p_max, bgColor, radius);

        // Draw border
        ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.5f, 0.3f, 0.7f, 0.8f));
        drawList->AddRect(p_min, p_max, borderColor, radius, 0, 1.5f);

        // Draw knob (circle)
        float knobX = pos.x + radius + (g_ToggleAnimations[animKey] * (size.x - 2 * radius));
        ImVec2 knobCenter = ImVec2(knobX, pos.y + radius);
        ImU32 knobColor = ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        drawList->AddCircleFilled(knobCenter, radius * 0.7f, knobColor, 16);

        // Display label after toggle
        ImGui::SameLine();
        ImGui::Text(label);

        return toggled;
    }

    /**
     * @brief Alternative: Larger, more visible toggle switch
     */
    inline bool ToggleSwitchLarge(const char* label, bool* v, ImVec4 colorOn = ImVec4(0.65f, 0.35f, 0.95f, 1.0f), ImVec4 colorOff = ImVec4(0.12f, 0.10f, 0.18f, 1.0f))
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Get current cursor position
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImVec2(48.0f, 22.0f);  // Larger toggle switch size (reduced)
        float radius = size.y * 0.5f;

        // Create unique animation key for this toggle
        std::string animKey = std::string(label) + "##toggle_large";
        if (g_ToggleAnimations.find(animKey) == g_ToggleAnimations.end())
        {
            g_ToggleAnimations[animKey] = *v ? 1.0f : 0.0f;
        }

        // Interactive area
        ImGui::InvisibleButton(label, size);
        bool toggled = false;
        if (ImGui::IsItemClicked())
        {
            *v = !(*v);
            toggled = true;
        }

        // Smooth animation
        float targetPos = *v ? 1.0f : 0.0f;
        g_ToggleAnimations[animKey] += (targetPos - g_ToggleAnimations[animKey]) * ANIMATION_SPEED;

        // Draw background rectangle
        ImVec2 p_min = pos;
        ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);
        ImU32 bgColor = ImGui::GetColorU32(*v ? colorOn : colorOff);
        drawList->AddRectFilled(p_min, p_max, bgColor, radius);

        // Draw border
        ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.5f, 0.3f, 0.7f, 0.8f));
        drawList->AddRect(p_min, p_max, borderColor, radius, 0, 2.0f);

        // Draw knob (circle)
        float knobX = pos.x + radius + (g_ToggleAnimations[animKey] * (size.x - 2 * radius));
        ImVec2 knobCenter = ImVec2(knobX, pos.y + radius);
        ImU32 knobColor = ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        drawList->AddCircleFilled(knobCenter, radius * 0.8f, knobColor, 16);

        // Display label after toggle
        ImGui::SameLine();
        ImGui::Text(label);

        return toggled;
    }
}

