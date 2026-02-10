cmake_minimum_required(VERSION 3.15)
project(RayBible VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Raylib dependency
find_package(raylib 5.0 QUIET)

if (NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

# Executable
add_executable(raybible 
    main.cpp 
    bible_data.cpp
    utils.cpp 
    managers.cpp 
    bible_logic.cpp 
    app_state.cpp 
    ui_renderer.cpp
)

# Link libraries
target_link_libraries(raybible PRIVATE raylib)

# Platform-specific HTTP libraries
if (WIN32)
    # Windows uses WinINet (already linked via pragma in code)
    target_link_libraries(raybible PRIVATE wininet)
else()
    # Linux/macOS uses libcurl
    find_package(CURL REQUIRED)
    target_link_libraries(raybible PRIVATE CURL::libcurl)
endif()

# Platform-specific settings
if (APPLE)
    target_link_libraries(raybible PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Install target
install(TARGETS raybible DESTINATION bin)