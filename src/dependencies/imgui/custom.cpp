#include "custom.hpp"
using namespace ImGui;

void c_custom::render_rect_for_horizontal_bar( ImVec2 pos, ImColor color, float alpha ) {

    auto draw = GetWindowDrawList( );
    draw->AddRectFilled( pos - ImVec2( 4, 5 ), pos + ImVec2( 4, 5 ), ImColor( 1.f, 1.f, 1.f, alpha ), 2 );
}

void c_custom::spinner( const char *label, float radius1, float radius2, float thickness, float b_thickness, const ImColor &ball, const ImColor &bg, float speed, size_t balls ) {

    float radius = ImMax(radius1, radius2);
    SPINNER_HEADER(pos, size, centre, num_segments);

    const float start = (float)ImGui::GetTime()* speed;
    const float bg_angle_offset = ImSpinner::PI_2 / num_segments;

    window->DrawList->PathClear();
    for (size_t i = 0; i <= num_segments; i++)
    {
    const float a = start + (i * bg_angle_offset);
    window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a) * radius1, centre.y + ImSin(a) * radius1));
    }
    window->DrawList->PathStroke(ImSpinner::color_alpha(bg, 1.f), false, thickness);

    for (size_t b_num = 0; b_num < balls; ++b_num)
    {
    float b_start = ImSpinner::PI_2 / balls;
    const float a = b_start * b_num + start;
    window->DrawList->AddCircleFilled(ImVec2(centre.x + ImCos(a) * radius2, centre.y + ImSin(a) * radius2), b_thickness, ImSpinner::color_alpha(ball, 1.f));
    }
}

struct tab_ {

    float anim;
    ImVec2 pos;
};

bool c_custom::tab( const char* name, bool current ) {

    auto window = GetCurrentWindow( );
    auto id = window->GetID( name );

    auto name_size = CalcTextSize( name, 0, 1 );

    auto pos = window->DC.CursorPos;
    auto draw = window->DrawList;

    ImVec2 size = ImVec2( 150, 40 );
    ImRect bb( pos, pos + size );
    ItemAdd( bb, id );
    ItemSize( bb, GetStyle( ).FramePadding.y );

    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );

    static std::unordered_map < ImGuiID, tab_ > values;
    auto value = values.find( id );
    if ( value == values.end( ) ) {

        values.insert( { id, { 0.f, bb.Min - window->Pos } } );
        value = values.find( id );
    }

    value->second.anim = ImLerp( value->second.anim, current ? 1.f : 0.f, 0.035f );

    if ( hovered && IsMouseClicked( ImGuiMouseButton_Left ) )
        value->second.pos = GetMousePos( ) - window->Pos;

    PushClipRect( bb.Min, bb.Max, false );
    draw->AddCircleFilled( window->Pos + value->second.pos, ( size.x + 5 ) * value->second.anim, ImColor( 1.f, 1.f, 1.f, ( 0.03f * GetStyle( ).Alpha ) * value->second.anim ) );
    PopClipRect( );
    draw->AddRectFilledMultiColor( bb.Min, ImVec2( bb.Min.x + 7 * value->second.anim, bb.Max.y ), custom.get_accent_color( GetStyle( ).Alpha * value->second.anim ), custom.get_accent_color( 0.015f * ( GetStyle( ).Alpha * value->second.anim ) ), custom.get_accent_color( 0.015f * ( GetStyle( ).Alpha * value->second.anim ) ), custom.get_accent_color( GetStyle( ).Alpha * value->second.anim ) );
    draw->AddRectFilled( bb.Min, ImVec2( bb.Min.x + 3, bb.Max.y ), custom.get_accent_color( GetStyle( ).Alpha * value->second.anim ) );
    draw->AddText( ImVec2( bb.Min.x + 15, bb.GetCenter( ).y - name_size.y / 2 ), GetColorU32( ImGuiCol_Text, value->second.anim > 0.5f ? value->second.anim : 0.5f ), name, FindRenderedTextEnd( name ) );

    return pressed;
}

struct subtab_ {

    float anim;
    ImVec2 pos;
};

