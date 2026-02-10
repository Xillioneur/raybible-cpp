#include "app_state.h"
#include "managers.h"
#include "bible_logic.h"
#include "utils.h"
#include "ui_renderer.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>

AppState::AppState() {
    g_settings.Load();
    darkMode = g_settings.darkMode; fontSize = g_settings.fontSize; lineSpacing = g_settings.lineSpacing;
    curBookIdx = g_settings.lastBookIdx; curChNum = g_settings.lastChNum; transIdx = g_settings.lastTransIdx;
    parallelMode = g_settings.parallelMode; transIdx2 = g_settings.transIdx2; bookMode = g_settings.bookMode;
    trans = TRANSLATIONS[transIdx].code; trans2 = TRANSLATIONS[transIdx2].code;
    UpdateColors(); UpdateTitle();
}

void AppState::SaveSettings() {
    g_settings.darkMode = darkMode; g_settings.fontSize = fontSize; g_settings.lineSpacing = lineSpacing;
    g_settings.lastBookIdx = curBookIdx; g_settings.lastChNum = curChNum; g_settings.lastTransIdx = transIdx;
    g_settings.parallelMode = parallelMode; g_settings.transIdx2 = transIdx2; g_settings.bookMode = bookMode;
    g_settings.Save();
}

void AppState::UpdateColors() {
    if (darkMode) {
        bg = {15,15,15,255}; text = {220,210,190,255}; accent = {180,40,40,255}; hdr = {25,25,25,255}; vnum = {160,140,80,255};
        ok = {100,220,150,255}; err = {220,100,100,255}; pageBg = {30,28,25,255}; pageShadow = {0,0,0,180};
    } else {
        bg = {245,240,225,255}; text = {25,20,15,255}; accent = {140,20,20,255}; hdr = {235,230,215,255}; vnum = {180,140,40,255};
        ok = {50,160,90,255}; err = {180,60,60,255}; pageBg = {255,253,245,255}; pageShadow = {0,0,0,60};
    }
}

void AppState::ToggleDarkMode() { darkMode = !darkMode; UpdateColors(); SaveSettings(); }
void AppState::SetStatus(const std::string& msg, float secs) { statusMsg = msg; statusTimer = secs; }

void AppState::InitBuffer() {
    if (isLoading) return;
    isLoading = true;
    std::thread([this]() {
        std::lock_guard<std::mutex> lock(bufferMutex);
        buf.clear(); buf2.clear();
        Chapter c = LoadOrFetch(curBookIdx, curChNum, trans);
        c.bookIndex = curBookIdx; c.bookAbbrev = BIBLE_BOOKS[curBookIdx].abbrev;
        buf.push_back(c);
        if (parallelMode) {
            Chapter c2 = LoadOrFetch(curBookIdx, curChNum, trans2);
            c2.bookIndex = curBookIdx; c2.bookAbbrev = BIBLE_BOOKS[curBookIdx].abbrev;
            buf2.push_back(c2);
        }
        bufAnchorBook = curBookIdx; bufAnchorCh = curChNum;
        if (c.isLoaded) {
            int nb = curBookIdx, nc = curChNum;
            if (NextChapter(nb, nc)) {
                Chapter n = LoadOrFetch(nb, nc, trans);
                n.bookIndex = nb; n.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
                buf.push_back(n);
                if (parallelMode) {
                    Chapter n2 = LoadOrFetch(nb, nc, trans2);
                    n2.bookIndex = nb; n2.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
                    buf2.push_back(n2);
                }
            }
            g_hist.Add(c.book, curBookIdx, curChNum, trans);
        }
        needsPageRebuild = true;
        isLoading = false;
    }).detach();
}

void AppState::ToggleParallelMode(Font font) {
    parallelMode = !parallelMode;
    if (parallelMode) { trans2 = TRANSLATIONS[transIdx2].code; InitBuffer(); } 
    else { std::lock_guard<std::mutex> lock(bufferMutex); buf2.clear(); needsPageRebuild = true; }
    targetScrollY = 0; scrollY = 0; 
    SaveSettings(); UpdateTitle();
}

