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
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1140, 740, "RayBible - Elegant Bible Reader");
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
        if (saveTimer <= 0) {
            state.SaveSettings();
            saveTimer = 1.0f;
        }

        state.Update();

        state.scrollY += (state.targetScrollY - state.scrollY) * 12.0f * dt;
        if (std::abs(state.targetScrollY - state.scrollY) < 0.5f) state.scrollY = state.targetScrollY;

        state.UpdateGlobalSearch();

        if (state.needsPageRebuild) {
            state.RebuildPages(font);
            state.needsPageRebuild = false;
        }

        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

        if (ctrl && IsKeyPressed(KEY_D)) state.ToggleDarkMode();
        if (ctrl && IsKeyPressed(KEY_H)) state.showHistory = !state.showHistory;
        if (ctrl && IsKeyPressed(KEY_R)) state.ForceRefresh(font);
        if (ctrl && IsKeyPressed(KEY_G)) state.showGlobalSearch = !state.showGlobalSearch;
        if (ctrl && IsKeyPressed(KEY_P)) state.showPlan = !state.showPlan;
        if (ctrl && IsKeyPressed(KEY_S)) { state.showCache = !state.showCache; if (state.showCache) state.cacheStats = g_cache.Stats(); }
        if (ctrl && IsKeyPressed(KEY_B)) { state.bookMode = !state.bookMode; if (state.bookMode) state.needsPageRebuild = true; state.SaveSettings(); }
        
        if (ctrl && IsKeyPressed(KEY_F)) { state.showSearch = !state.showSearch; if (!state.showSearch) { memset(state.searchBuf, 0, sizeof(state.searchBuf)); state.searchResults.clear(); } }
        if (ctrl && IsKeyPressed(KEY_J)) { state.showJump = !state.showJump; if (!state.showJump) memset(state.jumpBuf, 0, sizeof(state.jumpBuf)); }
        if (IsKeyPressed(KEY_F1)) state.showHelp = !state.showHelp;

        if (IsKeyPressed(KEY_ESCAPE)) {
            state.showSearch = state.showGlobalSearch = state.showJump = state.showHelp = state.showPlan = state.showHistory = state.showFavorites = state.showCache = state.showBookDrop = state.showTransDrop = state.showTransDrop2 = state.gSearchActive = state.showBurgerMenu = state.showNoteEditor = false;
            memset(state.searchBuf, 0, sizeof(state.searchBuf)); memset(state.jumpBuf, 0, sizeof(state.jumpBuf)); memset(state.gSearchBuf, 0, sizeof(state.gSearchBuf));
        }

        if (state.bookMode && !state.showSearch && !state.showJump && !state.showGlobalSearch) {
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_SPACE)) state.BookPageNext(font);
            if (IsKeyPressed(KEY_LEFT)) state.BookPagePrev(font);
        }

        if (state.showJump) {
            int k = GetCharPressed(); while (k > 0) { size_t len = strlen(state.jumpBuf); if (k >= 32 && k <= 126 && len < 126) { state.jumpBuf[len] = (char)k; state.jumpBuf[len + 1] = 0; } k = GetCharPressed(); }
            if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(state.jumpBuf); if (len > 0) state.jumpBuf[len - 1] = 0; }
            if (IsKeyPressed(KEY_ENTER)) {
                int nb, nc, nv;
                if (ParseReference(state.jumpBuf, nb, nc, nv)) {
                    state.curBookIdx = nb; state.curChNum = nc;
                    state.targetScrollY = 0; state.scrollY = 0; state.scrollToVerse = nv;
                    state.InitBuffer();
                    state.showJump = false; memset(state.jumpBuf, 0, sizeof(state.jumpBuf));
                }
            }
        }

        if (!state.showSearch && !state.showGlobalSearch && !state.showJump) {
            int key = GetCharPressed(); if (key == '?') state.showHelp = !state.showHelp;
        }

        if (state.showSearch) {
            int k = GetCharPressed(); while (k > 0) { size_t len = strlen(state.searchBuf); if (k >= 32 && k <= 126 && len < 254) { state.searchBuf[len] = (char)k; state.searchBuf[len + 1] = 0; } k = GetCharPressed(); }
            if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(state.searchBuf); if (len > 0) state.searchBuf[len - 1] = 0; }
            if (strlen(state.searchBuf) > 0) { std::vector<Verse> all; for (const auto& ch : state.buf) for (const auto& v : ch.verses) all.push_back(v); state.searchResults = SearchVerses(all, state.searchBuf, state.searchCS); }
            else state.searchResults.clear();
        }

        if (state.showGlobalSearch) {
            int k = GetCharPressed(); while (k > 0) { size_t len = strlen(state.gSearchBuf); if (k >= 32 && k <= 126 && len < 254) { state.gSearchBuf[len] = (char)k; state.gSearchBuf[len + 1] = 0; } k = GetCharPressed(); }
            if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(state.gSearchBuf); if (len > 0) state.gSearchBuf[len - 1] = 0; }
            if (IsKeyPressed(KEY_ENTER) && !state.gSearchActive) state.StartGlobalSearch();
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();
            if (state.showBookDrop && !CheckCollisionPointRec(mp, {310, 60, 200, 400})) state.showBookDrop = false;
            if (state.showTransDrop && !CheckCollisionPointRec(mp, {610, 60, 148, (float)(TRANSLATIONS.size() * 36)})) state.showTransDrop = false;
            if (state.showTransDrop2 && !CheckCollisionPointRec(mp, {696, 60, 148, (float)(TRANSLATIONS.size() * 36)})) state.showTransDrop2 = false;
        }

        BeginDrawing();
        ClearBackground(state.bg);
        if (state.bookMode) DrawBookMode(state, font); else DrawScrollMode(state, font);
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
        if (state.showNoteEditor) DrawNoteEditor(state, font);
        DrawTooltip(state, font);
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}