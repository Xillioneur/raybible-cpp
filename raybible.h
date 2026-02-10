#pragma once
#ifndef RAYBIBLE_H
#define RAYBIBLE_H

#include "raylib.h"
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>

// --- Data Structures ---

struct Verse {
    int number;
    std::string text;
};

struct Chapter {
    std::string book;
    std::string bookAbbrev;
    int bookIndex;
    int chapter;
    std::string translation;
    std::vector<Verse> verses;
    time_t fetchedAt;
    bool fromCache;
    bool isLoaded;
};

struct BookInfo {
    std::string name;
    std::string abbrev;
    int chapters;
};

struct Translation {
    std::string code;
    std::string name;
};

struct Page {
    std::vector<std::string> lines;
    std::vector<std::string> lines2;
    int startVerse;
    int endVerse;
    int chapterBufIndex;
    bool isChapterStart;
};

struct Favorite {
    std::string book;
    std::string translation;
    std::string note;
    std::string text;
    int chapter;
    int verse;
    time_t addedAt;

    std::string GetKey() const {
        return translation + ":" + book + ":" + std::to_string(chapter) + ":" + std::to_string(verse);
    }
    std::string GetDisplay() const {
        return book + " " + std::to_string(chapter) + ":" + std::to_string(verse) + " (" + translation + ")";
    }
};

struct HistoryEntry {
    std::string book;
    std::string translation;
    int bookIndex;
    int chapter;
    time_t accessedAt;

    std::string GetKey() const {
        return translation + ":" + book + ":" + std::to_string(chapter);
    }
};

struct SearchMatch {
    int verseNumber;
    std::string text;
    size_t matchPos;
    size_t matchLen;
};

struct GlobalSearchMatch {
    int bookIdx;
    int chapter;
    int verse;
    std::string bookName;
    std::string text;
};

struct CacheStats {
    int totalChapters;
    int totalVerses;
    long totalSize;
    std::map<std::string, int> byTranslation;
};

// --- Constants ---

extern const std::vector<BookInfo> BIBLE_BOOKS;
extern const std::vector<Translation> TRANSLATIONS;

#endif // RAYBIBLE_H
