# Bible Reader – Design Document  
**Project Name:** RayBible (working title)  
**Platform:** Desktop (Windows, macOS, Linux) — later possible HTML5 / Raspberry Pi  
**Technology:** C++17/20 + raylib 5.x + raygui + nlohmann/json + cpr (HTTP)  
**Target audience:** Personal use, devotional reading, simple study — users who want a lightweight, offline-capable, distraction-free Bible reader  
**License intention:** MIT / open-source (your choice)

## 1. Vision & Goals

**One-liner:**  
A clean, fast, beautiful, and distraction-free Bible reader that loads passages from modern Bible APIs, with good readability and basic navigation — built from scratch using raylib.

**Core philosophy:**

- Minimal & fast startup (< 1 second)
- Keyboard + mouse friendly
- High readability (good fonts, spacing, night mode)
- Offline caching eventually
- No telemetry, no ads, no unnecessary features

**Non-goals (v1):**

- Advanced study tools (concordance, original languages, commentaries)
- Social features / sharing
- Mobile / touch-first design
- Dozens of simultaneous translations open

## 2. Key Features (MVP – Minimum Viable Product)

| Priority | Feature                              | Description                                                                 | Notes / Dependencies                     |
|----------|--------------------------------------|-----------------------------------------------------------------------------|------------------------------------------|
| P0       | Fetch & display single passage       | Load any verse/chapter/range via API (e.g. john 3:16, psalm 23, matthew 5-7) | Free Use Bible API (bible.helloao.org)   |
| P0       | Book / Chapter / Verse navigation    | Dropdowns or buttons to pick book → chapter → optional verse               | Needs list of 66 books + chapter counts  |
| P1       | Translation selector                 | Choose from popular codes (web, kjv, bsb, esv, niv, …)                     | API usually has /translations endpoint   |
| P1       | Scrollable reading area              | Smooth vertical scrolling, mouse wheel + drag support                       | raygui ScrollPanel or custom             |
| P1       | Night / Light mode                   | Toggleable theme (dark background + light text vs reverse)                 | Simple color swap                        |
| P2       | Verse numbers & formatting           | Verse numbers in superscript/small, basic paragraph spacing                 | Use DrawTextEx + measured positioning    |
| P2       | Copy verse(s) to clipboard           | Right-click or hotkey copies selected text                                  | raylib GetClipboardText / SetClipboardText |
| P2       | Basic search (within chapter)        | Find words/phrases in current chapter                                       | Simple string search                     |
| P3       | Offline cache                        | Save fetched chapters locally (json or folder structure)                    | File I/O + directory per translation     |
| P3       | Recent passages / favorites          | Quick access to last 10–20 passages + star favorite verses                  | Simple vector + json save                |
| Stretch  | Side-by-side comparison              | Show two translations at once                                               | Two scroll areas                         |
| Stretch  | Daily reading plan support           | Load simple plan (e.g. chronological, canonical)                            | JSON plan file or API                    |

## 3. User Interface Layout (v1)

**Window size:** 1100 × 720 (resizable)

**Main screen layout (vertical stack):**

```
┌───────────────────────────────────────────────────────────────┐
│ Header Bar (50–60 px)                                         │
│   [Logo / "RayBible"]   Translation ▼   Night/Light ☀/☾   [?] │
└───────────────────────────────────────────────────────────────┘

┌────────────────────── Navigation Bar (50–60 px) ──────────────┐
│ Book ▼     Chapter ▼     [Go to verse: ___]     [Fetch]       │
└───────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────┐
│ Main Reading Area (fills remaining space)                     │
│                                                               │
│   John 3:16 (WEB)                                             │
│                                                               │
│    16  For God so loved the world that he gave his one ...    │
│                                                               │
│   [Scrollable content]                                        │
│                                                               │
└───────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────┐
│ Status / Footer (30 px)                                       │
│   Last fetched: 2s ago   •   182 verses cached               │
└───────────────────────────────────────────────────────────────┘
```

**Hotkeys (planned):**

- Ctrl + D / Ctrl + N → toggle dark/light mode
- Ctrl + F → focus search box
- Ctrl + R → reload current passage
- Esc → clear selection / go back to menu
- Arrow keys / mouse wheel → scroll

## 4. Technical Architecture

**Layers:**

```
┌───────────────────────────────┐
│          UI / Presentation    │   ← raylib drawing + raygui controls
│   (screens: main reader, menu)│
└───────────────────────────────┘
            │
            ▼
┌───────────────────────────────┐
│         Application Logic     │   ← state, navigation, theme, cache manager
│     (BibleApp class / state)  │
└───────────────────────────────┘
            │
            ▼
┌───────────────────────────────┐
│         Data Layer            │   ← API client + local cache (json files)
│     (BibleAPI, CacheManager)  │
└───────────────────────────────┘
            │
            ▼
┌───────────────────────────────┐
│     External Dependencies     │
│ raylib • raygui • nlohmann/json • cpr • std::filesystem │
└───────────────────────────────┘
```

**Important data structures:**

```cpp
struct Verse {
    int number;
    std::string text;
    // future: footnotes, cross-refs
};

struct Chapter {
    std::string reference;
    std::string translation;
    std::vector<Verse> verses;
    std::time_t fetchedAt;
};

struct AppState {
    std::string currentTranslation = "web";
    std::string currentReference  = "john 3";   // normalized
    std::optional<Chapter> currentChapter;
    bool darkMode = true;
    float fontSize = 21.0f;
    // scroll offset, favorites, cache index...
};
```

## 5. Development Phases (Roadmap)

**Phase 0 – Setup & Proof of Concept** (1–2 sessions)

- Project structure (CMake + main.cpp)
- raylib window + basic raygui controls
- Fetch & display one hard-coded passage
- Dark/light toggle

**Phase 1 – Core Reading Experience**

- Translation & reference input fields
- Book + chapter dropdowns
- Scrollable verse display with proper line spacing
- Clean formatting (verse numbers, paragraphs)

**Phase 2 – Polish & Usability**

- Font loading (TTF – better than default sprite font)
- Mouse-wheel + drag scrolling
- Copy to clipboard
- Status messages / loading indicator
- Error handling (bad reference, offline, rate limit)

**Phase 3 – Power Features**

- Local caching (save chapters)
- Favorites / recent list
- In-chapter search
- Basic history navigation

**Phase 4 – Nice-to-haves (if we want to go further)**

- Two-column comparison
- Simple reading plan loader
- Export passage as image/text
- Command palette / quick jump

## 6. API Choice & Fallbacks

**Primary:** https://bible.helloao.org/ (Free Use Bible API)

- No key
- Many translations
- Clean JSON
- Supports ranges

**Fallbacks (in order):**

1. https://bible-api.com/
2. https://bolls.life/api (if we need more formats)
3. Local files (future offline mode)

## 7. Next Step Proposal

We should start with **Phase 0** — get a minimal window running that fetches and displays **John 3:16** using raylib + raygui + the API.

Would you like to:

A. Start writing the CMakeLists.txt + basic project structure  
B. Jump straight to the main.cpp skeleton with window, header, and API fetch  
C. Refine this design document first (add/remove features, change layout, etc.)
