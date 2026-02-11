#include "ui_renderer.h"
#include "bible_logic.h"
#include "managers.h"
#include "utils.h"
#include <algorithm>
#include <cstring>
#include <sstream>

std::vector<std::string> WrapText(const std::string& text, Font font, float fontSize, float maxWidth) {
    std::vector<std::string> lines;
    if (maxWidth <= 0) return lines;
    std::string cur;
    std::istringstream ws(text); std::string word;
    while (ws >> word) {
        std::string test = cur.empty() ? word : cur + " " + word;
        if (MeasureTextEx(font, test.c_str(), fontSize, 1).x > maxWidth && !cur.empty()) { lines.push_back(cur); cur = word; }
        else cur = test;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

std::vector<Page> BuildPages(const std::deque<Chapter>& chapters, const std::deque<Chapter>& chapters2, bool parallelMode, Font font,
                                     float pageW, float pageH, float fSize, float lSpacing) {
    std::vector<Page> pages;
    if (chapters.empty()) return pages;
    const float verseGap = 14.0f, footerRoom = 50.0f, headingH = 42.0f;

    if (!parallelMode) {
        for (int ci = 0; ci < (int)chapters.size(); ci++) {
            const Chapter& ch = chapters[ci];
            if (!ch.isLoaded || ch.verses.empty()) continue;
            Page cur{}; cur.chapterBufIndex = ci; cur.isChapterStart = true; cur.startVerse = ch.verses[0].number;
            float usedY = headingH;
            for (int vi = 0; vi < (int)ch.verses.size(); vi++) {
                const auto& v = ch.verses[vi];
                std::string nl = std::to_string(v.number) + " ";
                Vector2 nsz = MeasureTextEx(font, nl.c_str(), fSize, 1);
                auto lines = WrapText(v.text, font, fSize, pageW - nsz.x - 60.0f);
                float vH = lines.size() * (fSize + lSpacing) + verseGap;
                if (usedY + vH > pageH - footerRoom && !cur.lines.empty()) {
                    pages.push_back(cur); cur.lines.clear(); cur.chapterBufIndex = ci; cur.isChapterStart = false; cur.startVerse = v.number; usedY = 0;
                }
                for (size_t li = 0; li < lines.size(); li++) {
                    if (li == 0) cur.lines.push_back(nl + lines[li]);
                    else cur.lines.push_back(std::string(4, ' ') + lines[li]);
                }
                usedY += vH; cur.endVerse = v.number;
            }
            if (!cur.lines.empty()) pages.push_back(cur);
        }
    } else {
        for (int ci = 0; ci < (int)chapters.size(); ci++) {
            const Chapter& ch1 = chapters[ci];
            const Chapter* ch2 = (ci < (int)chapters2.size()) ? &chapters2[ci] : nullptr;
            if (!ch1.isLoaded || ch1.verses.empty()) continue;
            Page cur{}; cur.chapterBufIndex = ci; cur.isChapterStart = true; cur.startVerse = ch1.verses[0].number;
            float colW = (pageW - 80.0f) / 2.0f, usedY = headingH;
            size_t maxVerses = ch1.verses.size();
            if (ch2 && ch2->isLoaded && ch2->verses.size() > maxVerses) maxVerses = ch2->verses.size();
            for (size_t vi = 0; vi < maxVerses; vi++) {
                std::vector<std::string> v1Lines, v2Lines; float v1H = 0, v2H = 0;
                if (vi < ch1.verses.size()) {
                    const auto& v = ch1.verses[vi]; std::string nl = std::to_string(v.number) + " "; Vector2 nsz = MeasureTextEx(font, nl.c_str(), fSize, 1);
                    auto lines = WrapText(v.text, font, fSize, colW - nsz.x - 5.0f);
                    for (size_t li = 0; li < lines.size(); li++) {
                        if (li == 0) v1Lines.push_back(nl + lines[li]); else v1Lines.push_back(std::string(4, ' ') + lines[li]);
                    }
                    v1H = v1Lines.size() * (fSize + lSpacing) + verseGap;
                }
                if (ch2 && ch2->isLoaded && vi < ch2->verses.size()) {
                    const auto& v = ch2->verses[vi]; std::string nl = std::to_string(v.number) + " "; Vector2 nsz = MeasureTextEx(font, nl.c_str(), fSize, 1);
                    auto lines = WrapText(v.text, font, fSize, colW - nsz.x - 5.0f);
                    for (size_t li = 0; li < lines.size(); li++) {
                        if (li == 0) v2Lines.push_back(nl + lines[li]); else v2Lines.push_back(std::string(4, ' ') + lines[li]);
                    }
                    v2H = v2Lines.size() * (fSize + lSpacing) + verseGap;
                }
                float vMaxH = std::max(v1H, v2H);
                if (usedY + vMaxH > pageH - footerRoom && (!cur.lines.empty() || !cur.lines2.empty())) {
                    pages.push_back(cur); cur.lines.clear(); cur.lines2.clear(); cur.chapterBufIndex = ci; cur.isChapterStart = false;
                    cur.startVerse = (vi < ch1.verses.size()) ? ch1.verses[vi].number : ch1.verses.back().number; usedY = 0;
                }
                cur.lines.insert(cur.lines.end(), v1Lines.begin(), v1Lines.end());
                cur.lines2.insert(cur.lines2.end(), v2Lines.begin(), v2Lines.end());
                int diff = (int)v1Lines.size() - (int)v2Lines.size();
                if (diff < 0) for (int i = 0; i < -diff; i++) cur.lines.push_back(""); else if (diff > 0) for (int i = 0; i < diff; i++) cur.lines2.push_back("");
                cur.lines.push_back(""); cur.lines2.push_back(""); usedY += vMaxH;
                if (vi < ch1.verses.size()) cur.endVerse = ch1.verses[vi].number;
            }
            if (!cur.lines.empty() || !cur.lines2.empty()) pages.push_back(cur);
        }
    }
    return pages;
}

static void DrawVerseText(Font font, const Verse& v, float x, float& y, float maxW, float fSize, float lSpacing, float vGap, Color textCol, Color numCol, bool isFav, bool hovered, const std::vector<SearchMatch>& matches, Color hlCol) {
    std::string numLabel = std::to_string(v.number); float numFSize = fSize * 0.6f; Vector2 numSz = MeasureTextEx(font, numLabel.c_str(), numFSize, 1);
    Color nc = isFav ? Color{255, 210, 60, 255} : numCol; DrawTextEx(font, numLabel.c_str(), {x, y + 2}, numFSize, 1, nc);
    if (hovered || isFav) { const char* star = isFav ? "*" : "o"; DrawTextEx(font, star, {x + numSz.x + 2, y + 2}, numFSize, 1, nc); }
    float tx = x + numSz.x + (hovered || isFav ? 15.0f : 8.0f); auto lines = WrapText(v.text, font, fSize, maxW - (tx - x));
    for (size_t li = 0; li < lines.size(); li++) {
        float rx = (li == 0) ? tx : x + 10.0f;
        for (const auto& m : matches) {
            if (m.verseNumber == v.number) {
                std::string lineLower = ToLower(lines[li]); size_t p = 0;
                while ((p = lineLower.find(ToLower(v.text.substr(m.matchPos, m.matchLen)), p)) != std::string::npos) {
                    Vector2 pre = MeasureTextEx(font, lines[li].substr(0, p).c_str(), fSize, 1); Vector2 mid = MeasureTextEx(font, lines[li].substr(p, m.matchLen).c_str(), fSize, 1);
                    DrawRectangleRec({rx + pre.x, y, mid.x, fSize + 2}, {hlCol.r, hlCol.g, hlCol.b, 120}); p += m.matchLen;
                }
            }
        }
        DrawTextEx(font, lines[li].c_str(), {rx, y}, fSize, 1, textCol); y += fSize + lSpacing;
    }
    y += vGap;
}

static std::string FmtBytes(long b) { if (b < 1024) return std::to_string(b) + " B"; if (b < 1 << 20) return std::to_string(b / 1024) + " KB"; return std::to_string(b / (1 << 20)) + " MB"; }

void DrawTooltip(AppState& s, Font f) {
    if (strlen(s.tooltip) == 0) return; Vector2 mp = GetMousePosition(); Vector2 sz = MeasureTextEx(f, s.tooltip, 14, 1);
    float tx = mp.x + 15, ty = mp.y + 15; if (tx + sz.x + 10 > GetScreenWidth()) tx = mp.x - sz.x - 15;
    DrawRectangleRec({tx, ty, sz.x + 10, sz.y + 6}, {20, 20, 20, 230}); DrawRectangleLinesEx({tx, ty, sz.x + 10, sz.y + 6}, 1, s.vnum); DrawTextEx(f, s.tooltip, {tx + 5, ty + 3}, 14, 1, RAYWHITE);
}

void DrawHelpPanel(AppState& s, Font f) {
    float pw = 500, ph = 420, px = (GetScreenWidth() - pw) / 2.f, py = 120;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawTextEx(f, "RayBible Keyboard Shortcuts", {px + 20, py + 20}, 24, 1, s.accent);
    float y = py + 65; auto row = [&](const char* k, const char* d) { DrawTextEx(f, k, {px + 25, y}, 17, 1, s.vnum); DrawTextEx(f, d, {px + 180, y}, 17, 1, s.text); y += 28; };
    row("Ctrl + F", "Search in current view"); row("Ctrl + G", "Global Bible search"); row("Ctrl + J", "Jump to reference"); row("Ctrl + H", "View history");
    row("Ctrl + P", "Daily reading plan"); row("Ctrl + S", "Cache statistics"); row("Ctrl + B", "Toggle Book/Scroll mode"); row("Ctrl + D", "Toggle Dark/Light mode");
    row("Ctrl + R", "Refresh current passage"); row("F1 or ?", "Show this help panel"); row("ESC", "Close any panel or dropdown");
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showHelp = false;
}

void DrawBurgerMenu(AppState& s, Font f) {
    float pw = 240, ph = 360, px = (float)GetScreenWidth() - pw - 10, py = 65;
    DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum);
    float y = py + 15;
    auto item = [&](const char* lbl, bool* toggle, bool isBtn = true) {
        Rectangle r = {px + 10, y, pw - 20, 35}; bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 80});
        DrawTextEx(f, lbl, {r.x + 10, r.y + 8}, 16, 1, hov ? RAYWHITE : s.text);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { if (toggle) *toggle = !*toggle; s.showBurgerMenu = false; return true; }
        y += 40; return false;
    };
    if (item(s.darkMode ? "Light Mode" : "Dark Mode", nullptr)) s.ToggleDarkMode();
    if (item("History", nullptr)) s.showHistory = true;
    if (item("Favorites", nullptr)) s.showFavorites = true;
    if (item("Reading Plan", nullptr)) s.showPlan = true;
    if (item("Cache Stats", nullptr)) { s.showCache = true; s.cacheStats = g_cache.Stats(); }
    if (item("Help / Shortcuts", nullptr)) s.showHelp = true;
    if (item("History Back", nullptr)) s.GoBack();
    if (item("History Forward", nullptr)) s.GoForward();
}

