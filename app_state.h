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

struct AppState {
    // --- UI/UX Extras ---
    bool showHelp = false;
    char tooltip[64]{};
    std::atomic<bool> isLoading{false};
    std::atomic<bool> needsPageRebuild{false};
    std::mutex bufferMutex;

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
    void ForceRefresh(Font font);
    void UpdateTitle();
    void StartGlobalSearch();
    void UpdateGlobalSearch();
};

#endif // APP_STATE_H
