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

std::string JStr(const std::string& j, const std::string& k) {
    std::string sk = "\"" + k + "\":\"";
    size_t p = j.find(sk);
    if (p == std::string::npos) return "";
    p += sk.size();
    size_t e = j.find("\"", p);
    if (e == std::string::npos) return "";
    std::string r = j.substr(p, e - p);
    size_t pos = 0;
    while ((pos = r.find("\\n", pos)) != std::string::npos) { r.replace(pos, 2, " "); pos += 1; }
    pos = 0;
    while ((pos = r.find("\\\"", pos)) != std::string::npos) { r.replace(pos, 2, "\""); pos += 1; }
    return r;
}
int JInt(const std::string& j, const std::string& k) {
    std::string sk = "\"" + k + "\":";
    size_t p = j.find(sk);
    if (p == std::string::npos) return 0;
    p += sk.size();
    size_t e = j.find_first_of(",}", p);
    if (e == std::string::npos) return 0;
    try { return std::stoi(j.substr(p, e - p)); } catch (...) { return 0; }
}
long JLong(const std::string& j, const std::string& k) {
    std::string sk = "\"" + k + "\":";
    size_t p = j.find(sk);
    if (p == std::string::npos) return 0;
    p += sk.size();
    size_t e = j.find_first_of(",}", p);
    if (e == std::string::npos) return 0;
    try { return std::stol(j.substr(p, e - p)); } catch (...) { return 0; }
}
std::vector<std::string> JArr(const std::string& j, const std::string& k) {
    std::vector<std::string> r;
    std::string sk = "\"" + k + "\":[";
    size_t p = j.find(sk);
    if (p == std::string::npos) return r;
    p += sk.size();
    int depth = 1;
    size_t e = p;
    while (e < j.size() && depth > 0) {
        if (j[e] == '[') depth++;
        else if (j[e] == ']') depth--;
        if (depth > 0) e++;
    }
    if (e >= j.size()) return r;
    std::string a = j.substr(p, e - p);
    for (size_t s = 0; (s = a.find("{", s)) != std::string::npos;) {
        size_t en = a.find("}", s);
        if (en == std::string::npos) break;
        r.push_back(a.substr(s, en - s + 1));
        s = en + 1;
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