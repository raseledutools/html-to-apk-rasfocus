cmake_minimum_required(VERSION 3.10)
project(RasFocus_Pro VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JSON লাইব্রেরি অটোমেটিক ডাউনলোড এবং অ্যাড করার কোড
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

# WebView লাইব্রেরি অটোমেটিক ডাউনলোড করার কোড
file(DOWNLOAD https://raw.githubusercontent.com/webview/webview/master/webview.h ${CMAKE_CURRENT_SOURCE_DIR}/webview.h)

# আপনার মেইন প্রজেক্ট ফাইলগুলো (WIN32 কমান্ডটি পেছনের কালো কনসোল হাইড করবে)
add_executable(RasFocus_Pro WIN32 main.cpp adblocker.cpp app.rc)

# উইন্ডোজের প্রয়োজনীয় লাইব্রেরি লিংক করা
target_link_libraries(RasFocus_Pro PRIVATE nlohmann_json::nlohmann_json advapi32 user32)
