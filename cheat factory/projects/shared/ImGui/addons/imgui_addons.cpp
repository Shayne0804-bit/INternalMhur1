//============ Copyright KiwiHax, All rights reserved ============//
//
//  Purpose: 
//
//================================================================//

#include "../imgui_internal.h"
#include "imgui_addons.h"

#include <map>
#include <unordered_map>
#include <string>

using namespace ImGui;

ImVec4 ImAdd::HexToColorVec4(unsigned int hex_color, float alpha)
{
    ImVec4 color;

    color.x = ((hex_color >> 16) & 0xFF) / 255.0f;
    color.y = ((hex_color >> 8) & 0xFF) / 255.0f;
    color.z = (hex_color & 0xFF) / 255.0f;
    color.w = alpha;

    return color;
}

void ImAdd::SeparatorText(const char* label, float thickness)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(-0.1f, g.FontSize), label_size.x, g.FontSize);

    const ImRect total_bb(pos, pos + size);
    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id)) {
        return;
    }

    window->DrawList->AddText(pos, GetColorU32(ImGuiCol_TextDisabled), label);

    if (thickness > 0)
        window->DrawList->AddLine(pos + ImVec2(label_size.x + style.ItemInnerSpacing.x, size.y / 2), pos + ImVec2(size.x, size.y / 2), GetColorU32(ImGuiCol_Border), thickness);
}

void ImAdd::VSeparator(float margin, float thickness)
{
    if (thickness <= 0)
        return;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(thickness, -0.1f), thickness, thickness);

    const ImRect bb(pos, pos + size);
    const ImRect bb_rect(pos + ImVec2(0, margin), pos + size - ImVec2(0, margin));

    ItemSize(ImVec2(thickness, 0.0f));
    if (!ItemAdd(bb, 0))
        return;

    window->DrawList->AddRectFilled(bb_rect.Min, bb_rect.Max, GetColorU32(ImGuiCol_Border));
}

