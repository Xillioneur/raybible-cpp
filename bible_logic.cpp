#include "bible_logic.h"
#include "utils.h"
#include "managers.h"
#include <sstream>
#include <algorithm>

Chapter FetchFromAPI(int bookIdx, int chNum, const std::string& trans) {
    Chapter r{};
    r.bookIndex  = bookIdx;
    r.bookAbbrev = BIBLE_BOOKS[bookIdx].abbrev;
    r.chapter    = chNum;
    r.translation = trans;
    r.fetchedAt  = time(nullptr);
    r.fromCache  = false;
    r.isLoaded   = false;

    // Use bolls.life API for more translations and better stability
    // Format: https://bolls.life/get-text/{TRANS}/{BOOK_ID}/{CH}/
    // Book ID is 1-based index (Genesis = 1, John = 43)
    int bollsId = bookIdx + 1;
    std::string url = "https://bolls.life/get-text/" + trans + "/" + std::to_string(bollsId) + "/" + std::to_string(chNum) + "/";
    
    std::string resp = HttpGet(url);
    if (resp.empty() || resp == "[]" || resp.find("not found") != std::string::npos) {
        r.book = BIBLE_BOOKS[bookIdx].name + " " + std::to_string(chNum);
        r.verses.push_back({1, "Error loading content or Translation not supported for this book. Try a different translation."});
        r.isLoaded = true; 
        return r;
    }

    r.book = BIBLE_BOOKS[bookIdx].name + " " + std::to_string(chNum);
    auto verseObjects = JArr(resp, ""); // Parse top-level array
    for (const auto& vo : verseObjects) {
        Verse v;
        v.number = JInt(vo, "verse");
        std::string raw = JStr(vo, "text");
        v.rawText = raw; // Save original with tags
        v.text   = StripTags(raw); // Clean version
        
        // Final polish
        size_t p = 0;
        while ((p = v.text.find('\n', p)) != std::string::npos) { v.text.replace(p, 1, " "); }
        p = 0;
        while ((p = v.text.find("  ", p)) != std::string::npos) { v.text.replace(p, 2, " "); }
        v.text.erase(0, v.text.find_first_not_of(" \t\r"));
        v.text.erase(v.text.find_last_not_of(" \t\r") + 1);

        if (!v.text.empty()) {
            r.verses.push_back(v);
        }
    }
    
    if (!r.verses.empty()) { 
        g_cache.Save(r); 
        r.isLoaded = true; 
    } else {
        r.verses.push_back({1, "Passage found but contains no verses."});
        r.isLoaded = true;
    }
    return r;
}

Chapter LoadOrFetch(int bookIdx, int chNum, const std::string& trans) {
    if (g_cache.Has(trans, BIBLE_BOOKS[bookIdx].abbrev, chNum)) {
        Chapter ch = g_cache.Load(trans, BIBLE_BOOKS[bookIdx].abbrev, chNum);
        ch.bookIndex  = bookIdx;
        ch.bookAbbrev = BIBLE_BOOKS[bookIdx].abbrev;
        return ch;
    }
    return FetchFromAPI(bookIdx, chNum, trans);
}

bool NextChapter(int& bookIdx, int& chNum) {
    chNum++;
    if (chNum > (int)BIBLE_BOOKS[bookIdx].chapters) { chNum = 1; bookIdx++; }
    if (bookIdx >= (int)BIBLE_BOOKS.size()) {
        bookIdx = (int)BIBLE_BOOKS.size() - 1;
        chNum   = BIBLE_BOOKS[bookIdx].chapters;
        return false;
    }
    return true;
}

bool PrevChapter(int& bookIdx, int& chNum) {
    chNum--;
    if (chNum < 1) {
        bookIdx--;
        if (bookIdx < 0) { bookIdx = 0; chNum = 1; return false; }
        chNum = BIBLE_BOOKS[bookIdx].chapters;
    }
    return true;
}

bool ParseReference(std::string input, int& bookIdx, int& chNum, int& vNum) {
    if (input.empty()) return false;
    std::string s = ToLower(input);
    std::replace(s.begin(), s.end(), ':', ' ');
    int foundBk = -1;
    size_t bestMatchLen = 0;
    for (int i = 0; i < (int)BIBLE_BOOKS.size(); i++) {
        std::string bn = ToLower(BIBLE_BOOKS[i].name);
        std::string ba = ToLower(BIBLE_BOOKS[i].abbrev);
        if (s.find(bn) == 0 && bn.length() > bestMatchLen) { foundBk = i; bestMatchLen = bn.length(); }
        else if (s.find(ba) == 0 && ba.length() > bestMatchLen) { foundBk = i; bestMatchLen = ba.length(); }
    }
    if (foundBk == -1) return false;
    bookIdx = foundBk;
    std::string remaining = s.substr(bestMatchLen);
    std::stringstream ss(remaining);
    int c = 1, v = 1;
    if (ss >> c) { chNum = c; if (ss >> v) vNum = v; else vNum = 1; }
    else { chNum = 1; vNum = 1; }
    if (chNum < 1) chNum = 1;
    if (chNum > BIBLE_BOOKS[bookIdx].chapters) chNum = BIBLE_BOOKS[bookIdx].chapters;
    return true;
}

std::vector<std::pair<int, int>> GetDailyReading(int dayOfYear) {
    std::vector<std::pair<int, int>> r;
    int targetStart = (dayOfYear - 1) * 3;
    for (int i = 0; i < 3; i++) {
        int target = targetStart + i;
        int current = 0;
        for (int b = 0; b < (int)BIBLE_BOOKS.size(); b++) {
            if (current + BIBLE_BOOKS[b].chapters > target) { r.push_back({b, target - current + 1}); break; }
            current += BIBLE_BOOKS[b].chapters;
        }
    }
    return r;
}

std::vector<SearchMatch> SearchVerses(const std::deque<Chapter>& chapters, const std::string& q, bool cs) {
    std::vector<SearchMatch> m;
    if (q.empty()) return m;
    std::string sq = cs ? q : ToLower(q);
    for (const auto& ch : chapters) {
        for (const auto& v : ch.verses) {
            std::string st = cs ? v.text : ToLower(v.text);
            size_t p = 0;
            while ((p = st.find(sq, p)) != std::string::npos) {
                m.push_back({ch.bookIndex, ch.chapter, v.number, v.text, p, sq.size()});
                p += sq.size();
            }
        }
    }
    return m;
}
