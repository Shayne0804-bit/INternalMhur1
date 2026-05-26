#pragma once

#include <imgui.h>
#include <string>
#include <cmath>
#include <algorithm>

// C++11 compatible clamp macro for compilers without C++17 std::clamp
#ifndef CLAMP
#define CLAMP(val, minVal, maxVal) ((val) < (minVal) ? (minVal) : ((val) > (maxVal) ? (maxVal) : (val)))
#endif

namespace ImGuiSliderHelper
{
    /**
     * @brief Custom slider for float values - Zero1 style
     * Features:
     * - Ctrl + Drag for fine adjustment (15% sensitivity)
     * - Shift + Click to type exact value
     * - Visual feedback with fill bar and circle handle
     * 
     * @param label Display label (use "##xxx" to hide)
     * @param value Reference to the value being edited
     * @param width Width of the slider in pixels
     * @param min Minimum value
     * @param max Maximum value
     * @return true if value changed this frame
     */
    inline bool SliderFloat(const char* label, float* value, float width, float min, float max)
    {
        float x = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::BeginGroup();
        
        if (strncmp(label, "##", 2) != 0)
        {
            ImGui::Text("%s", label);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
        }
        
        ImGui::PushID(label);
        ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
        
        // Calculate normalized value (0.0 to 1.0)
        float normalizedValue = (*value - min) / (max - min);
        normalizedValue = CLAMP(normalizedValue, 0.0f, 1.0f);
        
        float handleX = cursorScreenPos.x + normalizedValue * width;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw background bar (dark gray)
        drawList->AddRectFilled(
            ImVec2(cursorScreenPos.x, cursorScreenPos.y + 4.0f),
            ImVec2(cursorScreenPos.x + width, cursorScreenPos.y + 19.0f),
            ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)),
            4.0f
        );
        
        // Draw fill bar (accent color - mauve/magenta from menu)
        drawList->AddRectFilled(
            ImVec2(cursorScreenPos.x, cursorScreenPos.y + 4.0f),
            ImVec2(handleX, cursorScreenPos.y + 19.0f),
            ImGui::GetColorU32(ImVec4(0.65f, 0.35f, 0.95f, 1.0f)),
            4.0f
        );
        
        // Draw handle circle (light gray)
        drawList->AddCircleFilled(
            ImVec2(handleX, cursorScreenPos.y + 11.0f),
            10.0f,
            ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f))
        );
        
        // Invisible button for interaction
        ImGui::InvisibleButton("##slider", ImVec2(width, 30.0f));
        
        bool changed = false;
        ImGuiIO& io = ImGui::GetIO();
        bool keyCtrl = io.KeyCtrl;
        bool keyShift = io.KeyShift;
        
        // Shift + Click to open input dialog
        if (ImGui::IsItemHovered() && ImGui::IsItemClicked(ImGuiMouseButton_Left) && keyShift)
        {
            ImGui::SetNextWindowPos(ImVec2(handleX, cursorScreenPos.y - 4.0f), ImGuiCond_Always);
            ImGui::OpenPopup("##slider_edit");
        }
        
        bool popupOpen = ImGui::IsPopupOpen("##slider_edit");
        
        // Handle mouse dragging
        if (!popupOpen && ImGui::IsItemActive())
        {
            if (keyCtrl)
            {
                // Fine adjustment with Ctrl
                float delta = io.MouseDelta.x / width * (max - min) * 0.15f;
                float newValue = CLAMP(*value + delta, min, max);
                if (std::abs(newValue - *value) > FLT_EPSILON)
                {
                    *value = newValue;
                    changed = true;
                }
            }
            else
            {
                // Normal adjustment
                float mouseX = ImGui::GetMousePos().x - cursorScreenPos.x;
                float newValue = min + (mouseX / width) * (max - min);
                newValue = CLAMP(newValue, min, max);
                if (std::abs(newValue - *value) > FLT_EPSILON)
                {
                    *value = newValue;
                    changed = true;
                }
            }
        }
        
        // Show tooltip with usage info
        if (ImGui::IsItemHovered() && !popupOpen)
        {
            ImGui::BeginTooltip();
            ImGui::Text("Ctrl + Drag: fine adjust");
            ImGui::Text("Shift + Click: type a value");
            ImGui::EndTooltip();
        }
        
        // Display current value
        ImGui::SameLine();
        ImGui::Text("   %.3f", *value);
        
        // Input popup for typing exact value
        if (ImGui::BeginPopup("##slider_edit"))
        {
            ImGui::Text("Enter Value:");
            ImGui::SetNextItemWidth(100.0f);
            ImGui::SetKeyboardFocusHere();
            
            float tempValue = *value;
            if (ImGui::InputFloat("##input", &tempValue, 0.0f, 0.0f, "%.3f"))
            {
                *value = CLAMP(tempValue, min, max);
                changed = true;
            }
            
            if (ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                ImGui::CloseCurrentPopup();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
        ImGui::EndGroup();
        
        return changed;
    }

    /**
     * @brief Custom slider for int values - Zero1 style
     * Features:
     * - Ctrl + Drag for fine adjustment
     * - Shift + Click to type exact value
     * - Visual feedback with fill bar and circle handle
     * 
     * @param label Display label (use "##xxx" to hide)
     * @param value Reference to the value being edited
     * @param width Width of the slider in pixels
     * @param min Minimum value
     * @param max Maximum value
     * @return true if value changed this frame
     */
    inline bool SliderInt(const char* label, int* value, float width, int min, int max)
    {
        float x = ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::BeginGroup();
        
        if (strncmp(label, "##", 2) != 0)
        {
            ImGui::Text("%s", label);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
        }
        
        ImGui::PushID(label);
        ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
        
        // Calculate normalized value (0.0 to 1.0)
        float range = (float)max - (float)min;
        if (range < 1.0f) range = 1.0f;
        float normalizedValue = ((float)*value - (float)min) / range;
        normalizedValue = CLAMP(normalizedValue, 0.0f, 1.0f);
        
        float handleX = cursorScreenPos.x + normalizedValue * width;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw background bar (dark gray)
        drawList->AddRectFilled(
            ImVec2(cursorScreenPos.x, cursorScreenPos.y + 4.0f),
            ImVec2(cursorScreenPos.x + width, cursorScreenPos.y + 19.0f),
            ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)),
            4.0f
        );
        
        // Draw fill bar (accent color - mauve/magenta from menu)
        drawList->AddRectFilled(
            ImVec2(cursorScreenPos.x, cursorScreenPos.y + 4.0f),
            ImVec2(handleX, cursorScreenPos.y + 19.0f),
            ImGui::GetColorU32(ImVec4(0.65f, 0.35f, 0.95f, 1.0f)),
            4.0f
        );
        
        // Draw handle circle (dark gray)
        drawList->AddCircleFilled(
            ImVec2(handleX, cursorScreenPos.y + 11.0f),
            10.0f,
            ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f))
        );
        
        // Invisible button for interaction
        ImGui::InvisibleButton("##slider", ImVec2(width, 30.0f));
        
        bool changed = false;
        ImGuiIO& io = ImGui::GetIO();
        bool keyCtrl = io.KeyCtrl;
        bool keyShift = io.KeyShift;
        
        // Shift + Click to open input dialog
        if (ImGui::IsItemHovered() && ImGui::IsItemClicked(ImGuiMouseButton_Left) && keyShift)
        {
            ImGui::SetNextWindowPos(ImVec2(handleX, cursorScreenPos.y - 4.0f), ImGuiCond_Always);
            ImGui::OpenPopup("##slider_edit_int");
        }
        
        bool popupOpen = ImGui::IsPopupOpen("##slider_edit_int");
        
        // Handle mouse dragging
        if (!popupOpen && ImGui::IsItemActive())
        {
            if (keyCtrl)
            {
                // Fine adjustment with Ctrl
                float delta = io.MouseDelta.x / width * (max - min) * 0.15f;
                int newValue = CLAMP(*value + (int)std::round(delta), min, max);
                if (newValue != *value)
                {
                    *value = newValue;
                    changed = true;
                }
            }
            else
            {
                // Normal adjustment
                float mouseX = ImGui::GetMousePos().x - cursorScreenPos.x;
                int newValue = min + (int)std::round((mouseX / width) * (max - min));
                newValue = CLAMP(newValue, min, max);
                if (newValue != *value)
                {
                    *value = newValue;
                    changed = true;
                }
            }
        }
        
        // Show tooltip with usage info
        if (ImGui::IsItemHovered() && !popupOpen)
        {
            ImGui::BeginTooltip();
            ImGui::Text("Ctrl + Drag: fine adjust");
            ImGui::Text("Shift + Click: type a value");
            ImGui::EndTooltip();
        }
        
        // Display current value
        ImGui::SameLine();
        ImGui::Text("   %d", *value);
        
        // Input popup for typing exact value
        if (ImGui::BeginPopup("##slider_edit_int"))
        {
            ImGui::Text("Enter Value:");
            ImGui::SetNextItemWidth(90.0f);
            ImGui::SetKeyboardFocusHere();
            
            int tempValue = *value;
            if (ImGui::InputInt("##input_int", &tempValue, 0, 0))
            {
                int newValue = CLAMP(tempValue, min, max);
                if (newValue != *value)
                {
                    *value = newValue;
                    changed = true;
                }
            }
            
            if (ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                ImGui::CloseCurrentPopup();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
        ImGui::EndGroup();
        
        return changed;
    }
}