void DrawNoteEditor(AppState& s, Font f) {
    float pw = 500, ph = 300, px = (GetScreenWidth() - pw) / 2.f, py = 150;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Edit Verse Note", {px + 20, py + 20}, 24, 1, s.accent);
    Rectangle box = {px + 20, py + 60, pw - 40, 150}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum);
    std::string currentNote = s.noteBuf; auto lines = WrapText(currentNote + "_", f, 18, box.width - 20); float ly = box.y + 10;
    for (const auto& ln : lines) { DrawTextEx(f, ln.c_str(), {box.x + 10, ly}, 18, 1, s.text); ly += 22; }
    int key = GetCharPressed(); while (key > 0) { size_t len = strlen(s.noteBuf); if (key >= 32 && key <= 126 && len < 511) { s.noteBuf[len] = (char)key; s.noteBuf[len+1] = 0; } key = GetCharPressed(); }
    if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(s.noteBuf); if (len > 0) s.noteBuf[len-1] = 0; }
    Rectangle saveBtn = {px + pw - 220, py + ph - 50, 100, 34}, cancelBtn = {px + pw - 110, py + ph - 50, 100, 34};
    bool sHov = CheckCollisionPointRec(GetMousePosition(), saveBtn), cHov = CheckCollisionPointRec(GetMousePosition(), cancelBtn);
    DrawRectangleRec(saveBtn, sHov ? s.ok : s.bg); DrawRectangleLinesEx(saveBtn, 1, s.vnum); DrawTextEx(f, "SAVE", {saveBtn.x + 25, saveBtn.y + 8}, 18, 1, sHov ? RAYWHITE : s.text);
    if (sHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        size_t p1 = s.selectedFavoriteKey.find(':'), p2 = s.selectedFavoriteKey.find(':', p1 + 1), p3 = s.selectedFavoriteKey.find(':', p2 + 1);
        if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) { std::string t = s.selectedFavoriteKey.substr(0, p1), b = s.selectedFavoriteKey.substr(p1 + 1, p2 - p1 - 1); int ch = std::stoi(s.selectedFavoriteKey.substr(p2 + 1, p3 - p2 - 1)), v = std::stoi(s.selectedFavoriteKey.substr(p3 + 1)); g_favs.UpdateNote(b, ch, v, t, s.noteBuf); }
        s.showNoteEditor = false;
    }
    DrawRectangleRec(cancelBtn, cHov ? s.accent : s.bg); DrawRectangleLinesEx(cancelBtn, 1, s.vnum); DrawTextEx(f, "CANCEL", {cancelBtn.x + 15, cancelBtn.y + 8}, 18, 1, cHov ? RAYWHITE : s.text);
    if (cHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showNoteEditor = false;
}

