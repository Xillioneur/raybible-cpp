#include "ui_renderer.h"
#include "bible_logic.h"
#include "managers.h"
#include "utils.h"
#include <algorithm>
#include <cstring>
#include <sstream>

// --- Common Helpers ---

static std::string FmtBytes(long b) { 
    if (b < 1024) return std::to_string(b) + " B"; 
    if (b < 1 << 20) return std::to_string(b / 1024) + " KB"; 
    return std::to_string(b / (1 << 20)) + " MB"; 
}

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

static std::vector<std::string> WrapStudyText(const std::string& raw, Font font, float fontSize, float maxWidth) {
    std::vector<std::string> lines;
    if (maxWidth <= 0) return lines;
    auto getClean = [](const std::string& s) {
        std::string r; bool in = false;
        for (char c : s) { if (c == '<') in = true; else if (c == '>') in = false; else if (!in) r += c; }
        return r;
    };
    std::string curLine, cleanCurLine;
    std::istringstream ws(raw); std::string part;
    while (ws >> part) {
        std::string cleanPart = getClean(part);
        std::string testLine = cleanCurLine.empty() ? cleanPart : cleanCurLine + " " + cleanPart;
        float extraWidth = (float)part.size() * (fontSize * 0.1f); 
        if (MeasureTextEx(font, testLine.c_str(), fontSize, 1).x + extraWidth > maxWidth && !curLine.empty()) {
            lines.push_back(curLine); curLine = part; cleanCurLine = cleanPart;
        } else { curLine = curLine.empty() ? part : curLine + " " + part; cleanCurLine = testLine; }
    }
    if (!curLine.empty()) lines.push_back(curLine);
    return lines;
}

std::vector<Page> BuildPages(const std::deque<Chapter>& chapters, const std::deque<Chapter>& chapters2, bool parallelMode, Font font, float pageW, float pageH, float fSize, float lSpacing) {
    std::vector<Page> pages;
    if (chapters.empty()) return pages;
    const float verseGap = 14.0f, footerRoom = 50.0f, headingH = 42.0f;
    if (!parallelMode) {
        for (int ci = 0; ci < (int)chapters.size(); ci++) {
            const Chapter& ch = chapters[ci]; if (!ch.isLoaded || ch.verses.empty()) continue;
            Page cur{}; cur.chapterBufIndex = ci; cur.isChapterStart = true; cur.startVerse = ch.verses[0].number;
            float usedY = headingH;
            for (int vi = 0; vi < (int)ch.verses.size(); vi++) {
                const auto& v = ch.verses[vi]; std::string nl = std::to_string(v.number) + " "; Vector2 nsz = MeasureTextEx(font, nl.c_str(), fSize, 1);
                auto lines = WrapText(v.text, font, fSize, pageW - nsz.x - 60.0f); float vH = (float)lines.size() * (fSize + lSpacing) + verseGap;
                if (usedY + vH > pageH - footerRoom && !cur.lines.empty()) { pages.push_back(cur); cur.lines.clear(); cur.lineVerses.clear(); cur.chapterBufIndex = ci; cur.isChapterStart = false; cur.startVerse = v.number; usedY = 0; }
                for (size_t li = 0; li < lines.size(); li++) { if (li == 0) cur.lines.push_back(nl + lines[li]); else cur.lines.push_back(std::string(4, ' ') + lines[li]); cur.lineVerses.push_back(v.number); }
                usedY += vH; cur.endVerse = v.number;
            }
            if (!cur.lines.empty()) pages.push_back(cur);
        }
    } else {
        for (int ci = 0; ci < (int)chapters.size(); ci++) {
            const Chapter& ch1 = chapters[ci]; const Chapter* ch2 = (ci < (int)chapters2.size()) ? &chapters2[ci] : nullptr;
            if (!ch1.isLoaded || ch1.verses.empty()) continue;
            Page cur{}; cur.chapterBufIndex = ci; cur.isChapterStart = true; cur.startVerse = ch1.verses[0].number;
            float colW = (pageW - 80.0f) / 2.0f, usedY = headingH; size_t maxVerses = std::max(ch1.verses.size(), (ch2 && ch2->isLoaded) ? ch2->verses.size() : 0);
            for (size_t vi = 0; vi < maxVerses; vi++) {
                std::vector<std::string> v1Lines, v2Lines; float v1H = 0, v2H = 0; int v1Num = -1, v2Num = -1;
                if (vi < ch1.verses.size()) { const auto& v = ch1.verses[vi]; v1Num = v.number; std::string nl = std::to_string(v.number) + " "; Vector2 nsz = MeasureTextEx(font, nl.c_str(), fSize, 1); auto lines = WrapText(v.text, font, fSize, colW - nsz.x - 5.0f); for (size_t li = 0; li < lines.size(); li++) { if (li == 0) v1Lines.push_back(nl + lines[li]); else v1Lines.push_back(std::string(4, ' ') + lines[li]); } v1H = (float)v1Lines.size() * (fSize + lSpacing) + verseGap; }
                if (ch2 && ch2->isLoaded && vi < ch2->verses.size()) { const auto& v = ch2->verses[vi]; v2Num = v.number; std::string nl = std::to_string(v.number) + " "; Vector2 nsz = MeasureTextEx(font, nl.c_str(), fSize, 1); auto lines = WrapText(v.text, font, fSize, colW - nsz.x - 5.0f); for (size_t li = 0; li < lines.size(); li++) { if (li == 0) v2Lines.push_back(nl + lines[li]); else v2Lines.push_back(std::string(4, ' ') + lines[li]); } v2H = (float)v2Lines.size() * (fSize + lSpacing) + verseGap; }
                float vMaxH = std::max(v1H, v2H); if (usedY + vMaxH > pageH - footerRoom && (!cur.lines.empty() || !cur.lines2.empty())) { pages.push_back(cur); cur.lines.clear(); cur.lines2.clear(); cur.lineVerses.clear(); cur.lineVerses2.clear(); cur.chapterBufIndex = ci; cur.isChapterStart = false; cur.startVerse = (vi < ch1.verses.size()) ? ch1.verses[vi].number : ch1.verses.back().number; usedY = 0; }
                cur.lines.insert(cur.lines.end(), v1Lines.begin(), v1Lines.end()); for (size_t i = 0; i < v1Lines.size(); i++) cur.lineVerses.push_back(v1Num);
                cur.lines2.insert(cur.lines2.end(), v2Lines.begin(), v2Lines.end()); for (size_t i = 0; i < v2Lines.size(); i++) cur.lineVerses2.push_back(v2Num);
                int diff = (int)v1Lines.size() - (int)v2Lines.size(); if (diff < 0) for (int i = 0; i < -diff; i++) { cur.lines.push_back(""); cur.lineVerses.push_back(-1); } else if (diff > 0) for (int i = 0; i < diff; i++) { cur.lines2.push_back(""); cur.lineVerses2.push_back(-1); }
                cur.lines.push_back(""); cur.lineVerses.push_back(-1); cur.lines2.push_back(""); cur.lineVerses2.push_back(-1); usedY += vMaxH; if (vi < ch1.verses.size()) cur.endVerse = ch1.verses[vi].number;
            }
            if (!cur.lines.empty() || !cur.lines2.empty()) pages.push_back(cur);
        }
    }
    return pages;
}

void DrawLogo(float x, float y, float size, Color accent, Color gold) {
    float h = size, w = size * 1.2f; DrawCircleGradient((int)(x + w/2), (int)(y + h/2), size * 0.8f, {accent.r, accent.g, accent.b, 40}, {0,0,0,0});
    DrawRectangleRounded({x, y, w/2, h}, 0.2f, 8, accent); DrawRectangleRounded({x + w/2 + 2, y, w/2, h}, 0.2f, 8, accent);
    DrawRectangleRounded({x + w/2 - 2, y - 2, 6, h + 4}, 1.0f, 4, gold); DrawRectangle(x + w * 0.7f, y, 4, h * 0.6f, gold);
}

void SaveVerseImage(const Verse& v, const std::string& ref, const std::string& trans, Font font) {
    const int W = 800, H = 450; RenderTexture2D target = LoadRenderTexture(W, H); BeginTextureMode(target); ClearBackground({20, 20, 20, 255});
    DrawRectangleLinesEx({10, 10, (float)W - 20, (float)H - 20}, 3, {180, 140, 40, 255}); DrawRectangleLinesEx({20, 20, (float)W - 40, (float)H - 40}, 1, {140, 20, 20, 255});
    float fs = 32.0f; std::string quote = "\"" + v.text + "\""; auto lines = WrapText(quote, font, fs, (float)W - 120); float totalTxtH = (float)lines.size() * (fs + 8); float ty = ((float)H - totalTxtH) / 2.0f - 30;
    for (const auto& line : lines) { Vector2 sz = MeasureTextEx(font, line.c_str(), fs, 1); DrawTextEx(font, line.c_str(), {((float)W - sz.x) / 2.0f, ty}, fs, 1, {245, 240, 225, 255}); ty += fs + 8; }
    std::string refStr = ref + ":" + std::to_string(v.number) + " (" + trans + ")"; Vector2 rsz = MeasureTextEx(font, refStr.c_str(), 24, 1); DrawTextEx(font, refStr.c_str(), {((float)W - rsz.x) / 2.0f, ty + 20}, 24, 1, {180, 140, 40, 255});
    DrawTextEx(font, "Divine Word", {(float)W - 140, (float)H - 45}, 18, 1, {140, 20, 20, 150}); EndTextureMode();
    Image img = LoadImageFromTexture(target.texture); ImageFlipVertical(&img); std::string filename = "verse_" + std::to_string(time(NULL)) + ".png"; ExportImage(img, filename.c_str()); UnloadImage(img); UnloadRenderTexture(target);
}