bool c_custom::subtab( const char* name, bool current, int m_size ) {

    auto window = GetCurrentWindow( );
    auto id = window->GetID( name );

    auto name_size = CalcTextSize( name, 0, 1 );

    auto pos = window->DC.CursorPos;
    auto draw = window->DrawList;

    ImVec2 size = ImVec2( GetWindowWidth( ) / m_size, GetWindowHeight( ) );
    ImRect bb( pos, pos + size );
    ItemAdd( bb, id );
    ItemSize( bb, GetStyle( ).FramePadding.y );

    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );

    static std::unordered_map < ImGuiID, subtab_ > values;
    auto value = values.find( id );
    if ( value == values.end( ) ) {

        values.insert( { id, { 0.f, bb.Min - window->Pos } } );
        value = values.find( id );
    }

    value->second.anim = ImLerp( value->second.anim, current ? 1.f : 0.f, 0.045f );

    if ( hovered && IsMouseClicked( ImGuiMouseButton_Left ) )
        value->second.pos = GetMousePos( ) - window->Pos;

    PushClipRect( bb.Min, bb.Max, false );
    draw->AddCircleFilled( window->Pos + value->second.pos, ( size.x + 5 ) * value->second.anim, ImColor( 1.f, 1.f, 1.f, ( 0.03f * GetStyle( ).Alpha ) * value->second.anim ) );
    PopClipRect( );
    draw->AddRectFilledMultiColor( bb.Min, ImVec2( bb.Max.x, bb.Min.y + 7 * value->second.anim ), custom.get_accent_color( GetStyle( ).Alpha * value->second.anim ), custom.get_accent_color( GetStyle( ).Alpha * value->second.anim ), custom.get_accent_color( 0.015f * ( GetStyle( ).Alpha * value->second.anim ) ), custom.get_accent_color( 0.015f * ( GetStyle( ).Alpha * value->second.anim ) ) );
    draw->AddRectFilled( bb.Min, ImVec2( bb.Max.x, bb.Min.y + 3 ), custom.get_accent_color( GetStyle( ).Alpha * value->second.anim ) );
    draw->AddText( bb.GetCenter( ) - name_size / 2, GetColorU32( ImGuiCol_Text, value->second.anim > 0.5f ? value->second.anim : 0.5f ), name, FindRenderedTextEnd( name ) );

    return pressed;
}

void c_custom::group_box( const char* name, ImVec2 size_arg ) {

    ImGuiWindow* window = GetCurrentWindow( );
    ImVec2 pos = window->DC.CursorPos;

    auto name_size = GetIO( ).Fonts->Fonts[1]->CalcTextSizeA( GetIO( ).Fonts->Fonts[1]->FontSize, FLT_MAX, 0.f, name );

    BeginChild( std::string( name ).append( ".main" ).c_str( ), size_arg, false, ImGuiWindowFlags_NoScrollbar );

    GetWindowDrawList( )->AddRectFilled( pos, pos + size_arg, to_vec4( 22, 22, 22, custom.m_anim ), 4 );
    GetWindowDrawList( )->AddRectFilled( pos, pos + ImVec2( size_arg.x, 40 ), to_vec4( 15, 15, 15, custom.m_anim ), 4, ImDrawFlags_RoundCornersTop );
    GetWindowDrawList( )->AddLine( pos + ImVec2( 1, 40 ), pos + ImVec2( size_arg.x - 1, 40 ), ImColor( 1.f, 1.f, 1.f, 0.05f * custom.m_anim ) );
    GetWindowDrawList( )->AddRect( pos, pos + size_arg, ImColor( 1.f, 1.f, 1.f, 0.05f * custom.m_anim ), 4 );

    GetWindowDrawList( )->AddText( GetIO( ).Fonts->Fonts[1], GetIO( ).Fonts->Fonts[1]->FontSize, pos + ImVec2( 12, 11 ), GetColorU32( ImGuiCol_Text, custom.m_anim ), name, FindRenderedTextEnd( name ) );

    PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 8 } );

    SetCursorPosY( 41 );
    BeginChild( name, { size_arg.x, size_arg.y - 42 }, false, ImGuiWindowFlags_AlwaysUseWindowPadding );
    SetCursorPosX( 12 );

    BeginGroup( );

    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 8, 6 } );
    PushStyleVar( ImGuiStyleVar_Alpha, custom.m_anim );
}

void c_custom::end_group_box( ) {

    PopStyleVar( 3 );
    EndGroup( );
    EndChild( );
    EndChild( );
}

void c_custom::custom_child(ImFont* font, const char* name, const char* title, const ImVec2 pos, const ImVec2 size )
{
    ImGui::GetWindowDrawList( )->AddRectFilled( ImGui::GetWindowPos() + pos, ImGui::GetWindowPos( ) + pos + ImVec2( size.x, 48 ), ImColor( 22,22,22 ), 5, ImDrawFlags_RoundCornersTop );
	ImGui::GetWindowDrawList()->AddText( font, 26, ImGui::GetWindowPos() + pos + ImVec2( 10, 6 ), ImColor( 200,200,200 ), title );

    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImColor(24,24,24).Value);
    ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 5 );
	ImGui::SetCursorPos( { pos.x, pos.y + 40 } );
	ImGui::BeginChild( name, { size.x, size.y - 32 }, false );

	ImGui::SetCursorPos( {15, 20} );
	ImGui::BeginGroup( );
	ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 10, 10 } );
}

void c_custom::end_custom_child( )
{
    ImGui::PopStyleVar( 2 );
	ImGui::EndGroup( );
	ImGui::EndChild( );
    ImGui::PopStyleColor();
}