bool ImAdd::TabIcon(ImTextureRef icon_texture, const char* label, int* v, int tab_id, bool expandable, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    bool has_label = label_size.x > 0;
    bool has_icon = icon_texture != nullptr;

    bool active = false;
    if (v)
    {
        active = *v == tab_id;
    }
    
    float expanded_width = (has_label ? (label_size.x + style.FramePadding.x) : 0.0f) + (has_icon ? (g.FontSize + style.FramePadding.x) : 0.0f) + style.FramePadding.x;
    float unexpanded_width = g.FontSize + style.FramePadding.x * 2.0f;
    float finnal_width = (expandable && has_label) ? (active ? expanded_width : unexpanded_width) : expanded_width;
    
    // Animations
    struct stColors_State {
        float   Width;
        ImColor Shadow;
        ImColor Frame;
        ImColor Border;
        ImColor Label;
        ImColor Icon;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    static bool init_width = true;

    if (init_width)
    {
        it_anim->second.Width = finnal_width;
    }

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 finnal_size = CalcItemSize(
        size_arg, 
        it_anim->second.Width,
        label_size.y + style.FramePadding.y * 2.0f
    );

    const ImRect total_bb(pos, pos + finnal_size);
    ItemSize(finnal_size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    if (v)
    {
        if (pressed) *v = tab_id;
    }

    // Colors
    ImVec4 colShadowMain = GetStyleColorVec4(ImGuiCol_FrameBgShadow);
    ImVec4 colShadowNull = colShadowMain;
    colShadowNull.w = 0.0f;
    ImVec4 colShadow = active ? colShadowMain : colShadowNull;
    colShadow.w *= style.Alpha;

    ImVec4 colFrameMain = GetStyleColorVec4(active ? ImGuiCol_TabActive : hovered ? ImGuiCol_TabHovered : ImGuiCol_Tab);
    ImVec4 colFrame = colFrameMain;
    colFrame.w *= style.Alpha;

    ImVec4 colBorderMain = GetStyleColorVec4(ImGuiCol_Border);
    ImVec4 colBorderNull = colBorderMain; colBorderNull.w = 0.0f;
    ImVec4 colBorder = active ? colBorderMain : colBorderNull;
    if (!active && hovered)
        colBorder = colBorderMain;
    else if (!active && !expandable && has_label)
    {
        colBorder = colBorderMain;
        colBorder.w *= 0.55f;
    }
    colBorder.w *= style.Alpha;

    ImVec4 colLabelMain = GetStyleColorVec4((active || hovered) ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImVec4 colLabelNull = colLabelMain; colLabelNull.w = 0.0f;
    ImVec4 colLabel = (!expandable || active) ? colLabelMain : colLabelNull;
    colLabel.w *= style.Alpha;

    ImVec4 colIcon = colLabelMain;
    colIcon.w *= style.Alpha;

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Width   = finnal_width;
        it_anim->second.Shadow   = colShadow;
        it_anim->second.Frame   = colFrame;
        it_anim->second.Border  = colBorder;
        it_anim->second.Label   = colLabel;
        it_anim->second.Icon   = colIcon;

        init_width = false;
    }

    if (expandable && has_label)
    {
        it_anim->second.Width = ImLerp<float>(it_anim->second.Width, finnal_width, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    }

    it_anim->second.Shadow.Value     = ImLerp(it_anim->second.Shadow.Value, colShadow, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Frame.Value     = ImLerp(it_anim->second.Frame.Value, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Border.Value    = ImLerp(it_anim->second.Border.Value, colBorder, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Label.Value     = ImLerp(it_anim->second.Label.Value, colLabel, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Icon.Value      = ImLerp(it_anim->second.Icon.Value, colIcon, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, it_anim->second.Frame, style.FrameRounding);
    window->DrawList->AddRectFilledMultiColorRounded(total_bb.Min, total_bb.Max, GetColorU32(it_anim->second.Shadow, 0.0f), GetColorU32(it_anim->second.Shadow, 0.0f), it_anim->second.Shadow, it_anim->second.Shadow, style.FrameRounding);

    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(total_bb.Min, total_bb.Max, it_anim->second.Border, style.FrameRounding, 0, style.FrameBorderSize);
    }

    if (has_icon)
    {
        window->DrawList->AddImage(icon_texture, pos + style.FramePadding, pos + ImVec2(g.FontSize, g.FontSize) + style.FramePadding, ImVec2(0, 0), ImVec2(1, 1), it_anim->second.Icon);
    }

    ImVec2 label_pos(pos + ImVec2(has_icon ? finnal_size.y : style.FramePadding.x, style.FramePadding.y));

    if (has_label)
    {
        window->DrawList->PushClipRect(total_bb.Min, total_bb.Max, true);
        PushStyleColor(ImGuiCol_Text, it_anim->second.Label.Value);
        RenderText(label_pos, label);
        PopStyleColor();
        window->DrawList->PopClipRect();
    }

    return pressed;
}

bool ImAdd::ButtonXMark(const char* str_id, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, GetFrameHeight(), GetFrameHeight());

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    bool was_disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

    // Colors
    ImVec4 colXMark = GetStyleColorVec4((held || was_disabled) ? ImGuiCol_TextDisabled : ImGuiCol_Text);

    // Animation
    struct stColors_State {
        ImColor XMark;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.XMark = colXMark;
    }

    it_anim->second.XMark.Value = ImLerp(it_anim->second.XMark.Value, colXMark, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);

    ImVec2 center = bb.GetCenter();
    float cross_extent = g.FontSize * 0.5f * 0.7071f - 1.0f;
    center -= ImVec2(0.5f, 0.5f);
    window->DrawList->AddLine(center + ImVec2(+cross_extent, +cross_extent), center + ImVec2(-cross_extent, -cross_extent), it_anim->second.XMark, 1.5f);
    window->DrawList->AddLine(center + ImVec2(+cross_extent, -cross_extent), center + ImVec2(-cross_extent, +cross_extent), it_anim->second.XMark, 1.5f);

    return pressed;
}

bool ImAdd::Button(ImTextureRef icon_texture, const char* label, const ImVec2& size_arg, ImGuiButtonFlags button_flags, ImDrawFlags draw_flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const float square_sz = g.FontSize + style.FramePadding.y * 2.0f;

    bool has_label = label_size.x > 0.0f;
    bool has_icon = icon_texture.GetTexID() != ImTextureID_Invalid;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, style.FramePadding.x + (has_label ? label_size.x + style.FramePadding.x : 0.0f) + (has_icon ? g.FontSize + style.FramePadding.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f);
    ImVec2 content_size = ImVec2((has_label ? label_size.x : 0.0f) + (has_icon ? g.FontSize : 0.0f) + (has_label && has_icon ? style.FramePadding.x : 0.0f), g.FontSize);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held, button_flags);

    // Colors
    ImVec4 colFrame = GetStyleColorVec4((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);

    // Animations
    struct stColors_State {
        ImColor Frame;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
    }

    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, it_anim->second.Frame, style.FrameRounding, draw_flags);
    window->DrawList->AddRectFilledMultiColorRounded(total_bb.Min, total_bb.Max, GetColorU32(ImGuiCol_ButtonShadow, 0.0f), GetColorU32(ImGuiCol_ButtonShadow, 0.0f), GetColorU32(ImGuiCol_ButtonShadow), GetColorU32(ImGuiCol_ButtonShadow), style.FrameRounding, draw_flags);

    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(total_bb.Min, total_bb.Max, GetColorU32(ImGuiCol_Border), style.FrameRounding, draw_flags, style.FrameBorderSize);
    }

    ImVec2 starting_pos = pos + ImTrunc((size - content_size) * style.ButtonTextAlign);

    if (has_icon) window->DrawList->AddImage(icon_texture, starting_pos, starting_pos + ImVec2(g.FontSize, g.FontSize));

    if (has_label) RenderText(starting_pos + ImVec2(has_icon ? (style.FramePadding.x + g.FontSize) : 0.0f, 0.0f), label);

    return pressed;
}

bool ImAdd::Button(const char* label, const ImVec2& size_arg, ImGuiButtonFlags button_flags, ImDrawFlags draw_flags)
{
    return ImAdd::Button(nullptr, label, size_arg, button_flags, draw_flags);
}

void ImAdd::ScrollBar(const char* str_id, ImGuiWindow* window, const ImVec2& size_arg)
{
    if (!window || window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    // Layout
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, GetFrameHeight(), CalcItemWidth());
    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return;

    // Interactions
    bool hovered, held;
    ButtonBehavior(total_bb, id, &hovered, &held);

    // Colors
    ImVec4 colFrame = GetStyleColorVec4(held ? ImGuiCol_ScrollbarGrabActive : hovered ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab);

    // Animations
    struct stColors_State {
        ImColor Frame;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
    }

    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    // Scroll metrics
    float visible_height = size.y;
    float total_height = window->ContentSize.y;
    float scroll_max = ImMax(window->ScrollMax.y, 0.0f);
    float scroll_y = window->Scroll.y;

    float scroll_height = (total_height > 0.0f)
        ? (visible_height / total_height) * visible_height
        : visible_height;

    scroll_height = ImClamp(scroll_height, 15.0f, visible_height);

    float scroll_top = (scroll_max > 0.0f)
        ? (scroll_y / scroll_max) * (visible_height - scroll_height)
        : 0.0f;

    // Smooth mouse wheel scroll
    static std::unordered_map<ImGuiID, float> scroll_targets;
    auto& target = scroll_targets[id];

    // Handle dragging
    if (held && scroll_max > 0.0f)
    {
        float mouse_delta = g.IO.MouseDelta.y;
        float scrollable_range = visible_height - scroll_height;
        if (scrollable_range > 0.0f)
        {
            float ratio = scroll_max / scrollable_range;
            window->Scroll.y = ImClamp(window->Scroll.y + mouse_delta * ratio, 0.0f, scroll_max);
        }
        // Keep smooth scroll target in sync
        target = window->Scroll.y;
    }
    else
    {
        ImRect win_rect = window->Rect();
        bool hovered_window = ImGui::IsMouseHoveringRect(win_rect.Min, win_rect.Max);

        if (hovered_window)
        {
            window->Flags |= ImGuiWindowFlags_NoScrollWithMouse;
            const float scroll_speed = 40.0f;
            target -= g.IO.MouseWheel * scroll_speed;
            target = ImClamp(target, 0.0f, scroll_max);
        }

        window->Scroll.y = ImLerp(window->Scroll.y, target, 10.0f * g.IO.DeltaTime);
    }

    ImRect grab_bb(ImVec2(total_bb.Min.x, total_bb.Min.y + scroll_top), ImVec2(total_bb.Max.x, total_bb.Min.y + scroll_top + scroll_height));

    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, GetColorU32(ImGuiCol_ScrollbarBg), style.ScrollbarRounding);
    window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, it_anim->second.Frame, style.ScrollbarRounding);
    window->DrawList->AddRectFilledMultiColorRounded(grab_bb.Min, grab_bb.Max, GetColorU32(ImGuiCol_FrameBgShadow, 0.0f), GetColorU32(ImGuiCol_FrameBgShadow, 0.0f), GetColorU32(ImGuiCol_FrameBgShadow), GetColorU32(ImGuiCol_FrameBgShadow), style.ScrollbarRounding);

    RenderNavCursor(total_bb, id);
}

bool ImAdd::BeginChild(const char* str_id, const ImVec2& size_arg, bool child_style)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    std::string str_id_full = std::string(str_id) + "::child";
    std::string str_id_body = std::string(str_id) + "::child::body";
    std::string str_id_scrollbar = std::string(str_id) + "::child::scrollbar";

    PushStyleVar(ImGuiStyleVar_WindowPadding, child_style ? style.ChildPadding : style.WindowPadding);
    bool result = ImGui::BeginChild(
        str_id_full.c_str(),
        size_arg,
        style.ChildBorderSize > 0 ? ImGuiChildFlags_Border : ImGuiChildFlags_AlwaysUseWindowPadding, (child_style ? 0 : ImGuiWindowFlags_NoBackground) | ImGuiWindowFlags_NoScrollbar
    );

    if (result)
    {
        ImGuiWindow*    pWindow     = GetCurrentWindow();
        ImDrawList*     pDrawList   = pWindow->DrawList;
        ImVec2          pos         = pWindow->Pos;
        ImVec2          size        = pWindow->Size;
        bool            has_scroll  = pWindow->ScrollMax.y > 0;

        if (has_scroll)
        {
            ImVec2 cursor_pos = GetCursorScreenPos();

            SetCursorScreenPos(pos + ImVec2(size.x - style.ScrollbarSize - style.WindowPadding.x, style.WindowPadding.y));
            ImAdd::ScrollBar(str_id_scrollbar.c_str(), pWindow, ImVec2(style.ScrollbarSize, size.y - style.WindowPadding.y * 2.0f));

            SetCursorScreenPos(cursor_pos);

            pWindow->ContentRegionRect.Max.x -= style.ScrollbarSize + style.WindowPadding.x;
        }
    }

    PopStyleVar();

    PushItemWidth(GetContentRegionAvail().x);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ChildPadding);

    return result;
}

