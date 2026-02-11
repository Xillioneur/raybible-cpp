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
    targetScrollY = scrollY = g_settings.lastScrollY;
    pageIdx = g_settings.lastPageIdx;
    trans = TRANSLATIONS[transIdx].code; trans2 = TRANSLATIONS[transIdx2].code;
    UpdateColors(); UpdateTitle();
    workerThread = std::thread(&AppState::WorkerLoop, this);
}

AppState::~AppState() {
    quitWorker = true;
    queueCondVar.notify_all();
    if (workerThread.joinable()) workerThread.join();
}

void AppState::PushTask(std::function<void()> task, bool clearQueue) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (clearQueue) { std::queue<std::function<void()>> empty; std::swap(taskQueue, empty); }
        taskQueue.push(task);
    }
    queueCondVar.notify_one();
}

void AppState::WorkerLoop() {
    while (!quitWorker) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondVar.wait(lock, [this]() { return !taskQueue.empty() || quitWorker; });
            if (quitWorker) break;
            task = taskQueue.front(); taskQueue.pop();
        }
        if (task) task();
    }
}

void AppState::SaveSettings() {
    g_settings.darkMode = darkMode; g_settings.fontSize = fontSize; g_settings.lineSpacing = lineSpacing;
    g_settings.lastBookIdx = curBookIdx; g_settings.lastChNum = curChNum; g_settings.lastTransIdx = transIdx;
    g_settings.parallelMode = parallelMode; g_settings.transIdx2 = transIdx2; g_settings.bookMode = bookMode;
    g_settings.lastScrollY = targetScrollY; g_settings.lastPageIdx = pageIdx; g_settings.Save();
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

void AppState::InitBuffer(bool resetScroll) {
    isLoading = true;
    if (resetScroll) { targetScrollY = 0; scrollY = 0; scrollChapterIdx = 0; }
    PushTask([this]() {
        { std::lock_guard<std::mutex> lock(bufferMutex); buf.clear(); buf2.clear(); }
        Chapter c = LoadOrFetch(curBookIdx, curChNum, trans);
        c.bookIndex = curBookIdx; c.bookAbbrev = BIBLE_BOOKS[curBookIdx].abbrev;
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            buf.push_back(c);
            if (parallelMode) {
                Chapter c2 = LoadOrFetch(curBookIdx, curChNum, trans2);
                c2.bookIndex = curBookIdx; c2.bookAbbrev = BIBLE_BOOKS[curBookIdx].abbrev;
                buf2.push_back(c2);
            }
            bufAnchorBook = curBookIdx; bufAnchorCh = curChNum;
            needsPageRebuild = true;
        }
        if (c.isLoaded) {
            int nb = curBookIdx, nc = curChNum;
            if (NextChapter(nb, nc)) {
                Chapter n = LoadOrFetch(nb, nc, trans); n.bookIndex = nb; n.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
                {
                    std::lock_guard<std::mutex> lock(bufferMutex);
                    buf.push_back(n);
                    if (parallelMode) {
                        Chapter n2 = LoadOrFetch(nb, nc, trans2);
                        n2.bookIndex = nb; n2.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
                        buf2.push_back(n2);
                    }
                    needsPageRebuild = true;
                }
            }
            g_hist.Add(c.book, curBookIdx, curChNum, trans);
            if (!isNavigating) PushNavPoint(curBookIdx, curChNum);
        }
        isLoading = false;
    }, true);
}

void AppState::ToggleParallelMode(Font font) {
    parallelMode = !parallelMode;
    if (parallelMode) { trans2 = TRANSLATIONS[transIdx2].code; InitBuffer(); } 
    else { std::lock_guard<std::mutex> lock(bufferMutex); buf2.clear(); needsPageRebuild = true; }
    targetScrollY = 0; scrollY = 0; SaveSettings(); UpdateTitle();
}

void AppState::GrowBottom() {
    if (isLoading || buf.empty()) return;
    isLoading = true;
    PushTask([this]() {
        int nb, nc;
        { std::lock_guard<std::mutex> lock(bufferMutex); const Chapter& last = buf.back(); nb = last.bookIndex; nc = last.chapter; }
        if (NextChapter(nb, nc)) {
            Chapter ch = LoadOrFetch(nb, nc, trans); ch.bookIndex = nb; ch.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                buf.push_back(ch);
                if (parallelMode) { Chapter ch2 = LoadOrFetch(nb, nc, trans2); ch2.bookIndex = nb; ch2.bookAbbrev = BIBLE_BOOKS[nb].abbrev; buf2.push_back(ch2); }
                if ((int)buf.size() > BUF_MAX) { buf.pop_front(); if (parallelMode) buf2.pop_front(); NextChapter(bufAnchorBook, bufAnchorCh); }
                needsPageRebuild = true;
            }
        }
        isLoading = false;
    });
}

void AppState::GrowTop() {
    if (isLoading || buf.empty()) return;
    isLoading = true;
    PushTask([this]() {
        int nb, nc;
        { std::lock_guard<std::mutex> lock(bufferMutex); const Chapter& first = buf.front(); nb = first.bookIndex; nc = first.chapter; }
        if (PrevChapter(nb, nc)) {
            Chapter ch = LoadOrFetch(nb, nc, trans); ch.bookIndex = nb; ch.bookAbbrev = BIBLE_BOOKS[nb].abbrev;
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                buf.push_front(ch);
                if (parallelMode) { Chapter ch2 = LoadOrFetch(nb, nc, trans2); ch2.bookIndex = nb; ch2.bookAbbrev = BIBLE_BOOKS[nb].abbrev; buf2.push_front(ch2); }
                bufAnchorBook = nb; bufAnchorCh = nc;
                if ((int)buf.size() > BUF_MAX) { buf.pop_back(); if (parallelMode) buf2.pop_back(); }
                needsPageRebuild = true;
            }
        }
        isLoading = false;
    });
}

