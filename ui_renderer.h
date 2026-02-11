#pragma once
#ifndef RAYBIBLE_UI_RENDERER_H
#define RAYBIBLE_UI_RENDERER_H

#include "raybible.h"
#include "app_state.h"

std::vector<std::string> WrapText(const std::string& text, Font font, float fontSize, float maxWidth);
std::vector<Page> BuildPages(const std::deque<Chapter>& chapters, const std::deque<Chapter>& chapters2, bool parallelMode, Font font,
                                     float pageW, float pageH, float fSize, float lSpacing);

void DrawHeader(AppState& s, Font f);
void DrawFooter(AppState& s, Font f);
void DrawScrollMode(AppState& s, Font f);
void DrawBookMode(AppState& s, Font f);
void DrawTooltip(AppState& s, Font f);

void DrawHistoryPanel(AppState& s, Font f);
void DrawFavoritesPanel(AppState& s, Font f);
void DrawCachePanel(AppState& s, Font f);
void DrawSearchPanel(AppState& s, Font f);
void DrawJumpPanel(AppState& s, Font f);
void DrawPlanPanel(AppState& s, Font f);
void DrawGlobalSearchPanel(AppState& s, Font f);
void DrawHelpPanel(AppState& s, Font f);
void DrawBurgerMenu(AppState& s, Font f);
void DrawNoteEditor(AppState& s, Font f);

void SaveVerseImage(const Verse& v, const std::string& ref, const std::string& trans, Font font);

#endif // UI_RENDERER_H