void AppState::GrowBottom() {
    if (isLoading || buf.empty()) return;
    isLoading = true;
    std::thread([this]() {
        std::lock_guard<std::mutex> lock(bufferMutex);
        const Chapter& last = buf.back();
        int nb = last.bookIndex, nc = last.chapter;
        if (NextChapter(nb, nc)) {
            Chapter ch = LoadOrFetch(nb, nc, trans);
            ch.bookIndex = nb; ch.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
            buf.push_back(ch);
            if (parallelMode) {
                Chapter ch2 = LoadOrFetch(nb, nc, trans2);
                ch2.bookIndex = nb; ch2.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
                buf2.push_back(ch2);
            }
            if ((int)buf.size() > BUF_MAX) {
                buf.pop_front();
                if (parallelMode) buf2.pop_front();
                NextChapter(bufAnchorBook, bufAnchorCh);
            }
        }
        needsPageRebuild = true;
        isLoading = false;
    }).detach();
}

void AppState::GrowTop() {
    if (isLoading || buf.empty()) return;
    isLoading = true;
    std::thread([this]() {
        std::lock_guard<std::mutex> lock(bufferMutex);
        const Chapter& first = buf.front();
        int nb = first.bookIndex, nc = first.chapter;
        if (PrevChapter(nb, nc)) {
            Chapter ch = LoadOrFetch(nb, nc, trans);
            ch.bookIndex = nb; ch.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
            buf.push_front(ch);
            if (parallelMode) {
                Chapter ch2 = LoadOrFetch(nb, nc, trans2);
                ch2.bookIndex = nb; ch2.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
                buf2.push_front(ch2);
            }
            bufAnchorBook = nb; bufAnchorCh = nc;
            if ((int)buf.size() > BUF_MAX) {
                buf.pop_back();
                if (parallelMode) buf2.pop_back();
            }
        }
        needsPageRebuild = true;
        isLoading = false;
    }).detach();
}

void AppState::RebuildPages(Font font) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    pages = BuildPages(buf, buf2, parallelMode, font, 700, 500, fontSize, lineSpacing);
    if (pageIdx >= (int)pages.size()) pageIdx = (int)pages.size() - 1; if (pageIdx < 0) pageIdx = 0; 
    SaveSettings();
}

void AppState::BookPageNext(Font font) { 
    if (pages.empty()) return; 
    if (pageIdx >= (int)pages.size() - 1) { 
        if (!isLoading) GrowBottom(); 
    } 
    if (pageIdx < (int)pages.size() - 1) pageIdx++; 
}

void AppState::BookPagePrev(Font font) { 
    if (pages.empty()) return; 
    if (pageIdx <= 0) { 
        if (!isLoading) GrowTop(); 
    } else pageIdx--; 
}

void AppState::ForceRefresh(Font font) {
    std::string p = "cache/" + trans + "/" + BIBLE_BOOKS[curBookIdx].abbrev + "/" + std::to_string(curChNum) + ".json";
    remove(p.c_str()); InitBuffer(); SetStatus("Passage refreshed from API.");
}

void AppState::UpdateTitle() {
    std::lock_guard<std::mutex> lock(bufferMutex);
    if (!buf.empty() && buf[0].isLoaded) { 
        std::string t = "RayBible - " + buf[0].book; 
        if (parallelMode) t += " / " + trans2; else t += " (" + trans + ")"; 
        SetWindowTitle(t.c_str()); 
    }
    else SetWindowTitle("RayBible");
}

void AppState::StartGlobalSearch() { gSearchResults.clear(); gSearchActive = true; gSearchProgress = 0; SetStatus("Global search started..."); }
void AppState::UpdateGlobalSearch() {
    if (!gSearchActive) return;
    for (int i = 0; i < 3 && gSearchProgress < (int)BIBLE_BOOKS.size(); i++) {
        int b = gSearchProgress; std::string ba = BIBLE_BOOKS[b].abbrev;
        for (int c = 1; c <= BIBLE_BOOKS[b].chapters; c++) {
            if (g_cache.Has(trans, ba, c)) {
                Chapter ch = g_cache.Load(trans, ba, c); std::string query = ToLower(gSearchBuf);
                for (const auto& v : ch.verses) if (ToLower(v.text).find(query) != std::string::npos) gSearchResults.push_back({b, c, v.number, BIBLE_BOOKS[b].name, v.text});
            }
        }
        gSearchProgress++;
    }
    if (gSearchProgress >= (int)BIBLE_BOOKS.size()) { gSearchActive = false; SetStatus("Global search complete: " + std::to_string(gSearchResults.size()) + " matches."); }
}