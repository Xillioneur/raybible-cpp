#include "managers.h"
#include "utils.h"
#include <sstream>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
#endif

// --- CacheManager ---

CacheManager::CacheManager() { base = "cache"; MakeDir(base); }
std::string CacheManager::Path(const std::string& t, const std::string& b, int c) const { return base + "/" + t + "/" + b + "/" + std::to_string(c) + ".json"; }
std::string CacheManager::TDir(const std::string& t) const { return base + "/" + t; }
std::string CacheManager::BDir(const std::string& t, const std::string& b) const { return base + "/" + t + "/" + b; }

bool CacheManager::Has(const std::string& t, const std::string& b, int c) const { 
    std::lock_guard<std::mutex> lock(mtx);
    return FileExists(Path(t, b, c)); 
}

Chapter CacheManager::Load(const std::string& t, const std::string& b, int cn) const {
    std::lock_guard<std::mutex> lock(mtx);
    Chapter ch{};
    ch.isLoaded = false;
    std::string json = ReadFile(Path(t, b, cn));
    if (json.empty()) return ch;
    ch.book        = JStr(json, "book");
    ch.chapter     = JInt(json, "chapter");
    ch.translation = JStr(json, "translation");
    ch.fetchedAt   = JLong(json, "fetchedAt");
    ch.fromCache   = true;
    ch.isLoaded    = true;
    for (const auto& v : JArr(json, "verses")) {
        Verse vr;
        vr.number = JInt(v, "number");
        vr.rawText = JStr(v, "rawText");
        vr.text   = StripTags(JStr(v, "text"));
        if (!vr.text.empty()) ch.verses.push_back(vr);
    }
    return ch;
}

bool CacheManager::Save(const Chapter& ch) const {
    std::lock_guard<std::mutex> lock(mtx);
    MakeDir(TDir(ch.translation));
    std::string ba;
    for (const auto& bk : BIBLE_BOOKS) {
        if (ch.book.find(bk.name) != std::string::npos || ch.book.find(bk.abbrev) != std::string::npos) { ba = bk.abbrev; break; }
    }
    if (ba.empty()) return false;
    MakeDir(BDir(ch.translation, ba));

    std::ostringstream j;
    j << "{\n"
      << "  \"book\":\"" << ch.book << "\",\n"
      << "  \"chapter\":" << ch.chapter << ",\n"
      << "  \"translation\":\"" << ch.translation << "\",\n"
      << "  \"fetchedAt\":" << ch.fetchedAt << ",\n"
      << "  \"verses\":[\n";
    for (size_t i = 0; i < ch.verses.size(); i++) {
        std::string t = ch.verses[i].text;
        size_t pos = 0;
        while ((pos = t.find("\"", pos)) != std::string::npos) { t.replace(pos, 1, "\\\""); pos += 2; }
        std::string rt = ch.verses[i].rawText;
        pos = 0;
        while ((pos = rt.find("\"", pos)) != std::string::npos) { rt.replace(pos, 1, "\\\""); pos += 2; }
        j << "    {\"number\":" << ch.verses[i].number << ",\"text\":\"" << t << "\",\"rawText\":\"" << rt << "\"}";
        if (i + 1 < ch.verses.size()) j << ",";
        j << "\n";
    }
    j << "  ]\n}";
    return WriteFile(Path(ch.translation, ba, ch.chapter), j.str());
}

void CacheManager::ClearCache() {
    std::lock_guard<std::mutex> lock(mtx);
#ifdef _WIN32
    system("rmdir /s /q cache");
#else
    system("rm -rf cache");
#endif
    MakeDir(base);
}

