#include "SDKDrawing.h"
#include <imgui.h>

namespace SDKDrawing
{
	void DrawBox2D(int X, int Y, int W, int H, ImU32 color, int thickness)
	{
		if (!ImGui::GetCurrentContext())
			return;

		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
		if (!DrawList)
			return;

		// Dessiner un rectangle wireframe simple
		DrawList->AddRect(ImVec2(X, Y), ImVec2(X + W, Y + H), color, 0.0f, 0, thickness);
	}

	void DrawBox2DWithOutline(int X, int Y, int W, int H, ImU32 color, int thickness)
	{
		if (!ImGui::GetCurrentContext())
			return;

		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
		if (!DrawList)
			return;

		// Bordure noire d'abord pour la visibilité
		ImU32 blackColor = ImGui::GetColorU32(ImVec4(0, 0, 0, 1.0f));
		DrawList->AddRect(ImVec2(X, Y), ImVec2(X + W, Y + H), blackColor, 0.0f, 0, thickness + 2);

		// Puis la couleur par-dessus
		DrawList->AddRect(ImVec2(X, Y), ImVec2(X + W, Y + H), color, 0.0f, 0, thickness);

		// Coins accentués (comme dans Unreal-Internal-Base)
		float cornerSize = 10.0f;

		// Top-left
		DrawList->AddLine(ImVec2(X, Y), ImVec2(X + cornerSize, Y), color, thickness);
		DrawList->AddLine(ImVec2(X, Y), ImVec2(X, Y + cornerSize), color, thickness);

		// Top-right
		DrawList->AddLine(ImVec2(X + W, Y), ImVec2(X + W - cornerSize, Y), color, thickness);
		DrawList->AddLine(ImVec2(X + W, Y), ImVec2(X + W, Y + cornerSize), color, thickness);

		// Bottom-left
		DrawList->AddLine(ImVec2(X, Y + H), ImVec2(X + cornerSize, Y + H), color, thickness);
		DrawList->AddLine(ImVec2(X, Y + H), ImVec2(X, Y + H - cornerSize), color, thickness);

		// Bottom-right
		DrawList->AddLine(ImVec2(X + W, Y + H), ImVec2(X + W - cornerSize, Y + H), color, thickness);
		DrawList->AddLine(ImVec2(X + W, Y + H), ImVec2(X + W, Y + H - cornerSize), color, thickness);
	}

	void DrawText(int X, int Y, ImU32 color, const char* text)
	{
		if (!ImGui::GetCurrentContext())
			return;

		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
		if (!DrawList)
			return;

		DrawList->AddText(ImVec2(X, Y), color, text);
	}

	void Initialize()
	{
		// Rien à initialiser pour le moment
	}

	void Shutdown()
	{
		// Rien à nettoyer pour le moment
	}
}
