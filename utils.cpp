#include "utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <cstring>
#include "raylib.h"

#ifdef _WIN32
    #include <windows.h>
    #include <wininet.h>
    #include <direct.h>
    #define mkdir(p,m) _mkdir(p)
#else
    #include <curl/curl.h>
    #include <dirent.h>
    #include <unistd.h>
#endif

bool DirExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFDIR);
}
bool MakeDir(const std::string& p) {
    if (DirExists(p)) return true;
    return mkdir(p.c_str(), 0755) == 0;
}
bool FileExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFREG);
}
long GetFileSize(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? (long)st.st_size : 0L;
}
std::string ReadFile(const std::string& p) {
    std::ifstream f(p);
    if (!f.is_open()) return "";
    std::stringstream b;
    b << f.rdbuf();
    return b.str();
}
bool WriteFile(const std::string& p, const std::string& c) {
    std::ofstream f(p);
    if (!f.is_open()) return false;
    f << c;
    return true;
}

void CopyToClipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (h) {
        memcpy(GlobalLock(h), text.c_str(), text.size() + 1);
        GlobalUnlock(h);
        SetClipboardData(CF_TEXT, h);
    }
    CloseClipboard();
#else
    SetClipboardText(text.c_str());
#endif
}

std::string ToLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

std::string StripTags(const std::string& s) {
    if (s.empty()) return "";
    std::string r = s;
    
    // 1. Decode HTML entities
    size_t pos = 0;
    auto replaceAll = [&](const std::string& from, const std::string& to) {
        size_t p = 0;
        while ((p = r.find(from, p)) != std::string::npos) {
            r.replace(p, from.size(), to);
            p += to.size();
        }
    };
    replaceAll("&lt;", "<"); replaceAll("&gt;", ">"); replaceAll("&amp;", "&"); replaceAll("&quot;", "\"");

    // 2. Remove Strong's tags and their numeric content: <S>3068</S> or <S 3068>
    // We target anything starting with <S or <s followed by digits or >
    std::string finalStr;
    bool skippingStrong = false;
    bool inTag = false;
    std::string currentTag;

    for (size_t i = 0; i < r.size(); ++i) {
        char c = r[i];
        if (c == '<') {
            inTag = true;
            currentTag = "";
        } else if (c == '>') {
            inTag = false;
            // Check if we just closed a Strong's tag
            if (currentTag == "S" || currentTag == "s" || (currentTag.size() > 1 && (currentTag[0] == 'S' || currentTag[0] == 's'))) {
                // If it's an opening Strong's tag, start skipping until the closing tag
                skippingStrong = true;
            } else if (currentTag == "/S" || currentTag == "/s") {
                skippingStrong = false;
            }
        } else if (inTag) {
            currentTag += c;
        } else if (!skippingStrong) {
            finalStr += c;
        }
    }
    
    // 3. One more pass to catch any missed <...> patterns or isolated digits that look like Strong's
    // But usually the state machine above is enough if the tags are balanced.
    // Let's do a simple clean pass for any remaining tags.
    std::string cleaned;
    bool in = false;
    for (char c : finalStr) {
        if (c == '<') in = true;
        else if (c == '>') in = false;
        else if (!in) cleaned += c;
    }

    // Clean up double spaces
    pos = 0;
    while ((pos = cleaned.find("  ", pos)) != std::string::npos) { cleaned.replace(pos, 2, " "); }
    if (!cleaned.empty() && cleaned[0] == ' ') cleaned.erase(0, 1);
    
    return cleaned;
}

static size_t SkipWS(const std::string& s, size_t p) {
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r')) p++;
    return p;
}

static size_t FindKey(const std::string& j, const std::string& k) {
    std::string sk = "\"" + k + "\"";
    size_t p = 0;
    while ((p = j.find(sk, p)) != std::string::npos) {
        size_t next = SkipWS(j, p + sk.size());
        if (next < j.size() && j[next] == ':') return next + 1;
        p += sk.size();
    }
    return std::string::npos;
}

