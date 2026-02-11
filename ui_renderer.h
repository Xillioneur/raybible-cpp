#include "raybible.h"
#include "app_state.h"
#include <vector>
#include <string>

// --- Helper Functions ---
std::vector<std::string> WrapText(const std::string& text, Font font, float fontSize, float maxWidth);
std::vector<Page> BuildPages(const std::deque<Chapter>& chapters, const std::deque<Chapter>& chapters2, bool parallelMode, Font font, float pageW, float pageH, float fSize, float lSpacing);

inline void closeAllPanels(AppState& s) {
    s.showHistory = s.showFavorites = s.showCache = s.showPlan = s.showHelp = s.showSearch = s.showJump = s.showGlobalSearch = s.showNoteEditor = s.showBurgerMenu = s.showWordStudy = false;
    s.showBookDrop = s.showTransDrop = s.showTransDrop2 = false;
}

inline bool IsAnyOverlayOpen(const AppState& s) {
    return s.showHistory || s.showFavorites || s.showCache || s.showPlan || s.showHelp || s.showSearch || s.showJump || s.showGlobalSearch || s.showNoteEditor || s.showBurgerMenu || s.showBookDrop || s.showTransDrop || s.showTransDrop2 || s.showWordStudy;
}

// --- Main Drawing Functions ---
void DrawHeader(AppState& s, Font f);
void DrawFooter(AppState& s, Font f);
void DrawScrollMode(AppState& s, Font f);
void DrawBookMode(AppState& s, Font f);
void DrawTooltip(AppState& s, Font f);

// --- Panel Drawing Functions ---
void DrawHistoryPanel(AppState& s, Font f);
void DrawFavoritesPanel(AppState& s, Font f);
void DrawCachePanel(AppState& s, Font f);
void DrawPlanPanel(AppState& s, Font f);
void DrawHelpPanel(AppState& s, Font f);
void DrawSearchPanel(AppState& s, Font f);
void DrawJumpPanel(AppState& s, Font f);
void DrawGlobalSearchPanel(AppState& s, Font f);
void DrawBurgerMenu(AppState& s, Font f);
void DrawNoteEditor(AppState& s, Font f);
void DrawWordStudyPanel(AppState& s, Font f);

// --- Extras ---
void SaveVerseImage(const Verse& v, const std::string& ref, const std::string& trans, Font font);
