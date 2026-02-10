#include "raybible.h"

const std::vector<BookInfo> BIBLE_BOOKS = {
    {"Genesis","gen",50},      {"Exodus","exo",40},       {"Leviticus","lev",27},
    {"Numbers","num",36},      {"Deuteronomy","deu",34},  {"Joshua","jos",24},
    {"Judges","jdg",21},       {"Ruth","rut",4},          {"1 Samuel","1sa",31},
    {"2 Samuel","2sa",24},     {"1 Kings","1ki",22},      {"2 Kings","2ki",25},
    {"1 Chronicles","1ch",29}, {"2 Chronicles","2ch",36}, {"Ezra","ezr",10},
    {"Nehemiah","neh",13},     {"Esther","est",10},       {"Job","job",42},
    {"Psalms","psa",150},      {"Proverbs","pro",31},     {"Ecclesiastes","ecc",12},
    {"Song of Solomon","sng",8},{"Isaiah","isa",66},      {"Jeremiah","jer",52},
    {"Lamentations","lam",5},  {"Ezekiel","ezk",48},      {"Daniel","dan",12},
    {"Hosea","hos",14},        {"Joel","jol",3},          {"Amos","amo",9},
    {"Obadiah","oba",1},       {"Jonah","jon",4},         {"Micah","mic",7},
    {"Nahum","nam",3},         {"Habakkuk","hab",3},      {"Zephaniah","zep",3},
    {"Haggai","hag",2},        {"Zechariah","zec",14},    {"Malachi","mal",4},
    {"Matthew","mat",28},      {"Mark","mrk",16},         {"Luke","luk",24},
    {"John","jhn",21},         {"Acts","act",28},         {"Romans","rom",16},
    {"1 Corinthians","1co",16},{"2 Corinthians","2co",13},{"Galatians","gal",6},
    {"Ephesians","eph",6},     {"Philippians","php",4},   {"Colossians","col",4},
    {"1 Thessalonians","1th",5},{"2 Thessalonians","2th",3},{"1 Timothy","1ti",6},
    {"2 Timothy","2ti",4},     {"Titus","tit",3},         {"Philemon","phm",1},
    {"Hebrews","heb",13},      {"James","jas",5},         {"1 Peter","1pe",5},
    {"2 Peter","2pe",3},       {"1 John","1jn",5},        {"2 John","2jn",1},
    {"3 John","3jn",1},        {"Jude","jud",1},          {"Revelation","rev",22}
};

const std::vector<Translation> TRANSLATIONS = {
    {"web","World English Bible"},
    {"kjv","King James Version"},
    {"bsb","Berean Standard Bible"},
    {"bbe","Bible in Basic English"}
};
