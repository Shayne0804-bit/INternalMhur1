#pragma once
#include <imgui.h>

namespace SDKDrawing
{
	// Dessiner un box wireframe optimisé avec ImGui
	void DrawBox2D(int X, int Y, int W, int H, ImU32 color, int thickness = 2);
	
	// Dessiner un box avec bordures noires (meilleure lisibilité)
	void DrawBox2DWithOutline(int X, int Y, int W, int H, ImU32 color, int thickness = 2);
	
	// Dessiner du texte à l'écran
	void DrawText(int X, int Y, ImU32 color, const char* text);
	
	// Initialiser le système de dessin
	void Initialize();
	
	// Nettoyer les ressources
	void Shutdown();
}
