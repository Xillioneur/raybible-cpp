#include "managers.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
#endif

CacheManager g_cache;
StudyManager g_study;
HistoryManager g_hist;
SettingsManager g_settings;

// --- CacheManager ---
CacheManager::CacheManager() { base = "cache"; MakeDir(base); }
std::string CacheManager::Path(const std::string& t, const std::string& b, int c) const { return base + "/" + t + "/" + b + "/" + std::to_string(c) + ".json"; }
std::string CacheManager::TDir(const std::string& t) const { return base + "/" + t; }
std::string CacheManager::BDir(const std::string& t, const std::string& b) const { return base + "/" + t + "/" + b; }

bool CacheManager::Has(const std::string& t, const std::string& b, int c) const { return FileExists(Path(t, b, c)); }

Chapter CacheManager::Load(const std::string& t, const std::string& b, int cn) const {
    std::lock_guard<std::mutex> lock(mtx);
    std::string json = ReadFile(Path(t, b, cn));
    Chapter ch{};
    ch.book = JStr(json, "book");
    ch.chapter = JInt(json, "chapter");
    ch.translation = JStr(json, "translation");
    ch.fetchedAt = (time_t)JLong(json, "fetchedAt");
    ch.fromCache = true;
    ch.isLoaded = true;
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

// --- StudyManager ---

StudyManager::StudyManager() { file = "study_data.txt"; Load(); }
void StudyManager::Load() {
    std::string c = ReadFile(file);
    if (c.empty()) return;
    std::istringstream iss(c); std::string ln;
    while (std::getline(iss, ln)) {
        if (ln.empty()) continue;
        std::istringstream ls(ln); VerseData d; std::string temp;
        std::getline(ls, d.translation, '|');
        std::getline(ls, d.book, '|');
        std::getline(ls, temp, '|'); try { d.chapter = std::stoi(temp); } catch(...) { d.chapter = 1; }
        std::getline(ls, temp, '|'); try { d.verse = std::stoi(temp); } catch(...) { d.verse = 1; }
        std::getline(ls, temp, '|'); try { d.highlightColor = std::stoi(temp); } catch(...) { d.highlightColor = 0; }
        std::getline(ls, temp, '|'); try { d.isBookmarked = (temp == "1"); } catch(...) { d.isBookmarked = false; }
        std::getline(ls, temp, '|'); try { d.addedAt = (time_t)std::stoll(temp); } catch(...) { d.addedAt = 0; }
        std::getline(ls, d.text, '|');
        std::getline(ls, d.note);
        d.note = ReplaceAll(d.note, "\\n", "\n");
        if (!d.book.empty()) data.push_back(d);
    }
}
void StudyManager::Save() {
    std::ostringstream o;
    for (const auto& d : data) {
        std::string escapedNote = ReplaceAll(d.note, "\n", "\\n");
        o << d.translation << "|" << d.book << "|" << d.chapter << "|" << d.verse << "|" << d.highlightColor << "|" << (d.isBookmarked ? "1" : "0") << "|" << (long long)d.addedAt << "|" << d.text << "|" << escapedNote << "\n";
    }
    WriteFile(file, o.str());
}

void StudyManager::SetNote(const std::string& b, int ch, int v, const std::string& t, const std::string& note, const std::string& text) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + b + ":" + std::to_string(ch) + ":" + std::to_string(v);
    for (auto& d : data) {
        if (d.GetKey() == k) { d.note = note; if (!text.empty()) d.text = text; Save(); return; }
    }
    VerseData d; d.book = b; d.chapter = ch; d.verse = v; d.translation = t; d.note = note; d.text = text; d.addedAt = time(nullptr);
    data.push_back(d); Save();
}

