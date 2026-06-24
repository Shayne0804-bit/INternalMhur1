#pragma once

#include <imgui.h>

namespace ImGuiHelper
{
    /**
     * @brief Custom toggle switch
     * Adapted for RUGIR-INTERNAL color scheme (Magenta/Purple accent)
     * 
     * @param label Display label
     * @param v Reference to boolean value
     * @param colorOn Color when toggle is ON (accent magenta)
     * @param colorOff Color when toggle is OFF (background medium)
     * @return true if value changed
     */
    inline bool ToggleSwitch(const char* label, bool* v)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec4 colorOn = ImVec4(0.62f, 0.30f, 1.00f, 1.0f);
        ImVec4 colorOff = ImVec4(0.06f, 0.05f, 0.09f, 1.0f);
        ImVec4 glowColor = ImVec4(0.74f, 0.42f, 1.00f, 0.45f);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImVec2(40.0f, 20.0f);
        float radius = size.y * 0.5f;

        ImGui::InvisibleButton(label, size);
        bool toggled = false;
        if (ImGui::IsItemClicked())
        {
            *v = !(*v);
            toggled = true;
        }

        float targetPos = *v ? 1.0f : 0.0f;

        ImVec2 p_min = pos;
        ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);
        ImU32 bgColor = ImGui::GetColorU32(*v ? colorOn : colorOff);
        drawList->AddRectFilled(p_min, p_max, bgColor, radius);

        // Glow effect when ON
        if (*v)
        {
            ImU32 glowColorU32 = ImGui::GetColorU32(glowColor);
            drawList->AddRect(p_min, p_max, glowColorU32, radius, 0, 2.0f);
        }
        else
        {
            // Border when OFF
            ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.20f, 0.15f, 0.32f, 1.0f));
            drawList->AddRect(p_min, p_max, borderColor, radius, 0, 1.0f);
        }

        // Knob - WHITE
        float knobX = pos.x + radius + (targetPos * (size.x - 2 * radius));
        ImVec2 knobCenter = ImVec2(knobX, pos.y + radius);
        ImU32 knobColor = ImGui::GetColorU32(ImVec4(0.94f, 0.98f, 1.0f, 1.0f));
        drawList->AddCircleFilled(knobCenter, radius * 0.65f, knobColor, 12);

        if (label && !(label[0] == '#' && label[1] == '#'))
        {
            ImGui::SameLine();
            ImGui::Text("%s", label);
        }

        return toggled;
    }

    /**
     * @brief Alternative: Larger, more visible toggle switch
     */
    inline bool ToggleSwitchLarge(const char* label, bool* v)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec4 colorOn = ImVec4(0.62f, 0.30f, 1.00f, 1.0f);
        ImVec4 colorOff = ImVec4(0.06f, 0.05f, 0.09f, 1.0f);
        ImVec4 glowColor = ImVec4(0.74f, 0.42f, 1.00f, 0.45f);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImVec2(56.0f, 28.0f);
        float radius = size.y * 0.5f;

        ImGui::InvisibleButton(label, size);
        bool toggled = false;
        if (ImGui::IsItemClicked())
        {
            *v = !(*v);
            toggled = true;
        }

        float targetPos = *v ? 1.0f : 0.0f;

        ImVec2 p_min = pos;
        ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);
        ImU32 bgColor = ImGui::GetColorU32(*v ? colorOn : colorOff);
        drawList->AddRectFilled(p_min, p_max, bgColor, radius);

        // Glow effect when ON
        if (*v)
        {
            ImU32 glowColorU32 = ImGui::GetColorU32(glowColor);
            drawList->AddRect(p_min, p_max, glowColorU32, radius, 0, 2.5f);
        }
        else
        {
            // Border when OFF
            ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.20f, 0.15f, 0.32f, 1.0f));
            drawList->AddRect(p_min, p_max, borderColor, radius, 0, 1.5f);
        }

        // Knob - WHITE
        float knobX = pos.x + radius + (targetPos * (size.x - 2 * radius));
        ImVec2 knobCenter = ImVec2(knobX, pos.y + radius);
        ImU32 knobColor = ImGui::GetColorU32(ImVec4(0.94f, 0.98f, 1.0f, 1.0f));
        drawList->AddCircleFilled(knobCenter, radius * 0.7f, knobColor, 14);

        if (label && !(label[0] == '#' && label[1] == '#'))
        {
            ImGui::SameLine();
            ImGui::Text("%s", label);
        }

        return toggled;
    }
}