void ImAdd::EndChild()
{
    PopStyleVar();
    PopItemWidth();
    ImGui::EndChild();
}

bool ImAdd::ToggleButton(const char* label, bool* v)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    bool has_label = label_size.x > 0;

    float height = g.FontSize + style.CellPadding.y * 2.0f;

    float grab_padding = ImTrunc(g.FontSize / 3.0f);
    float grab_size = height - grab_padding * 2.0f;
    float grab_radius = ImTrunc(grab_size / 2) + 1.0f;
    float frame_width = grab_size * 2.0f + grab_padding * 3.0f;
    float rounding = height / 2.0f;

    float width = has_label ? GetContentRegionAvail().x : frame_width;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(width, height);

    const ImRect total_bb(pos, pos + size);
    const ImRect frame_bb(pos + ImVec2(has_label ? size.x - frame_width : 0.0f, 0.0f), pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    if (pressed)
    {
        *v = !*v;
    }

    // Colors
    ImVec4 colFrame = GetStyleColorVec4(*v ? (style.FrameBorderSize > 0 ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab) : (hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    colFrame.w *= style.Alpha;

    ImVec4 colBorderMain = GetStyleColorVec4(ImGuiCol_SliderGrab);
    ImVec4 colBorderNull = colBorderMain; colBorderNull.w = 0.0f;
    ImVec4 colBorder = (*v ? colBorderMain : colBorderNull);
    colBorder.w *= style.Alpha;

    ImVec4 colGrab = GetStyleColorVec4(*v ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled);
    colGrab.w *= style.Alpha;

    float fGrabProg = *v ? 1.0f : 0.0f;

    // Animations
    struct stColors_State {
        ImColor Frame;
        ImColor Border;
        ImColor Grab;
        float GrabProg;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Border = colBorder;
        it_anim->second.Grab = colGrab;
        it_anim->second.GrabProg = fGrabProg;
    }

    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, colBorder, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Grab.Value = ImLerp(it_anim->second.Grab.Value, colGrab, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.GrabProg = ImLerp<float>(it_anim->second.GrabProg, fGrabProg, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, GetColorU32(it_anim->second.Frame, style.Alpha), rounding);
    window->DrawList->AddRectFilledMultiColorRounded(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_ButtonShadow, 0.0f), GetColorU32(ImGuiCol_ButtonShadow, 0.0f), GetColorU32(ImGuiCol_ButtonShadow, *v ? 1.0f : 0.0f), GetColorU32(ImGuiCol_ButtonShadow, *v ? 1.0f : 0.0f), rounding);

    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, it_anim->second.Border, rounding, 0, style.FrameBorderSize);
    }

    window->DrawList->AddCircleFilled(frame_bb.Min + ImVec2(ImTrunc(height / 2) + (grab_size + grab_padding) * it_anim->second.GrabProg, ImTrunc(height / 2)), grab_radius, it_anim->second.Grab);

    RenderText(ImVec2(pos.x, pos.y + style.CellPadding.y), label);

    return pressed;
}