void StudyManager::SetHighlight(const std::string& b, int ch, int v, const std::string& t, int color, const std::string& text) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + b + ":" + std::to_string(ch) + ":" + std::to_string(v);
    for (auto& d : data) {
        if (d.GetKey() == k) { d.highlightColor = color; if (!text.empty()) d.text = text; Save(); return; }
    }
    VerseData d; d.book = b; d.chapter = ch; d.verse = v; d.translation = t; d.highlightColor = color; d.text = text; d.addedAt = time(nullptr);
    data.push_back(d); Save();
}

void StudyManager::SetBookmark(const std::string& b, int ch, int v, const std::string& t, bool bookmarked, const std::string& text) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + b + ":" + std::to_string(ch) + ":" + std::to_string(v);
    for (auto& d : data) {
        if (d.GetKey() == k) { d.isBookmarked = bookmarked; if (!text.empty()) d.text = text; Save(); return; }
    }
    VerseData d; d.book = b; d.chapter = ch; d.verse = v; d.translation = t; d.isBookmarked = bookmarked; d.text = text; d.addedAt = time(nullptr);
    data.push_back(d); Save();
}

VerseData* StudyManager::Get(const std::string& b, int ch, int v, const std::string& t) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + b + ":" + std::to_string(ch) + ":" + std::to_string(v);
    for (auto& d : data) if (d.GetKey() == k) return &d;
    return nullptr;
}

bool StudyManager::HasAny(const std::string& b, int ch, int v, const std::string& t) const {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + b + ":" + std::to_string(ch) + ":" + std::to_string(v);
    for (const auto& d : data) if (d.GetKey() == k) return (d.highlightColor > 0 || d.isBookmarked || !d.note.empty());
    return false;
}

void StudyManager::Remove(const std::string& b, int ch, int v, const std::string& t) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string k = t + ":" + b + ":" + std::to_string(ch) + ":" + std::to_string(v);
    data.erase(std::remove_if(data.begin(), data.end(), [&k](const VerseData& d) { return d.GetKey() == k; }), data.end());
    Save();
}

void StudyManager::ClearAll() {
    std::lock_guard<std::mutex> lock(mtx);
    data.clear();
    Save();
}

std::vector<VerseData> StudyManager::All() const {
    std::lock_guard<std::mutex> lock(mtx);
    return data;
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
    std::istringstream iss(c); std::string k, v;
    while (iss >> k >> v) {
        if (k == "theme") theme = std::stoi(v);
        else if (k == "fontSize") fontSize = std::stof(v);
        else if (k == "lineSpacing") lineSpacing = std::stof(v);
        else if (k == "lastBookIdx") lastBookIdx = std::stoi(v);
        else if (k == "lastChNum") lastChNum = std::stoi(v);
        else if (k == "lastTransIdx") lastTransIdx = std::stoi(v);
        else if (k == "parallelMode") parallelMode = (v == "1");
        else if (k == "transIdx2") transIdx2 = std::stoi(v);
        else if (k == "bookMode") bookMode = (v == "1");
        else if (k == "lastScrollY") lastScrollY = std::stof(v);
        else if (k == "lastPageIdx") lastPageIdx = std::stoi(v);
        else if (k == "winW") winW = std::stoi(v);
        else if (k == "winH") winH = std::stoi(v);
        else if (k == "winX") winX = std::stoi(v);
        else if (k == "winY") winY = std::stoi(v);
    }
}
void SettingsManager::Save() {
    std::ostringstream o;
    o << "theme " << theme << "\nfontSize " << fontSize << "\nlineSpacing " << lineSpacing << "\nlastBookIdx " << lastBookIdx << "\nlastChNum " << lastChNum << "\nlastTransIdx " << lastTransIdx << "\nparallelMode " << (parallelMode ? "1" : "0") << "\ntransIdx2 " << transIdx2 << "\nbookMode " << (bookMode ? "1" : "0") << "\nlastScrollY " << lastScrollY << "\nlastPageIdx " << lastPageIdx << "\nwinW " << winW << "\nwinH " << winH << "\nwinX " << winX << "\nwinY " << winY << "\n";
    WriteFile(file, o.str());
}