void SaveVerseImage(const Verse& v, const std::string& ref, const std::string& trans, Font font) {
    const int W = 800, H = 450; RenderTexture2D target = LoadRenderTexture(W, H); BeginTextureMode(target); ClearBackground({20, 20, 20, 255});
    DrawRectangleLinesEx({10, 10, W - 20, H - 20}, 3, {180, 140, 40, 255}); DrawRectangleLinesEx({20, 20, W - 40, H - 40}, 1, {140, 20, 20, 255});
    float fs = 32.0f; std::string quote = "\"" + v.text + "\""; auto lines = WrapText(quote, font, fs, W - 120); float totalTxtH = lines.size() * (fs + 8); float ty = (H - totalTxtH) / 2.0f - 30;
    for (const auto& line : lines) { Vector2 sz = MeasureTextEx(font, line.c_str(), fs, 1); DrawTextEx(font, line.c_str(), {(W - sz.x) / 2.0f, ty}, fs, 1, {245, 240, 225, 255}); ty += fs + 8; }
    std::string refStr = ref + ":" + std::to_string(v.number) + " (" + trans + ")"; Vector2 rsz = MeasureTextEx(font, refStr.c_str(), 24, 1); DrawTextEx(font, refStr.c_str(), {(W - rsz.x) / 2.0f, ty + 20}, 24, 1, {180, 140, 40, 255});
    DrawTextEx(font, "RayBible", {W - 120, H - 45}, 18, 1, {140, 20, 20, 150}); EndTextureMode();
    Image img = LoadImageFromTexture(target.texture); ImageFlipVertical(&img); std::string filename = "verse_" + std::to_string(time(NULL)) + ".png"; ExportImage(img, filename.c_str()); UnloadImage(img); UnloadRenderTexture(target);
}

void DrawHistoryPanel(AppState& s, Font f) {
    float pw = 480, ph = 520, px = (GetScreenWidth() - pw) / 2.f, py = 80;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawTextEx(f, "Reading History", {px + 20, py + 20}, 24, 1, s.accent);
    float y = py + 65; const auto& hist = g_hist.All();
    if (hist.empty()) { DrawTextEx(f, "No history yet.", {px + 20, y}, 18, 1, s.vnum); } else {
        for (int i = 0; i < (int)hist.size() && i < 11; i++) {
            const auto& h = hist[i]; std::string lbl = h.book + " (" + h.translation + ")"; Rectangle r = {px + 20, y, pw - 40, 36}; bool hov = CheckCollisionPointRec(GetMousePosition(), r);
            if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); DrawTextEx(f, lbl.c_str(), {r.x + 10, r.y + 8}, 17, 1, hov ? RAYWHITE : s.text);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.curBookIdx = h.bookIndex; s.curChNum = h.chapter; for (int j = 0; j < (int)TRANSLATIONS.size(); j++) if (TRANSLATIONS[j].code == h.translation) { s.transIdx = j; break; } s.trans = h.translation; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); s.showHistory = false; } y += 40;
        }
    }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showHistory = false;
}

void DrawFavoritesPanel(AppState& s, Font f) {
    float pw = 520, ph = 560, px = (GetScreenWidth() - pw) / 2.f, py = 60;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawTextEx(f, "Favorites", {px + 20, py + 20}, 24, 1, s.accent);
    float y = py + 65; const auto& favs = g_favs.All();
    if (favs.empty()) { DrawTextEx(f, "No favorites yet.", {px + 20, y}, 18, 1, s.vnum); } else {
        for (int i = 0; i < (int)favs.size() && i < 11; i++) {
            const auto& fv = favs[i]; std::string lbl = "* " + fv.GetDisplay(); std::string prev = fv.text.empty() ? "" : (fv.text.size() > 55 ? fv.text.substr(0, 52) + "..." : fv.text);
            Rectangle r = {px + 20, y, pw - 40, 44}; bool hov = CheckCollisionPointRec(GetMousePosition(), r);
            if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); DrawTextEx(f, lbl.c_str(), {r.x + 10, r.y + 4}, 16, 1, s.vnum); DrawTextEx(f, prev.c_str(), {r.x + 10, r.y + 24}, 14, 1, s.text);
            Rectangle nBtn = { r.x + r.width - 90, r.y + 10, 80, 24 }; bool nHov = CheckCollisionPointRec(GetMousePosition(), nBtn); DrawRectangleRec(nBtn, nHov ? s.accent : s.hdr); DrawRectangleLinesEx(nBtn, 1, s.vnum); DrawTextEx(f, "NOTE", { nBtn.x + 20, nBtn.y + 5 }, 12, 1, nHov ? RAYWHITE : s.vnum);
            if (nHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.showNoteEditor = true; s.selectedFavoriteKey = fv.GetKey(); strncpy(s.noteBuf, fv.note.c_str(), 511); }
            else if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { for (int j = 0; j < (int)BIBLE_BOOKS.size(); j++) if (fv.book.find(BIBLE_BOOKS[j].name) != std::string::npos) { s.curBookIdx = j; break; } s.curChNum = fv.chapter; s.trans = fv.translation; for (int j = 0; j < (int)TRANSLATIONS.size(); j++) if (TRANSLATIONS[j].code == fv.translation) { s.transIdx = j; break; } s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); s.showFavorites = false; } y += 48;
        }
    }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showFavorites = false;
}