bool ImAdd::CheckBox(const char* label, bool* checked)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const float square_sz = g.FontSize + style.CellPadding.y * 2.0f;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(square_sz + style.ItemInnerSpacing.x + label_size.x, square_sz);

    const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    if (pressed) *checked = !*checked;

    // Colors
    ImVec4 colFrame = GetStyleColorVec4(*checked ? ImGuiCol_SliderGrab : (hovered && held) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);

    ImVec4 colBorderMain = GetStyleColorVec4(ImGuiCol_Border);
    ImVec4 colBorderNull = colBorderMain;
    colBorderNull.w = 0.0f;
    ImVec4 colBorder = *checked ? colBorderNull : colBorderMain;

    ImVec4 colShadowMain = GetStyleColorVec4(ImGuiCol_FrameBgShadow);
    ImVec4 colShadowNull = colShadowMain;
    colShadowNull.w = 0.0f;
    ImVec4 colShadow = *checked ? colShadowMain : colShadowNull;

    ImVec4 colCheckMain = GetStyleColorVec4(ImGuiCol_CheckMark);
    ImVec4 colCheckNull = colCheckMain;
    colCheckNull.w = 0.0f;
    ImVec4 colCheck = *checked ? colCheckMain : colCheckNull;

    // Animations
    struct stColors_State {
        ImColor Frame;
        ImColor Border;
        ImColor Shadow;
        ImColor Check;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Border = colBorder;
        it_anim->second.Shadow = colShadow;
        it_anim->second.Check = colCheck;
    }

    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Border.Value = ImLerp(it_anim->second.Border.Value, colBorder, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Shadow.Value = ImLerp(it_anim->second.Shadow.Value, colShadow, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Check.Value = ImLerp(it_anim->second.Check.Value, colCheck, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, it_anim->second.Frame, style.FrameRounding);
    window->DrawList->AddRectFilledMultiColorRounded(check_bb.Min, check_bb.Max, GetColorU32(it_anim->second.Shadow, 0.0f), GetColorU32(it_anim->second.Shadow, 0.0f), it_anim->second.Shadow, it_anim->second.Shadow, style.FrameRounding);

    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(check_bb.Min, check_bb.Max, it_anim->second.Border, style.FrameRounding, 0, style.FrameBorderSize);
    }

    const float pad = ImMax(1.0f, IM_TRUNC(square_sz / 3.5f));
    ImAdd::RenderCheckMark(window->DrawList, check_bb.Min + ImVec2(pad, pad), it_anim->second.Check, square_sz - pad * 2.0f);

    window->DrawList->AddText(pos + ImVec2(size.y + style.ItemInnerSpacing.x, style.CellPadding.y), GetColorU32(ImGuiCol_Text), label);

    return pressed;
}

bool ImAdd::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float width = CalcItemWidth();
    const ImVec2 pos = window->DC.CursorPos;

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const bool has_label = label_size.x > 0;
    const float frame_pos_y = has_label ? (g.FontSize + style.ItemInnerSpacing.y) : 0.0f;
    const float frame_height = g.FontSize;

    const ImRect frame_bb(pos + ImVec2(0, frame_pos_y), pos + ImVec2(width, frame_pos_y + frame_height));
    const ImRect total_bb(pos, frame_bb.Max);

    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id, &frame_bb, 0))
        return false;

    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
    const bool held = g.ActiveId == id;
    const bool make_active = (clicked || g.NavActivateId == id);

    if (make_active)
    {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    // Colors
    ImVec4 colFrame = GetStyleColorVec4((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImVec4 colLine = GetStyleColorVec4(held ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);

    // Animations
    struct stColors_State {
        ImColor Frame;
        ImColor Line;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Line = colLine;
    }

    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Line.Value = ImLerp(it_anim->second.Line.Value, colLine, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    // Grab logic
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, 0, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    float relative_value = 0.0f;
    if (data_type == ImGuiDataType_Float)
    {
        float val = *(float*)p_data;
        float min_val = *(float*)p_min;
        float max_val = *(float*)p_max;
        relative_value = (val - min_val) / (max_val - min_val);
    }
    else if (data_type == ImGuiDataType_S32)
    {
        int val = *(int*)p_data;
        int min_val = *(int*)p_min;
        int max_val = *(int*)p_max;
        relative_value = (float)(val - min_val) / (float)(max_val - min_val);
    }

    relative_value = ImClamp(relative_value, 0.0f, 1.0f);

    // Draw frame

    const float pad = ImTrunc(frame_height / 3.0f);

    window->DrawList->AddRectFilled(frame_bb.Min + ImVec2(0, pad), frame_bb.Max - ImVec2(0, pad), it_anim->second.Frame, 1.0f);

    ImVec2 fill_end = ImTrunc(ImVec2(frame_bb.Min.x + relative_value * (frame_bb.GetWidth() - style.GrabMinSize), frame_bb.Max.y));

    ImRect slider_bb = ImRect(ImTrunc(frame_bb.Min + ImVec2(0.0f, pad)), ImTrunc(fill_end + ImVec2(style.GrabMinSize / 2, -pad)));

    if (slider_bb.Max.x > slider_bb.Min.x + style.FrameRounding)
    {
        window->DrawList->AddRectFilled(slider_bb.Min, slider_bb.Max, it_anim->second.Line, style.FrameRounding);
        window->DrawList->AddRectFilledMultiColorRounded(slider_bb.Min, slider_bb.Max, GetColorU32(ImGuiCol_FrameBgShadow, 0.0f), GetColorU32(ImGuiCol_FrameBgShadow), GetColorU32(ImGuiCol_FrameBgShadow), GetColorU32(ImGuiCol_FrameBgShadow, 0.0f), style.FrameRounding);
    }

    ImRect new_grab_bb = ImRect(ImTrunc(ImVec2(fill_end.x, frame_bb.Min.y)), ImTrunc(ImVec2(fill_end.x + style.GrabMinSize, frame_bb.Max.y)));

    window->DrawList->AddRectFilled(new_grab_bb.Min, new_grab_bb.Max, GetColorU32(ImGuiCol_Text), style.GrabRounding);
    window->DrawList->AddRectFilledMultiColorRounded(new_grab_bb.Min, new_grab_bb.Max, GetColorU32(ImGuiCol_FrameBgShadow, 0.0f), GetColorU32(ImGuiCol_FrameBgShadow, 0.0f), GetColorU32(ImGuiCol_FrameBgShadow), GetColorU32(ImGuiCol_FrameBgShadow), style.GrabRounding);

    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);

    if (has_label) {
        RenderText(total_bb.Min, label);
        window->DrawList->AddText(total_bb.Min + ImVec2(width - CalcTextSize(value_buf).x, 0), GetColorU32(ImGuiCol_TextDisabled), value_buf);
    }

    return value_changed;
}

bool ImAdd::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
{
    return ImAdd::SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format);
}

bool ImAdd::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format)
{
    return ImAdd::SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format);
}