// --- Internal Rendering ---

static void DrawStudyLine(Font font, const std::string& line, float x, float y, float fSize, Color textCol, Color tagCol, AppState& s) {
    float curX = x; bool inTag = false, inStrongs = false; std::string tagContent, word;
    auto flushWord = [&]() { if (word.empty()) return; DrawTextEx(font, word.c_str(), {curX, y}, fSize, 1, textCol); curX += MeasureTextEx(font, word.c_str(), fSize, 1).x; word = ""; };
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '<') { flushWord(); inTag = true; tagContent = ""; }
        else if (c == '>') { inTag = false; if (tagContent == "S" || tagContent == "s") inStrongs = true; else if (tagContent == "/S" || tagContent == "/s") inStrongs = false; }
        else if (inTag) { tagContent += c; }
        else if (inStrongs) { std::string num; while (i < line.size() && line[i] != '<') { num += line[i]; i++; } i--; 
            float tagFSize = fSize * 0.55f; Vector2 sz = MeasureTextEx(font, num.c_str(), tagFSize, 1); Rectangle r = {curX, y, sz.x + 2, tagFSize + 2}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); DrawTextEx(font, num.c_str(), {curX, y}, tagFSize, 1, hov ? RAYWHITE : tagCol); if (hov) { strncpy(s.tooltip, "Study Word", 63); if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.LookupStrongs(num); s.showSidebar = true; } } curX += sz.x + 3; }
        else { if (c == ' ') { flushWord(); curX += MeasureTextEx(font, " ", fSize, 1).x; } else word += c; }
    }
    flushWord();
}

static void DrawVerseText(Font font, const Verse& v, float x, float& y, float maxW, float fSize, float lSpacing, float vGap, Color textCol, Color numCol, bool hovered, const std::vector<SearchMatch>& matches, Color hlCol, AppState& s, const std::string& book, int chapter, const std::string& trans) {
    auto* vd = g_study.Get(book, chapter, v.number, trans);
    int colorIdx = vd ? vd->highlightColor : 0; bool isBookmarked = vd ? vd->isBookmarked : false; bool hasNote = vd ? !vd->note.empty() : false;
    bool isSelected = s.selectedVerses.count(v.number);
    if (colorIdx > 0 || isSelected) { 
        Color hcs[] = {BLANK, {255,255,0,80}, {0,255,0,80}, {0,200,255,80}, {255,100,200,80}};
        Color fill = isSelected ? Color{s.accent.r, s.accent.g, s.accent.b, 40} : hcs[colorIdx];
        DrawRectangleRec({x - 5, y - 2, maxW + 10, fSize + lSpacing + 4}, fill); 
    }
    std::string numLabel = std::to_string(v.number); float numFSize = fSize * 0.6f; Vector2 numSz = MeasureTextEx(font, numLabel.c_str(), numFSize, 1);
    Color nc = isBookmarked ? Color{255, 210, 60, 255} : numCol; DrawTextEx(font, numLabel.c_str(), {x, y + 2}, numFSize, 1, nc); 
    if (hasNote) { DrawCircleGradient((int)(x + numSz.x + 6), (int)(y + 8), 3, s.accent, {0,0,0,0}); }
    float tx = x + numSz.x + (hovered || isBookmarked ? 15.0f : 8.0f);
    if (s.studyMode && !v.rawText.empty()) { auto lines = WrapStudyText(v.rawText, font, fSize, maxW - (tx - x)); for (const auto& ln : lines) { float rx = (&ln == &lines[0]) ? tx : x + 10.0f; DrawStudyLine(font, ln, rx, y, fSize, textCol, {200, 160, 40, 200}, s); y += fSize + lSpacing; } }
    else { auto lines = WrapText(v.text, font, fSize, maxW - (tx - x)); for (size_t li = 0; li < lines.size(); li++) { float rx = (li == 0) ? tx : x + 10.0f; if (!matches.empty()) { std::string lineLower = ToLower(lines[li]); for (const auto& m : matches) { if (m.matchPos < v.text.size()) { std::string matchStr = ToLower(v.text.substr(m.matchPos, std::min(m.matchLen, v.text.size() - m.matchPos))); size_t p = 0; while ((p = lineLower.find(matchStr, p)) != std::string::npos) { Vector2 pre = MeasureTextEx(font, lines[li].substr(0, p).c_str(), fSize, 1); Vector2 mid = MeasureTextEx(font, matchStr.c_str(), fSize, 1); DrawRectangleRec({rx + pre.x, y, mid.x, fSize + 2}, {hlCol.r, hlCol.g, hlCol.b, 120}); p += std::max((size_t)1, matchStr.size()); } } } } DrawTextEx(font, lines[li].c_str(), {rx, y}, fSize, 1, textCol); y += fSize + lSpacing; } }
    y += vGap;
}

// --- UI Panels ---

void DrawTooltip(AppState& s, Font f) {
    if (strlen(s.tooltip) == 0) return; Vector2 mp = GetMousePosition(); Vector2 sz = MeasureTextEx(f, s.tooltip, 14, 1);
    float tx = mp.x + 15, ty = mp.y + 15; if (tx + sz.x + 10 > (float)GetScreenWidth()) tx = mp.x - sz.x - 15;
    DrawRectangleRec({tx, ty, sz.x + 10, sz.y + 6}, {20, 20, 20, 230}); DrawRectangleLinesEx({tx, ty, sz.x + 10, sz.y + 6}, 1, s.vnum); DrawTextEx(f, s.tooltip, {tx + 5, ty + 3}, 14, 1, RAYWHITE);
}

void DrawAboutPanel(AppState& s, Font f) {
    float pw = 500, ph = 400, px = ((float)GetScreenWidth() - pw) / 2.f, py = 120;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.accent); 
    DrawLogo(px + pw/2 - 30, py + 40, 50, s.accent, s.vnum); float y = py + 110;
    std::string brand = "Divine Word"; Vector2 bsz = MeasureTextEx(f, brand.c_str(), 32, 1); DrawTextEx(f, brand.c_str(), {px + (pw - bsz.x)/2, y}, 32, 1, s.text); y += 40;
    std::string tag = "Holy Bible Study Application"; Vector2 tsz = MeasureTextEx(f, tag.c_str(), 18, 1); DrawTextEx(f, tag.c_str(), {px + (pw - tsz.x)/2, y}, 18, 1, s.vnum); y += 50;
    std::string desc = "A high-performance, professional Bible reader featuring advanced study tools, multi-translation comparison, and a distraction-free experience.";
    auto lines = WrapText(desc, f, 16, pw - 80); for (const auto& ln : lines) { Vector2 lsz = MeasureTextEx(f, ln.c_str(), 16, 1); DrawTextEx(f, ln.c_str(), {px + (pw - lsz.x)/2, y}, 16, 1, s.text); y += 22; }
    DrawTextEx(f, s.version.c_str(), {px + 40, py + ph - 40}, 14, 1, s.vnum);
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb);
    DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text);
    if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showAbout = false;
}

