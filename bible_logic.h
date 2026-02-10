#pragma once
#ifndef BIBLE_LOGIC_H
#define BIBLE_LOGIC_H

#include "raybible.h"
#include <string>
#include <vector>

Chapter FetchFromAPI(int bookIdx, int chNum, const std::string& trans);
Chapter LoadOrFetch(int bookIdx, int chNum, const std::string& trans);
bool NextChapter(int& bookIdx, int& chNum);
bool PrevChapter(int& bookIdx, int& chNum);
bool ParseReference(std::string input, int& bookIdx, int& chNum, int& vNum);
std::vector<std::pair<int, int>> GetDailyReading(int dayOfYear);
std::vector<SearchMatch> SearchVerses(const std::vector<Verse>& verses, const std::string& q, bool cs);

#endif // BIBLE_LOGIC_H