std::string JStr(const std::string& j, const std::string& k) {
    size_t p = FindKey(j, k);
    if (p == std::string::npos) return "";
    p = SkipWS(j, p);
    if (p >= j.size() || j[p] != '\"') return "";
    p++;
    size_t e = p;
    while (e < j.size()) {
        if (j[e] == '\"' && j[e-1] != '\\') break;
        e++;
    }
    if (e >= j.size()) return "";
    std::string r = j.substr(p, e - p);
    size_t pos = 0;
    while ((pos = r.find("\\n", pos)) != std::string::npos) { r.replace(pos, 2, " "); pos += 1; }
    pos = 0;
    while ((pos = r.find("\\\"", pos)) != std::string::npos) { r.replace(pos, 2, "\""); pos += 1; }
    
    // Decode \uXXXX
    pos = 0;
    while ((pos = r.find("\\u", pos)) != std::string::npos) {
        if (pos + 5 < r.size()) {
            try {
                int code = std::stoi(r.substr(pos + 2, 4), nullptr, 16);
                std::string utf8;
                if (code <= 0x7F) utf8 += (char)code;
                else if (code <= 0x7FF) { utf8 += (char)(0xC0 | (code >> 6)); utf8 += (char)(0x80 | (code & 0x3F)); }
                else { utf8 += (char)(0xE0 | (code >> 12)); utf8 += (char)(0x80 | ((code >> 6) & 0x3F)); utf8 += (char)(0x80 | (code & 0x3F)); }
                r.replace(pos, 6, utf8);
                pos += utf8.size();
            } catch (...) { pos += 2; }
        } else pos += 2;
    }
    return r;
}

int JInt(const std::string& j, const std::string& k) {
    size_t p = FindKey(j, k);
    if (p == std::string::npos) return 0;
    p = SkipWS(j, p);
    size_t e = j.find_first_of(",}] \t\n\r", p);
    if (e == std::string::npos) return 0;
    try { return std::stoi(j.substr(p, e - p)); } catch (...) { return 0; }
}

long JLong(const std::string& j, const std::string& k) {
    size_t p = FindKey(j, k);
    if (p == std::string::npos) return 0;
    p = SkipWS(j, p);
    size_t e = j.find_first_of(",}] \t\n\r", p);
    if (e == std::string::npos) return 0;
    try { return std::stol(j.substr(p, e - p)); } catch (...) { return 0; }
}

std::vector<std::string> JArr(const std::string& j, const std::string& k) {
    std::vector<std::string> r;
    size_t p = 0;
    if (!k.empty()) {
        p = FindKey(j, k);
        if (p == std::string::npos) return r;
        p = SkipWS(j, p);
    } else {
        p = SkipWS(j, 0);
    }
    
    if (p >= j.size() || j[p] != '[') return r;
    p++;
    
    int depth = 1;
    size_t e = p;
    bool inStr = false;
    while (e < j.size() && depth > 0) {
        if (j[e] == '\"' && (e == 0 || j[e-1] != '\\')) inStr = !inStr;
        if (!inStr) {
            if (j[e] == '[') depth++;
            else if (j[e] == ']') {
                depth--;
                if (depth == 0) break;
            }
        }
        e++;
    }
    if (e >= j.size()) return r;
    std::string a = j.substr(p, e - p);

    size_t s = 0;
    while (s < a.size()) {
        size_t start = a.find('{', s);
        if (start == std::string::npos) break;
        
        int objDepth = 0;
        size_t cur = start;
        bool objInStr = false;
        while (cur < a.size()) {
            if (a[cur] == '\"' && (cur == 0 || a[cur-1] != '\\')) objInStr = !objInStr;
            if (!objInStr) {
                if (a[cur] == '{') objDepth++;
                else if (a[cur] == '}') {
                    objDepth--;
                    if (objDepth == 0) break;
                }
            }
            cur++;
        }
        
        if (cur < a.size()) {
            r.push_back(a.substr(start, cur - start + 1));
            s = cur + 1;
        } else break;
    }
    return r;
}

#ifdef _WIN32
std::string HttpGet(const std::string& url) {
    std::string result, host, path;
    size_t pe = url.find("://");
    if (pe != std::string::npos) {
        size_t hs = pe + 3, ps = url.find("/", hs);
        if (ps != std::string::npos) { host = url.substr(hs, ps - hs); path = url.substr(ps); }
        else { host = url.substr(hs); path = "/"; }
    }
    HINTERNET hi = InternetOpenA("RayBible/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hi) return "";
    HINTERNET hc = InternetConnectA(hi, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hc) { InternetCloseHandle(hi); return ""; }
    HINTERNET hr = HttpOpenRequestA(hc, "GET", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE, 0);
    if (!hr) { InternetCloseHandle(hc); InternetCloseHandle(hi); return ""; }
    if (HttpSendRequestA(hr, NULL, 0, NULL, 0)) {
        char buf[4096]; DWORD br;
        while (InternetReadFile(hr, buf, sizeof(buf) - 1, &br) && br > 0) { buf[br] = 0; result += buf; }
    }
    InternetCloseHandle(hr); InternetCloseHandle(hc); InternetCloseHandle(hi);
    return result;
}
#else
static size_t CurlWrite(void* c, size_t s, size_t n, std::string* o) {
    o->append((char*)c, s * n); return s * n;
}
std::string HttpGet(const std::string& url) {
    std::string result;
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &result);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK ? result : "";
}
#endif