void DrawSidebar(AppState& s, Font f) {
    if (!s.showSidebar) return;
    float sw = s.sidebarWidth; float sx = (float)GetScreenWidth() - sw; float TOP = 60, BOT = 38; float sh = (float)GetScreenHeight() - TOP - BOT;
    DrawRectangleRec({sx, TOP, sw, sh}, s.hdr); DrawLineEx({sx, TOP}, {sx, TOP + sh}, 1, s.vnum);
    float y = TOP + 20; DrawTextEx(f, "Divine Insight", {sx + 20, y}, 22, 1, s.accent); y += 40;
    Rectangle resizer = {sx - 5, TOP, 10, sh};
    if (CheckCollisionPointRec(GetMousePosition(), resizer)) { SetMouseCursor(MOUSE_CURSOR_RESIZE_EW); if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) { s.sidebarWidth -= GetMouseDelta().x; s.sidebarWidth = std::clamp(s.sidebarWidth, 250.f, 600.f); } } else SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    
    if (s.selectedVerses.size() > 1) {
        DrawTextEx(f, (std::to_string(s.selectedVerses.size()) + " verses selected").c_str(), {sx + 20, y}, 18, 1, s.text); y += 30;
        if (s.buf.empty()) return;
        std::string b = s.buf[0].book; int ch = s.buf[0].chapter;
        Rectangle ball = {sx + 20, y, 140, 30}; bool bah = CheckCollisionPointRec(GetMousePosition(), ball);
        DrawRectangleRec(ball, bah ? s.accent : s.bg); DrawRectangleLinesEx(ball, 1, s.vnum);
        DrawTextEx(f, "Bookmark All", {ball.x + 15, ball.y + 6}, 16, 1, bah ? RAYWHITE : s.text);
        if (bah && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { for (int v : s.selectedVerses) g_study.SetBookmark(b, ch, v, s.trans, true, (v <= (int)s.buf[0].verses.size()) ? s.buf[0].verses[v-1].text : ""); } 
        y += 40;
        DrawTextEx(f, "Highlight All:", {sx + 20, y}, 14, 1, s.vnum); y += 20;
        Color hcs[] = {{255,255,0,255}, {0,255,0,255}, {0,200,255,255}, {255,100,200,255}};
        for (int i = 0; i < 4; i++) {
            Rectangle hr = {sx + 20 + (float)i * 35, y, 30, 30}; if (CheckCollisionPointRec(GetMousePosition(), hr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { for (int v : s.selectedVerses) g_study.SetHighlight(b, ch, v, s.trans, i + 1, (v <= (int)s.buf[0].verses.size()) ? s.buf[0].verses[v-1].text : ""); }
            DrawRectangleRec(hr, hcs[i]);
        }
        return;
    }

    if (s.lastSelectedVerse == -1 || s.buf.empty()) { DrawTextEx(f, "Select a verse to see study info.", {sx + 20, y}, 16, 1, s.vnum); return; }
    std::string b = s.buf[0].book; int ch = s.buf[0].chapter; int v = s.lastSelectedVerse;
    std::string ref = b + " " + std::to_string(ch) + ":" + std::to_string(v); DrawTextEx(f, ref.c_str(), {sx + 20, y}, 20, 1, s.text); y += 30;
    auto* vd = g_study.Get(b, ch, v, s.trans);
    bool isBk = vd ? vd->isBookmarked : false;
    Rectangle bkr = {sx + 20, y, 120, 30}; bool bkh = CheckCollisionPointRec(GetMousePosition(), bkr);
    DrawRectangleRec(bkr, isBk ? s.accent : (bkh ? s.vnum : s.bg)); DrawRectangleLinesEx(bkr, 1, s.vnum);
    DrawTextEx(f, isBk ? "Bookmarked" : "Bookmark", {bkr.x + 10, bkr.y + 6}, 16, 1, (isBk || bkh) ? RAYWHITE : s.text);
    if (bkh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_study.SetBookmark(b, ch, v, s.trans, !isBk, (v <= (int)s.buf[0].verses.size()) ? s.buf[0].verses[v-1].text : "");
    y += 40; DrawTextEx(f, "Highlight:", {sx + 20, y}, 14, 1, s.vnum); y += 20;
    Color hcs[] = {{255,255,0,255}, {0,255,0,255}, {0,200,255,255}, {255,100,200,255}};
    for (int i = 0; i < 4; i++) {
        Rectangle hr = {sx + 20 + (float)i * 35, y, 30, 30}; bool hh = CheckCollisionPointRec(GetMousePosition(), hr);
        DrawRectangleRec(hr, hcs[i]); if (vd && vd->highlightColor == i + 1) DrawRectangleLinesEx(hr, 2, BLACK);
        if (hh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_study.SetHighlight(b, ch, v, s.trans, i + 1, (v <= (int)s.buf[0].verses.size()) ? s.buf[0].verses[v-1].text : "");
    }
    Rectangle clr = {sx + 20 + 4 * 35, y, 30, 30}; if (CheckCollisionPointRec(GetMousePosition(), clr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) g_study.SetHighlight(b, ch, v, s.trans, 0);
    DrawRectangleLinesEx(clr, 1, s.vnum); DrawLineEx({clr.x, clr.y}, {clr.x + 30, clr.y + 30}, 1, s.err);
    y += 45; DrawTextEx(f, "Notes:", {sx + 20, y}, 14, 1, s.vnum); y += 20;
    Rectangle nBox = {sx + 20, y, sw - 40, 100}; bool nh = CheckCollisionPointRec(GetMousePosition(), nBox);
    DrawRectangleRec(nBox, s.bg); DrawRectangleLinesEx(nBox, 1, s.isEditingNote ? s.accent : s.vnum);
    if (nh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.isEditingNote = true; if (vd) strncpy(s.noteBuf, vd->note.c_str(), 511); else memset(s.noteBuf, 0, 512); }
    std::string displayNote = s.isEditingNote ? std::string(s.noteBuf) + "|" : (vd && !vd->note.empty() ? vd->note : "Click to add note...");
    auto lines = WrapText(displayNote, f, 15, nBox.width - 10); float ly = nBox.y + 5;
    for (const auto& ln : lines) { if (ly > nBox.y + nBox.height - 15) break; DrawTextEx(f, ln.c_str(), {nBox.x + 5, ly}, 15, 1, s.text); ly += 18; }
    y += 115; if (s.currentStrongs.active) {
        DrawLineEx({sx + 20, y}, {sx + sw - 20, y}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 80}); y += 15;
        std::string head = s.currentStrongs.lexeme + " (" + s.currentStrongs.transliteration + ")";
        DrawTextEx(f, head.c_str(), {sx + 20, y}, 18, 1, s.accent); y += 22;
        DrawTextEx(f, s.currentStrongs.shortDef.c_str(), {sx + 20, y}, 16, 1, s.text); y += 25;
        auto defLines = WrapText(s.currentStrongs.definition, f, 14, sw - 40);
        for (const auto& ln : defLines) { if (y > TOP + sh - 40) break; DrawTextEx(f, ln.c_str(), {sx + 20, y}, 14, 1, s.text); y += 17; } }
}

void DrawBurgerMenu(AppState& s, Font f) {
    float pw = 280, ph = 580, px = (float)GetScreenWidth() - pw - 10, py = 65;
    DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); float y = py + 15;
    auto header = [&](const char* lbl) { DrawTextEx(f, lbl, {px + 15, y}, 14, 1, s.accent); y += 25; };
    auto item = [&](const char* lbl, const char* key, bool* toggle) -> bool { 
        Rectangle r = {px + 10, y, pw - 20, 35}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 80});
        DrawTextEx(f, lbl, {r.x + 10, r.y + 8}, 16, 1, hov ? RAYWHITE : s.text);
        if (key) { float kw = MeasureTextEx(f, key, 12, 1).x; DrawTextEx(f, key, {r.x + r.width - kw - 10, r.y + 11}, 12, 1, hov ? RAYWHITE : s.vnum); }
        bool clicked = hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        if (clicked) { bool nextVal = toggle ? !(*toggle) : false; closeAllPanels(s); if (toggle) *toggle = nextVal; s.showBurgerMenu = false; } y += 40; return clicked;
    };
    header("READING"); item(s.bookMode ? "Mode: Book" : "Mode: Scroll", "Ctrl+B", nullptr); item(s.zenMode ? "Reading Mode: ON" : "Reading Mode: OFF", "Z", nullptr); item("Parallel View", "P", &s.parallelMode);
    y += 10; header("STUDY TOOLS"); item("Study Sidebar", "S", &s.showSidebar); item(s.studyMode ? "Strong's: ON" : "Strong's: OFF", "T", nullptr); item("Study Collection", "Ctrl+S", &s.showFavorites); item("Reading Plan", "Ctrl+P", &s.showPlan);
    y += 10; header("NAVIGATION"); item("History", "Ctrl+H", &s.showHistory); item("Jump to Ref", "Ctrl+J", &s.showJump); item("Search Bible", "Ctrl+G", &s.showGlobalSearch);
    y += 10; header("APPEARANCE"); if (item("Cycle Theme", "Ctrl+D", nullptr)) s.NextTheme(); item("Keyboard Help", "F1", &s.showHelp);
    y += 10; header("SYSTEM"); if (item("Clear Cache", nullptr, &s.showCache)) { s.cacheStats = g_cache.Stats(); } item("About Divine Word", nullptr, &s.showAbout);
}

void DrawHistoryPanel(AppState& s, Font f) {
    float pw = 480, ph = 520, px = ((float)GetScreenWidth() - pw) / 2.f, py = 80;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr);
    DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Reading History", {px + 20, py + 20}, 24, 1, s.accent); float y = py + 65; const auto& hist = g_hist.All();
    if (hist.empty()) { DrawTextEx(f, "No history yet.", {px + 20, y}, 18, 1, s.vnum); } else {
        for (int i = 0; i < (int)hist.size() && i < 11; i++) { const auto& h = hist[i]; std::string lbl = h.book + " (" + h.translation + ")"; Rectangle r = {px + 20, y, pw - 40, 36}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); DrawTextEx(f, lbl.c_str(), {r.x + 10, r.y + 8}, 17, 1, hov ? RAYWHITE : s.text);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.curBookIdx = h.bookIndex; s.curChNum = h.chapter; for (int j = 0; j < (int)TRANSLATIONS.size(); j++) if (TRANSLATIONS[j].code == h.translation) { s.transIdx = j; break; } s.trans = h.translation; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); s.showHistory = false; } y += 40; } }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb); DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text); if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showHistory = false;
}

