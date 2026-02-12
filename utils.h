#pragma once
#ifndef RAYBIBLE_UTILS_H
#define RAYBIBLE_UTILS_H

#include <string>
#include <vector>

// File System
bool DirExists(const std::string& p);
bool MakeDir(const std::string& p);
bool FileExists(const std::string& p);
long GetFileSize(const std::string& p);
std::string ReadFile(const std::string& p);
bool WriteFile(const std::string& p, const std::string& c);

// Clipboard
void CopyToClipboard(const std::string& text);

// String helpers
std::string ToLower(const std::string& s);
std::string StripTags(const std::string& s);
std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);

// JSON Parser (simple)
std::string JStr(const std::string& j, const std::string& k);
int JInt(const std::string& j, const std::string& k);
long JLong(const std::string& j, const std::string& k);
std::vector<std::string> JArr(const std::string& j, const std::string& k);

// HTTP Client
std::string HttpGet(const std::string& url);

#endif // UTILS_H