bool ImAdd::SliderAlpha(const char* str_id, ImVec4& col)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    float width = CalcItemWidth();
    float height = GetFrameHeight();
    float half_height = ImTrunc(height / 2.0f);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(width, height);

    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    const float slider_min_x = bb.Min.x + half_height;
    const float slider_max_x = bb.Max.x - half_height;

    if (held)
    {
        // Calculate the available slider area
        const float slider_width = slider_max_x - slider_min_x;

        if (slider_width > 0.0f)
        {
            // Clamp mouse position to slider bounds and calculate alpha value
            float mouse_x = ImClamp(io.MousePos.x, slider_min_x, slider_max_x);
            col.w = 1.0f - ((mouse_x - slider_min_x) / slider_width); // Inverted for correct alpha mapping
            col.w = ImClamp(col.w, 0.0f, 1.0f);
        }
    }

    ImColor col_rgb = ImVec4(col.x, col.y, col.z, 1.0f);
    ImColor col_rgb_width_alpha = ImVec4(col.x, col.y, col.z, col.w);

    window->DrawList->AddRectFilled(pos, pos + ImVec2(half_height, height), col_rgb, style.FrameRounding, ImDrawFlags_RoundCornersLeft);
    RenderColorRectWithAlphaCheckerboard(window->DrawList, pos + ImVec2(half_height, 0), pos + size, 0, half_height, ImVec2(0.0f, 0.0f), style.FrameRounding, ImDrawFlags_RoundCornersRight);
    window->DrawList->AddRectFilledMultiColorRounded(pos + ImVec2(half_height, 0), pos + ImVec2(width - half_height, height), col_rgb, col_rgb & ~IM_COL32_A_MASK, col_rgb & ~IM_COL32_A_MASK, col_rgb);

    // Calculate circle position within slider bounds (inverted for correct visual mapping)
    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);
    const float slider_width = slider_max_x - slider_min_x;
    float circle_x = slider_min_x + slider_width * (1.0f - col.w); // Inverted for correct visual position

    RenderColorRectWithAlphaCheckerboard(window->DrawList, ImVec2(circle_x - circle_radius, pos.y + half_height - circle_radius), ImVec2(circle_x + circle_radius, pos.y + half_height + circle_radius), col_rgb_width_alpha, circle_radius, ImVec2(0.0f, 0.0f), circle_radius + 1.0f);
    window->DrawList->AddCircle(ImVec2(circle_x, pos.y + half_height), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);

    return pressed || held;
}

bool ImAdd::SliderHue(const char* str_id, ImVec4& col)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    float width = CalcItemWidth();
    float height = GetFrameHeight();
    float half_height = ImTrunc(height / 2.0f);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(width, height);

    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    const float slider_min_x = bb.Min.x + half_height;
    const float slider_max_x = bb.Max.x - half_height;

    // Extract HSV values from RGB color
    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);

    if (held)
    {
        // Calculate the available slider area
        const float slider_width = slider_max_x - slider_min_x;

        if (slider_width > 0.0f)
        {
            // Clamp mouse position to slider bounds and calculate hue value
            float mouse_x = ImClamp(io.MousePos.x, slider_min_x, slider_max_x);
            h = (mouse_x - slider_min_x) / slider_width; // Hue ranges from 0.0 to 1.0
            h = ImClamp(h, 0.0f, 1.0f);

            // Convert back to RGB
            ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);
        }
    }

    // Draw hue gradient in the middle section
    const ImVec2 gradient_start = pos + ImVec2(half_height, 0);
    const ImVec2 gradient_end = pos + ImVec2(width - half_height, height);

    // Draw hue spectrum gradient
    const int num_segments = 6; // For the 6 main hue segments
    const float segment_width = (width - height) / num_segments;

    for (int i = 0; i < num_segments; i++)
    {
        float hue_start = i / (float)num_segments;
        float hue_end = (i + 1) / (float)num_segments;

        ImVec4 color_start, color_end;
        ImGui::ColorConvertHSVtoRGB(hue_start, 1.0f, 1.0f, color_start.x, color_start.y, color_start.z);
        ImGui::ColorConvertHSVtoRGB(hue_end, 1.0f, 1.0f, color_end.x, color_end.y, color_end.z);
        color_start.w = color_end.w = 1.0f;

        ImVec2 seg_start = gradient_start + ImVec2(segment_width * i, 0);
        ImVec2 seg_end = gradient_start + ImVec2(segment_width * (i + 1), height);

        window->DrawList->AddRectFilledMultiColor(
            seg_start, seg_end,
            ImColor(color_start), ImColor(color_end), ImColor(color_end), ImColor(color_start)
        );
    }

    // Draw static color areas on left and right (repeating the hue spectrum edges)
    ImVec4 left_color, right_color;
    ImGui::ColorConvertHSVtoRGB(0.0f, 1.0f, 1.0f, left_color.x, left_color.y, left_color.z);
    ImGui::ColorConvertHSVtoRGB(1.0f, 1.0f, 1.0f, right_color.x, right_color.y, right_color.z);
    left_color.w = right_color.w = 1.0f;

    window->DrawList->AddRectFilled(pos, pos + ImVec2(half_height, height), ImColor(left_color), style.FrameRounding, ImDrawFlags_RoundCornersLeft);
    window->DrawList->AddRectFilled(pos + ImVec2(width - half_height, 0), pos + size, ImColor(right_color), style.FrameRounding, ImDrawFlags_RoundCornersRight);

    // Calculate circle position within slider bounds
    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);
    const float slider_width = slider_max_x - slider_min_x;
    float circle_x = slider_min_x + slider_width * h;

    // Het HUE color
    ImVec4 hue_color;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hue_color.x, hue_color.y, hue_color.z); // Full saturation & value
    hue_color.w = 1.0f;

    window->DrawList->AddCircleFilled(ImVec2(circle_x, pos.y + half_height), circle_radius, GetColorU32(hue_color));
    window->DrawList->AddCircle(ImVec2(circle_x, pos.y + half_height), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);

    return pressed || held;
}