CacheStats CacheManager::Stats() const {
    std::lock_guard<std::mutex> lock(mtx);
    CacheStats s{};
    for (const auto& tr : TRANSLATIONS) {
        int tc = 0;
        for (const auto& bk : BIBLE_BOOKS) {
            std::string bd = BDir(tr.code, bk.abbrev);
            if (!DirExists(bd)) continue;
#ifdef _WIN32
            WIN32_FIND_DATAA fd;
            HANDLE hf = FindFirstFileA((bd + "\\*").c_str(), &fd);
            if (hf != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        std::string fn = fd.cFileName;
                        if (fn.find(".json") != std::string::npos) {
                            std::string fp = bd + "/" + fn;
                            s.totalSize += GetFileSize(fp);
                            s.totalChapters++; tc++;
                            s.totalVerses += (int)JArr(ReadFile(fp), "verses").size();
                        }
                    }
                } while (FindNextFileA(hf, &fd));
                FindClose(hf);
            }
#else
            DIR* d = opendir(bd.c_str());
            if (!d) continue;
            struct dirent* e;
            while ((e = readdir(d))) {
                if (e->d_type == DT_REG) {
                    std::string fn = e->d_name;
                    if (fn.find(".json") != std::string::npos) {
                        std::string fp = bd + "/" + fn;
                        s.totalSize += GetFileSize(fp);
                        s.totalChapters++; tc++;
                        s.totalVerses += (int)JArr(ReadFile(fp), "verses").size();
                    }
                }
            }
            closedir(d);
#endif
        }
        s.byTranslation[tr.code] = tc;
    }
    return s;
}

// --- FavoritesManager ---

FavoritesManager::FavoritesManager() { file = "favorites.txt"; Load(); }
void FavoritesManager::Load() {
    std::string c = ReadFile(file);
    if (c.empty()) return;
    std::istringstream iss(c); std::string ln;
    while (std::getline(iss, ln)) {
        if (ln.empty()) continue;
        std::istringstream ls(ln); Favorite f; std::string temp;
        std::getline(ls, f.translation, '|');
        std::getline(ls, f.book, '|');
        std::getline(ls, temp, '|'); try { f.chapter = std::stoi(temp); } catch(...) { f.chapter = 1; }
        std::getline(ls, temp, '|'); try { f.verse = std::stoi(temp); } catch(...) { f.verse = 1; }
        std::getline(ls, temp, '|'); try { f.addedAt = (time_t)std::stoll(temp); } catch(...) { f.addedAt = 0; }
        std::getline(ls, f.note);
        if (!f.book.empty()) favs.push_back(f);
    }
}
void FavoritesManager::Save() {
    std::ostringstream o;
    for (const auto& f : favs)
        o << f.translation << "|" << f.book << "|" << f.chapter << "|" << f.verse << "|" << (long long)f.addedAt << "|" << f.note << "\n";
    WriteFile(file, o.str());
}
void FavoritesManager::Add(const std::string& book, int ch, int v, const std::string& t, const std::string& verseText, const std::string& n) {
    std::lock_guard<std::mutex> lock(mtx);
    Favorite f; f.book = book; f.chapter = ch; f.verse = v; f.translation = t; f.text = verseText; f.note = n; f.addedAt = time(nullptr);
    for (const auto& ex : favs) if (ex.GetKey() == f.GetKey()) return;
    favs.push_back(f); Save();
}
void FavoritesManager::Remove(const std::string& book, int ch, int v, const std::string& t) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + book + ":" + std::to_string(ch) + ":" + std::to_string(v);
    favs.erase(std::remove_if(favs.begin(), favs.end(), [&k](const Favorite& f) { return f.GetKey() == k; }), favs.end());
    Save();
}
void FavoritesManager::UpdateNote(const std::string& book, int ch, int v, const std::string& t, const std::string& note) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + book + ":" + std::to_string(ch) + ":" + std::to_string(v);
    for (auto& f : favs) { if (f.GetKey() == k) { f.note = note; Save(); break; } }
}
bool FavoritesManager::Has(const std::string& book, int ch, int v, const std::string& t) const {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + book + ":" + std::to_string(ch) + ":" + std::to_string(v);
    for (const auto& f : favs) if (f.GetKey() == k) return true;
    return false;
}
std::vector<Favorite> FavoritesManager::All() const {
    std::lock_guard<std::mutex> lock(mtx);
    return favs;
}

// --- HistoryManager ---

