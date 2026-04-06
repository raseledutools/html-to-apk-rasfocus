cmake_minimum_required(VERSION 3.10)
project(RasFocus_Pro VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WebView.h অটোমেটিক ডাউনলোড হবে
file(DOWNLOAD https://raw.githubusercontent.com/webview/webview/master/webview.h ${CMAKE_CURRENT_SOURCE_DIR}/webview.h)

# আপনার ফাইলগুলো (json.hpp এখন ফোল্ডারেই আছে)
add_executable(RasFocus_Pro WIN32 main.cpp adblocker.cpp app.rc)

# উইন্ডোজ লাইব্রেরি লিংক করা
target_link_libraries(RasFocus_Pro PRIVATE advapi32 user32 shell32 ole32 uuid)
