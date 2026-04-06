cmake_minimum_required(VERSION 3.10)
project(RasFocus_Pro VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JSON লাইব্রেরি ডাউনলোড এবং সেটআপ
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# WebView.h ডাউনলোড
file(DOWNLOAD https://raw.githubusercontent.com/webview/webview/master/webview.h ${CMAKE_CURRENT_SOURCE_DIR}/webview.h)

# আপনার ফাইলগুলো
add_executable(RasFocus_Pro WIN32 main.cpp adblocker.cpp app.rc)

# গুরুত্বপূর্ণ: এখানে লাইব্রেরি লিংক করা হয়েছে যাতে কম্পাইলার ফাইল খুঁজে পায়
target_link_libraries(RasFocus_Pro PRIVATE nlohmann_json::nlohmann_json advapi32 user32 shell32 ole32 uuid)