bool ImAdd::ColorPalette(const char* str_id, ImVec4& col, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;
    const ImGuiID id = window->GetID(str_id);

    // Use CalcItemWidth for both dimensions if not specified
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), CalcItemWidth());

    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    // Extract HSV values from RGB color
    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);

    if (held || hovered)
    {
        if (held)
        {
            // Calculate saturation and value from mouse position
            s = ImClamp((io.MousePos.x - bb.Min.x) / size.x, 0.0f, 1.0f);
            v = 1.0f - ImClamp((io.MousePos.y - bb.Min.y) / size.y, 0.0f, 1.0f); // Inverted Y axis (0 at bottom, 1 at top)

            // Convert back to RGB
            ImGui::ColorConvertHSVtoRGB(h, s, v, col.x, col.y, col.z);
        }
    }

    // Get the hue color at full saturation and value
    ImVec4 hue_color;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hue_color.x, hue_color.y, hue_color.z);
    hue_color.w = 1.0f;
    ImU32 hue_color32 = ImGui::ColorConvertFloat4ToU32(hue_color);

    // Draw the saturation-value gradient using ImGui's method
    // First rectangle: White to hue color (top to bottom)
    window->DrawList->AddRectFilledMultiColorRounded(bb.Min, bb.Max,
        IM_COL32_WHITE, hue_color32, hue_color32, IM_COL32_WHITE,
        style.FrameRounding + (style.FrameRounding > 0 ? 1.5f : 0.0f)
    );

    // Second rectangle: Transparent to black (left to right) overlayed on top
    window->DrawList->AddRectFilledMultiColorRounded(bb.Min, bb.Max,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32_BLACK, IM_COL32_BLACK,
        style.FrameRounding
    );

    if (style.FrameBorderSize > 0)
    {
        //window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
    }

    // Draw the selection circle
    float circle_x = bb.Min.x + s * size.x;
    float circle_y = bb.Min.y + (1.0f - v) * size.y; // Inverted Y axis
    const float circle_radius = ImMax(ImTrunc(g.FontSize / 2.0f) - 2.0f, 1.0f);

    ImVec4 col_rgb_without_alpha(col.x, col.y, col.z, 1.0f);

    window->DrawList->AddCircleFilled(ImVec2(circle_x, circle_y), circle_radius, GetColorU32(col_rgb_without_alpha));
    window->DrawList->AddCircle(ImVec2(circle_x, circle_y), circle_radius + 1.0f, IM_COL32_BLACK, 0, 1.0f);
    window->DrawList->AddCircle(ImVec2(circle_x, circle_y), circle_radius, IM_COL32_WHITE, 0, 1.0f);

    return pressed || held;
}

bool ImAdd::ColorButton(const char* desc_id, const ImVec4& col, bool has_alpha, float square_sz)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(desc_id);

    ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size(CalcItemSize(ImVec2(square_sz, square_sz), square_sz, square_sz));
    const ImRect bb(pos, pos + size);

    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImVec4 col_rgb = col;
    ImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);

    ImVec4 col_source = has_alpha ? col_rgb : col_rgb_without_alpha;

    if (col_source.w < 1.0f && has_alpha)
        RenderColorRectWithAlphaCheckerboard(window->DrawList, pos, pos + size, GetColorU32(col_source), ImTrunc(square_sz / 2), ImVec2(0, 0), ImTrunc(square_sz / 2));
    else
        window->DrawList->AddRectFilled(pos, pos + size, GetColorU32(col_source), ImTrunc(square_sz / 2));

    RenderNavCursor(bb, id);

    return pressed;
}

bool ImAdd::ColorPicker4(const char* label, float col[4], bool has_alpha)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;

    ImVec4 col_v4(col[0], col[1], col[2], has_alpha ? col[3] : 1.0F);

    //float width = 200;
    float width = CalcItemWidth();

    PushID(label);
    BeginGroup();

    std::string str_label = label;
    std::string str_colp_id = str_label + "::color_palette";
    std::string str_hue_id = str_label + "::hue";

    bool result = ImAdd::ColorPalette(str_colp_id.c_str(), col_v4, ImVec2(width, ImTrunc(width / 2)));

    //PushItemWidth(width);
    PushStyleVar(ImGuiStyleVar_FramePadding, style.CellPadding);

    result |= ImAdd::SliderHue(str_hue_id.c_str(), col_v4);

    if (has_alpha)
    {
        std::string str_alpha_id = str_label + "::alpha";
        result |= ImAdd::SliderAlpha(str_alpha_id.c_str(), col_v4);

    }

    PopStyleVar();
    //PopItemWidth();

    EndGroup();
    PopID();

    col[0] = col_v4.x;
    col[1] = col_v4.y;
    col[2] = col_v4.z;
    col[3] = col_v4.w;

    return result;
}

