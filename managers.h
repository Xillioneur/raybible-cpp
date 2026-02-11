#pragma once
#ifndef MANAGERS_H
#define MANAGERS_H

#include "raybible.h"
#include <string>
#include <vector>
#include <mutex>

class CacheManager {
    std::string base;
    std::string Path(const std::string& t, const std::string& b, int c) const;
    std::string TDir(const std::string& t) const;
    std::string BDir(const std::string& t, const std::string& b) const;
    mutable std::mutex mtx;
public:
    CacheManager();
    bool Has(const std::string& t, const std::string& b, int c) const;
    Chapter Load(const std::string& t, const std::string& b, int cn) const;
    bool Save(const Chapter& ch) const;
    void ClearCache();
    CacheStats Stats() const;
};

class FavoritesManager {
    std::vector<Favorite> favs;
    std::string file;
    mutable std::mutex mtx;
    void Load();
    void Save();
public:
    FavoritesManager();
    void Add(const std::string& book, int ch, int v, const std::string& t, const std::string& verseText = "", const std::string& n = "");
    void Remove(const std::string& book, int ch, int v, const std::string& t);
    void UpdateNote(const std::string& book, int ch, int v, const std::string& t, const std::string& note);
    bool Has(const std::string& book, int ch, int v, const std::string& t) const;
    std::vector<Favorite> All() const; // Return copy for thread safety
};

class HistoryManager {
    std::vector<HistoryEntry> hist;
    static const int MAX = 20;
    std::string file;
    mutable std::mutex mtx;
    void Load();
    void Save();
public:
    HistoryManager();
    void Add(const std::string& book, int bookIdx, int ch, const std::string& t);
    std::vector<HistoryEntry> All() const; // Return copy
};

class SettingsManager {
    std::string file = "settings.txt";
public:
    bool darkMode = true;
    float fontSize = 19.0f;
    float lineSpacing = 7.0f;
    int lastBookIdx = 42;
    int lastChNum = 3;
    int lastTransIdx = 0;
    bool parallelMode = false;
    int transIdx2 = 1;
    bool bookMode = false;
    float lastScrollY = 0.0f;
    int lastPageIdx = 0;

    void Load();
    void Save();
};

extern CacheManager g_cache;
extern FavoritesManager g_favs;
extern HistoryManager g_hist;
extern SettingsManager g_settings;

#endif // MANAGERS_H
