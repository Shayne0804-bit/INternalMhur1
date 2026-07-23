#if RUGIR_MENU_AGENCY
    // ============================================================================
    //  AGENCY MENU — Lumin-style shell.
    //  Full re-skin of the Agency config: activation modal, sidebar icon tabs
    //  (Flaticon uicons), pill subtabs, rounded panels, animated selection.
    //  All content renderers (RenderPreview*Page) are reused untouched — only
    //  the shell is new. Compiled for the Agency build only.
    // ============================================================================

    // Lumin palette.
    static const ImVec4 kLmLayout = ImVec4(25.0f / 255.0f, 25.0f / 255.0f, 28.0f / 255.0f, 1.0f);
    static const ImVec4 kLmChild  = ImVec4(28.0f / 255.0f, 28.0f / 255.0f, 33.0f / 255.0f, 1.0f);
    static const ImVec4 kLmWidget = ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 40.0f / 255.0f, 1.0f);
    static const ImVec4 kLmBorder = ImVec4(35.0f / 255.0f, 35.0f / 255.0f, 44.0f / 255.0f, 1.0f);
    static const ImVec4 kLmText   = ImVec4(110.0f / 255.0f, 110.0f / 255.0f, 129.0f / 255.0f, 1.0f);
    static const ImVec4 kLmAccent = ImVec4(176.0f / 255.0f, 180.0f / 255.0f, 255.0f / 255.0f, 1.0f);
    static const ImVec4 kLmWhite  = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    static const ImVec4 kLmBlack  = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    static const ImVec4 kLmDanger = ImVec4(250.0f / 255.0f, 75.0f / 255.0f, 85.0f / 255.0f, 1.0f);

    // Global UI scale — Lumin's own resize method. NOT constexpr: the whole menu
    // is drawn through LM() = value * scale, so changing this factor rescales the
    // ENTIRE layout (fonts, spacing, panels) proportionally. The window's corner
    // drag drives this value, so resizing the window shrinks/grows everything.
    static float g_LmScale = 1.30f;
    static float LM(float v) { return v * g_LmScale; }

    static ImU32 LmCol(const ImVec4& c, float alphaMul = 1.0f)
    {
        return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alphaMul));
    }

    // Frame-rate independent exponential easing (Lumin gui->easing equivalent).
    static void LmEase(float& value, float target, float speed)
    {
        const float dt = ImGui::GetIO().DeltaTime;
        float t = 1.0f - expf(-speed * (dt > 0.1f ? 0.1f : dt));
        value += (target - value) * t;
        if (fabsf(target - value) < 0.0005f)
            value = target;
    }

    // Locks the window's aspect ratio to the Lumin main-shell ratio (527:900)
    // while the user drags the corner, so scaling stays uniform (Lumin's own
    // resize behaviour — the whole UI grows/shrinks as one, never stretches).
    static void LmAspectConstraint(ImGuiSizeCallbackData* data)
    {
        const float ratio = 527.0f / 900.0f;
        data->DesiredSize.y = data->DesiredSize.x * ratio;
    }

    static void LmEase4(ImVec4& v, const ImVec4& t, float speed)
    {
        LmEase(v.x, t.x, speed);
        LmEase(v.y, t.y, speed);
        LmEase(v.z, t.z, speed);
        LmEase(v.w, t.w, speed);
    }

    static void LmUtf8(unsigned int cp, char out[5])
    {
        if (cp <= 0x7F) { out[0] = (char)cp; out[1] = 0; return; }
        if (cp <= 0x7FF)
        {
            out[0] = (char)(0xC0 | (cp >> 6));
            out[1] = (char)(0x80 | (cp & 0x3F));
            out[2] = 0;
            return;
        }
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        out[3] = 0;
    }

    // Icon glyph centered inside [mn, mx].
    static void LmIcon(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx, unsigned int cp, ImU32 col, float size)
    {
        if (!cp)
            return;
        char glyph[5];
        LmUtf8(cp, glyph);
        ImFont* f = g_LuminIconFont ? g_LuminIconFont : (g_FreeFontSmall ? g_FreeFontSmall : ImGui::GetFont());
        ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph);
        dl->AddText(f, size, ImVec2(mn.x + (mx.x - mn.x - ts.x) * 0.5f, mn.y + (mx.y - mn.y - ts.y) * 0.5f), col, glyph);
    }

    static void LmText(ImDrawList* dl, ImFont* f, float size, const ImVec2& pos, ImU32 col, const char* text)
    {
        dl->AddText(f ? f : ImGui::GetFont(), size, pos, col, text);
    }

    static ImVec2 LmTextSize(ImFont* f, float size, const char* text)
    {
        ImFont* rf = f ? f : ImGui::GetFont();
        return rf->CalcTextSizeA(size, FLT_MAX, 0.0f, text ? text : "");
    }

    // Text aligned inside a rect (ax/ay in [0,1]).
    static void LmTextAligned(ImDrawList* dl, ImFont* f, float size, const ImVec2& mn, const ImVec2& mx, ImU32 col, const char* text, float ax, float ay)
    {
        ImVec2 ts = LmTextSize(f, size, text);
        dl->AddText(f ? f : ImGui::GetFont(), size, ImVec2(mn.x + (mx.x - mn.x - ts.x) * ax, mn.y + (mx.y - mn.y - ts.y) * ay), col, text);
    }

    // Lumin brand mark (lamp) + title + subtitle. rect height ~LM(50).
    static void LmBrandHeader(ImDrawList* dl, const ImVec2& pos, float width, const char* title, const char* subtitle)
    {
        const ImVec2 innerMin(pos.x + LM(10), pos.y + LM(8));
        const ImVec2 markMin = innerMin;
        const ImVec2 markMax(markMin.x + LM(34), markMin.y + LM(34));
        (void)width;

        dl->AddRectFilled(markMin, markMax, LmCol(kLmWidget), LM(10));
        dl->AddRectFilled(ImVec2(markMin.x + LM(3), markMin.y + LM(3)), ImVec2(markMax.x - LM(3), markMax.y - LM(3)), LmCol(kLmAccent, 0.12f), LM(8));

        const ImVec2 c((markMin.x + markMax.x) * 0.5f, (markMin.y + markMax.y) * 0.5f);
        dl->AddCircleFilled(c, LM(12), LmCol(kLmAccent, 0.08f), 24);
        dl->AddLine(ImVec2(c.x, c.y - LM(13)), ImVec2(c.x, c.y + LM(12)), LmCol(kLmAccent), LM(2));
        dl->AddCircleFilled(ImVec2(c.x, c.y - LM(3)), LM(5), LmCol(kLmAccent), 24);
        dl->AddCircleFilled(ImVec2(c.x, c.y - LM(3)), LM(2), LmCol(kLmBlack, 0.38f), 24);
        dl->AddLine(ImVec2(c.x, c.y + LM(3)), ImVec2(c.x - LM(8), c.y + LM(13)), LmCol(kLmAccent, 0.85f), LM(1.5f));
        dl->AddLine(ImVec2(c.x, c.y + LM(3)), ImVec2(c.x + LM(8), c.y + LM(13)), LmCol(kLmAccent, 0.85f), LM(1.5f));
        dl->AddRectFilled(ImVec2(markMin.x + LM(6), markMin.y + LM(23)), ImVec2(markMin.x + LM(9), markMin.y + LM(29)), LmCol(kLmWhite, 0.18f), LM(1));
        dl->AddRectFilled(ImVec2(markMin.x + LM(11), markMin.y + LM(20)), ImVec2(markMin.x + LM(14), markMin.y + LM(29)), LmCol(kLmWhite, 0.18f), LM(1));
        dl->AddRectFilled(ImVec2(markMin.x + LM(20), markMin.y + LM(21)), ImVec2(markMin.x + LM(23), markMin.y + LM(29)), LmCol(kLmWhite, 0.18f), LM(1));
        dl->AddRectFilled(ImVec2(markMin.x + LM(25), markMin.y + LM(24)), ImVec2(markMin.x + LM(28), markMin.y + LM(29)), LmCol(kLmWhite, 0.18f), LM(1));

        LmText(dl, g_FreeFontLarge, LM(13), ImVec2(innerMin.x + LM(44), innerMin.y + LM(2)), LmCol(kLmWhite), title);
        LmText(dl, g_FreeFontSmall, LM(11), ImVec2(innerMin.x + LM(44), innerMin.y + LM(18)), LmCol(kLmText), subtitle);
    }

    // Lumin "active" icon: circle + check mark.
    static void LmActiveIcon(ImDrawList* dl, const ImVec2& center, ImU32 col)
    {
        dl->AddCircle(center, LM(7), col, 24, LM(1.3f));
        dl->AddLine(ImVec2(center.x - LM(3), center.y), ImVec2(center.x - LM(1), center.y + LM(3)), col, LM(1.5f));
        dl->AddLine(ImVec2(center.x - LM(1), center.y + LM(3)), ImVec2(center.x + LM(4), center.y - LM(3)), col, LM(1.5f));
    }

    // Primary button (accent pill, black label, optional "active" check icon).
    static bool LmPrimaryButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, bool withActiveIcon, bool disabled = false)
    {
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(id);
        ImGui::InvisibleButton("##lm-primary", size);
        const bool hovered = !disabled && ImGui::IsItemHovered();
        const bool held = !disabled && ImGui::IsItemActive();
        const bool clicked = !disabled && ImGui::IsItemClicked();
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 mx = ImGui::GetItemRectMax();
        ImGui::PopID();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 bg = held ? kLmWhite : kLmAccent;
        if (disabled)
            bg = ImVec4(kLmAccent.x, kLmAccent.y, kLmAccent.z, 0.45f);
        dl->AddRectFilled(mn, mx, LmCol(bg), LM(11));
        if (hovered && !held)
            dl->AddRectFilled(mn, mx, LmCol(kLmWhite, 0.08f), LM(11));

        const ImU32 fg = LmCol(kLmBlack, disabled ? 0.55f : 1.0f);
        ImVec2 ts = LmTextSize(g_FreeFontLarge, LM(12), label);
        const float iconW = withActiveIcon ? LM(14) + LM(7) : 0.0f;
        const float startX = mn.x + ((mx.x - mn.x) - ts.x - iconW) * 0.5f;
        if (withActiveIcon)
            LmActiveIcon(dl, ImVec2(startX + LM(7), (mn.y + mx.y) * 0.5f), fg);
        LmText(dl, g_FreeFontLarge, LM(12), ImVec2(startX + iconW, mn.y + ((mx.y - mn.y) - ts.y) * 0.5f), fg, label);
        return clicked;
    }

    // Flat secondary button (widget background).
    static bool LmFlatButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label)
    {
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(id);
        ImGui::InvisibleButton("##lm-flat", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 mx = ImGui::GetItemRectMax();
        ImGui::PopID();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(mn, mx, LmCol(kLmWidget, hovered ? 1.0f : 0.78f), LM(9));
        dl->AddRect(mn, mx, LmCol(kLmBorder), LM(9));
        LmTextAligned(dl, g_FreeFontSmall, LM(12), mn, mx, LmCol(hovered ? kLmWhite : kLmText), label, 0.5f, 0.5f);
        return clicked;
    }

    // Lumin login loading overlay: 3 pulsing bars over the card.
    static void LmLoginLoading(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx, float alpha, const char* description)
    {
        if (alpha <= 0.001f)
            return;

        static float pulse = 0.0f;
        pulse += ImGui::GetIO().DeltaTime * 7.0f;

        dl->AddRectFilled(mn, mx, LmCol(kLmBlack, 0.68f * alpha), LM(12));

        const ImVec2 center((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
        const float barWidth = LM(8);
        const float barSpacing = LM(18);
        const float baseHeight = LM(20);
        for (int i = 0; i < 3; i++)
        {
            const float wave = (sinf(pulse - i * 0.75f) + 1.0f) * 0.5f;
            const float height = baseHeight + LM(18) * wave;
            const ImVec2 bc(center.x + (i - 1) * barSpacing, center.y - LM(12));
            const ImVec2 bMin(bc.x - barWidth * 0.5f, bc.y - height * 0.5f);
            const ImVec2 bMax(bc.x + barWidth * 0.5f, bc.y + height * 0.5f);
            dl->AddRectFilled(bMin, bMax, LmCol(kLmAccent, alpha), LM(99));
            dl->AddRectFilled(bMin, bMax, LmCol(kLmBlack, 0.22f * (1.0f - wave) * alpha), LM(99));
        }

        LmTextAligned(dl, g_FreeFontSmall, LM(11), mn, ImVec2(mx.x, mx.y - LM(34)), LmCol(kLmWhite, alpha), description, 0.5f, 0.78f);
    }

    // Full Lumin re-theme of the shared ImGui/ImAdd style so every reused
    // content widget (cards, sliders, toggles, buttons, dropdowns) renders in
    // the Lumin design language — not just the shell.
    static void SetupLuminAgencyTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const ImVec4 accentHover  = ImVec4(0.78f, 0.81f, 1.0f, 1.0f);
        const ImVec4 accentActive = ImVec4(0.60f, 0.63f, 0.95f, 1.0f);
        const ImVec4 widgetHover  = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 48.0f / 255.0f, 1.0f);
        const ImVec4 widgetActive = ImVec4(46.0f / 255.0f, 46.0f / 255.0f, 56.0f / 255.0f, 1.0f);

        g_Colors.bgDarkest         = kLmLayout;
        g_Colors.bgDark            = kLmChild;
        g_Colors.bgMedium          = kLmWidget;
        g_Colors.bgLight           = widgetActive;
        g_Colors.accentColor       = kLmAccent;
        g_Colors.accentColorHover  = accentHover;
        g_Colors.accentColorActive = accentActive;
        g_Colors.textPrimary       = kLmWhite;
        g_Colors.textSecondary     = ImVec4(0.62f, 0.62f, 0.72f, 1.0f);
        g_Colors.textDisabled      = kLmText;
        g_Colors.primaryBlue       = accentActive;
        g_Colors.accentCyan        = accentHover;
        g_Colors.accentMagenta     = ImVec4(1.0f, 1.0f, 1.0f, 0.55f);
        g_Colors.accentPurple      = kLmAccent;
        g_Colors.accentAqua        = accentHover;

        style.ChildRounding     = LM(14);
        style.ChildBorderSize   = 0.0f;
        style.FrameRounding     = LM(8);
        style.PopupRounding     = LM(10);
        style.FramePadding      = ImVec2(LM(9), LM(5));
        style.GrabRounding      = LM(99);
        style.ScrollbarSize     = LM(5);
        style.ScrollbarRounding = LM(99);
        style.ChildPadding      = ImVec2(LM(12), LM(9));

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_PopupBg]              = ImVec4(kLmChild.x, kLmChild.y, kLmChild.z, 0.98f);
        colors[ImGuiCol_Border]               = kLmBorder;
        colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_Text]                 = kLmWhite;
        colors[ImGuiCol_TextDisabled]         = kLmText;
        colors[ImGuiCol_CheckMark]            = kLmAccent;
        colors[ImGuiCol_FrameBg]              = kLmWidget;
        colors[ImGuiCol_FrameBgHovered]       = widgetHover;
        colors[ImGuiCol_FrameBgActive]        = widgetActive;
        colors[ImGuiCol_FrameBgShadow]        = ImVec4(0.0f, 0.0f, 0.0f, 0.10f);
        colors[ImGuiCol_Button]               = kLmWidget;
        colors[ImGuiCol_ButtonHovered]        = widgetHover;
        colors[ImGuiCol_ButtonActive]         = ImVec4(kLmAccent.x, kLmAccent.y, kLmAccent.z, 0.85f);
        colors[ImGuiCol_ButtonShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.10f);
        colors[ImGuiCol_SliderGrab]           = kLmAccent;
        colors[ImGuiCol_SliderGrabActive]     = accentHover;
        colors[ImGuiCol_Header]               = kLmWidget;
        colors[ImGuiCol_HeaderHovered]        = widgetHover;
        colors[ImGuiCol_HeaderActive]         = widgetActive;
        colors[ImGuiCol_Separator]            = kLmBorder;
        colors[ImGuiCol_SeparatorHovered]     = accentHover;
        colors[ImGuiCol_SeparatorActive]      = kLmAccent;
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_ScrollbarGrab]        = kLmWidget;
        colors[ImGuiCol_ScrollbarGrabHovered] = widgetHover;
        colors[ImGuiCol_ScrollbarGrabActive]  = widgetActive;
        colors[ImGuiCol_Tab]                  = kLmWidget;
        colors[ImGuiCol_TabHovered]           = widgetHover;
        colors[ImGuiCol_TabActive]            = kLmAccent;
    }

    static void DrawAgencyStyleMenu()
    {
        struct Subtab { const char* label; unsigned int icon; };
        struct Page { const char* label; unsigned int icon; const Subtab* subtabs; int subtabCount; const int* subtabMap; };

        SetupLuminAgencyTheme();

        const Subtab espSubtabs[]      = { { "ESP", 0xF5F8 } };
        const Subtab aimbotSubtabs[]   = { { "Aimbot", 0xFD5F } };
        const Subtab combatSubtabs[]   = { { "Combat", 0xFD32 }, { "Conditions", 0xF8E1 } };
        const Subtab hacksSubtabs[]    = { { "Imitation/Copy", 0xFEA6 }, { "Kota/Items", 0xFE19 } };
        const Subtab settingsSubtabs[] = { { "Profiles", 0xF71C } };
        const Subtab miscSubtabs[]     = { { "Kill All / Players", 0xFD32 } };

        // Character: no "Invincible" subtab. Map Agency index -> original index
        // consumed by RenderPreviewCharacterPage (skips original 4 = Invincible).
        const Subtab characterSubtabs[] = {
            { "Swap", 0xFEA0 }, { "Tools", 0xF8E1 }, { "DEKU OFA OPTI", 0xF309 },
            { "Action Cancel", 0xFB7C }, { "DownPower", 0xF3A2 }, { "NEJIRE OPTI", 0xFF21 }
        };
        const int characterMap[] = { 0, 1, 2, 3, 5, 6 };

        // Lobby: no "License EXP" subtab (indices 0..3 map 1:1 to the original).
        const Subtab lobbySubtabs[] = {
            { "Apply Team", 0xFEA6 }, { "Apply All", 0xFF21 }, { "Specific", 0xFD5F }, { "Change Team", 0xFB7C }
        };

        const Page pages[] = {
            { "ESP",       0xF5F8, espSubtabs,       IM_ARRAYSIZE(espSubtabs),       nullptr },
            { "Character", 0xFEA0, characterSubtabs, IM_ARRAYSIZE(characterSubtabs), characterMap },
            { "Aimbot",    0xFD5F, aimbotSubtabs,    IM_ARRAYSIZE(aimbotSubtabs),    nullptr },
            { "Combat",    0xFD32, combatSubtabs,    IM_ARRAYSIZE(combatSubtabs),    nullptr },
            { "Hacks",     0xF8E1, hacksSubtabs,     IM_ARRAYSIZE(hacksSubtabs),     nullptr },
            { "Lobby",     0xFEA6, lobbySubtabs,     IM_ARRAYSIZE(lobbySubtabs),     nullptr },
            { "Misc",      0xFD32, miscSubtabs,      IM_ARRAYSIZE(miscSubtabs),      nullptr },
            { "Settings",  0xF87D, settingsSubtabs,  IM_ARRAYSIZE(settingsSubtabs),  nullptr },
        };
        const int pageCount = IM_ARRAYSIZE(pages);
        static int selectedSubtabs[8] = {};

        if (g_SelectedTab < 0 || g_SelectedTab >= pageCount)
            g_SelectedTab = 0;

        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();

        // ---- Auth gate / window size transition (Lumin login <-> panel) -----
        if (!Auth::IsAuthorized())
            g_MenuEntered = false;
        const bool loginMode = !Auth::IsAuthorized() || !g_MenuEntered;

        // Base (scale-1.0) reference sizes. The whole UI is drawn in LM(base) =
        // base * g_LmScale, so the window's natural size is base * g_LmScale.
        const ImVec2 loginBase(380.0f, 266.0f);
        const ImVec2 mainBase(900.0f, 527.0f);
        const ImVec2 baseTarget = loginMode ? loginBase : mainBase;
        const ImVec2 targetSize(baseTarget.x * g_LmScale, baseTarget.y * g_LmScale);

        static ImVec2 winSize(0.0f, 0.0f);
        if (winSize.x <= 0.0f)
            winSize = targetSize;

        // Ease the window size for the login<->main transition (and first show).
        LmEase(winSize.x, targetSize.x, 24.0f);
        LmEase(winSize.y, targetSize.y, 24.0f);
        const bool animating =
            fabsf(winSize.x - targetSize.x) > 0.5f || fabsf(winSize.y - targetSize.y) > 0.5f;

        // Resizing is Lumin's OWN mechanism: it does not stretch the window, it
        // drives the global g_LmScale. We only lock the size while animating the
        // login/main transition or on the login card; in the settled main shell we
        // let the user drag the corner, then convert that drag into a new scale.
        const bool userResizable = !animating && !loginMode;

        if (!userResizable)
            ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);
        else
            ImGui::SetNextWindowSize(winSize, ImGuiCond_FirstUseEver);

        // Clamp the drag to a sane scale band (0.70x .. 2.00x of the 900px base)
        // and keep the Lumin aspect ratio, so scaling stays uniform.
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(mainBase.x * 0.70f, mainBase.y * 0.70f),
            ImVec2(mainBase.x * 2.00f, mainBase.y * 2.00f),
            LmAspectConstraint);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGuiWindowFlags winFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBackground;
        // Lock resizing only during the size animation / login card; free otherwise.
        if (!userResizable)
            winFlags |= ImGuiWindowFlags_NoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        bool open = ImGui::Begin("RUGIR INTERNAL##agency", &g_Visible, winFlags);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        // In the settled main shell, translate the live window width into the global
        // Lumin scale, so a corner drag shrinks/grows the ENTIRE UI (fonts, spacing,
        // panels) uniformly next frame. Only in main mode (base width 900).
        if (userResizable)
        {
            const ImVec2 realSize = ImGui::GetWindowSize();
            g_LmScale = std::clamp(realSize.x / mainBase.x, 0.70f, 2.00f);
            winSize = realSize;
        }

        if (!open)
        {
            ImGui::End();
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 windowMax(p.x + winSize.x, p.y + winSize.y);

        // ---- Backdrop (Lumin layout panel) -----------------------------------
        drawList->AddRectFilled(p, windowMax, LmCol(kLmLayout, 0.985f), LM(12));
        drawList->AddRect(p, windowMax, LmCol(kLmBorder), LM(12), 0, 1.0f);

        // ====================================================================
        //  LOGIN MODE — Lumin activation card.
        // ====================================================================
        if (loginMode)
        {
            const Auth::State state = Auth::GetState();
            const bool authorized = Auth::IsAuthorized();
            const bool busy = (state == Auth::State::Checking);

            // Inner card.
            const ImVec2 cardMin(p.x + LM(14), p.y + LM(14));
            const ImVec2 cardMax(windowMax.x - LM(14), windowMax.y - LM(14));
            drawList->AddRectFilled(cardMin, cardMax, LmCol(kLmChild), LM(12));

            // Accent flips to red while the key is being rejected (Lumin shake).
            static float invalidTimer = -1.0f;
            static ImVec4 loginAccent = kLmAccent;
            static Auth::State prevState = Auth::State::Idle;
            if (state == Auth::State::Denied && prevState == Auth::State::Checking)
                invalidTimer = 0.0f;
            prevState = state;
            const bool invalidActive = invalidTimer >= 0.0f && invalidTimer < 0.8f;
            if (invalidTimer >= 0.0f)
            {
                invalidTimer += io.DeltaTime;
                if (invalidTimer >= 0.8f)
                    invalidTimer = -1.0f;
            }
            LmEase4(loginAccent, invalidActive ? kLmDanger : kLmAccent, 18.0f);

            LmBrandHeader(drawList, cardMin, cardMax.x - cardMin.x, "VALARIA", authorized ? "License active" : "License access");

            float y = cardMin.y + LM(50) + LM(14);
            const float fieldX = cardMin.x + LM(12);
            const float fieldW = (cardMax.x - cardMin.x) - LM(24);

            if (!authorized)
            {
                // Pre-fill the saved key once; never auto-connect.
                static bool s_prefilled = false;
                if (!s_prefilled)
                {
                    s_prefilled = true;
                    const std::string saved = Auth::GetSavedKey();
                    if (!saved.empty())
                    {
                        std::memset(g_LicenseKeyBuffer, 0, sizeof(g_LicenseKeyBuffer));
                        const size_t maxN = sizeof(g_LicenseKeyBuffer) - 1;
                        const size_t n = saved.size() < maxN ? saved.size() : maxN;
                        std::memcpy(g_LicenseKeyBuffer, saved.data(), n);
                    }
                }

                // License key field (Lumin text_field look).
                ImGui::SetCursorScreenPos(ImVec2(fieldX, y));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, kLmWidget);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(kLmWidget.x + 0.03f, kLmWidget.y + 0.03f, kLmWidget.z + 0.03f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(kLmWidget.x + 0.03f, kLmWidget.y + 0.03f, kLmWidget.z + 0.03f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(loginAccent.x, loginAccent.y, loginAccent.z, 0.35f));
                ImGui::PushStyleColor(ImGuiCol_Text, kLmWhite);
                ImGui::PushStyleColor(ImGuiCol_TextDisabled, kLmText);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, LM(8));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(LM(10), LM(10)));
                ImGui::PushItemWidth(fieldW);
                ImGui::BeginDisabled(busy);
                ImGui::InputTextWithHint("##LuminLicenseKey", "License key", g_LicenseKeyBuffer, sizeof(g_LicenseKeyBuffer));
                ImGui::EndDisabled();
                ImGui::PopItemWidth();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(6);
                y += LM(36) + LM(8);

                // Status / error row with the little key icon.
                const std::string status = Auth::GetStatusText();
                if (!status.empty() && !busy)
                {
                    const ImVec2 keyCenter(fieldX + LM(8), y + LM(11));
                    const ImU32 keyColor = LmCol(state == Auth::State::Denied ? kLmDanger : kLmAccent);
                    drawList->AddCircle(ImVec2(keyCenter.x - LM(3), keyCenter.y), LM(3), keyColor, 24, LM(1.3f));
                    drawList->AddLine(keyCenter, ImVec2(keyCenter.x + LM(8), keyCenter.y), keyColor, LM(1.3f));
                    drawList->AddLine(ImVec2(keyCenter.x + LM(5), keyCenter.y), ImVec2(keyCenter.x + LM(5), keyCenter.y + LM(3)), keyColor, LM(1.3f));
                    LmText(drawList, g_FreeFontSmall, LM(11), ImVec2(fieldX + LM(22), y + LM(4)), LmCol(kLmText), status.c_str());
                }
                y += LM(24);

                // Activate button with shake on rejection.
                const float shake = invalidActive ? sinf(invalidTimer * 48.0f) * LM(5.0f) * (1.0f - (invalidTimer / 0.8f)) : 0.0f;
                if (!busy && LmPrimaryButton("lm-activate", ImVec2(fieldX + shake, y), ImVec2(fieldW, LM(42)), "Activate", true))
                {
                    if (g_LicenseKeyBuffer[0] != '\0')
                        Auth::ActivateAsync(g_LicenseKeyBuffer);
                    else
                        invalidTimer = 0.0f;
                }
                y += LM(42) + LM(10);

                // HWID line, dim.
                char hwid[96];
                snprintf(hwid, sizeof(hwid), "HWID: %s", Auth::GetHwidShort().c_str());
                LmText(drawList, g_FreeFontSmall, LM(10), ImVec2(fieldX, cardMax.y - LM(20)), LmCol(kLmText, 0.8f), hwid);

                // Verification overlay (3 pulsing bars) while checking.
                static float loadingAlpha = 0.0f;
                LmEase(loadingAlpha, busy ? 1.0f : 0.0f, 16.0f);
                LmLoginLoading(drawList, cardMin, cardMax, loadingAlpha, "Please wait, account verification");
            }
            else
            {
                // Authorized: license summary + CONTINUE / LOGOUT (Lumin styling).
                const std::string tier = Auth::GetTier();
                const std::string expires = Auth::GetExpiresAt();

                std::string line1 = "License active";
                if (!tier.empty())
                    line1 += " - " + tier;
                LmText(drawList, g_FreeFontLarge, LM(13), ImVec2(fieldX, y), LmCol(kLmWhite), line1.c_str());
                y += LM(20);
                std::string line2 = expires.empty() ? "Expiration: unlimited" : ("Expires: " + expires);
                LmText(drawList, g_FreeFontSmall, LM(11), ImVec2(fieldX, y), LmCol(kLmText), line2.c_str());
                y += LM(18);
                char hwid[96];
                snprintf(hwid, sizeof(hwid), "HWID: %s", Auth::GetHwidShort().c_str());
                LmText(drawList, g_FreeFontSmall, LM(10), ImVec2(fieldX, y), LmCol(kLmText, 0.8f), hwid);
                y += LM(24);

                if (LmPrimaryButton("lm-continue", ImVec2(fieldX, y), ImVec2(fieldW, LM(42)), "Continue", true))
                    g_MenuEntered = true;
                y += LM(42) + LM(8);
                if (LmFlatButton("lm-logout", ImVec2(fieldX, y), ImVec2(fieldW, LM(30)), "Log out"))
                {
                    Auth::Clear();
                    g_MenuEntered = false;
                    std::memset(g_LicenseKeyBuffer, 0, sizeof(g_LicenseKeyBuffer));
                }
            }

            ImGui::End();
            return;
        }

        g_LicenseSectionUnlocked = true;

        // ====================================================================
        //  MAIN SHELL — sidebar icon tabs + header pill subtabs + content.
        // ====================================================================
        const float pad = LM(10);
        const float sidebarW = LM(162.5f);
        const float topH = LM(50);
        const float headerH = LM(40);
        const float rowGap = LM(2);

        // Sidebar glass panel (full height).
        const ImVec2 sideMin(p.x + pad, p.y + pad);
        const ImVec2 sideMax(p.x + pad + sidebarW, windowMax.y - pad);
        drawList->AddRectFilled(sideMin, sideMax, LmCol(kLmChild), LM(14));

        LmBrandHeader(drawList, sideMin, sidebarW, "VALARIA", "Agency panel");

        // ---- Sidebar tabs ----------------------------------------------------
        static ImVec4 sideSelRect(0, 0, 0, 0);
        static float tabAlphas[8] = {};
        static ImVec4 tabIconCols[8];
        static bool tabAnimInit = false;
        if (!tabAnimInit)
        {
            tabAnimInit = true;
            for (int i = 0; i < 7; i++)
            {
                tabAlphas[i] = 0.58f;
                tabIconCols[i] = kLmText;
            }
        }

        const float tabX = sideMin.x + LM(10);
        const float tabW = sidebarW - LM(20);
        const float tabH = LM(32);
        float tabY = sideMin.y + topH + LM(10);

        // Animated selection pill behind the tabs.
        ImVec4 sideTarget = sideSelRect;
        ImVec2 tabRects[8][2];
        for (int i = 0; i < pageCount; i++)
        {
            tabRects[i][0] = ImVec2(tabX, tabY);
            tabRects[i][1] = ImVec2(tabX + tabW, tabY + tabH);
            if (g_SelectedTab == i)
                sideTarget = ImVec4(tabX, tabY, tabX + tabW, tabY + tabH);
            tabY += tabH + LM(4);
        }
        if (sideSelRect.z <= sideSelRect.x + 1.0f)
            sideSelRect = sideTarget;
        LmEase4(sideSelRect, sideTarget, 18.0f);

        if (sideSelRect.z > sideSelRect.x + 1.0f)
        {
            drawList->AddRectFilled(ImVec2(sideSelRect.x, sideSelRect.y), ImVec2(sideSelRect.z, sideSelRect.w), LmCol(kLmWidget), LM(9.1f));
            // Accent indicator bar.
            const float barX = sideSelRect.x + LM(2);
            drawList->AddRectFilled(ImVec2(barX, sideSelRect.y + LM(6)), ImVec2(barX + LM(2), sideSelRect.w - LM(6)), LmCol(kLmAccent), LM(1));
        }

        for (int i = 0; i < pageCount; i++)
        {
            const ImVec2 mn = tabRects[i][0];
            const ImVec2 mx = tabRects[i][1];
            ImGui::SetCursorScreenPos(mn);
            ImGui::PushID(pages[i].label);
            ImGui::InvisibleButton("##lm-tab", ImVec2(mx.x - mn.x, mx.y - mn.y));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked())
                g_SelectedTab = i;
            ImGui::PopID();

            const bool selected = g_SelectedTab == i;
            LmEase(tabAlphas[i], (selected || hovered) ? 1.0f : 0.58f, 18.0f);
            LmEase4(tabIconCols[i], (selected || hovered) ? kLmWhite : kLmText, 18.0f);

            ImVec4 iconCol = tabIconCols[i];
            iconCol.w *= tabAlphas[i];
            LmIcon(drawList, mn, ImVec2(mn.x + LM(32), mn.y + tabH), pages[i].icon, LmCol(iconCol), LM(12.5f));
            LmTextAligned(drawList, g_FreeFontLarge, LM(12), ImVec2(mn.x + LM(32), mn.y), ImVec2(mx.x - LM(11), mx.y), LmCol(kLmWhite, tabAlphas[i]), pages[i].label, 0.0f, 0.5f);
        }

        // ---- Header row (pill subtabs + status/close) -------------------------
        const float featX = p.x + pad + sidebarW + pad;
        const float featW = windowMax.x - featX - pad;
        const ImVec2 hdrMin(featX, p.y + pad);
        const ImVec2 hdrMax(featX + featW, p.y + pad + headerH);
        drawList->AddRectFilled(hdrMin, hdrMax, LmCol(kLmChild), LM(14));

        const Page& page = pages[g_SelectedTab];
        if (selectedSubtabs[g_SelectedTab] < 0 || selectedSubtabs[g_SelectedTab] >= page.subtabCount)
            selectedSubtabs[g_SelectedTab] = 0;

        static ImVec4 pillSelRect(0, 0, 0, 0);
        static int pillPrevTab = -1;
        ImVec4 pillTarget = pillSelRect;

        float pillX = hdrMin.x + LM(8);
        const float pillY = hdrMin.y + LM(4);
        const float pillH = LM(32);
        for (int i = 0; i < page.subtabCount; i++)
        {
            const float textW = LmTextSize(g_FreeFontSmall, LM(11), page.subtabs[i].label).x;
            const float pillW = LM(42) + textW;
            const ImVec2 mn(pillX, pillY);
            const ImVec2 mx(pillX + pillW, pillY + pillH);

            if (selectedSubtabs[g_SelectedTab] == i)
                pillTarget = ImVec4(mn.x, mn.y, mx.x, mx.y);
            pillX += pillW + LM(8);
        }
        if (pillPrevTab != g_SelectedTab || pillSelRect.z <= pillSelRect.x + 1.0f)
        {
            pillSelRect = pillTarget;
            pillPrevTab = g_SelectedTab;
        }
        LmEase4(pillSelRect, pillTarget, 18.0f);
        if (pillSelRect.z > pillSelRect.x + 1.0f)
            drawList->AddRectFilled(ImVec2(pillSelRect.x, pillSelRect.y), ImVec2(pillSelRect.z, pillSelRect.w), LmCol(kLmWidget), LM(9.1f));

        pillX = hdrMin.x + LM(8);
        for (int i = 0; i < page.subtabCount; i++)
        {
            const float textW = LmTextSize(g_FreeFontSmall, LM(11), page.subtabs[i].label).x;
            const float pillW = LM(42) + textW;
            const ImVec2 mn(pillX, pillY);
            const ImVec2 mx(pillX + pillW, pillY + pillH);

            ImGui::SetCursorScreenPos(mn);
            ImGui::PushID(page.subtabs[i].label);
            ImGui::PushID(g_SelectedTab);
            ImGui::InvisibleButton("##lm-pill", ImVec2(pillW, pillH));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked())
                selectedSubtabs[g_SelectedTab] = i;
            ImGui::PopID();
            ImGui::PopID();

            const bool selected = selectedSubtabs[g_SelectedTab] == i;
            const float a = (selected || hovered) ? 1.0f : 0.58f;
            LmIcon(drawList, mn, ImVec2(mn.x + LM(32), mx.y), page.subtabs[i].icon, LmCol((selected || hovered) ? kLmWhite : kLmText, a), LM(12));
            LmTextAligned(drawList, g_FreeFontSmall, LM(11), ImVec2(mn.x + LM(32), mn.y), ImVec2(mx.x - LM(10), mx.y), LmCol(kLmWhite, a), page.subtabs[i].label, 0.0f, 0.5f);

            pillX += pillW + LM(8);
        }

        // Right side of the header: version + FPS + close.
        {
            char verText[48] = {};
            snprintf(verText, sizeof(verText), "v%s", SelfUpdate::ClientVersion());
            char fpsText[32] = {};
            snprintf(fpsText, sizeof(fpsText), "%.0f FPS", io.Framerate);

            const float closeSize = LM(16);
            const ImVec2 closeMin(hdrMax.x - LM(14) - closeSize, hdrMin.y + (headerH - closeSize) * 0.5f);
            const ImVec2 closeMax(closeMin.x + closeSize, closeMin.y + closeSize);
            ImGui::SetCursorScreenPos(closeMin);
            ImGui::InvisibleButton("##lm-close", ImVec2(closeSize, closeSize));
            const bool closeHovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked())
                g_Visible = false;
            const ImU32 xCol = LmCol(closeHovered ? kLmWhite : kLmText);
            drawList->AddLine(ImVec2(closeMin.x + LM(3), closeMin.y + LM(3)), ImVec2(closeMax.x - LM(3), closeMax.y - LM(3)), xCol, LM(1.6f));
            drawList->AddLine(ImVec2(closeMax.x - LM(3), closeMin.y + LM(3)), ImVec2(closeMin.x + LM(3), closeMax.y - LM(3)), xCol, LM(1.6f));

            const ImVec2 fpsSize = LmTextSize(g_FreeFontSmall, LM(11), fpsText);
            LmText(drawList, g_FreeFontSmall, LM(11), ImVec2(closeMin.x - LM(12) - fpsSize.x, hdrMin.y + (headerH - fpsSize.y) * 0.5f), LmCol(kLmText), fpsText);
            const ImVec2 verSize = LmTextSize(g_FreeFontSmall, LM(11), verText);
            LmText(drawList, g_FreeFontSmall, LM(11), ImVec2(closeMin.x - LM(12) - fpsSize.x - LM(12) - verSize.x, hdrMin.y + (headerH - verSize.y) * 0.5f), LmCol(kLmText, 0.8f), verText);
        }

        // ---- Content ----------------------------------------------------------
        const float contentTop = p.y + pad + topH + rowGap;
        const ImVec2 contentSize(featW, windowMax.y - contentTop - pad);
        char pageId[40] = {};
        snprintf(pageId, sizeof(pageId), "agency-page:%d:%d", g_SelectedTab, selectedSubtabs[g_SelectedTab]);

        ImGui::SetCursorScreenPos(ImVec2(featX, contentTop));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(LM(10), LM(10)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(LM(10), LM(8)));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
        if (ImGui::BeginChild(pageId, contentSize, false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            float groupWidth = std::floor((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) / 2.0f);
            int subtab = selectedSubtabs[g_SelectedTab];
            switch (g_SelectedTab)
            {
            case 0: RenderPreviewEspPage(subtab, groupWidth); break;
            case 1: RenderPreviewCharacterPage(page.subtabMap ? page.subtabMap[subtab] : subtab, groupWidth); break;
            case 2: RenderPreviewAimbotPage(subtab, groupWidth); break;
            case 3: RenderPreviewCombatPage(subtab, groupWidth); break;
            case 4: RenderPreviewHacksPage(subtab, groupWidth); break;
            case 5: RenderPreviewLobbyPage(subtab, groupWidth); break;
            case 6: RenderPreviewMiscPage(subtab, groupWidth); break;
            case 7: RenderPreviewSettingsPage(groupWidth); break;
            default: break;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImGui::End();
    }

    // ============================================================================
    //  SELF-UPDATE OVERLAY — Lumin re-skin (Agency only). Same SelfUpdate state
    //  machine as DrawSelfUpdateOverlay; only the visuals differ.
    // ============================================================================
    static void DrawSelfUpdateOverlayLumin()
    {
        if (!SelfUpdate::HasPendingUI())
            return;

        SelfUpdate::Progress prog = SelfUpdate::GetProgress();
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* fg = ImGui::GetForegroundDrawList();

        auto largeFont = g_FreeFontLarge ? g_FreeFontLarge : ImGui::GetFont();
        auto smallFont = g_FreeFontSmall ? g_FreeFontSmall : ImGui::GetFont();

        // Lumin slider-style progress bar (rounded track + accent fill + knob).
        auto progressBar = [&](const ImVec2& mn, const ImVec2& mx, float frac, bool indeterminate)
        {
            const float rounding = LM(99);
            fg->AddRectFilled(mn, mx, LmCol(kLmWidget), rounding);
            if (indeterminate)
            {
                float t = (float)ImGui::GetTime();
                float w = (mx.x - mn.x) * 0.28f;
                float x0 = mn.x + ((mx.x - mn.x) + w) * (0.5f + 0.5f * sinf(t * 2.0f)) - w;
                if (x0 < mn.x) x0 = mn.x;
                float x1 = x0 + w;
                if (x1 > mx.x) x1 = mx.x;
                fg->AddRectFilled(ImVec2(x0, mn.y), ImVec2(x1, mx.y), LmCol(kLmAccent), rounding);
                return;
            }
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
            float x = (mx.x - mn.x) * frac;
            if (x < LM(12)) x = LM(12);
            fg->AddRectFilled(mn, ImVec2(mn.x + x, mx.y), LmCol(kLmAccent), rounding);
            fg->AddCircleFilled(ImVec2(mn.x + x - LM(6), (mn.y + mx.y) * 0.5f), LM(4), LmCol(kLmBlack), 24);
        };

        auto fgButton = [&](const ImVec2& mn, const ImVec2& mx, const char* label, bool primary) -> bool
        {
            const ImVec2 mouse = io.MousePos;
            const bool hovered = mouse.x >= mn.x && mouse.x <= mx.x && mouse.y >= mn.y && mouse.y <= mx.y;
            const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            if (primary)
            {
                fg->AddRectFilled(mn, mx, LmCol(hovered ? kLmWhite : kLmAccent), LM(11));
                LmTextAligned(fg, largeFont, LM(12), mn, mx, LmCol(kLmBlack), label, 0.5f, 0.5f);
            }
            else
            {
                fg->AddRectFilled(mn, mx, LmCol(kLmWidget, hovered ? 1.0f : 0.78f), LM(9));
                fg->AddRect(mn, mx, LmCol(kLmBorder), LM(9));
                LmTextAligned(fg, smallFont, LM(12), mn, mx, LmCol(hovered ? kLmWhite : kLmText), label, 0.5f, 0.5f);
            }
            return clicked;
        };

        // ---- AUTO states: top-center toast, no buttons, no dim ----------------
        if (prog.state == SelfUpdate::State::AutoUpdating ||
            prog.state == SelfUpdate::State::WaitingCompatible)
        {
            const float tW = LM(360);
            const float tH = LM(84);
            const ImVec2 tMin(io.DisplaySize.x * 0.5f - tW * 0.5f, LM(24));
            const ImVec2 tMax(tMin.x + tW, tMin.y + tH);

            fg->AddRectFilled(tMin, tMax, LmCol(kLmChild, 0.98f), LM(14));
            fg->AddRect(tMin, tMax, LmCol(kLmBorder), LM(14));

            const float px = tMin.x + LM(14);
            const float pxR = tMax.x - LM(14);
            float ty = tMin.y + LM(10);

            LmText(fg, smallFont, LM(10), ImVec2(px, ty), LmCol(kLmAccent), "VALARIA");
            const ImVec2 tagSz = LmTextSize(smallFont, LM(10), "VALARIA");
            LmText(fg, smallFont, LM(10), ImVec2(px + tagSz.x + LM(7), ty), LmCol(kLmText), "AUTO-UPDATE");
            ty += LM(17);

            if (prog.state == SelfUpdate::State::AutoUpdating)
            {
                LmText(fg, smallFont, LM(11), ImVec2(px, ty), LmCol(kLmWhite), "Game update detected - installing...");
                ty += LM(17);

                const float barH = LM(10);
                progressBar(ImVec2(px, ty), ImVec2(pxR, ty + barH), (float)prog.fraction, prog.bytesTotal == 0);
                ty += barH + LM(7);

                std::string bytes = prog.bytesTotal
                    ? SelfUpdate_HumanBytes((double)prog.bytesReceived) + " / " + SelfUpdate_HumanBytes((double)prog.bytesTotal)
                    : SelfUpdate_HumanBytes((double)prog.bytesReceived) + " received";
                LmText(fg, smallFont, LM(10), ImVec2(px, ty), LmCol(kLmText), bytes.c_str());

                std::string spd = SelfUpdate_HumanBytes(prog.speedBytesPerSec) + "/s";
                const ImVec2 ss = LmTextSize(smallFont, LM(10), spd.c_str());
                LmText(fg, smallFont, LM(10), ImVec2(pxR - ss.x, ty), LmCol(kLmAccent), spd.c_str());
            }
            else // WaitingCompatible
            {
                LmText(fg, smallFont, LM(11), ImVec2(px, ty), LmCol(kLmWhite), "Game updated - cheat update pending...");
                ty += LM(19);

                float t = (float)ImGui::GetTime();
                const ImVec2 sc(px + LM(7), ty + LM(7));
                const float r = LM(6);
                const int seg = 12;
                for (int i = 0; i < seg; ++i)
                {
                    float a0 = (float)i / seg * 6.2831853f + t * 3.0f;
                    float alpha = (float)i / seg;
                    ImVec2 p0(sc.x + cosf(a0) * r, sc.y + sinf(a0) * r);
                    ImVec2 p1(sc.x + cosf(a0) * (r - LM(3)), sc.y + sinf(a0) * (r - LM(3)));
                    fg->AddLine(p0, p1, LmCol(kLmAccent, alpha), LM(1.6f));
                }
                LmText(fg, smallFont, LM(10), ImVec2(px + LM(22), ty + LM(1)), LmCol(kLmText), "Retrying in background...");
            }
            return;
        }

        // ---- Modal states: dim + centered Lumin card --------------------------
        const float cardW = LM(380);
        const float cardH = LM(212);
        const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        const ImVec2 cardMin(c.x - cardW * 0.5f, c.y - cardH * 0.5f);
        const ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);

        fg->AddRectFilled(ImVec2(0, 0), io.DisplaySize, LmCol(kLmBlack, 0.68f));
        fg->AddRectFilled(cardMin, cardMax, LmCol(kLmChild), LM(16));
        fg->AddRect(cardMin, cardMax, LmCol(kLmBorder), LM(16));

        // Mini brand row.
        LmText(fg, largeFont, LM(13), ImVec2(cardMin.x + LM(16), cardMin.y + LM(12)), LmCol(kLmAccent), "VALARIA");
        const ImVec2 brandSz = LmTextSize(largeFont, LM(13), "VALARIA");
        LmText(fg, smallFont, LM(10), ImVec2(cardMin.x + LM(16) + brandSz.x + LM(8), cardMin.y + LM(15)), LmCol(kLmText), "UPDATE");
        fg->AddLine(ImVec2(cardMin.x + LM(16), cardMin.y + LM(34)), ImVec2(cardMax.x - LM(16), cardMin.y + LM(34)), LmCol(kLmBorder), 1.0f);

        const float padX = LM(16);
        const float contentX = cardMin.x + padX;
        const float contentW = cardW - padX * 2.0f;
        float y = cardMin.y + LM(46);

        auto centeredText = [&](ImFont* f, float sz, float yy, ImU32 col, const char* txt)
        {
            LmTextAligned(fg, f, sz, ImVec2(cardMin.x, yy), ImVec2(cardMax.x, yy), col, txt, 0.5f, 0.0f);
        };

        const float btnH = LM(34);
        const float btnY = cardMax.y - btnH - LM(14);

        switch (prog.state)
        {
        case SelfUpdate::State::Available:
        {
            centeredText(largeFont, LM(14), y, LmCol(kLmWhite), "A new update is available");
            y += LM(24);
            std::string sub = "Version " + prog.latestVersion + " is online. Download it?";
            centeredText(smallFont, LM(11), y, LmCol(kLmText), sub.c_str());
            if (!prog.notes.empty())
            {
                y += LM(16);
                std::string nl = prog.notes; size_t p = 0, f;
                while ((f = nl.find('\n', p)) != std::string::npos) {
                    centeredText(smallFont, LM(10), y, LmCol(kLmAccent), nl.substr(p, f - p).c_str());
                    y += LM(14); p = f + 1;
                }
                centeredText(smallFont, LM(10), y, LmCol(kLmAccent), nl.substr(p).c_str());
            }

            const float gap = LM(10);
            const float bw = (contentW - gap) * 0.5f;
            if (fgButton(ImVec2(contentX, btnY), ImVec2(contentX + bw, btnY + btnH), "Download", true))
                SelfUpdate::AcceptDownload();
            if (fgButton(ImVec2(contentX + bw + gap, btnY), ImVec2(cardMax.x - padX, btnY + btnH), "Later", false))
                SelfUpdate::DeclineDownload();
            break;
        }
        case SelfUpdate::State::Downloading:
        {
            centeredText(largeFont, LM(14), y, LmCol(kLmWhite), "Downloading");
            y += LM(30);

            const float barH = LM(12);
            progressBar(ImVec2(contentX, y), ImVec2(contentX + contentW, y + barH), (float)prog.fraction, prog.bytesTotal == 0);
            y += barH + LM(12);

            if (prog.bytesTotal)
            {
                char pct[16];
                float frac = (float)prog.fraction;
                if (frac < 0.0f) frac = 0.0f;
                if (frac > 1.0f) frac = 1.0f;
                snprintf(pct, sizeof(pct), "%.0f%%", frac * 100.0f);
                centeredText(smallFont, LM(11), y, LmCol(kLmWhite), pct);
                y += LM(18);
            }

            std::string bytes = prog.bytesTotal
                ? SelfUpdate_HumanBytes((double)prog.bytesReceived) + " / " + SelfUpdate_HumanBytes((double)prog.bytesTotal)
                : SelfUpdate_HumanBytes((double)prog.bytesReceived) + " received";
            LmText(fg, smallFont, LM(11), ImVec2(contentX, y), LmCol(kLmText), bytes.c_str());

            std::string spd = SelfUpdate_HumanBytes(prog.speedBytesPerSec) + "/s";
            const ImVec2 sp = LmTextSize(smallFont, LM(11), spd.c_str());
            LmText(fg, smallFont, LM(11), ImVec2(contentX + contentW - sp.x, y), LmCol(kLmAccent), spd.c_str());
            break;
        }
        case SelfUpdate::State::Ready:
        {
            centeredText(largeFont, LM(14), y, LmCol(kLmWhite), "Download complete");
            y += LM(24);
            centeredText(smallFont, LM(11), y, LmCol(kLmText), "Click apply to install the update.");
            y += LM(17);
            centeredText(smallFont, LM(10), y, LmCol(kLmText, 0.8f), "The cheat will reload automatically.");

            if (fgButton(ImVec2(contentX + contentW * 0.25f, btnY), ImVec2(contentX + contentW * 0.75f, btnY + btnH), "Apply update", true))
                SelfUpdate::ApplyUpdate();
            break;
        }
        case SelfUpdate::State::Applying:
        {
            centeredText(largeFont, LM(14), y, LmCol(kLmWhite), "Applying update");
            y += LM(26);
            int dots = (int)(ImGui::GetTime() * 2.0f) % 4;
            std::string s = "Reloading module";
            for (int i = 0; i < dots; ++i) s += ".";
            centeredText(smallFont, LM(11), y, LmCol(kLmText), s.c_str());
            y += LM(18);
            centeredText(smallFont, LM(10), y, LmCol(kLmText, 0.8f), "Do not close the game.");

            // Lumin loading bars under the text.
            static float pulse = 0.0f;
            pulse += io.DeltaTime * 7.0f;
            const ImVec2 center(c.x, btnY + btnH * 0.5f - LM(4));
            for (int i = 0; i < 3; i++)
            {
                const float wave = (sinf(pulse - i * 0.75f) + 1.0f) * 0.5f;
                const float height = LM(14) + LM(12) * wave;
                const ImVec2 bc(center.x + (i - 1) * LM(15), center.y);
                fg->AddRectFilled(ImVec2(bc.x - LM(3.5f), bc.y - height * 0.5f), ImVec2(bc.x + LM(3.5f), bc.y + height * 0.5f), LmCol(kLmAccent), LM(99));
            }
            break;
        }
        case SelfUpdate::State::Error:
        {
            centeredText(largeFont, LM(14), y, LmCol(kLmDanger), "Update error");
            y += LM(24);
            std::string msg = prog.errorText.empty() ? "Unknown error." : prog.errorText;
            centeredText(smallFont, LM(10), y, LmCol(kLmText), msg.c_str());

            if (fgButton(ImVec2(contentX + contentW * 0.30f, btnY), ImVec2(contentX + contentW * 0.70f, btnY + btnH), "Close", false))
                SelfUpdate::DismissError();
            break;
        }
        default:
            break;
        }

        // Swallow clicks landing on the dim backdrop so they don't reach the game.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            io.MouseClicked[ImGuiMouseButton_Left] = false;
    }

    // ============================================================================
    //  LUMIN NOTIFICATIONS — top-right toast stack (c_notify port).
    // ============================================================================
    struct LmNotifyState
    {
        std::string title;
        std::string text;
        LmNotifyType type = LmNotifySuccess;
        float timer = 0.0f;
        float alpha = 0.0f;
        float slide = 28.0f;
        float pos = 0.0f;
        bool active = true;
    };

    static std::vector<LmNotifyState> g_LmNotifications;
    static constexpr float kLmNotifyLifetime = 4.0f;

    void LmAddNotify(std::string title, std::string text, LmNotifyType type)
    {
        // Same dedup rule as Lumin: swallow immediate duplicates.
        if (!g_LmNotifications.empty())
        {
            LmNotifyState& last = g_LmNotifications.back();
            if (last.title == title && last.text == text && last.type == type && last.timer < 0.22f)
                return;
        }

        LmNotifyState n;
        n.title = std::move(title);
        n.text = std::move(text);
        n.type = type;
        g_LmNotifications.push_back(std::move(n));
    }

    static ImVec4 LmNotifyColor(LmNotifyType type)
    {
        if (type == LmNotifyError)
            return ImVec4(245.0f / 255.0f, 70.0f / 255.0f, 86.0f / 255.0f, 1.0f);
        return type == LmNotifyWarning ? kLmText : kLmAccent;
    }

    static const char* LmNotifyBadgeText(LmNotifyType type)
    {
        if (type == LmNotifyWarning)
            return "DISABLED";
        if (type == LmNotifyError)
            return "ERROR";
        return "ENABLED";
    }

    void LmRenderNotifies()
    {
        if (g_LmNotifications.empty())
            return;

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const float ns = 0.81f;  // Lumin notify_visual_scale
        auto NS = [&](float v) { return LM(v * ns); };

        const ImVec2 size(NS(288), NS(76));
        const ImVec2 padding(NS(16), NS(16));
        float accumulated = padding.y;

        for (size_t i = 0; i < g_LmNotifications.size();)
        {
            LmNotifyState& n = g_LmNotifications[i];
            if (n.active)
                n.timer += io.DeltaTime;
            if (n.timer >= kLmNotifyLifetime)
                n.active = false;

            LmEase(n.alpha, n.active ? 1.0f : 0.0f, 14.0f);
            LmEase(n.slide, n.active ? 0.0f : 28.0f, 18.0f);
            if (n.pos <= 0.0f)
                n.pos = accumulated;
            LmEase(n.pos, accumulated, 14.0f);

            if (!n.active && n.alpha <= 0.015f)
            {
                g_LmNotifications.erase(g_LmNotifications.begin() + i);
                continue;
            }

            if (n.alpha > 0.01f)
            {
                const float alpha = n.alpha;
                const ImVec4 status = LmNotifyColor(n.type);
                const bool disabled = n.type == LmNotifyWarning;
                const ImVec2 mn(io.DisplaySize.x - padding.x - size.x + NS(n.slide), n.pos);
                const ImVec2 mx(mn.x + size.x, mn.y + size.y);
                const float rounding = NS(12);
                const float progress = 1.0f - (n.timer / kLmNotifyLifetime);

                fg->AddRectFilled(mn, mx, LmCol(kLmChild, 0.98f * alpha), rounding);
                fg->AddRect(mn, mx, LmCol(kLmBorder, 0.96f * alpha), rounding);

                // Status badge (rounded square + check / minus mark).
                const ImVec2 bMin(mn.x + NS(13), mn.y + NS(15));
                const ImVec2 bMax(mn.x + NS(49), mn.y + NS(51));
                fg->AddRectFilled(bMin, bMax, LmCol(kLmWidget, 0.94f * alpha), NS(9));
                fg->AddRect(bMin, bMax, LmCol(status, (disabled ? 0.30f : 0.44f) * alpha), NS(9));
                const ImVec2 mc((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f - NS(0.5f));
                if (disabled)
                {
                    fg->AddRectFilled(ImVec2(mc.x - NS(5.8f), mc.y - NS(1.15f)), ImVec2(mc.x + NS(5.8f), mc.y + NS(1.15f)),
                        LmCol(status, 0.95f * alpha), NS(2));
                }
                else
                {
                    fg->AddLine(ImVec2(mc.x - NS(5.4f), mc.y - NS(0.6f)), ImVec2(mc.x - NS(1.6f), mc.y + NS(3.0f)), LmCol(status, alpha), NS(2));
                    fg->AddLine(ImVec2(mc.x - NS(1.6f), mc.y + NS(3.0f)), ImVec2(mc.x + NS(6.0f), mc.y - NS(5.0f)), LmCol(status, alpha), NS(2));
                }

                // Status pill (top right).
                const ImVec2 pillMin(mx.x - NS(84), mn.y + NS(16));
                const ImVec2 pillMax(mx.x - NS(13), mn.y + NS(34));
                fg->AddRectFilled(pillMin, pillMax, LmCol(status, (disabled ? 0.075f : 0.12f) * alpha), NS(99));
                fg->AddRect(pillMin, pillMax, LmCol(status, (disabled ? 0.22f : 0.36f) * alpha), NS(99));
                LmTextAligned(fg, g_FreeFontSmall, NS(8.5f), pillMin, pillMax, LmCol(status, alpha), LmNotifyBadgeText(n.type), 0.5f, 0.5f);

                // Title + text.
                LmText(fg, g_FreeFontLarge, NS(12), ImVec2(mn.x + NS(60), mn.y + NS(14)), LmCol(kLmWhite, alpha), n.title.c_str());
                LmText(fg, g_FreeFontSmall, NS(10), ImVec2(mn.x + NS(60), mn.y + NS(33)), LmCol(kLmText, 0.96f * alpha), n.text.c_str());
                fg->AddLine(ImVec2(mn.x + NS(60), mn.y + NS(54)), ImVec2(mx.x - NS(15), mn.y + NS(54)), LmCol(kLmBorder, 0.55f * alpha), 1.0f);

                // Remaining-lifetime track.
                const ImVec2 tMin(mn.x + NS(13), mn.y + NS(64));
                const ImVec2 tMax(mx.x - NS(13), mx.y - NS(8));
                fg->AddRectFilled(tMin, tMax, LmCol(kLmWidget, 0.78f * alpha), NS(99));
                fg->AddRectFilled(tMin, ImVec2(tMin.x + (tMax.x - tMin.x) * (progress < 0.0f ? 0.0f : progress), tMax.y),
                    LmCol(status, (disabled ? 0.40f : 0.88f) * alpha), NS(99));

                accumulated += size.y + NS(10);
            }

            ++i;
        }
    }

    // ============================================================================
    //  LUMIN WIDGET BODIES — intercepted ImAdd/ImGuiHelper implementations.
    // ============================================================================
    static std::string LmDisplayLabel(const char* label)
    {
        const char* cut = strstr(label, "##");
        return cut ? std::string(label, cut) : std::string(label);
    }

    // Lumin switch row: label left, animated pill switch right, hairline below.
    static bool LmSwitchRowImpl(const char* label, bool* v)
    {
        const std::string display = LmDisplayLabel(label);

        float rowW = ImGui::GetContentRegionAvail().x;
        if (rowW < LM(70))
            rowW = LM(70);
        const float rowH = LM(26);
        ImGui::InvisibleButton(label, ImVec2(rowW, rowH));
        const bool hovered = ImGui::IsItemHovered();
        bool changed = false;
        if (ImGui::IsItemClicked())
        {
            *v = !*v;
            changed = true;
            LmAddNotify(*v ? "Function enabled" : "Function disabled", display, *v ? LmNotifySuccess : LmNotifyWarning);
        }

        static std::unordered_map<ImGuiID, float> s_anim;
        float& t = s_anim[ImGui::GetItemID()];
        if (changed && s_anim.size() > 512)
            s_anim.clear();
        if (t < 0.0f || t > 1.0f)
            t = *v ? 1.0f : 0.0f;
        LmEase(t, *v ? 1.0f : 0.0f, 20.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        const float cy = (mn.y + mx.y) * 0.5f;

        const ImVec2 swSize(LM(33), LM(18));
        const ImVec2 swMin(mx.x - swSize.x, cy - swSize.y * 0.5f);
        const ImVec2 swMax(mx.x, cy + swSize.y * 0.5f);

        LmTextAligned(dl, g_FreeFontLarge, LM(11.5f), ImVec2(mn.x, mn.y), ImVec2(swMin.x - LM(10), mx.y),
            LmCol((*v || hovered) ? kLmWhite : kLmText), display.c_str(), 0.0f, 0.5f);

        const ImVec4 trackOn(38.0f / 255.0f, 37.0f / 255.0f, 50.0f / 255.0f, 1.0f);
        dl->AddRectFilled(swMin, swMax, LmCol(*v ? trackOn : kLmWidget), LM(99));

        // Small state marker on the opposite side of the knob.
        const float markerX = swMax.x - LM(8.5f) + ((swMin.x + LM(8.5f)) - (swMax.x - LM(8.5f))) * t;
        dl->AddRectFilled(ImVec2(markerX - LM(1.2f), cy - LM(3.8f)), ImVec2(markerX + LM(1.2f), cy + LM(3.8f)),
            LmCol(*v ? kLmAccent : ImVec4(132.0f / 255.0f, 113.0f / 255.0f, 198.0f / 255.0f, 1.0f), *v ? 0.92f : 0.58f), LM(2));

        // Knob (accent when on) with dark inner dot — Lumin's signature look.
        const float knobX = swMin.x + LM(9) + ((swMax.x - LM(9)) - (swMin.x + LM(9))) * t;
        const ImVec4 knobOff(96.0f / 255.0f, 88.0f / 255.0f, 142.0f / 255.0f, 1.0f);
        const ImVec4 innerOff(31.0f / 255.0f, 31.0f / 255.0f, 39.0f / 255.0f, 1.0f);
        dl->AddCircleFilled(ImVec2(knobX, cy), LM(6.2f), LmCol(*v ? kLmAccent : knobOff), 24);
        dl->AddCircleFilled(ImVec2(knobX, cy), LM(3.2f), LmCol(*v ? kLmChild : innerOff, *v ? 1.0f : 0.95f), 24);

        dl->AddLine(ImVec2(mn.x, mx.y - 0.5f), ImVec2(mx.x, mx.y - 0.5f), LmCol(kLmBorder, 0.72f), 1.0f);

        return changed;
    }

    bool LuminImGuiHelper::ToggleSwitch(const char* label, bool* v) { return LmSwitchRowImpl(label, v); }
    bool LuminImAdd::ToggleButton(const char* label, bool* v)       { return LmSwitchRowImpl(label, v); }
    bool LuminImAdd::CheckBox(const char* label, bool* checked)     { return LmSwitchRowImpl(label, checked); }

    // Lumin slider row: label left, value right, thin fully-rounded track with
    // accent fill + dark knob.
    static bool LmSliderCore(const char* label, float* value, float vmin, float vmax, const char* valueText)
    {
        const std::string display = LmDisplayLabel(label);

        float rowW = ImGui::GetContentRegionAvail().x;
        if (rowW < LM(60))
            rowW = LM(60);
        const float labelH = LM(15);
        const float trackH = LM(11);
        const float rowH = labelH + LM(4) + trackH + LM(7);
        const ImVec2 pos = ImGui::GetCursorScreenPos();

        ImGui::PushID(label);
        ImGui::InvisibleButton("##lm-slider", ImVec2(rowW, rowH));
        const ImGuiID id = ImGui::GetItemID();
        const bool active = ImGui::IsItemActive();
        ImGui::PopID();

        const ImVec2 trackMin(pos.x, pos.y + labelH + LM(4));
        const ImVec2 trackMax(pos.x + rowW, trackMin.y + trackH);

        bool changed = false;
        if (active && vmax > vmin)
        {
            // Only drag when the press started on (or near) the track strip —
            // clicking the label row must not jump the value.
            const ImVec2 click = ImGui::GetIO().MouseClickedPos[0];
            if (click.y >= trackMin.y - LM(4) && click.y <= trackMax.y + LM(5))
            {
                float t = (ImGui::GetIO().MousePos.x - trackMin.x) / rowW;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                const float nv = vmin + t * (vmax - vmin);
                if (nv != *value)
                {
                    *value = nv;
                    changed = true;
                }
            }
        }

        float frac = (vmax > vmin) ? (*value - vmin) / (vmax - vmin) : 0.0f;
        frac = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac);

        static std::unordered_map<ImGuiID, float> s_fill;
        float& fill = s_fill[id];
        if (fill < 0.0f || fill > 1.0f)
            fill = frac;
        LmEase(fill, frac, 18.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        LmTextAligned(dl, g_FreeFontLarge, LM(11.5f), ImVec2(pos.x, pos.y), ImVec2(pos.x + rowW - LM(60), pos.y + labelH),
            LmCol(kLmWhite), display.c_str(), 0.0f, 0.5f);
        LmTextAligned(dl, g_FreeFontLarge, LM(11.5f), ImVec2(pos.x + rowW - LM(120), pos.y), ImVec2(pos.x + rowW, pos.y + labelH),
            LmCol(kLmWhite), valueText, 1.0f, 0.5f);

        dl->AddRectFilled(trackMin, trackMax, LmCol(kLmWidget), LM(99));
        float x = rowW * fill;
        if (x < LM(12))
            x = LM(12);
        dl->AddRectFilled(trackMin, ImVec2(trackMin.x + x, trackMax.y), LmCol(kLmAccent), LM(99));
        dl->AddCircleFilled(ImVec2(trackMin.x + x - LM(6), (trackMin.y + trackMax.y) * 0.5f), LM(4), LmCol(kLmBlack), 24);

        return changed;
    }

    bool LuminImAdd::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
    {
        char valueText[64];
        snprintf(valueText, sizeof(valueText), format ? format : "%.1f", *v);
        float f = *v;
        const bool changed = LmSliderCore(label, &f, v_min, v_max, valueText);
        if (changed)
            *v = f;
        return changed;
    }

    bool LuminImAdd::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format)
    {
        char valueText[64];
        snprintf(valueText, sizeof(valueText), format ? format : "%d", *v);
        float f = (float)*v;
        const bool changed = LmSliderCore(label, &f, (float)v_min, (float)v_max, valueText);
        if (changed)
            *v = (int)(f + (f >= 0.0f ? 0.5f : -0.5f));
        return changed;
    }
#endif  // RUGIR_MENU_AGENCY