HistoryManager::HistoryManager() { file = "history.txt"; Load(); }
void HistoryManager::Load() {
    std::string c = ReadFile(file);
    if (c.empty()) return;
    std::istringstream iss(c); std::string ln;
    while (std::getline(iss, ln)) {
        if (ln.empty()) continue;
        std::istringstream ls(ln); HistoryEntry e; std::string temp;
        std::getline(ls, e.translation, '|');
        std::getline(ls, e.book, '|');
        std::getline(ls, temp, '|'); try { e.bookIndex = std::stoi(temp); } catch(...) { e.bookIndex = 0; }
        std::getline(ls, temp, '|'); try { e.chapter = std::stoi(temp); } catch(...) { e.chapter = 1; }
        std::getline(ls, temp, '|'); try { e.accessedAt = (time_t)std::stoll(temp); } catch(...) { e.accessedAt = 0; }
        if (!e.book.empty()) hist.push_back(e);
    }
}
void HistoryManager::Save() {
    std::ostringstream o;
    for (const auto& e : hist)
        o << e.translation << "|" << e.book << "|" << e.bookIndex << "|" << e.chapter << "|" << (long long)e.accessedAt << "\n";
    WriteFile(file, o.str());
}
void HistoryManager::Add(const std::string& book, int bookIdx, int ch, const std::string& t) {
    std::lock_guard<std::mutex> lock(mtx);
    HistoryEntry e; e.book = book; e.bookIndex = bookIdx; e.chapter = ch; e.translation = t; e.accessedAt = time(nullptr);
    hist.erase(std::remove_if(hist.begin(), hist.end(), [&e](const HistoryEntry& h) { return h.GetKey() == e.GetKey(); }), hist.end());
    hist.insert(hist.begin(), e);
    if ((int)hist.size() > MAX) hist.resize(MAX);
    Save();
}
std::vector<HistoryEntry> HistoryManager::All() const {
    std::lock_guard<std::mutex> lock(mtx);
    return hist;
}

// --- SettingsManager ---

void SettingsManager::Load() {
    std::string c = ReadFile(file);
    if (c.empty()) return;
    std::istringstream iss(c); std::string key;
    while (iss >> key) {
        if (key == "theme") iss >> theme;
        else if (key == "fontSize") iss >> fontSize;
        else if (key == "lineSpacing") iss >> lineSpacing;
        else if (key == "lastBookIdx") iss >> lastBookIdx;
        else if (key == "lastChNum") iss >> lastChNum;
        else if (key == "lastTransIdx") iss >> lastTransIdx;
        else if (key == "parallelMode") iss >> parallelMode;
        else if (key == "transIdx2") iss >> transIdx2;
        else if (key == "bookMode") iss >> bookMode;
        else if (key == "lastScrollY") iss >> lastScrollY;
        else if (key == "lastPageIdx") iss >> lastPageIdx;
        else if (key == "winW") iss >> winW;
        else if (key == "winH") iss >> winH;
        else if (key == "winX") iss >> winX;
        else if (key == "winY") iss >> winY;
    }
}
void SettingsManager::Save() {
    std::ostringstream o;
    o << "theme " << theme << "\n" 
      << "fontSize " << fontSize << "\n" 
      << "lineSpacing " << lineSpacing << "\n"
      << "lastBookIdx " << lastBookIdx << "\n" 
      << "lastChNum " << lastChNum << "\n" 
      << "lastTransIdx " << lastTransIdx << "\n"
      << "parallelMode " << parallelMode << "\n" 
      << "transIdx2 " << transIdx2 << "\n" 
      << "bookMode " << bookMode << "\n"
      << "lastScrollY " << lastScrollY << "\n"
      << "lastPageIdx " << lastPageIdx << "\n"
      << "winW " << winW << "\n"
      << "winH " << winH << "\n"
      << "winX " << winX << "\n"
      << "winY " << winY << "\n";
    WriteFile(file, o.str());
}

CacheManager g_cache;
FavoritesManager g_favs;
HistoryManager g_hist;
SettingsManager g_settings;