void DrawFavoritesPanel(AppState& s, Font f) {
    float pw = 520, ph = 560, px = ((float)GetScreenWidth() - pw) / 2.f, py = 60;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Study Collection", {px + 20, py + 20}, 24, 1, s.accent); float y = py + 65; const auto& all = g_study.All();
    if (all.empty()) { DrawTextEx(f, "No study data yet.", {px + 20, y}, 18, 1, s.vnum); } else {
        for (int i = 0; i < (int)all.size() && i < 10; i++) { const auto& vd = all[i]; std::string lbl = vd.GetDisplay(); std::string prev = vd.text.empty() ? "" : (vd.text.size() > 55 ? vd.text.substr(0, 52) + "..." : vd.text);
            Rectangle r = {px + 20, y, pw - 40, 44}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); if (vd.highlightColor > 0) { Color hcs[] = {BLANK, YELLOW, GREEN, SKYBLUE, PINK}; DrawRectangle(r.x, r.y, 4, r.height, hcs[vd.highlightColor]); } DrawTextEx(f, lbl.c_str(), {r.x + 10, r.y + 4}, 16, 1, s.vnum); DrawTextEx(f, prev.c_str(), {r.x + 10, r.y + 24}, 14, 1, s.text);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { for (int j = 0; j < (int)BIBLE_BOOKS.size(); j++) if (vd.book.find(BIBLE_BOOKS[j].name) != std::string::npos) { s.curBookIdx = j; break; } s.curChNum = vd.chapter; s.trans = vd.translation; for (int j = 0; j < (int)TRANSLATIONS.size(); j++) if (TRANSLATIONS[j].code == vd.translation) { s.transIdx = j; break; } s.targetScrollY = 0; s.scrollY = 0; s.lastSelectedVerse = vd.verse; s.InitBuffer(); s.showFavorites = false; } y += 48; } }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb); DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text); if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showFavorites = false;
}

void DrawCachePanel(AppState& s, Font f) {
    float pw = 420, ph = 480, px = ((float)GetScreenWidth() - pw) / 2.f, py = 100;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Cache Statistics", {px + 20, py + 20}, 24, 1, s.accent); float y = py + 65;
    auto row = [&](const char* k, const std::string& v) { DrawTextEx(f, k, {px + 25, y}, 17, 1, s.vnum); DrawTextEx(f, v.c_str(), {px + 220, y}, 17, 1, s.text); y += 30; };
    row("Chapters cached:", std::to_string(s.cacheStats.totalChapters)); row("Total verses:", std::to_string(s.cacheStats.totalVerses)); row("Disk usage:", FmtBytes(s.cacheStats.totalSize));
    y += 15; DrawTextEx(f, "By translation:", {px + 25, y}, 18, 1, s.accent); y += 32;
    for (const auto& t : TRANSLATIONS) { int cnt = 0; auto it = s.cacheStats.byTranslation.find(t.code); if (it != s.cacheStats.byTranslation.end()) cnt = it->second; row(("  " + t.code + ":").c_str(), std::to_string(cnt) + " chapters"); }
    Rectangle clrBtn = { px + 25, py + ph - 50, 140, 34 }; bool clrHov = CheckCollisionPointRec(GetMousePosition(), clrBtn); DrawRectangleRec(clrBtn, clrHov ? s.err : s.hdr); DrawRectangleLinesEx(clrBtn, 1, s.err); DrawTextEx(f, "CLEAR CACHE", { clrBtn.x + 15, clrBtn.y + 8 }, 16, 1, clrHov ? RAYWHITE : s.err);
    if (clrHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { g_cache.ClearCache(); s.cacheStats = g_cache.Stats(); s.SetStatus("Cache cleared.", 2.0f); }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb); DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text); if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showCache = false;
}

void DrawPlanPanel(AppState& s, Font f) {
    float pw = 420, ph = 320, px = ((float)GetScreenWidth() - pw) / 2.f, py = 120;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum);
    time_t now = time(NULL); struct tm* t = localtime(&now); int dayOfYear = t->tm_yday + 1; char dateStr[64]; strftime(dateStr, sizeof(dateStr), "%B %d, %Y", t);
    DrawTextEx(f, "Daily Reading Plan", {px + 20, py + 20}, 24, 1, s.accent); DrawTextEx(f, dateStr, {px + 20, py + 48}, 16, 1, s.vnum); auto plan = GetDailyReading(dayOfYear); float y = py + 85;
    for (const auto& p : plan) { std::string lbl = BIBLE_BOOKS[p.first].name + " " + std::to_string(p.second); Rectangle r = {px + 20, y, pw - 40, 44}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); DrawRectangleLinesEx(r, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 80}); DrawTextEx(f, lbl.c_str(), {r.x + 12, r.y + 12}, 20, 1, s.text);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.curBookIdx = p.first; s.curChNum = p.second; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.showPlan = false; } y += 48; }
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb); DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text); if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showPlan = false;
}

void DrawHelpPanel(AppState& s, Font f) {
    float pw = 500, ph = 420, px = ((float)GetScreenWidth() - pw) / 2.f, py = 120;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum);
    DrawTextEx(f, "Divine Word Keyboard Shortcuts", {px + 20, py + 20}, 24, 1, s.accent); float y = py + 65; auto row = [&](const char* k, const char* d) { DrawTextEx(f, k, {px + 25, y}, 17, 1, s.vnum); DrawTextEx(f, d, {px + 180, y}, 17, 1, s.text); y += 28; };
    row("Ctrl + F", "Open Study Collection"); row("Ctrl + G", "Global Bible search"); row("Ctrl + J", "Jump to reference"); row("Ctrl + H", "View history"); row("Ctrl + P", "Daily reading plan"); row("S", "Toggle Study Sidebar"); row("Ctrl + B", "Toggle Book/Scroll mode"); row("Ctrl + D", "Toggle Themes"); row("Ctrl + R", "Refresh current passage"); row("F1 or ?", "Show this help panel"); row("ESC", "Close any panel or dropdown");
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb); DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text); if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showHelp = false;
}

void DrawSearchPanel(AppState& s, Font f) {
    float pw = 360, ph = 480, px = (float)GetScreenWidth() - pw - 15, py = 75;
    DrawRectangleRec({px, py, pw, ph}, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Search Current Buffer", {px + 15, py + 15}, 20, 1, s.accent);
    Rectangle box = {px + 15, py + 48, pw - 30, 34}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum); DrawTextEx(f, s.searchBuf, {box.x + 8, box.y + 8}, 18, 1, s.text);
    Rectangle gBtn = {px + 15, py + 90, pw - 30, 30}; bool gHov = CheckCollisionPointRec(GetMousePosition(), gBtn); DrawRectangleRec(gBtn, gHov ? s.accent : s.hdr); DrawRectangleLinesEx(gBtn, 1, s.vnum); DrawTextEx(f, "Search Entire Bible...", {gBtn.x + 10, gBtn.y + 7}, 15, 1, gHov ? RAYWHITE : s.vnum);
    if (gHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { closeAllPanels(s); s.showGlobalSearch = true; strncpy(s.gSearchBuf, s.searchBuf, 255); }
    Rectangle cbr = {px + 15, py + 130, 18, 18}; DrawRectangleRec(cbr, s.searchCS ? s.accent : s.bg); DrawRectangleLinesEx(cbr, 1, s.vnum); if (s.searchCS) DrawTextEx(f, "v", {cbr.x + 3, cbr.y}, 14, 1, RAYWHITE); DrawTextEx(f, "Case sensitive", {cbr.x + 28, cbr.y}, 16, 1, s.text);
    if (CheckCollisionPointRec(GetMousePosition(), cbr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.searchCS = !s.searchCS; if (strlen(s.searchBuf) > 0) { s.searchResults = SearchVerses(s.buf, s.searchBuf, s.searchCS); } }
    DrawTextEx(f, (std::to_string(s.searchResults.size()) + " match(es)").c_str(), {px + 15, py + 155}, 15, 1, s.vnum); float ry = py + 180;
    for (int i = 0; i < (int)s.searchResults.size() && i < 7; i++) { const auto& m = s.searchResults[i]; std::string lbl = "v." + std::to_string(m.verseNumber) + ": " + (m.text.size() > 40 ? m.text.substr(0, 37) + "..." : m.text); Rectangle r = {px + 15, ry, pw - 30, 34}; bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); if (i == s.searchSel) DrawRectangleLinesEx(r, 1, s.vnum); DrawTextEx(f, lbl.c_str(), {r.x + 8, r.y + 8}, 15, 1, s.text);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.searchSel = i; s.curBookIdx = m.bookIndex; s.curChNum = m.chapter; s.scrollToVerse = m.verseNumber; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.showSearch = false; } ry += 38; }
    Rectangle cl = {px + pw - 100, py + ph - 45, 85, 30}; bool clHov = CheckCollisionPointRec(GetMousePosition(), cl); DrawRectangleRec(cl, clHov ? s.accent : s.bg); DrawRectangleLinesEx(cl, 1, s.vnum); DrawTextEx(f, "Close", {cl.x + 20, cl.y + 7}, 16, 1, clHov ? RAYWHITE : s.text); if (clHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.showSearch = false; memset(s.searchBuf, 0, sizeof(s.searchBuf)); s.searchResults.clear(); }
}

void DrawJumpPanel(AppState& s, Font f) {
    float pw = 420, ph = 200, px = ((float)GetScreenWidth() - pw) / 2.f, py = 150;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Go to Reference", {px + 20, py + 20}, 24, 1, s.accent); Rectangle box = {px + 20, py + 60, pw - 40, 40}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum); DrawTextEx(f, s.jumpBuf, {box.x + 10, box.y + 10}, 22, 1, s.text); DrawTextEx(f, "e.g. John 3:16  or  Psalm 23", {px + 20, py + 110}, 16, 1, s.vnum);
    Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb); DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text); if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showJump = false;
}