void AppState::RebuildPages(Font font) { 
    std::lock_guard<std::mutex> lock(bufferMutex); 
    if (buf.empty()) return;
    pages = BuildPages(buf, buf2, parallelMode, font, 700, 500, fontSize, lineSpacing); 
    if (pageIdx >= (int)pages.size()) pageIdx = (int)pages.size() - 1; 
    if (pageIdx < 0) pageIdx = 0; 
    SaveSettings(); 
}

void AppState::BookPageNext(Font font) { if (pages.empty()) return; if (pageIdx >= (int)pages.size() - 1) { if (!isLoading) GrowBottom(); } if (pageIdx < (int)pages.size() - 1) pageIdx++; }
void AppState::BookPagePrev(Font font) { if (pages.empty()) return; if (pageIdx <= 0) { if (!isLoading) GrowTop(); } else pageIdx--; }

void AppState::PrevBook() { if (curBookIdx > 0) { curBookIdx--; curChNum = 1; InitBuffer(); } }
void AppState::NextBook() { if (curBookIdx < (int)BIBLE_BOOKS.size() - 1) { curBookIdx++; curChNum = 1; InitBuffer(); } }

void AppState::ForceRefresh(Font font) {
    std::string p = "cache/" + trans + "/" + BIBLE_BOOKS[curBookIdx].abbrev + "/" + std::to_string(curChNum) + ".json";
    remove(p.c_str()); InitBuffer(); SetStatus("Passage refreshed.");
}

void AppState::CopyChapter() {
    std::lock_guard<std::mutex> lock(bufferMutex); if (buf.empty() || !buf[0].isLoaded) return;
    std::string fullText = buf[0].book + " (" + buf[0].translation + ")\n\n";
    for (const auto& v : buf[0].verses) fullText += std::to_string(v.number) + " " + v.text + "\n";
    CopyToClipboard(fullText); SetStatus("Chapter copied!");
}

void AppState::UpdateTitle() {
    std::lock_guard<std::mutex> lock(bufferMutex);
    int ci = bookMode ? 0 : scrollChapterIdx;
    if (ci < (int)buf.size() && buf[ci].isLoaded) { 
        std::string t = "RayBible - " + buf[ci].book; 
        if (parallelMode) t += " / " + trans2; else t += " (" + trans + ")"; 
        SetWindowTitle(t.c_str()); 
    }
    else SetWindowTitle("RayBible");
}

void AppState::PushNavPoint(int b, int c) {
    if (navIndex >= 0 && navHistory[navIndex].bookIdx == b && navHistory[navIndex].chNum == c) return;
    if (navIndex < (int)navHistory.size() - 1) navHistory.erase(navHistory.begin() + navIndex + 1, navHistory.end());
    navHistory.push_back({b, c}); navIndex = (int)navHistory.size() - 1;
    if (navHistory.size() > 50) { navHistory.erase(navHistory.begin()); navIndex--; }
}

void AppState::GoBack() { if (navIndex > 0) { navIndex--; isNavigating = true; curBookIdx = navHistory[navIndex].bookIdx; curChNum = navHistory[navIndex].chNum; InitBuffer(); isNavigating = false; } }
void AppState::GoForward() { if (navIndex < (int)navHistory.size() - 1) { navIndex++; isNavigating = true; curBookIdx = navHistory[navIndex].bookIdx; curChNum = navHistory[navIndex].chNum; InitBuffer(); isNavigating = false; } }

void AppState::PrevSequential() { int b = curBookIdx, c = curChNum; if (PrevChapter(b, c)) { curBookIdx = b; curChNum = c; InitBuffer(); } }
void AppState::NextSequential() { int b = curBookIdx, c = curChNum; if (NextChapter(b, c)) { curBookIdx = b; curChNum = c; InitBuffer(); } }

void AppState::StartGlobalSearch() {
    gSearchResults.clear(); gSearchActive = true; gSearchProgress = 0;
    std::string query = ToLower(gSearchBuf); std::string currentTrans = trans;
    PushTask([this, query, currentTrans]() {
        for (int b = 0; b < (int)BIBLE_BOOKS.size(); b++) {
            if (quitWorker || !gSearchActive) break;
            std::string ba = BIBLE_BOOKS[b].abbrev;
            for (int c = 1; c <= BIBLE_BOOKS[b].chapters; c++) {
                if (g_cache.Has(currentTrans, ba, c)) {
                    Chapter ch = g_cache.Load(currentTrans, ba, c);
                    for (const auto& v : ch.verses) if (ToLower(v.text).find(query) != std::string::npos) { std::lock_guard<std::mutex> lock(bufferMutex); gSearchResults.push_back({b, c, v.number, BIBLE_BOOKS[b].name, v.text}); }
                }
            }
            gSearchProgress = b + 1;
        }
        gSearchActive = false;
    });
}

void AppState::UpdateGlobalSearch() {}
void AppState::Update() { UpdateTitle(); }