void DrawCachePanel(AppState& s, Font f) {
    float pw = 420, ph = 480, px = (GetScreenWidth() - pw) / 2.f, py = 100;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawTextEx(f, "Cache Statistics", {px + 20, py + 20}, 24, 1, s.accent);
    float y = py + 65; auto row = [&](const char* k, const std::string& v) { DrawTextEx(f, k, {px + 25, y}, 17, 1, s.vnum); DrawTextEx(f, v.c_str(), {px + 220, y}, 17, 1, s.text); y += 30; };
    row("Chapters cached:", std::to_string(s.cacheStats.totalChapters)); row("Total verses:", std::to_string(s.cacheStats.totalVerses)); row("Disk usage:", FmtBytes(s.cacheStats.totalSize));
    y += 15; DrawTextEx(f, "By translation:", {px + 25, y}, 18, 1, s.accent); y += 32;
    for (const auto& t : TRANSLATIONS) { int cnt = 0; auto it = s.cacheStats.byTranslation.find(t.code); if (it != s.cacheStats.byTranslation.end()) cnt = it->second; row(("  " + t.code + ":").c_str(), std::to_string(cnt) + " chapters"); }
    Rectangle clrBtn = { px + 25, py + ph - 50, 140, 34 }; bool clrHov = CheckCollisionPointRec(GetMousePosition(), clrBtn); DrawRectangleRec(clrBtn, clrHov ? s.err : s.hdr); DrawRectangleLinesEx(clrBtn, 1, s.err); DrawTextEx(f, "CLEAR CACHE", { clrBtn.x + 15, clrBtn.y + 8 }, 16, 1, clrHov ? RAYWHITE : s.err);
    if (clrHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { g_cache.ClearCache(); s.cacheStats = g_cache.Stats(); s.SetStatus("Cache cleared.", 2.0f); }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showCache = false;
}

void DrawGlobalSearchPanel(AppState& s, Font f) {
    float pw = 600, ph = 500, px = (GetScreenWidth() - pw) / 2.f, py = 100;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawTextEx(f, "Global Bible Search", {px + 20, py + 20}, 24, 1, s.accent);
    Rectangle box = {px + 20, py + 60, pw - 160, 40}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum); DrawTextEx(f, s.gSearchBuf, {box.x + 10, box.y + 10}, 20, 1, s.text);
    Rectangle sBtn = {px + pw - 130, py + 60, 110, 40}; bool sHov = CheckCollisionPointRec(GetMousePosition(), sBtn);
    DrawRectangleRec(sBtn, sHov ? s.accent : s.bg); DrawRectangleLinesEx(sBtn, 1, s.vnum); DrawTextEx(f, "SEARCH", {sBtn.x + 20, sBtn.y + 10}, 18, 1, sHov ? RAYWHITE : s.text);
    if (sHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.StartGlobalSearch();
    if (s.gSearchActive) { float progress = (float)s.gSearchProgress / BIBLE_BOOKS.size(); DrawRectangle(px + 20, py + 110, (pw - 40) * progress, 4, s.accent); }
    float ry = py + 130; BeginScissorMode(px + 20, (int)ry, (int)pw - 40, (int)ph - 190); static float scroll = 0;
    if (CheckCollisionPointRec(GetMousePosition(), {px + 20, ry, pw - 40, ph - 190})) scroll += GetMouseWheelMove() * 30; if (scroll > 0) scroll = 0;
    float itemY = ry + scroll;
    for (int i = 0; i < (int)s.gSearchResults.size(); i++) {
        const auto& m = s.gSearchResults[i]; std::string ref = m.bookName + " " + std::to_string(m.chapter) + ":" + std::to_string(m.verse); std::string preview = m.text.length() > 65 ? m.text.substr(0, 62) + "..." : m.text;
        Rectangle r = {px + 20, itemY, pw - 40, 45};
        if (itemY > ry - 50 && itemY < py + ph - 60) {
            bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60});
            DrawTextEx(f, ref.c_str(), {r.x + 5, r.y + 5}, 16, 1, s.vnum); DrawTextEx(f, preview.c_str(), {r.x + 5, r.y + 22}, 14, 1, s.text);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.curBookIdx = m.bookIdx; s.curChNum = m.chapter; s.scrollToVerse = m.verse; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.showGlobalSearch = false; }
        }
        itemY += 50;
    }
    EndScissorMode();
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showGlobalSearch = false;
}

void DrawSearchPanel(AppState& s, Font f) {
    float pw = 360, ph = 480, px = (float)GetScreenWidth() - pw - 15, py = 75;
    DrawRectangleRec({px, py, pw, ph}, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 80});
    DrawTextEx(f, "Search Current Buffer", {px + 15, py + 15}, 20, 1, s.accent);
    Rectangle box = {px + 15, py + 48, pw - 30, 34}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum); DrawTextEx(f, s.searchBuf, {box.x + 8, box.y + 8}, 18, 1, s.text);
    Rectangle gBtn = {px + 15, py + 90, pw - 30, 30}; bool gHov = CheckCollisionPointRec(GetMousePosition(), gBtn);
    DrawRectangleRec(gBtn, gHov ? s.accent : s.hdr); DrawRectangleLinesEx(gBtn, 1, s.vnum); DrawTextEx(f, "Search Entire Bible...", {gBtn.x + 10, gBtn.y + 7}, 15, 1, gHov ? RAYWHITE : s.vnum);
    if (gHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.showGlobalSearch = true; s.showSearch = false; strncpy(s.gSearchBuf, s.searchBuf, 255); }
    Rectangle cbr = {px + 15, py + 130, 18, 18}; DrawRectangleRec(cbr, s.searchCS ? s.accent : s.bg); DrawRectangleLinesEx(cbr, 1, s.vnum);
    if (s.searchCS) DrawTextEx(f, "v", {cbr.x + 3, cbr.y}, 14, 1, RAYWHITE); DrawTextEx(f, "Case sensitive", {cbr.x + 28, cbr.y}, 16, 1, s.text);
    if (CheckCollisionPointRec(GetMousePosition(), cbr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.searchCS = !s.searchCS; if (strlen(s.searchBuf) > 0) { std::vector<Verse> all; for (const auto& ch : s.buf) for (const auto& v : ch.verses) all.push_back(v); s.searchResults = SearchVerses(all, s.searchBuf, s.searchCS); } }
    DrawTextEx(f, (std::to_string(s.searchResults.size()) + " match(es)").c_str(), {px + 15, py + 155}, 15, 1, s.vnum);
    float ry = py + 180;
    for (int i = 0; i < (int)s.searchResults.size() && i < 7; i++) {
        const auto& m = s.searchResults[i]; std::string lbl = "v." + std::to_string(m.verseNumber) + ": " + (m.text.size() > 40 ? m.text.substr(0, 37) + "..." : m.text); Rectangle r = {px + 15, ry, pw - 30, 34}; bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); if (i == s.searchSel) DrawRectangleLinesEx(r, 1, s.vnum); DrawTextEx(f, lbl.c_str(), {r.x + 8, r.y + 8}, 15, 1, s.text);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.searchSel = i; ry += 38;
    }
    Rectangle cl = {px + pw - 100, py + ph - 45, 85, 30}; bool clHov = CheckCollisionPointRec(GetMousePosition(), cl); DrawRectangleRec(cl, clHov ? s.accent : s.bg); DrawRectangleLinesEx(cl, 1, s.vnum); DrawTextEx(f, "Close", {cl.x + 20, cl.y + 7}, 16, 1, clHov ? RAYWHITE : s.text);
    if (clHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.showSearch = false; memset(s.searchBuf, 0, sizeof(s.searchBuf)); s.searchResults.clear(); }
}

void DrawJumpPanel(AppState& s, Font f) {
    float pw = 420, ph = 200, px = (GetScreenWidth() - pw) / 2.f, py = 150;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawTextEx(f, "Go to Reference", {px + 20, py + 20}, 24, 1, s.accent);
    Rectangle box = {px + 20, py + 60, pw - 40, 40}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum); DrawTextEx(f, s.jumpBuf, {box.x + 10, box.y + 10}, 22, 1, s.text);
    DrawTextEx(f, "e.g. John 3:16  or  Psalm 23", {px + 20, py + 110}, 16, 1, s.vnum);
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showJump = false;
}

void DrawPlanPanel(AppState& s, Font f) {
    float pw = 420, ph = 320, px = (GetScreenWidth() - pw) / 2.f, py = 120;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawRectangleLinesEx({px + 4, py + 4, pw - 8, ph - 8}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    time_t now = time(NULL); struct tm* t = localtime(&now); int dayOfYear = t->tm_yday + 1; char dateStr[64]; strftime(dateStr, sizeof(dateStr), "%B %d, %Y", t);
    DrawTextEx(f, "Daily Reading Plan", {px + 20, py + 20}, 24, 1, s.accent); DrawTextEx(f, dateStr, {px + 20, py + 48}, 16, 1, s.vnum);
    auto plan = GetDailyReading(dayOfYear); float y = py + 85;
    for (const auto& p : plan) {
        std::string lbl = BIBLE_BOOKS[p.first].name + " " + std::to_string(p.second); Rectangle r = {px + 20, y, pw - 40, 44}; bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); DrawRectangleLinesEx(r, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 80}); DrawTextEx(f, lbl.c_str(), {r.x + 12, r.y + 12}, 20, 1, s.text);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.curBookIdx = p.first; s.curChNum = p.second; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.showPlan = false; } y += 48;
    }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showPlan = false;
}

