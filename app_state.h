#pragma once
#ifndef RAYBIBLE_APP_STATE_H
#define RAYBIBLE_APP_STATE_H

#include "raybible.h"
#include <string>
#include <vector>
#include <deque>

// Forward declaration of rendering functions to avoid circular dependency
void DrawHeader(struct AppState& s, Font f);
void DrawFooter(struct AppState& s, Font f);
void DrawScrollMode(struct AppState& s, Font f);
void DrawBookMode(struct AppState& s, Font f);
void DrawTooltip(struct AppState& s, Font f);

#include <condition_variable>
#include <functional>
#include <queue>

struct NavPoint {
    int bookIdx;
    int chNum;
};

struct AppState {
    // --- Navigation History ---
    std::vector<NavPoint> navHistory;
    int navIndex = -1;
    bool isNavigating = false;

    // --- UI/UX Extras ---
    bool showHelp = false;
    char tooltip[64]{};
    std::atomic<bool> isLoading{false};
    std::atomic<bool> needsPageRebuild{false};
    std::mutex bufferMutex;

    // --- Threading ---
    std::thread workerThread;
    std::mutex queueMutex;
    std::condition_variable queueCondVar;
    std::queue<std::function<void()>> taskQueue;
    std::atomic<bool> quitWorker{false};

    void PushTask(std::function<void()> task, bool clearQueue = false);
    void WorkerLoop();

    // --- Global Search ---
    bool showGlobalSearch = false;
    char gSearchBuf[256]{};
    std::vector<GlobalSearchMatch> gSearchResults;
    bool gSearchActive = false;
    int gSearchProgress = 0;
    float gSearchThreadTimer = 0;

    // --- Mode ---
    bool bookMode = false;
    bool darkMode = true;

    // --- Current position ---
    int  curBookIdx = 42;
    int  curChNum   = 3;
    int  transIdx   = 0;
    std::string trans;

    // --- Multi-chapter ring buffer ---
    std::deque<Chapter> buf;
    std::deque<Chapter> buf2;
    int  bufAnchorBook = 42;
    int  bufAnchorCh   = 3;
    static const int BUF_MAX = 5;

    // --- Scroll mode ---
    float scrollY = 0.0f;
    float targetScrollY = 0.0f;
    int   scrollToVerse = -1;

    // --- Parallel mode ---
    bool parallelMode = false;
    int transIdx2 = 1;
    std::string trans2 = "kjv";

    // --- Book mode ---
    std::vector<Page> pages;
    int pageIdx = 0;

    // --- Search ---
    bool showSearch = false;
    char searchBuf[256]{};
    bool searchCS = false;
    std::vector<SearchMatch> searchResults;
    int  searchSel = -1;

    // --- UI/UX ---
    float fontSize = 19.0f;
    float lineSpacing = 7.0f;

    // --- Panels ---
    bool showHistory   = false;
    bool showFavorites = false;
    bool showCache     = false;
    bool showJump      = false;
    bool showPlan      = false;
    bool showBurgerMenu = false;
    char jumpBuf[128]{};
    CacheStats cacheStats{};

    // --- Dropdowns ---
    bool  showBookDrop   = false;
    float bookDropScroll = 0.0f;
    bool  showTransDrop  = false;
    bool  showTransDrop2 = false;

    // --- Status toast ---
    std::string statusMsg;
    float       statusTimer = 0.0f;

    // --- Colors ---
    Color bg, text, accent, hdr, vnum, ok, err, pageBg, pageShadow;

    AppState();
    ~AppState();
    void SaveSettings();
    void UpdateColors();
    void ToggleDarkMode();
    void SetStatus(const std::string& msg, float secs = 2.5f);
    void InitBuffer();
    void ToggleParallelMode(Font font);
    void GrowBottom();
    void GrowTop();
    void RebuildPages(Font font);
    void BookPageNext(Font font);
    void BookPagePrev(Font font);
    void PrevBook();
    void NextBook();
    void ForceRefresh(Font font);
    void UpdateTitle();
    void GoBack();
    void GoForward();
    void PrevSequential();
    void NextSequential();
    void PushNavPoint(int b, int c);
    void StartGlobalSearch();
    void UpdateGlobalSearch();
    void Update(); // Main thread update
};

#endif // APP_STATE_H