void DrawGlobalSearchPanel(AppState& s, Font f) {
    float pw = 600, ph = 500, px = ((float)GetScreenWidth() - pw) / 2.f, py = 100;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Global Bible Search", {px + 20, py + 20}, 24, 1, s.accent); Rectangle box = {px + 20, py + 60, pw - 160, 40}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum); DrawTextEx(f, s.gSearchBuf, {box.x + 10, box.y + 10}, 20, 1, s.text);
    Rectangle sBtn = {px + pw - 130, py + 60, 110, 40}; bool sHov = CheckCollisionPointRec(GetMousePosition(), sBtn); DrawRectangleRec(sBtn, sHov ? s.accent : s.bg); DrawRectangleLinesEx(sBtn, 1, s.vnum); DrawTextEx(f, "SEARCH", {sBtn.x + 20, sBtn.y + 10}, 18, 1, sHov ? RAYWHITE : s.text); if (sHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.StartGlobalSearch(); if (s.gSearchActive) { float progress = (float)s.gSearchProgress / BIBLE_BOOKS.size(); DrawRectangle(px + 20, py + 110, (pw - 40) * progress, 4, s.accent); }
    float ry = py + 130; BeginScissorMode((int)(px + 20), (int)ry, (int)(pw - 40), (int)(ph - 190)); static float scroll = 0; if (CheckCollisionPointRec(GetMousePosition(), {px + 20, ry, pw - 40, ph - 190})) scroll += GetMouseWheelMove() * 30; if (scroll > 0) scroll = 0; float itemY = ry + scroll;
    for (int i = 0; i < (int)s.gSearchResults.size(); i++) { const auto& m = s.gSearchResults[i]; std::string ref = m.bookName + " " + std::to_string(m.chapter) + ":" + std::to_string(m.verse); std::string preview = m.text.length() > 65 ? m.text.substr(0, 62) + "..." : m.text; Rectangle r = {px + 20, itemY, pw - 40, 45};
        if (itemY > ry - 50 && itemY < py + ph - 60) { bool hov = CheckCollisionPointRec(GetMousePosition(), r); if (hov) DrawRectangleRec(r, {s.accent.r, s.accent.g, s.accent.b, 60}); DrawTextEx(f, ref.c_str(), {r.x + 5, r.y + 5}, 16, 1, s.vnum); DrawTextEx(f, preview.c_str(), {r.x + 5, r.y + 22}, 14, 1, s.text);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { s.curBookIdx = m.bookIdx; s.curChNum = m.chapter; s.scrollToVerse = m.verse; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.showGlobalSearch = false; } } itemY += 50; }
    EndScissorMode(); Rectangle cb = {px + pw - 120, py + ph - 50, 100, 34}; bool ch = CheckCollisionPointRec(GetMousePosition(), cb); DrawRectangleRec(cb, ch ? s.accent : s.bg); DrawRectangleLinesEx(cb, 1, s.vnum); DrawTextEx(f, "Close", {cb.x + 28, cb.y + 8}, 18, 1, ch ? RAYWHITE : s.text); if (ch && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showGlobalSearch = false;
}

void DrawNoteEditor(AppState& s, Font f) {
    float pw = 500, ph = 300, px = ((float)GetScreenWidth() - pw) / 2.f, py = 150;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 180}); DrawRectangle(px, py, pw, ph, s.hdr); DrawRectangleLinesEx({px, py, pw, ph}, 2, s.vnum); DrawTextEx(f, "Edit Verse Note", {px + 20, py + 20}, 24, 1, s.accent);
    Rectangle box = {px + 20, py + 60, pw - 40, 150}; DrawRectangleRec(box, s.bg); DrawRectangleLinesEx(box, 1, s.vnum); std::string currentNote = s.noteBuf; auto lines = WrapText(currentNote + "_", f, 18, box.width - 20); float ly = box.y + 10;
    for (const auto& ln : lines) { DrawTextEx(f, ln.c_str(), {box.x + 10, ly}, 18, 1, s.text); ly += 22; }
    int key = GetCharPressed(); while (key > 0) { size_t len = strlen(s.noteBuf); if (key >= 32 && key <= 126 && len < 511) { s.noteBuf[len] = (char)key; s.noteBuf[len+1] = 0; } key = GetCharPressed(); }
    if (IsKeyPressed(KEY_BACKSPACE)) { size_t len = strlen(s.noteBuf); if (len > 0) s.noteBuf[len-1] = 0; }
    Rectangle saveBtn = {px + pw - 220, py + ph - 50, 100, 34}, cancelBtn = {px + pw - 110, py + ph - 50, 100, 34}; bool sHov = CheckCollisionPointRec(GetMousePosition(), saveBtn), cHov = CheckCollisionPointRec(GetMousePosition(), cancelBtn); DrawRectangleRec(saveBtn, sHov ? s.ok : s.bg); DrawRectangleLinesEx(saveBtn, 1, s.vnum); DrawTextEx(f, "SAVE", {saveBtn.x + 25, saveBtn.y + 8}, 18, 1, sHov ? RAYWHITE : s.text);
    if (sHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { if (s.lastSelectedVerse != -1 && !s.buf.empty()) g_study.SetNote(s.buf[0].book, s.buf[0].chapter, s.lastSelectedVerse, s.trans, s.noteBuf); s.showNoteEditor = false; }
    DrawRectangleRec(cancelBtn, cHov ? s.accent : s.bg); DrawRectangleLinesEx(cancelBtn, 1, s.vnum); DrawTextEx(f, "CANCEL", {cancelBtn.x + 15, cancelBtn.y + 8}, 18, 1, cHov ? RAYWHITE : s.text); if (cHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.showNoteEditor = false;
}

void DrawHeader(AppState& s, Font f) {
    if (s.zenMode && !IsAnyOverlayOpen(s)) return;
    const float H = 60; DrawRectangle(0, 0, GetScreenWidth(), H, s.hdr); DrawLineEx({0, H - 2}, {(float)GetScreenWidth(), H - 2}, 2, s.vnum); DrawLineEx({0, H - 5}, {(float)GetScreenWidth(), H - 5}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 100});
    DrawLogo(20, 12, 36, s.accent, s.vnum); DrawLineEx({80, 12}, {80, 48}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 80}); Vector2 mp = GetMousePosition(); bool isClick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON); bool overlays = IsAnyOverlayOpen(s);
    auto navBtn = [&](float bx, const char* lbl, bool en, const char* tip) -> bool { Rectangle r = {bx, 15, 44, 30}; bool hov = !overlays && en && CheckCollisionPointRec(mp, r); if (hov) strncpy(s.tooltip, tip, 63); if (en) { DrawRectangleRec(r, hov ? s.accent : s.bg); DrawRectangleLinesEx(r, 1, s.vnum); DrawTextEx(f, lbl, {r.x + (r.width-MeasureTextEx(f,lbl,14,1).x)/2, r.y+8}, 14, 1, hov ? RAYWHITE : s.text); } else { DrawRectangleLinesEx(r, 1, {s.vnum.r,s.vnum.g,s.vnum.b,60}); DrawTextEx(f, lbl, {r.x + (r.width-MeasureTextEx(f,lbl,14,1).x)/2, r.y+8}, 14, 1, {s.text.r,s.text.g,s.text.b,60}); } return hov && isClick; };
    memset(s.tooltip, 0, sizeof(s.tooltip)); float bx = 100;
    if (navBtn(bx, "<<", s.curBookIdx > 0, "Prev Book")) s.PrevBook(); bx += 50; if (navBtn(bx, ">>", s.curBookIdx < (int)BIBLE_BOOKS.size() - 1, "Next Book")) s.NextBook(); bx += 50; if (navBtn(bx, "<", s.navIndex > 0, "History Back")) s.GoBack(); bx += 50; if (navBtn(bx, ">", s.navIndex < (int)s.navHistory.size() - 1, "History Forward")) s.GoForward(); bx += 60;
    Rectangle modeBtn = {bx, 15, 80, 30}; bool mHov = !overlays && CheckCollisionPointRec(mp, modeBtn); bx += 90; DrawRectangleRec(modeBtn, s.bookMode ? s.accent : (mHov ? Color{s.vnum.r, s.vnum.g, s.vnum.b, 80} : s.bg)); DrawRectangleLinesEx(modeBtn, 1, s.vnum); const char* modeL = s.bookMode ? "Book" : "Scroll"; DrawTextEx(f, modeL, {modeBtn.x + (modeBtn.width - MeasureTextEx(f, modeL, 16, 1).x)/2, modeBtn.y + 7}, 16, 1, s.bookMode ? RAYWHITE : s.text); if (mHov && isClick) { s.bookMode = !s.bookMode; if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); }
    Rectangle bkBtn = {bx, 15, 160, 30}; bool onBkBtn = CheckCollisionPointRec(mp, bkBtn); bool bkHov = !overlays && onBkBtn; bx += 170; DrawRectangleRec(bkBtn, s.bg); DrawRectangleLinesEx(bkBtn, 1, bkHov ? s.accent : s.vnum); std::string bkName = BIBLE_BOOKS[s.curBookIdx].name; if ((int)bkName.size() > 14) bkName = bkName.substr(0, 12) + ".."; DrawTextEx(f, bkName.c_str(), {bkBtn.x + 8, bkBtn.y + 6}, 17, 1, s.text); DrawTextEx(f, "v", {bkBtn.x + bkBtn.width - 18, bkBtn.y + 8}, 14, 1, s.vnum); if (bkHov && isClick) { bool wasOpen = s.showBookDrop; closeAllPanels(s); s.showBookDrop = !wasOpen; }
    float cBase = bx; Rectangle cMinus = {cBase, 15, 30, 30}, cPlus = {cBase + 72, 15, 30, 30}; bool cmH = !overlays && CheckCollisionPointRec(mp, cMinus), cpH = !overlays && CheckCollisionPointRec(mp, cPlus); bx += 110; DrawRectangleRec(cMinus, cmH ? s.accent : s.bg); DrawRectangleLinesEx(cMinus, 1, s.vnum); DrawTextEx(f, "-", {cMinus.x + 11, cMinus.y + 4}, 20, 1, cmH ? RAYWHITE : s.text); std::string chLbl = std::to_string(s.curChNum); Vector2 chSz = MeasureTextEx(f, chLbl.c_str(), 17, 1); DrawTextEx(f, chLbl.c_str(), {cBase + 30 + (42 - chSz.x)/2, 22}, 17, 1, s.text); DrawRectangleRec(cPlus, cpH ? s.accent : s.bg); DrawRectangleLinesEx(cPlus, 1, s.vnum); DrawTextEx(f, "+", {cPlus.x + 10, cPlus.y + 4}, 20, 1, cpH ? RAYWHITE : s.text); if (cmH && isClick) { PrevChapter(s.curBookIdx, s.curChNum); s.scrollY = 0; s.targetScrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; } if (cpH && isClick) { NextChapter(s.curBookIdx, s.curChNum); s.scrollY = 0; s.targetScrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
    float tx = bx; Rectangle tBtn = {tx, 15, 80, 30}; bool onTBtn = CheckCollisionPointRec(mp, tBtn); bool tHov = !overlays && onTBtn; DrawRectangleRec(tBtn, s.bg); DrawRectangleLinesEx(tBtn, 1, tHov ? s.accent : s.vnum); std::string tLbl = TRANSLATIONS[s.transIdx].code; std::transform(tLbl.begin(), tLbl.end(), tLbl.begin(), ::toupper); DrawTextEx(f, tLbl.c_str(), {tBtn.x + 8, tBtn.y + 6}, 17, 1, s.text); DrawTextEx(f, "v", {tBtn.x + tBtn.width - 16, tBtn.y + 8}, 14, 1, s.vnum); if (tHov && isClick) { bool wasOpen = s.showTransDrop; closeAllPanels(s); s.showTransDrop = !wasOpen; }
    float tx2 = tx + 86; Rectangle tBtn2 = {tx2, 15, 80, 30}; bool onTBtn2 = CheckCollisionPointRec(mp, tBtn2); if (s.parallelMode) { bool tHov2 = !overlays && onTBtn2; DrawRectangleRec(tBtn2, s.bg); DrawRectangleLinesEx(tBtn2, 1, tHov2 ? s.accent : s.vnum); std::string tLbl2 = TRANSLATIONS[s.transIdx2].code; std::transform(tLbl2.begin(), tLbl2.end(), tLbl2.begin(), ::toupper); DrawTextEx(f, tLbl2.c_str(), {tBtn2.x + 8, tBtn2.y + 6}, 17, 1, s.text); DrawTextEx(f, "v", {tBtn2.x + tBtn2.width - 16, tBtn2.y + 8}, 14, 1, s.vnum); if (tHov2 && isClick) { bool wasOpen = s.showTransDrop2; closeAllPanels(s); s.showTransDrop2 = !wasOpen; } bx = tx2 + 86; } else { bx = tx + 86; }
    float fx = bx; Rectangle fMinus = {fx, 15, 28, 30}, fPlus = {fx + 54, 15, 28, 30}; bool fmH = !overlays && CheckCollisionPointRec(mp, fMinus), fpH = !overlays && CheckCollisionPointRec(mp, fPlus); DrawRectangleRec(fMinus, fmH ? s.accent : s.bg); DrawRectangleLinesEx(fMinus, 1, s.vnum); DrawTextEx(f, "-", {fMinus.x + 9, fMinus.y + 4}, 20, 1, fmH ? RAYWHITE : s.text); DrawTextEx(f, std::to_string((int)s.fontSize).c_str(), {fx + 30, 22}, 16, 1, s.text); DrawRectangleRec(fPlus, fpH ? s.accent : s.bg); DrawRectangleLinesEx(fPlus, 1, s.vnum); DrawTextEx(f, "+", {fPlus.x + 8, fPlus.y + 4}, 20, 1, fpH ? RAYWHITE : s.text); if (fmH && isClick) { s.fontSize -= 1; if (s.fontSize < 10) s.fontSize = 10; if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); } if (fpH && isClick) { s.fontSize += 1; if (s.fontSize > 40) s.fontSize = 40; if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); }
    auto iconBtn2 = [&](float bx, const char* lbl, bool act, const char* tip) -> bool { Rectangle r = {bx, 15, 44, 30}; bool onBtn = CheckCollisionPointRec(mp, r); bool hov = (onBtn && (!overlays || act)); if (hov) strncpy(s.tooltip, tip, 63); Color bg2 = act ? s.accent : (hov ? Color{s.vnum.r, s.vnum.g, s.vnum.b, 80} : s.bg); DrawRectangleRec(r, bg2); DrawRectangleLinesEx(r, 1, s.vnum); Vector2 ts = MeasureTextEx(f, lbl, 14, 1); DrawTextEx(f, lbl, {r.x + (r.width - ts.x) / 2, r.y + 8}, 14, 1, act ? RAYWHITE : s.text); return hov && isClick; };
    float ibx = (float)GetScreenWidth() - 54; if (iconBtn2(ibx, "Menu", s.showBurgerMenu, "Menu")) { bool val = !s.showBurgerMenu; closeAllPanels(s); s.showBurgerMenu = val; } 
    if (iconBtn2(ibx - 50, "Side", s.showSidebar, "Study Sidebar (S)")) { s.showSidebar = !s.showSidebar; s.SaveSettings(); }
    if (iconBtn2(ibx - 100, "Srch", s.showSearch, "Search (Ctrl+F)")) { bool val = !s.showSearch; closeAllPanels(s); s.showSearch = val; } if (iconBtn2(ibx - 150, "Jump", s.showJump, "Jump (Ctrl+J)")) { bool val = !s.showJump; closeAllPanels(s); s.showJump = val; } if (iconBtn2(ibx - 200, "Par", s.parallelMode, "Parallel")) s.ToggleParallelMode(f); if (iconBtn2(ibx - 250, "Copy", false, "Copy Chapter")) s.CopyChapter(); if (iconBtn2(ibx - 300, "Thme", false, "Theme (Ctrl+D)")) s.NextTheme();
    if (s.showTransDrop) { float dy = H, dh = (float)TRANSLATIONS.size() * 36.0f; Rectangle dr = {tx, dy, 148, dh}; DrawRectangleRec(dr, s.hdr); DrawRectangleLinesEx(dr, 1, s.accent); for (int i = 0; i < (int)TRANSLATIONS.size(); i++) { Rectangle r = {tx, dy + (float)i * 36.f, 148, 36}; bool hov = CheckCollisionPointRec(mp, r); if (hov) DrawRectangleRec(r, s.accent); DrawTextEx(f, TRANSLATIONS[i].name.c_str(), {r.x + 8, r.y + 8}, 15, 1, hov ? RAYWHITE : s.text); if (hov && isClick) { s.transIdx = i; s.trans = TRANSLATIONS[i].code; s.showTransDrop = false; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); } } if (isClick && !CheckCollisionPointRec(mp, dr) && !onTBtn) s.showTransDrop = false; }
    if (s.showTransDrop2) { float dy = H, dh = (float)TRANSLATIONS.size() * 36.0f; Rectangle dr = {tx2, dy, 148, dh}; DrawRectangleRec(dr, s.hdr); DrawRectangleLinesEx(dr, 1, s.vnum); for (int i = 0; i < (int)TRANSLATIONS.size(); i++) { Rectangle r = {tx2, dy + (float)i * 36.f, 148, 36}; bool hov = CheckCollisionPointRec(mp, r); if (hov) DrawRectangleRec(r, s.accent); DrawTextEx(f, TRANSLATIONS[i].name.c_str(), {r.x + 8, r.y + 8}, 15, 1, hov ? RAYWHITE : s.text); if (hov && isClick) { s.transIdx2 = i; s.trans2 = TRANSLATIONS[i].code; s.showTransDrop2 = false; s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; s.SaveSettings(); } } if (isClick && !CheckCollisionPointRec(mp, dr) && !onTBtn2) s.showTransDrop2 = false; }
    if (s.showBookDrop) { float dy = H, visH = 400, bkW = 200; Rectangle dr = {bkBtn.x, dy, bkW, visH}; DrawRectangleRec(dr, s.hdr); DrawRectangleLinesEx(dr, 1, s.vnum); BeginScissorMode((int)dr.x, (int)dy, (int)bkW, (int)visH); for (int i = 0; i < (int)BIBLE_BOOKS.size(); i++) { float iy = dy + (float)i * 32.f - s.bookDropScroll; if (iy < dy - 32 || iy > dy + visH) continue; Rectangle r = {dr.x, iy, bkW, 32}; bool hov = CheckCollisionPointRec(mp, r); if (hov) DrawRectangleRec(r, s.accent); DrawTextEx(f, BIBLE_BOOKS[i].name.c_str(), {r.x + 8, r.y + 7}, 16, 1, hov ? RAYWHITE : s.text); if (hov && isClick) { s.curBookIdx = i; s.curChNum = 1; s.showBookDrop = false; s.bookDropScroll = 0; s.scrollY = 0; s.targetScrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; } } EndScissorMode(); if (CheckCollisionPointRec(mp, dr)) { s.bookDropScroll -= GetMouseWheelMove() * 32; float maxScroll = (float)BIBLE_BOOKS.size() * 32.f - visH; if (s.bookDropScroll < 0) s.bookDropScroll = 0; if (s.bookDropScroll > maxScroll) s.bookDropScroll = maxScroll; } if (isClick && !CheckCollisionPointRec(mp, dr) && !onBkBtn) s.showBookDrop = false; }
}