void DrawHeader(AppState& s, Font f) {
    const float H = 60; DrawRectangle(0, 0, GetScreenWidth(), H, s.hdr); DrawLineEx({0, H - 2}, {(float)GetScreenWidth(), H - 2}, 2, s.vnum); DrawLineEx({0, H - 5}, {(float)GetScreenWidth(), H - 5}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawTextEx(f, "RayBible", {18, 17}, 28, 1, s.accent);
    
    auto navBtn = [&](float bx, const char* lbl, bool en, const char* tip) -> bool {
        Rectangle r = {bx, 15, 30, 30}; bool hov = en && CheckCollisionPointRec(GetMousePosition(), r);
        if (hov) strncpy(s.tooltip, tip, 63);
        if (en) { DrawRectangleRec(r, hov ? s.accent : s.bg); DrawRectangleLinesEx(r, 1, s.vnum); DrawTextEx(f, lbl, {r.x + (30-MeasureTextEx(f,lbl,16,1).x)/2, r.y+7}, 16, 1, hov ? RAYWHITE : s.text); }
        else { DrawRectangleLinesEx(r, 1, {s.vnum.r,s.vnum.g,s.vnum.b,60}); DrawTextEx(f, lbl, {r.x + (30-MeasureTextEx(f,lbl,16,1).x)/2, r.y+7}, 16, 1, {s.text.r,s.text.g,s.text.b,60}); }
        return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    };

    memset(s.tooltip, 0, sizeof(s.tooltip));
    if (navBtn(140, "<<", s.curBookIdx > 0, "Prev Book")) s.PrevBook();
    if (navBtn(175, ">>", s.curBookIdx < (int)BIBLE_BOOKS.size() - 1, "Next Book")) s.NextBook();
    if (navBtn(210, "<", s.navIndex > 0, "History Back")) s.GoBack();
    if (navBtn(245, ">", s.navIndex < (int)s.navHistory.size() - 1, "History Forward")) s.GoForward();

    Rectangle modeBtn = {285, 15, 80, 30}; bool mHov = CheckCollisionPointRec(GetMousePosition(), modeBtn);
    DrawRectangleRec(modeBtn, s.bookMode ? s.accent : (mHov ? Color{s.vnum.r, s.vnum.g, s.vnum.b, 80} : s.bg)); DrawRectangleLinesEx(modeBtn, 1, s.vnum);
    const char* modeL = s.bookMode ? "Book" : "Scroll"; DrawTextEx(f, modeL, {modeBtn.x + (modeBtn.width - MeasureTextEx(f, modeL, 16, 1).x)/2, modeBtn.y + 7}, 16, 1, s.bookMode ? RAYWHITE : s.text);
    if (mHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.bookMode = !s.bookMode; if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); }
    
    Rectangle bkBtn = {375, 15, 160, 30}; bool bkHov = CheckCollisionPointRec(GetMousePosition(), bkBtn);
    DrawRectangleRec(bkBtn, s.bg); DrawRectangleLinesEx(bkBtn, 1, bkHov ? s.accent : s.vnum);
    std::string bkName = BIBLE_BOOKS[s.curBookIdx].name; if ((int)bkName.size() > 14) bkName = bkName.substr(0, 12) + "..";
    DrawTextEx(f, bkName.c_str(), {bkBtn.x + 8, bkBtn.y + 6}, 17, 1, s.text); DrawTextEx(f, "v", {bkBtn.x + bkBtn.width - 18, bkBtn.y + 8}, 14, 1, s.vnum);
    if (bkHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showBookDrop = !s.showBookDrop;
    
    float cBase = 545; Rectangle cMinus = {cBase, 15, 30, 30}, cPlus = {cBase + 72, 15, 30, 30}; bool cmH = CheckCollisionPointRec(GetMousePosition(), cMinus), cpH = CheckCollisionPointRec(GetMousePosition(), cPlus);
    DrawRectangleRec(cMinus, cmH ? s.accent : s.bg); DrawRectangleLinesEx(cMinus, 1, s.vnum); DrawTextEx(f, "-", {cMinus.x + 11, cMinus.y + 4}, 20, 1, cmH ? RAYWHITE : s.text);
    std::string chLbl = std::to_string(s.curChNum); Vector2 chSz = MeasureTextEx(f, chLbl.c_str(), 17, 1); DrawTextEx(f, chLbl.c_str(), {cBase + 30 + (42 - chSz.x)/2, 22}, 17, 1, s.text);
    DrawRectangleRec(cPlus, cpH ? s.accent : s.bg); DrawRectangleLinesEx(cPlus, 1, s.vnum); DrawTextEx(f, "+", {cPlus.x + 10, cPlus.y + 4}, 20, 1, cpH ? RAYWHITE : s.text);
    if (cmH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PrevChapter(s.curBookIdx, s.curChNum); s.scrollY = 0; s.targetScrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
    if (cpH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { NextChapter(s.curBookIdx, s.curChNum); s.scrollY = 0; s.targetScrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
    
    float tx = 655; Rectangle tBtn = {tx, 15, 80, 30}; bool tHov = CheckCollisionPointRec(GetMousePosition(), tBtn);
    DrawRectangleRec(tBtn, s.bg); DrawRectangleLinesEx(tBtn, 1, tHov ? s.accent : s.vnum);
    std::string tLbl = TRANSLATIONS[s.transIdx].code; std::transform(tLbl.begin(), tLbl.end(), tLbl.begin(), ::toupper);
    DrawTextEx(f, tLbl.c_str(), {tBtn.x + 8, tBtn.y + 6}, 17, 1, s.text); DrawTextEx(f, "v", {tBtn.x + tBtn.width - 16, tBtn.y + 8}, 14, 1, s.vnum);
    if (tHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showTransDrop = !s.showTransDrop;
    if (s.parallelMode) {
        Rectangle tBtn2 = {tx + 86, 15, 80, 30}; bool tHov2 = CheckCollisionPointRec(GetMousePosition(), tBtn2); DrawRectangleRec(tBtn2, s.bg); DrawRectangleLinesEx(tBtn2, 1, tHov2 ? s.accent : s.vnum);
        std::string tLbl2 = TRANSLATIONS[s.transIdx2].code; std::transform(tLbl2.begin(), tLbl2.end(), tLbl2.begin(), ::toupper);
        DrawTextEx(f, tLbl2.c_str(), {tBtn2.x + 8, tBtn2.y + 6}, 17, 1, s.text); DrawTextEx(f, "v", {tBtn2.x + tBtn2.width - 16, tBtn2.y + 8}, 14, 1, s.vnum);
        if (tHov2 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showTransDrop2 = !s.showTransDrop2;
    }
    float fx = s.parallelMode ? tx + 172 : tx + 86; Rectangle fMinus = {fx, 15, 28, 30}, fPlus = {fx + 54, 15, 28, 30}; bool fmH = CheckCollisionPointRec(GetMousePosition(), fMinus), fpH = CheckCollisionPointRec(GetMousePosition(), fPlus);
    DrawRectangleRec(fMinus, fmH ? s.accent : s.bg); DrawRectangleLinesEx(fMinus, 1, s.vnum); DrawTextEx(f, "-", {fMinus.x + 9, fMinus.y + 4}, 20, 1, fmH ? RAYWHITE : s.text);
    DrawTextEx(f, std::to_string((int)s.fontSize).c_str(), {fx + 30, 22}, 16, 1, s.text); DrawRectangleRec(fPlus, fpH ? s.accent : s.bg); DrawRectangleLinesEx(fPlus, 1, s.vnum); DrawTextEx(f, "+", {fPlus.x + 8, fPlus.y + 4}, 20, 1, fpH ? RAYWHITE : s.text);
    if (fmH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.fontSize -= 1; if (s.fontSize < 10) s.fontSize = 10; if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); }
    if (fpH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.fontSize += 1; if (s.fontSize > 40) s.fontSize = 40; if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); }
    
    auto iconBtn = [&](float bx, const char* lbl, bool act, const char* tip) -> bool {
        Rectangle r = {bx, 15, 44, 30}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) strncpy(s.tooltip, tip, 63);
        Color bg2 = act ? s.accent : (hov ? Color{s.vnum.r, s.vnum.g, s.vnum.b, 80} : s.bg); DrawRectangleRec(r, bg2); DrawRectangleLinesEx(r, 1, s.vnum);
        Vector2 ts = MeasureTextEx(f, lbl, 14, 1); DrawTextEx(f, lbl, {r.x + (r.width - ts.x) / 2, r.y + 8}, 14, 1, act ? RAYWHITE : s.text); return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    };
    float ibx = (float)GetScreenWidth() - 54;
    if (iconBtn(ibx, "Menu", s.showBurgerMenu, "Menu")) s.showBurgerMenu = !s.showBurgerMenu;
    if (iconBtn(ibx - 50, "Srch", s.showSearch, "Search (Ctrl+F)")) s.showSearch = !s.showSearch;
    if (iconBtn(ibx - 100, "Jump", s.showJump, "Jump (Ctrl+J)")) s.showJump = !s.showJump;
    if (iconBtn(ibx - 150, "Par", s.parallelMode, "Parallel")) s.ToggleParallelMode(f);
    if (iconBtn(ibx - 200, "Copy", false, "Copy Chapter")) s.CopyChapter();
    if (iconBtn(ibx - 250, s.darkMode ? "Day" : "Ngt", false, "Theme (Ctrl+D)")) s.ToggleDarkMode();
    
    if (s.showTransDrop) {
        float dy = H, dh = TRANSLATIONS.size() * 36.0f; DrawRectangle(tx, dy, 148, dh, s.hdr); DrawRectangleLinesEx({tx, dy, 148, dh}, 1, s.accent);
        for (int i = 0; i < (int)TRANSLATIONS.size(); i++) {
            Rectangle r = {tx, dy + i * 36.f, 148, 36}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, s.accent);
            DrawTextEx(f, TRANSLATIONS[i].name.c_str(), {r.x + 8, r.y + 8}, 15, 1, hov ? RAYWHITE : s.text); if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.transIdx = i; s.trans = TRANSLATIONS[i].code; s.showTransDrop = false; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); }
        }
    }
    if (s.showTransDrop2) {
        float tx2 = tx + 86, dy = H, dh = TRANSLATIONS.size() * 36.0f; DrawRectangle(tx2, dy, 148, dh, s.hdr); DrawRectangleLinesEx({tx2, dy, 148, dh}, 1, s.vnum);
        for (int i = 0; i < (int)TRANSLATIONS.size(); i++) {
            Rectangle r = {tx2, dy + i * 36.f, 148, 36}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, s.accent);
            DrawTextEx(f, TRANSLATIONS[i].name.c_str(), {r.x + 8, r.y + 8}, 15, 1, hov ? RAYWHITE : s.text); if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.transIdx2 = i; s.trans2 = TRANSLATIONS[i].code; s.showTransDrop2 = false; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); }
        }
    }
    if (s.showBookDrop) {
        float dy = H, visH = 400, bkW = 200; DrawRectangle(375, dy, bkW, visH, s.hdr); DrawRectangleLinesEx({375, dy, bkW, visH}, 1, s.vnum); BeginScissorMode(375, (int)dy, (int)bkW, (int)visH);
        for (int i = 0; i < (int)BIBLE_BOOKS.size(); i++) {
            float iy = dy + i * 32.f - s.bookDropScroll; if (iy < dy - 32 || iy > dy + visH) continue;
            Rectangle r = {375, iy, bkW, 32}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, s.accent);
            DrawTextEx(f, BIBLE_BOOKS[i].name.c_str(), {r.x + 8, r.y + 7}, 16, 1, hov ? RAYWHITE : s.text); if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.curBookIdx = i; s.curChNum = 1; s.showBookDrop = false; s.bookDropScroll = 0; s.scrollY = 0; s.targetScrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
        }
        EndScissorMode(); if (CheckCollisionPointRec(GetMousePosition(), {375, dy, bkW, visH})) { s.bookDropScroll -= GetMouseWheelMove() * 32; float maxScroll = BIBLE_BOOKS.size() * 32.f - visH; if (s.bookDropScroll < 0) s.bookDropScroll = 0; if (s.bookDropScroll > maxScroll) s.bookDropScroll = maxScroll; }
    }
}