bool ImAdd::ColorEdit4(const char* name, float col[4])
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec4 col_v4(col[0], col[1], col[2], col[3]);
    const ImVec2 label_size = CalcTextSize(name, NULL, true);

    std::string popup_str_id = std::string(std::string(name) + "::color_edit_4");

    BeginGroup();
    if (label_size.x > 0.0f)
    {
        Text(name);
        SameLine(CalcItemWidth() - g.FontSize);
    }
    ImVec2 pos = window->DC.CursorPos;
    bool result = ImAdd::ColorButton(name, col_v4, true, g.FontSize);
    EndGroup();

    if (result)
    {
        OpenPopup(popup_str_id.c_str());
    }

    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, style.FramePadding);

    if (BeginPopupEx(GetID(popup_str_id.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        SetWindowPos(pos - style.WindowPadding);

        ImVec4 col_rgba = ImVec4(0, 0, 0, 0);
        col_rgba.x = col[0];
        col_rgba.y = col[1];
        col_rgba.z = col[2];
        col_rgba.w = col[3];

        ImAdd::ColorButton((popup_str_id + "::color_preview").c_str(), col_rgba, true, GetFontSize());

        char hex_buf[64];
        int i[4] = { IM_F32_TO_INT8_UNBOUND(col[0]), IM_F32_TO_INT8_UNBOUND(col[1]), IM_F32_TO_INT8_UNBOUND(col[2]), IM_F32_TO_INT8_UNBOUND(col[3]) };
        ImFormatString(hex_buf, IM_ARRAYSIZE(hex_buf), "#%02x%02x%02x%02x", ImClamp(i[0], 0, 255), ImClamp(i[1], 0, 255), ImClamp(i[2], 0, 255), ImClamp(i[3], 0, 255));

        if (label_size.x > 0)
        {
            ImGui::SameLine();
            ImGui::Text("%s", name);
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - CalcTextSize(hex_buf).x + style.WindowPadding.x);
        ImGui::SetCursorPosY(style.WindowPadding.y);
        ImGui::TextDisabled("%s", hex_buf);

        ImAdd::ColorPicker4(name, col, true);

        EndPopup();
    }

    PopStyleVar(2);

    return result;
}

bool ImAdd::SelectableLabel(const char* label, bool selected, bool centered, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    // Colors
    ImVec4 colLabel = GetStyleColorVec4(hovered || selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    // Animations
    struct stColors_State {
        ImColor Label;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Label = colLabel;
    }

    it_anim->second.Label.Value = ImLerp(it_anim->second.Label.Value, colLabel, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    RenderNavCursor(total_bb, id);

    window->DrawList->AddText(pos + ImTrunc(ImVec2(centered ? (size.x / 2 - label_size.x / 2) : style.FramePadding.x, size.y / 2 - label_size.y / 2)), it_anim->second.Label, label);

    return pressed;
}

bool ImAdd::Combo(const char* label, int* selected_index, std::vector<const char*> items)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    const float width = CalcItemWidth();
    const float height = g.FontSize + style.CellPadding.y * 4.0f;

    int items_count = items.size();

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, height);

    const ImRect combo_bb(pos + ImVec2(ImTrunc(width * IMADD_ITEM_WIDTH_MULTIPILIER), 0.0f), pos + size);
    const ImRect total_bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(total_bb, id))
        return false;

    // Behaviors
    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);

    std::string popup_str_id = std::string(std::string(label) + "::combo_popup");

    if (pressed)
    {
        OpenPopup(popup_str_id.c_str());
    }

    PushStyleVar(ImGuiStyleVar_WindowPadding, style.CellPadding * 2);
    PushStyleVar(ImGuiStyleVar_ItemSpacing, style.CellPadding * 2);
    if (BeginPopupEx(GetID(popup_str_id.c_str()), ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
    {
        SetWindowPos(combo_bb.Min + ImVec2(0, height + style.ItemInnerSpacing.y), ImGuiCond_Always);
        SetWindowSize(ImVec2(combo_bb.GetWidth(), ImGui::GetFontSize() * items_count + style.CellPadding.y * (items_count + 1) * 2), ImGuiCond_Always);

        PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        for (int i = 0; i < items_count; i++)
        {
            if (ImAdd::SelectableLabel(items[i], i == *selected_index, false, ImVec2(-0.1f, GetFontSize())))
            {
                *selected_index = i;
                CloseCurrentPopup();
            }
        }
        PopStyleVar();

        EndPopup();
    }
    PopStyleVar(2);

    // Colors
    ImVec4 colFrame = GetStyleColorVec4((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);

    // Animations
    struct stColors_State {
        ImColor Frame;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
    }

    it_anim->second.Frame.Value = ImLerp(it_anim->second.Frame.Value, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    RenderNavCursor(combo_bb, id);

    RenderText(pos + ImVec2(0, style.CellPadding.y * 2), label);

    window->DrawList->AddRectFilled(combo_bb.Min, combo_bb.Max, it_anim->second.Frame, style.FrameRounding);
    window->DrawList->AddRectFilledMultiColorRounded(combo_bb.Min, combo_bb.Max, GetColorU32(ImGuiCol_ButtonShadow, 0.0f), GetColorU32(ImGuiCol_ButtonShadow, 0.0f), GetColorU32(ImGuiCol_ButtonShadow), GetColorU32(ImGuiCol_ButtonShadow), style.FrameRounding);

    //if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(combo_bb.Min, combo_bb.Max, GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.ChildBorderSize);
    }

    std::string preview_item;
    if (*selected_index > items.size()) {
        preview_item = "*unknown item*";
    }
    else
    {
        preview_item = items[*selected_index];
    }

    window->DrawList->AddText(combo_bb.Min + style.CellPadding * 2, GetColorU32(ImGuiCol_TextDisabled), preview_item.c_str());

    RenderArrow(window->DrawList, combo_bb.Min + ImVec2(combo_bb.GetWidth() - GetFontSize() - style.CellPadding.x * 2, style.CellPadding.y * 2), GetColorU32(ImGuiCol_TextDisabled), ImGui::GetFontSize(), ImGuiDir_Down);

    return pressed;
}

bool ImAdd::KeyBind(const char* label, ImGuiKey* k)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    bool has_label = label_size.x > 0;

    char buf_display[32] = "Unbinded";

    bool is_selecing = false;

    if (*k != 0 && g.ActiveId != id)
    {
        strcpy_s(buf_display, sizeof buf_display, GetKeyName(*k));
    }
    else if (g.ActiveId == id)
    {
        is_selecing = true;
        strcpy_s(buf_display, sizeof buf_display, "...");
    }

    ImVec2 buf_display_size = ImGui::CalcTextSize(buf_display, NULL, true);

    const float width = CalcItemWidth();
    const float height = g.FontSize + style.CellPadding.y * 2.0f;

    ImVec2 widget_size = ImVec2(width, height);
    ImVec2 frame_size = ImVec2(buf_display_size.x + style.CellPadding.x * 2.0f, g.FontSize + style.CellPadding.y * 2.0f);

    ImRect total_bb(pos, pos + (has_label ? widget_size : frame_size));
    ImRect frame_bb(has_label ? ImVec2(total_bb.Min.x + widget_size.x - frame_size.x, total_bb.Min.y) : pos, total_bb.Max);

    ImGui::ItemSize(total_bb);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id, 0);

    if (hovered)
    {
        ImGui::SetHoveredID(id);
        g.MouseCursor = ImGuiMouseCursor_Hand;
    }

    const bool user_clicked = hovered && IsMouseClicked(ImGuiMouseButton_Left);

    if (user_clicked)
    {
        ImGui::SetActiveID(id, window);
        ImGui::FocusWindow(window);
    }
    else if (IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (g.ActiveId == id)
            ImGui::ClearActiveID();
    }

    bool value_changed = false;
    int key = *k;

    if (hovered && IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (g.ActiveId != id)
        {
            // Start capturing
            memset(io.MouseDown, 0, sizeof(io.MouseDown));
            SetActiveID(id, window);
            FocusWindow(window);
        }
    }

    if (IsMouseClicked(ImGuiMouseButton_Left) && g.ActiveId == id && !hovered)
    {
        // Clicked outside - cancel
        ClearActiveID();
    }

    // Handle key capture
    if (g.ActiveId == id)
    {
        // Check keyboard keys if no mouse button was pressed
        if (!value_changed)
        {
            // Check all possible keys
            for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; i++) // only named keyboard/gamepad keys
            {
                ImGuiKey key_test = (ImGuiKey)i;

                // Skip mouse inputs
                if ((key_test >= ImGuiKey_MouseLeft && key_test <= ImGuiKey_MouseWheelY) || key_test == ImGuiKey_Escape)
                    continue;

                if (IsKeyPressed(key_test)) // Pressed, not Down, avoids "instant bind"
                {
                    *k = key_test;
                    value_changed = true;
                    ClearActiveID();
                    break;
                }
            }
        }

        // Escape cancels
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            ClearActiveID();
        }
    }

    // Render

    ImGui::RenderNavHighlight(total_bb, id);

    window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_Button), style.FrameRounding);

    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
    }

    RenderText(ImVec2(total_bb.Min.x, total_bb.Min.y + style.CellPadding.y), label);

    PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    RenderText(frame_bb.Min + style.CellPadding, buf_display);
    PopStyleColor();

    return value_changed;
}