void DrawScrollMode(AppState& s, Font f) {
    std::lock_guard<std::mutex> lock(s.bufferMutex); const float TOP = 60, BOT = 38; float sw = s.showSidebar ? s.sidebarWidth : 0; float mw = (float)GetScreenWidth() - sw; float h = (float)GetScreenHeight() - TOP - BOT; bool overlayOpen = IsAnyOverlayOpen(s);
    if (!overlayOpen && !s.isLoading) { float wheel = GetMouseWheelMove(); if (CheckCollisionPointRec(GetMousePosition(), {0, TOP, mw, h})) s.targetScrollY += wheel * 100.0f; }
    const float PAD = 40, FS = s.fontSize, LS = s.lineSpacing, VG = 14; BeginScissorMode(0, (int)TOP, (int)mw, (int)h); float yFinal = TOP + 18 + s.scrollY; s.scrollChapterIdx = 0;
    if (s.parallelMode) { float colW = (mw - PAD * 3) / 2.0f; yFinal = TOP + 18 + s.scrollY; float y1 = yFinal, y2 = yFinal; int chapterCount = (int)s.buf.size();
        for (int ci = 0; ci < chapterCount; ci++) { const Chapter& ch1 = s.buf[ci]; float chapterStartY = y1;
            if (ch1.isLoaded) { if (y1 <= TOP + 50 && y1 + 100 > TOP) s.scrollChapterIdx = ci; DrawTextEx(f, ch1.book.c_str(), {PAD, y1}, 24, 1, s.accent); DrawTextEx(f, ch1.translation.c_str(), {PAD + colW - 40, y1 + 6}, 12, 1, s.vnum); y1 += 34; DrawLineEx({PAD, y1}, {PAD + colW, y1}, 2, s.vnum); y1 += 14;
                for (const auto& v : ch1.verses) { bool isSel = s.selectedVerses.count(v.number); if (isSel) DrawRectangleRec({PAD - 5, y1 - 2, colW + 10, FS + LS + 4}, {s.accent.r, s.accent.g, s.accent.b, 40}); if (s.scrollToVerse == v.number && ci == 0) { s.targetScrollY = -(y1 - s.scrollY - TOP - 20); s.scrollToVerse = -1; } Rectangle vRec = {PAD, y1, colW, FS + 4}; bool vHov = CheckCollisionPointRec(GetMousePosition(), vRec); std::vector<SearchMatch> vm; for (const auto& m : s.searchResults) if (m.bookIndex == ch1.bookIndex && m.chapter == ch1.chapter && m.verseNumber == v.number) vm.push_back(m); DrawVerseText(f, v, PAD, y1, colW, FS, LS, VG, s.text, s.vnum, vHov, vm, {220, 180, 60, 120}, s, ch1.book, ch1.chapter, ch1.translation); if (vHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !overlayOpen) { s.lastSelectedVerse = v.number; s.scrollChapterIdx = ci; if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) s.ToggleVerseSelection(v.number); else { s.ClearSelection(); s.ToggleVerseSelection(v.number); } } if (vHov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !overlayOpen) { std::string q = "\"" + v.text + "\"\n\xE2\x80\x94 " + ch1.book + ":" + std::to_string(v.number) + " (" + ch1.translation + ")"; CopyToClipboard(q); SaveVerseImage(v, ch1.book, ch1.translation, f); s.SetStatus("Verse copied!"); } } } else { DrawTextEx(f, "Loading...", {PAD, y1}, 18, 1, s.vnum); y1 += 50; }
            float leftEndY = y1; y2 = chapterStartY; if (ci < (int)s.buf2.size()) { const Chapter& ch2 = s.buf2[ci]; if (ch2.isLoaded) { DrawTextEx(f, ch2.book.c_str(), {PAD * 2 + colW, y2}, 24, 1, s.accent); DrawTextEx(f, ch2.translation.c_str(), {PAD * 2 + colW * 2 - 40, y2 + 6}, 12, 1, s.vnum); y2 += 34; DrawLineEx({PAD * 2 + colW, y2}, {PAD * 2 + colW * 2, y2}, 2, s.vnum); y2 += 14; for (const auto& v : ch2.verses) { DrawVerseText(f, v, PAD * 2 + colW, y2, colW, FS, LS, VG, s.text, s.vnum, false, {}, {220, 180, 60, 120}, s, ch2.book, ch2.chapter, ch2.translation); } } else { DrawTextEx(f, "Loading...", {PAD * 2 + colW, y2}, 18, 1, s.vnum); y2 += 50; } } else { DrawTextEx(f, "Connecting...", {PAD * 2 + colW, y2}, 18, 1, s.vnum); y2 += 50; } y1 = y2 = std::max(leftEndY, y2) + 40; } yFinal = y1;
    } else { const float TW = mw - PAD * 2; float y = TOP + 18 + s.scrollY;
        for (int ci = 0; ci < (int)s.buf.size(); ci++) { const Chapter& ch = s.buf[ci]; if (!ch.isLoaded) continue; if (y <= TOP + 50 && y + 100 > TOP) s.scrollChapterIdx = ci; DrawTextEx(f, ch.book.c_str(), {PAD, y}, 28, 1, s.accent); y += 38; DrawLineEx({PAD, y}, {mw - PAD, y}, 2, s.vnum); DrawLineEx({PAD, y + 3}, {mw - PAD, y + 3}, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 80}); y += 18;
            for (const auto& v : ch.verses) { bool isSel = s.selectedVerses.count(v.number); if (isSel) DrawRectangleRec({PAD - 10, y - 2, TW + 20, s.fontSize + s.lineSpacing + 4}, {s.accent.r, s.accent.g, s.accent.b, 40}); if (s.scrollToVerse == v.number && ci == 0) { s.targetScrollY = -(y - s.scrollY - TOP - 20); s.scrollToVerse = -1; } Rectangle numR = {PAD, y, TW, FS + 4}; bool numHov = CheckCollisionPointRec(GetMousePosition(), numR); std::vector<SearchMatch> vm; for (const auto& m : s.searchResults) if (m.bookIndex == ch.bookIndex && m.chapter == ch.chapter && m.verseNumber == v.number) vm.push_back(m); DrawVerseText(f, v, PAD, y, TW, FS, LS, VG, s.text, s.vnum, numHov, vm, {220, 180, 60, 120}, s, ch.book, ch.chapter, ch.translation); if (numHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !overlayOpen) { s.lastSelectedVerse = v.number; s.scrollChapterIdx = ci; if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) s.ToggleVerseSelection(v.number); else { s.ClearSelection(); s.ToggleVerseSelection(v.number); } } if (numHov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !overlayOpen) { std::string q = "\"" + v.text + "\"\n\xE2\x80\x94 " + ch.book + ":" + std::to_string(v.number) + " (" + ch.translation + ")"; CopyToClipboard(q); SaveVerseImage(v, ch.book, ch.translation, f); s.SetStatus("Verse copied!"); } } y += 40; } yFinal = y; }
    EndScissorMode(); float contentH = yFinal - TOP - s.scrollY; if (contentH > h) { float barH = (h / contentH) * h; if (barH < 30) barH = 30; float barY = TOP + (-s.scrollY / (contentH - h)) * (h - barH); Rectangle scrollRect = { mw - 10, barY, 6, barH }; DrawRectangleRec(scrollRect, { s.vnum.r, s.vnum.g, s.vnum.b, 150 }); if (CheckCollisionPointRec(GetMousePosition(), { mw - 15, TOP, 15, h }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !overlayOpen) { float delta = GetMouseDelta().y; s.targetScrollY -= delta * (contentH / h); } }
    float ay = TOP + h / 2.0f - 25; auto drawFloatNav = [&](Rectangle r, const char* lbl, bool en) { bool hov = !overlayOpen && en && CheckCollisionPointRec(GetMousePosition(), r); if (en) { DrawRectangleRec(r, hov ? s.accent : (Color){ s.hdr.r, s.hdr.g, s.hdr.b, 180 }); DrawRectangleLinesEx(r, 2, s.vnum); Vector2 sz = MeasureTextEx(f, lbl, 24, 1); DrawTextEx(f, lbl, { r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2 }, 24, 1, hov ? RAYWHITE : s.text); } return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON); };
    if (drawFloatNav({ 5, ay, 40, 50 }, "<", !(s.curBookIdx == 0 && s.curChNum == 1))) { PrevChapter(s.curBookIdx, s.curChNum); s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
    if (drawFloatNav({ mw - 45, ay, 40, 50 }, ">", true)) { NextChapter(s.curBookIdx, s.curChNum); s.targetScrollY = 0; s.scrollY = 0; s.InitBuffer(); if (s.bookMode) s.needsPageRebuild = true; }
    float totalH = yFinal - TOP - s.scrollY; float maxUp = -(totalH - h + 60); if (s.targetScrollY > 60) s.targetScrollY = 60; if (maxUp > 0) maxUp = 0; if (!s.isLoading && s.targetScrollY < maxUp) s.targetScrollY = maxUp; if (!s.isLoading) { if (s.targetScrollY > 20) s.GrowTop(); if (s.targetScrollY < maxUp + 220) s.GrowBottom(); }
}

