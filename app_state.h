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

#include <set>

struct NavPoint {
    int bookIdx;
    int chNum;
};

struct AppState {
    // --- Navigation History ---
    std::vector<NavPoint> navHistory;
    int navIndex = -1;
    bool isNavigating = false;

    // --- Selection ---
    std::set<int> selectedVerses; // Verse numbers selected in current chapter
    void ToggleVerseSelection(int vNum);
    void CopySelection();
    void ClearSelection();

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
    int  theme = 0; // 0: Dark, 1: Light, 2: Sepia, 3: Parchment
    bool studyMode = false; // Enables Strong's numbers

    // --- Word Study ---
    bool showWordStudy = false;
    StrongsDef currentStrongs;
    void LookupStrongs(const std::string& number);

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
    int   scrollChapterIdx = 0; // Tracks which buffered chapter is visible

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
    bool showNoteEditor = false;
    char jumpBuf[128]{};
    char noteBuf[512]{};
    std::string selectedFavoriteKey;
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
    void NextTheme();
    void SetStatus(const std::string& msg, float secs = 2.5f);
    void InitBuffer(bool resetScroll = true);
    void ToggleParallelMode(Font font);
    void GrowBottom();
    void GrowTop();
    void RebuildPages(Font font);
    void BookPageNext(Font font);
    void BookPagePrev(Font font);
    void PrevBook();
    void NextBook();
    void ForceRefresh(Font font);
    void CopyChapter();
    void UpdateTitle();
    void GoBack();
    void GoForward();
    void PrevSequential();
    void NextSequential();
    void PushNavPoint(int b, int c);
    void StartGlobalSearch();
    void UpdateGlobalSearch();
    void Update(); // Main thread update
    bool InputActive() const { return showSearch || showJump || showGlobalSearch || showNoteEditor || showWordStudy; }
};

#endif // APP_STATE_H