void ImAdd::RenderArrow(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz, ImGuiDir direction)
{
    if (direction < 0 || direction >= ImGuiDir_COUNT) return;

    float thickness = 1.0f;

    float half = ImTrunc(sz / 2.0f);
    float pad_y = ImTrunc(sz / 2.4f);
    float pad_x = ImTrunc(sz / 5.0f);

    if (direction == ImGuiDir_Down)
    {
        draw_list->PathLineTo(pos + ImVec2(pad_x, pad_y));
        draw_list->PathLineTo(pos + ImVec2(half, sz - pad_y));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_x, pad_y));
    }
    else if (direction == ImGuiDir_Up)
    {
        draw_list->PathLineTo(pos + ImVec2(pad_x, sz - pad_y));
        draw_list->PathLineTo(pos + ImVec2(half, pad_y));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_x, sz - pad_y));
    }
    else if (direction == ImGuiDir_Right)
    {
        draw_list->PathLineTo(pos + ImVec2(pad_y, pad_x));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_y, half));
        draw_list->PathLineTo(pos + ImVec2(pad_y, sz - pad_x));
    }
    else if (direction == ImGuiDir_Left)
    {
        draw_list->PathLineTo(pos + ImVec2(sz - pad_y, pad_x));
        draw_list->PathLineTo(pos + ImVec2(pad_y, half));
        draw_list->PathLineTo(pos + ImVec2(sz - pad_y, sz - pad_x));
    }

    draw_list->PathStroke(col, 0, thickness);
}

void ImAdd::RenderCheckMark(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz)
{
    //float thickness = ImMax(sz / 5.0f, 1.0f);
    float thickness = 1.5f;

    sz -= thickness * 0.5f;
    pos += ImVec2(thickness * 0.25f, thickness * 0.25f);

    float third = sz / 3.0f;
    float bx = pos.x + third;
    float by = pos.y + sz - third * 0.5f;
    draw_list->PathLineTo(ImVec2(bx - third, by - third));
    draw_list->PathLineTo(ImVec2(bx, by));
    draw_list->PathLineTo(ImVec2(bx + third * 2.0f, by - third * 2.0f));
    draw_list->PathStroke(col, 0, thickness);
}