void DrawBookMode(AppState& s, Font f) {
    std::lock_guard<std::mutex> lock(s.bufferMutex); const float TOP = 60, BOT = 38; float sw = s.showSidebar ? s.sidebarWidth : 0; float mw = (float)GetScreenWidth() - sw; float ch = (float)GetScreenHeight() - TOP - BOT; float pageW = std::min(700.f, mw - 100), pageH = std::min(500.f, ch - 40), pageX = (mw - pageW) / 2.f, pageY = TOP + (ch - pageH) / 2.f;
    if (s.pages.empty()) { const char* msg = s.isLoading ? "Loading..." : "No content loaded yet."; Vector2 ms = MeasureTextEx(f, msg, 18, 1); DrawTextEx(f, msg, {(mw - ms.x) / 2.f, TOP + ch / 2.f - 9}, 18, 1, s.vnum); return; }
    DrawRectangle((int)(pageX + 6), (int)(pageY + 6), (int)pageW, (int)pageH, s.pageShadow); DrawRectangle((int)pageX, (int)pageY, (int)pageW, (int)pageH, s.pageBg); DrawRectangleLinesEx({pageX, pageY, pageW, pageH}, 2, s.accent);
    const Page& pg = s.pages[s.pageIdx]; float ty = pageY + 22;
    if (pg.isChapterStart && pg.chapterBufIndex < (int)s.buf.size()) { const std::string& hdr = s.buf[pg.chapterBufIndex].book; DrawTextEx(f, hdr.c_str(), {pageX + 28, ty}, 21, 1, s.accent); if (s.parallelMode) { std::string t1 = s.trans, t2 = s.trans2; std::transform(t1.begin(), t1.end(), t1.begin(), ::toupper); std::transform(t2.begin(), t2.end(), t2.begin(), ::toupper); DrawTextEx(f, t1.c_str(), {pageX + 28, ty + 24}, 12, 1, s.vnum); DrawTextEx(f, t2.c_str(), {pageX + pageW/2 + 12, ty + 24}, 12, 1, s.vnum); } DrawLineEx({pageX + 28, ty + 38}, {pageX + pageW - 28, ty + 38}, 1, {s.accent.r, s.accent.g, s.accent.b, 80}); ty += 52; }
    if (!s.parallelMode) { for (size_t i = 0; i < pg.lines.size(); i++) { int vNum = pg.lineVerses[i]; int ci = pg.chapterBufIndex; if (ci >= 0 && ci < (int)s.buf.size()) { const auto& ch = s.buf[ci]; const Verse* vPtr = nullptr; for (const auto& v : ch.verses) if (v.number == vNum) { vPtr = &v; break; }
                if (s.studyMode && vPtr && !vPtr->rawText.empty()) { DrawStudyLine(f, pg.lines[i], pageX + 28, ty, s.fontSize, s.text, {200, 160, 40, 200}, s); } else { for (const auto& m : s.searchResults) { if (m.bookIndex == ch.bookIndex && m.chapter == ch.chapter && m.verseNumber == vNum) { std::string matchStr = ToLower(s.searchBuf); std::string lineLower = ToLower(pg.lines[i]); size_t p = 0; while ((p = lineLower.find(matchStr, p)) != std::string::npos) { Vector2 pre = MeasureTextEx(f, pg.lines[i].substr(0, p).c_str(), s.fontSize, 1); Vector2 mid = MeasureTextEx(f, s.searchBuf, s.fontSize, 1); DrawRectangleRec({pageX + 28 + pre.x, ty, mid.x, s.fontSize + 2}, {220, 180, 60, 120}); p += matchStr.size(); } } } DrawTextEx(f, pg.lines[i].c_str(), {pageX + 28, ty}, s.fontSize, 1, s.text); } } ty += s.fontSize + s.lineSpacing; } }
    else { float startTy = ty; for (size_t i = 0; i < pg.lines.size(); i++) { if (s.studyMode) DrawStudyLine(f, pg.lines[i], pageX + 28, ty, s.fontSize, s.text, {200, 160, 40, 200}, s); else DrawTextEx(f, pg.lines[i].c_str(), {pageX + 28, ty}, s.fontSize, 1, s.text); ty += s.fontSize + s.lineSpacing; } ty = startTy; for (size_t i = 0; i < pg.lines2.size(); i++) { DrawTextEx(f, pg.lines2[i].c_str(), {pageX + pageW/2 + 12, ty}, s.fontSize, 1, s.text); ty += s.fontSize + s.lineSpacing; } }
    std::string pnum = "Page " + std::to_string(s.pageIdx + 1) + " / " + std::to_string(s.pages.size()); Vector2 pns = MeasureTextEx(f, pnum.c_str(), 13, 1); DrawTextEx(f, pnum.c_str(), {pageX + (pageW - pns.x) / 2.f, pageY + pageH - 22}, 13, 1, s.vnum);
    DrawTextEx(f, ("vv." + std::to_string(pg.startVerse) + "-" + std::to_string(pg.endVerse)).c_str(), {pageX + pageW - 88, pageY + 8}, 12, 1, s.vnum);
    float ay = pageY + pageH / 2.f - 25; Rectangle prevBtn = {pageX - 60, ay, 40, 50}, nextBtn = {pageX + pageW + 20, ay, 40, 50}; bool prevHov = CheckCollisionPointRec(GetMousePosition(), prevBtn), nextHov = CheckCollisionPointRec(GetMousePosition(), nextBtn), atStart = (s.pageIdx == 0 && !s.buf.empty() && s.buf.front().bookIndex == 0 && s.buf.front().chapter == 1);
    if (!atStart) { DrawRectangleRec(prevBtn, prevHov ? s.accent : (Color){ s.hdr.r, s.hdr.g, s.hdr.b, 180 }); DrawRectangleLinesEx(prevBtn, 2, s.vnum); DrawTextEx(f, "<", {prevBtn.x + 13, prevBtn.y + 12}, 24, 1, prevHov ? RAYWHITE : s.text); if (prevHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.BookPagePrev(f); }
    DrawRectangleRec(nextBtn, nextHov ? s.accent : (Color){ s.hdr.r, s.hdr.g, s.hdr.b, 180 }); DrawRectangleLinesEx(nextBtn, 2, s.vnum); DrawTextEx(f, ">", {nextBtn.x + 13, nextBtn.y + 12}, 24, 1, nextHov ? RAYWHITE : s.text); if (nextHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) s.BookPageNext(f);
}

void DrawFooter(AppState& s, Font f) {
    if (s.zenMode && !IsAnyOverlayOpen(s)) return;
    const float FH = 38; float fy = (float)GetScreenHeight() - FH; DrawRectangle(0, (int)fy, GetScreenWidth(), (int)FH, s.hdr); DrawLineEx({0, fy}, {(float)GetScreenWidth(), fy}, 1, s.vnum);
    if (s.isLoading) { float angle = (float)GetTime() * 300.0f; DrawPolyLinesEx({ (float)GetScreenWidth() - 30, fy + 19 }, 6, 10, angle, 2, s.accent); DrawTextEx(f, "Loading...", { (float)GetScreenWidth() - 110, fy + 10 }, 14, 1, s.accent); }
    DrawTextEx(f, "Divine Word v0.1", {18, fy + 10}, 14, 1, {s.vnum.r, s.vnum.g, s.vnum.b, 180});
    if (!s.buf.empty()) { std::lock_guard<std::mutex> lock(s.bufferMutex); int ci = s.bookMode ? (s.pageIdx < (int)s.pages.size() ? s.pages[s.pageIdx].chapterBufIndex : 0) : s.scrollChapterIdx;
        if (ci >= 0 && ci < (int)s.buf.size() && s.buf[ci].isLoaded) { std::string loc = s.buf[ci].book + " (" + s.buf[ci].translation + ")"; Vector2 locSz = MeasureTextEx(f, loc.c_str(), 14, 1); DrawTextEx(f, loc.c_str(), {((float)GetScreenWidth() - locSz.x)/2.0f, fy + 10}, 14, 1, s.vnum); } }
    if (s.statusTimer > 0) { Vector2 ss = MeasureTextEx(f, s.statusMsg.c_str(), 15, 1); DrawTextEx(f, s.statusMsg.c_str(), {((float)GetScreenWidth() - ss.x) / 2.f, fy + 10}, 15, 1, s.ok); }
    const char* hint = s.bookMode ? "Arrow keys / < > = turn page" : "Scroll = infinite  |  Shift+Click = multi-select"; Vector2 hs = MeasureTextEx(f, hint, 12, 1); DrawTextEx(f, hint, {(float)GetScreenWidth() - hs.x - 16, fy + 12}, 12, 1, s.vnum);
}
