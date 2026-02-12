#include "raylib.h"
#include "raybible.h"
#include "app_state.h"
#include "ui_renderer.h"
#include "utils.h"
#include "managers.h"
#include "bible_logic.h"
#include <cstring>
#include <cmath>

int main() {
    g_settings.Load();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(g_settings.winW, g_settings.winH, "Divine Word - Holy Bible");
    if (g_settings.winX != -1 && g_settings.winY != -1) SetWindowPosition(g_settings.winX, g_settings.winY);
    SetTargetFPS(60);

    Font font = GetFontDefault();
#ifdef __APPLE__
    const char* fontPath = "/System/Library/Fonts/Supplemental/Times New Roman.ttf";
    if (FileExists(fontPath)) font = LoadFontEx(fontPath, 64, 0, 10000);
#elif defined(_WIN32)
    const char* fontPath = "C:\\Windows\\Fonts\\times.ttf";
    if (FileExists(fontPath)) font = LoadFontEx(fontPath, 64, 0, 10000);
#else
    const char* paths[] = { "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf", "/usr/share/fonts/TTF/Times.TTF", "/usr/share/fonts/truetype/freefont/FreeSerif.ttf" };
    for (const char* p : paths) { if (FileExists(p)) { font = LoadFontEx(p, 64, 0, 10000); break; } }
#endif
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    AppState state;
    state.InitBuffer(false);

    float saveTimer = 1.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (state.statusTimer > 0) state.statusTimer -= dt;
        
        saveTimer -= dt;
        if (saveTimer <= 0) { state.SaveSettings(); saveTimer = 1.0f; }

        state.Update();
        state.scrollY += (state.targetScrollY - state.scrollY) * 12.0f * dt;
        if (std::abs(state.targetScrollY - state.scrollY) < 0.5f) state.scrollY = state.targetScrollY;

        state.UpdateGlobalSearch();
        if (state.needsPageRebuild) { state.RebuildPages(font); state.needsPageRebuild = false; }

        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

        // --- Keyboard Shortcut Logic ---
        if (!state.InputActive()) {
            if (ctrl && IsKeyPressed(KEY_D)) state.NextTheme();
            if (ctrl && IsKeyPressed(KEY_H)) { bool val = !state.showHistory; closeAllPanels(state); state.showHistory = val; }
            if (ctrl && IsKeyPressed(KEY_R)) state.ForceRefresh(font);
            if (ctrl && IsKeyPressed(KEY_G)) { bool val = !state.showGlobalSearch; closeAllPanels(state); state.showGlobalSearch = val; }
            if (ctrl && IsKeyPressed(KEY_P)) { bool val = !state.showPlan; closeAllPanels(state); state.showPlan = val; }
            if (ctrl && IsKeyPressed(KEY_S)) { bool val = !state.showFavorites; closeAllPanels(state); state.showFavorites = val; }
            if (ctrl && IsKeyPressed(KEY_B)) { state.bookMode = !state.bookMode; if (state.bookMode) state.needsPageRebuild = true; state.SaveSettings(); }
            if (IsKeyPressed(KEY_Z)) { state.zenMode = !state.zenMode; state.SaveSettings(); }
            if (IsKeyPressed(KEY_S)) { state.showSidebar = !state.showSidebar; state.SaveSettings(); }
            if (IsKeyPressed(KEY_T)) { state.studyMode = !state.studyMode; if (state.bookMode) state.needsPageRebuild = true; state.SaveSettings(); }
            
            if (ctrl && IsKeyPressed(KEY_F)) { bool val = !state.showSearch; closeAllPanels(state); state.showSearch = val; if (!state.showSearch) { memset(state.searchBuf, 0, sizeof(state.searchBuf)); state.searchResults.clear(); } }
            if (ctrl && IsKeyPressed(KEY_J)) { bool val = !state.showJump; closeAllPanels(state); state.showJump = val; if (!state.showJump) memset(state.jumpBuf, 0, sizeof(state.jumpBuf)); }
            if (IsKeyPressed(KEY_F1)) { bool val = !state.showHelp; closeAllPanels(state); state.showHelp = val; }
        }

        if (IsKeyPressed(KEY_ESCAPE)) {
            closeAllPanels(state);
            memset(state.searchBuf, 0, sizeof(state.searchBuf)); memset(state.jumpBuf, 0, sizeof(state.jumpBuf)); memset(state.gSearchBuf, 0, sizeof(state.gSearchBuf));
        }

        if (state.bookMode && !state.InputActive()) {
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_SPACE)) state.BookPageNext(font);
            if (IsKeyPressed(KEY_LEFT)) state.BookPagePrev(font);
        }

        // --- Text Input Polling (Unified) ---
        int charPressed = GetCharPressed();
        while (charPressed > 0) {
            if (state.showJump) { size_t len = strlen(state.jumpBuf); if (charPressed >= 32 && charPressed <= 126 && len < 126) { state.jumpBuf[len] = (char)charPressed; state.jumpBuf[len+1] = 0; } }
            else if (state.showSearch) { size_t len = strlen(state.searchBuf); if (charPressed >= 32 && charPressed <= 126 && len < 254) { state.searchBuf[len] = (char)charPressed; state.searchBuf[len+1] = 0; } }
            else if (state.showGlobalSearch) { size_t len = strlen(state.gSearchBuf); if (charPressed >= 32 && charPressed <= 126 && len < 254) { state.gSearchBuf[len] = (char)charPressed; state.gSearchBuf[len+1] = 0; } }
            else if (state.isEditingNote) { size_t len = strlen(state.noteBuf); if (charPressed >= 32 && charPressed <= 126 && len < 511) { state.noteBuf[len] = (char)charPressed; state.noteBuf[len+1] = 0; } }
            charPressed = GetCharPressed();
        }

        if (state.showJump) {
            if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(state.jumpBuf); if (len > 0) state.jumpBuf[len - 1] = 0; }
            if (IsKeyPressed(KEY_ENTER)) { int nb, nc, nv; if (ParseReference(state.jumpBuf, nb, nc, nv)) { state.curBookIdx = nb; state.curChNum = nc; state.targetScrollY = 0; state.scrollY = 0; state.scrollToVerse = nv; state.InitBuffer(); state.showJump = false; memset(state.jumpBuf, 0, sizeof(state.jumpBuf)); } }
        } else if (state.showSearch) {
            if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(state.searchBuf); if (len > 0) state.searchBuf[len - 1] = 0; }
            if (strlen(state.searchBuf) > 0) { state.searchResults = SearchVerses(state.buf, state.searchBuf, state.searchCS); } else state.searchResults.clear();
        } else if (state.showGlobalSearch) {
            if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(state.gSearchBuf); if (len > 0) state.gSearchBuf[len - 1] = 0; }
            if (IsKeyPressed(KEY_ENTER) && !state.gSearchActive) state.StartGlobalSearch();
        } else if (state.isEditingNote) {
            if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(state.noteBuf); if (len > 0) state.noteBuf[len - 1] = 0; }
            if (IsKeyPressed(KEY_ENTER)) {
                if (!state.buf.empty() && state.lastSelectedVerse != -1) {
                    std::string vText; int ci = state.scrollChapterIdx; if (ci >= 0 && ci < (int)state.buf.size()) { for (const auto& v : state.buf[ci].verses) { if (v.number == state.lastSelectedVerse) { vText = v.text; break; } } g_study.SetNote(state.buf[ci].book, state.buf[ci].chapter, state.lastSelectedVerse, state.trans, state.noteBuf, vText); }
                }
                state.isEditingNote = false;
            }
        }

        BeginDrawing();
        ClearBackground(state.bg);
        if (state.bookMode) DrawBookMode(state, font); else DrawScrollMode(state, font);
        DrawSidebar(state, font);
        DrawFooter(state, font);
        DrawHeader(state, font);
        if (state.showSearch) DrawSearchPanel(state, font);
        if (state.showGlobalSearch) DrawGlobalSearchPanel(state, font);
        if (state.showJump) DrawJumpPanel(state, font);
        if (state.showHistory) DrawHistoryPanel(state, font);
        if (state.showFavorites) DrawFavoritesPanel(state, font);
        if (state.showCache) DrawCachePanel(state, font);
        if (state.showPlan) DrawPlanPanel(state, font);
        if (state.showHelp) DrawHelpPanel(state, font);
        if (state.showBurgerMenu) DrawBurgerMenu(state, font);
        if (state.showAbout) DrawAboutPanel(state, font);
        DrawTooltip(state, font);
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}