void DrawScrollMode(AppState& s, Font f) {
    std::lock_guard<std::mutex> lock(s.bufferMutex);
    const float TOP = 60, BOT = 38; float h = GetScreenHeight() - TOP - BOT;
    bool overlayOpen = s.showSearch || s.showHistory || s.showFavorites || s.showCache || s.showBookDrop || s.showTransDrop || s.showTransDrop2 || s.showJump || s.showPlan || s.showGlobalSearch || s.showHelp || s.showBurgerMenu || s.showNoteEditor;
    if (!overlayOpen && !s.isLoading) { float wheel = GetMouseWheelMove(); if (CheckCollisionPointRec(GetMousePosition(), {0, TOP, (float)GetScreenWidth(), h})) s.targetScrollY += wheel * 100.0f; }
    const float PAD = 40, FS = s.fontSize, LS = s.lineSpacing, VG = 14; BeginScissorMode(0, (int)TOP, GetScreenWidth(), (int)h); float yFinal = TOP + 18 + s.scrollY;
    
    s.scrollChapterIdx = 0;

    if (s.parallelMode) {
        float colW = (GetScreenWidth() - PAD * 3) / 2.0f, y1 = TOP + 18 + s.scrollY, y2 = TOP + 18 + s.scrollY;
        for (int ci = 0; ci < (int)s.buf.size(); ci++) {
            const Chapter& ch = s.buf[ci]; if (!ch.isLoaded) continue;
            if (y1 <= TOP + 50 && y1 + 100 > TOP) s.scrollChapterIdx = ci;
            float chapterStartY = y1;
            DrawTextEx(f, ch.book.c_str(), {PAD, y1}, 24, 1, s.accent); DrawTextEx(f, ch.translation.c_str(), {PAD + colW - 40, y1 + 6}, 12, 1, s.vnum); y1 += 34; DrawLineEx({PAD, y1}, {PAD + colW, y1}, 2, s.vnum); y1 += 14;
            for (const auto& v : ch.verses) { bool isFav = g_favs.Has(ch.book, ch.chapter, v.number, ch.translation); if (isFav) DrawRectangleRec({PAD - 5, y1 - 2, colW + 10, FS + LS + 4}, {s.vnum.r, s.vnum.g, s.vnum.b, 25}); if (s.scrollToVerse == v.number && ci == 0) { s.targetScrollY = -(y1 - s.scrollY - TOP - 20); s.scrollToVerse = -1; } Rectangle vRec = {PAD, y1, colW, FS + 4}; bool vHov = CheckCollisionPointRec(GetMousePosition(), vRec); DrawVerseText(f, v, PAD, y1, colW, FS, LS, VG, s.text, s.vnum, isFav, vHov, {}, {220, 180, 60, 120}); if (vHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { if (GetMouseX() < PAD + 40) { if (isFav) g_favs.Remove(ch.book, ch.chapter, v.number, ch.translation); else g_favs.Add(ch.book, ch.chapter, v.number, ch.translation, v.text); } } if (vHov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { std::string quote = "\"" + v.text + "\"\n\xE2\x80\x94 " + ch.book + ":" + std::to_string(v.number) + " (" + ch.translation + ")"; CopyToClipboard(quote); SaveVerseImage(v, ch.book, ch.translation, f); s.SetStatus("Verse copied & Image saved!"); } }
            
            float leftEndY = y1; y2 = chapterStartY;
            if (ci < (int)s.buf2.size()) {
                const Chapter& ch2 = s.buf2[ci];
                if (ch2.isLoaded) {
                    DrawTextEx(f, ch2.book.c_str(), {PAD * 2 + colW, y2}, 24, 1, s.accent); DrawTextEx(f, ch2.translation.c_str(), {PAD * 2 + colW * 2 - 40, y2 + 6}, 12, 1, s.vnum); y2 += 34; DrawLineEx({PAD * 2 + colW, y2}, {PAD * 2 + colW * 2, y2}, 2, s.vnum); y2 += 14; 
                    for (const auto& v : ch2.verses) { DrawVerseText(f, v, PAD * 2 + colW, y2, colW, FS, LS, VG, s.text, s.vnum, false, false, {}, {220, 180, 60, 120}); }
                }
            }
            y1 = y2 = std::max(leftEndY, y2) + 40;
        }
        yFinal = y1;
    } else {
        const float TW = GetScreenWidth() - PAD * 2; float y = TOP + 18 + s.scrollY;
        for (int ci = 0; ci < (int)s.buf.size(); ci++) {
            const Chapter& ch = s.buf[ci]; if (!ch.isLoaded) continue;
            if (y <= TOP + 50 && y + 100 > TOP) s.scrollChapterIdx = ci;
            DrawTextEx(f, ch.book.c_str(), {PAD, y}, 28, 1, s.accent); y += 38; DrawLineEx({PAD, y}, {(float)GetScreenWidth() - PAD, y}, 2, s.vnum); DrawLineEx({PAD, y + 3}, {(float)GetScreenWidth() - PAD, y + 3}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 80}); y += 18;
            for (const auto& v : ch.verses) { bool isFav = g_favs.Has(ch.book, ch.chapter, v.number, ch.translation); if (isFav) DrawRectangleRec({PAD - 10, y - 2, TW + 20, s.fontSize + s.lineSpacing + 4}, {s.vnum.r, s.vnum.g, s.vnum.b, 25}); if (s.scrollToVerse == v.number && ci == 0) { s.targetScrollY = -(y - s.scrollY - TOP - 20); s.scrollToVerse = -1; } Rectangle numR = {PAD, y, TW, FS + 4}; bool numHov = CheckCollisionPointRec(GetMousePosition(), numR); std::vector<SearchMatch> vm; for (const auto& m : s.searchResults) if (m.verseNumber == v.number) vm.push_back(m); DrawVerseText(f, v, PAD, y, TW, FS, LS, VG, s.text, s.vnum, isFav, numHov, vm, {220, 180, 60, 120}); if (numHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { if (GetMouseX() < PAD + 50) { if (isFav) g_favs.Remove(ch.book, ch.chapter, v.number, ch.translation); else g_favs.Add(ch.book, ch.chapter, v.number, ch.translation, v.text); } } if (numHov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { std::string quote = "\"" + v.text + "\"\n\xE2\x80\x94 " + ch.book + ":" + std::to_string(v.number) + " (" + ch.translation + ")"; CopyToClipboard(quote); SaveVerseImage(v, ch.book, ch.translation, f); s.SetStatus("Verse copied & Image saved!"); } }
            y += 40;
        }
        yFinal = y;
    }
    EndScissorMode(); float contentH = yFinal - TOP - s.scrollY; if (contentH > h) { float barH = (h / contentH) * h; if (barH < 30) barH = 30; float barY = TOP + (-s.scrollY / (contentH - h)) * (h - barH); Rectangle scrollRect = { (float)GetScreenWidth() - 10, barY, 6, barH }; DrawRectangleRec(scrollRect, { s.vnum.r, s.vnum.g, s.vnum.b, 150 }); if (CheckCollisionPointRec(GetMousePosition(), { (float)GetScreenWidth() - 15, TOP, 15, h }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { float delta = GetMouseDelta().y; s.targetScrollY -= delta * (contentH / h); } }
    float ay = TOP + h / 2.0f - 25; auto drawFloatNav = [&](Rectangle r, const char* lbl, bool en) { bool hov = en && CheckCollisionPointRec(GetMousePosition(), r); if (en) { DrawRectangleRec(r, hov ? s.accent : (Color){ s.hdr.r, s.hdr.g, s.hdr.b, 180 }); DrawRectangleLinesEx(r, 2, s.vnum); Vector2 sz = MeasureTextEx(f, lbl, 24, 1); DrawTextEx(f, lbl, { r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2 }, 24, 1, hov ? RAYWHITE : s.text); } return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON); };
    if (drawFloatNav({ 5, ay, 40, 50 }, "<", !(s.curBookIdx == 0 && s.curChNum == 1))) { PrevChapter(s.curBookIdx, s.curChNum); s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
    if (drawFloatNav({ (float)GetScreenWidth() - 45, ay, 40, 50 }, ">", true)) { NextChapter(s.curBookIdx, s.curChNum); s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
    float totalH = yFinal - TOP - s.scrollY; float maxUp = -(totalH - h + 60); if (s.targetScrollY > 60) s.targetScrollY = 60; 
    if (!s.isLoading && s.targetScrollY < maxUp) s.targetScrollY = maxUp;
    if (!s.isLoading) { if (s.targetScrollY > 20) s.GrowTop(); if (s.targetScrollY < maxUp + 220) s.GrowBottom(); }
}

void DrawBookMode(AppState& s, Font f) {
    std::lock_guard<std::mutex> lock(s.bufferMutex);
    const float TOP = 60, BOT = 38; float ch = GetScreenHeight() - TOP - BOT; float pageW = 700, pageH = std::min(500.f, ch - 40), pageX = (GetScreenWidth() - pageW) / 2.f, pageY = TOP + (ch - pageH) / 2.f;
    if (s.pages.empty()) { const char* msg = s.isLoading ? "Loading..." : "No content loaded yet."; Vector2 ms = MeasureTextEx(f, msg, 18, 1); DrawTextEx(f, msg, {(GetScreenWidth() - ms.x) / 2.f, TOP + ch / 2.f - 9}, 18, 1, s.vnum); return; }
    DrawRectangle((int)(pageX + 6), (int)(pageY + 6), (int)pageW, (int)pageH, s.pageShadow); DrawRectangle((int)pageX, (int)pageY, (int)pageW, (int)pageH, s.pageBg); DrawRectangleLinesEx({pageX, pageY, pageW, pageH}, 2, s.accent);
    const Page& pg = s.pages[s.pageIdx]; float ty = pageY + 22;
    if (pg.isChapterStart && pg.chapterBufIndex < (int)s.buf.size()) {
        const std::string& hdr = s.buf[pg.chapterBufIndex].book; DrawTextEx(f, hdr.c_str(), {pageX + 28, ty}, 21, 1, s.accent);
        if (s.parallelMode) { std::string t1 = s.trans, t2 = s.trans2; std::transform(t1.begin(), t1.end(), t1.begin(), ::toupper); std::transform(t2.begin(), t2.end(), t2.begin(), ::toupper); DrawTextEx(f, t1.c_str(), {pageX + 28, ty + 24}, 12, 1, s.vnum); DrawTextEx(f, t2.c_str(), {pageX + pageW/2 + 12, ty + 24}, 12, 1, s.vnum); }
        DrawLineEx({pageX + 28, ty + 38}, {pageX + pageW - 28, ty + 38}, 1, {s.accent.r, s.accent.g, s.accent.b, 80}); ty += 52;
    }
    if (!s.parallelMode) { for (const auto& line : pg.lines) { DrawTextEx(f, line.c_str(), {pageX + 28, ty}, s.fontSize, 1, s.text); ty += s.fontSize + s.lineSpacing; } }
    else { float startTy = ty; for (const auto& line : pg.lines) { DrawTextEx(f, line.c_str(), {pageX + 28, ty}, s.fontSize, 1, s.text); ty += s.fontSize + s.lineSpacing; } ty = startTy; for (const auto& line : pg.lines2) { DrawTextEx(f, line.c_str(), {pageX + pageW/2 + 12, ty}, s.fontSize, 1, s.text); ty += s.fontSize + s.lineSpacing; } }
    std::string pnum = "Page " + std::to_string(s.pageIdx + 1) + " / " + std::to_string(s.pages.size()); Vector2 pns = MeasureTextEx(f, pnum.c_str(), 13, 1); DrawTextEx(f, pnum.c_str(), {pageX + (pageW - pns.x) / 2.f, pageY + pageH - 22}, 13, 1, s.vnum);
    DrawTextEx(f, ("vv." + std::to_string(pg.startVerse) + "-" + std::to_string(pg.endVerse)).c_str(), {pageX + pageW - 88, pageY + 8}, 12, 1, s.vnum);
    float ay = pageY + pageH / 2.f - 25; Rectangle prevBtn = {pageX - 60, ay, 40, 50}, nextBtn = {pageX + pageW + 20, ay, 40, 50}; bool prevHov = CheckCollisionPointRec(GetMousePosition(), prevBtn), nextHov = CheckCollisionPointRec(GetMousePosition(), nextBtn), atStart = (s.pageIdx == 0 && s.buf.front().bookIndex == 0 && s.buf.front().chapter == 1);
    if (!atStart) { DrawRectangleRec(prevBtn, prevHov ? s.accent : (Color){ s.hdr.r, s.hdr.g, s.hdr.b, 180 }); DrawRectangleLinesEx(prevBtn, 2, s.vnum); DrawTextEx(f, "<", {prevBtn.x + 13, prevBtn.y + 12}, 24, 1, prevHov ? RAYWHITE : s.text); if (prevHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.BookPagePrev(f); }
    DrawRectangleRec(nextBtn, nextHov ? s.accent : (Color){ s.hdr.r, s.hdr.g, s.hdr.b, 180 }); DrawRectangleLinesEx(nextBtn, 2, s.vnum); DrawTextEx(f, ">", {nextBtn.x + 13, nextBtn.y + 12}, 24, 1, nextHov ? RAYWHITE : s.text); if (nextHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.BookPageNext(f);
}

void DrawFooter(AppState& s, Font f) {
    const float FH = 38; float fy = (float)GetScreenHeight() - FH; DrawRectangle(0, (int)fy, GetScreenWidth(), (int)FH, s.hdr); DrawLineEx({0, fy}, {(float)GetScreenWidth(), fy}, 1, s.vnum);
    if (s.isLoading) { float angle = (float)GetTime() * 300.0f; DrawPolyLinesEx({ (float)GetScreenWidth() - 30, fy + 19 }, 6, 10, angle, 2, s.accent); DrawTextEx(f, "Loading...", { (float)GetScreenWidth() - 110, fy + 10 }, 14, 1, s.accent); }
    if (!s.buf.empty()) { std::lock_guard<std::mutex> lock(s.bufferMutex); int ci = s.scrollChapterIdx; if (s.bookMode && s.pageIdx < (int)s.pages.size()) ci = s.pages[s.pageIdx].chapterBufIndex; if (ci < (int)s.buf.size() && s.buf[ci].isLoaded) { std::string loc = s.buf[ci].book + " (" + s.buf[ci].translation + ")" + "  \xC2\xB7  " + std::to_string(s.buf.size()) + " ch loaded"; DrawTextEx(f, loc.c_str(), {18, fy + 10}, 14, 1, s.vnum); } }
    if (s.statusTimer > 0) { Vector2 ss = MeasureTextEx(f, s.statusMsg.c_str(), 15, 1); DrawTextEx(f, s.statusMsg.c_str(), {(GetScreenWidth() - ss.x) / 2.f, fy + 10}, 15, 1, s.ok); }
    const char* hint = s.bookMode ? "Arrow keys / < > = turn page  (auto-loads chapters)" : "Scroll = infinite  |  click verse # = star  |  right-click = copy"; Vector2 hs = MeasureTextEx(f, hint, 12, 1); DrawTextEx(f, hint, {(float)GetScreenWidth() - hs.x - 16, fy + 12}, 12, 1, s.vnum);
}