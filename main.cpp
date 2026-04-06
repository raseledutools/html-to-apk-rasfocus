#define WEBVIEW_WINAPI
#include <windows.h>
#include <nlohmann/json.hpp>
#include "webview.h"
#include <tlhelp32.h>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <shellapi.h>
#include <time.h>
#include <dwmapi.h>
#include <wininet.h>
#include <shlobj.h>
#include <iostream>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

using namespace std;
using json = nlohmann::json;

// ==========================================
// GLOBALS
// ==========================================
webview::webview* w_ptr = nullptr;
HWND hDimFilter, hWarmFilter;
HHOOK hKeyboardHook;

bool isSessionActive = false, isTimeMode = false, isPassMode = false, useAllowMode = false;
bool blockReels = false, blockShorts = false, isAdblockActive = false, blockAdult = true; 
bool isPomodoroMode = false, isPomodoroBreak = false;

int eyeBrightness = 100, eyeWarmth = 0, focusTimeTotalSeconds = 0, timerTicks = 0;
int pomoLengthMin = 25, pomoTotalSessions = 4, pomoCurrentSession = 1, pomoTicks = 0;
int trialDaysLeft = 7, pendingAdminCmd = 0;
bool isLicenseValid = false, isTrialExpired = false;

string currentSessionPass = "", userProfileName = "Rasel Mia", safeAdminMsg = "";
string pendingAdminChatStr = "", lastAdminChat = "", globalKeyBuffer = "";
const string FB_COLLECTION = "rasfocus_dashboard_v2";

vector<string> blockedApps, allowedApps;
vector<string> allowedWebs = {"github.com", "duet.ac.bd", "google.com"};
vector<string> blockedWebs = {"facebook.com", "youtube.com"};
vector<string> explicitKeywords = {"porn", "xxx", "sex", "nude", "nsfw"};
vector<string> safeBrowserTitles = {"new tab", "start", "blank page", "focus mode"};

extern void ToggleAdBlock(bool enable); 

// ==========================================
// FUNCTIONS
// ==========================================
int GetTotalDaysForPackage(string pkg) {
    if (pkg.find("1 Year") != string::npos) return 365;
    if (pkg.find("6 Months") != string::npos) return 180;
    return 7; 
}

string GetSecretDir() {
    string dir = "C:\\ProgramData\\SysCache_Ras";
    CreateDirectoryA(dir.c_str(), NULL);
    return dir + "\\";
}

string GetSessionFilePath() { return GetSecretDir() + "session.dat"; }

string GetDeviceID() {
    char compName[MAX_COMPUTERNAME_LENGTH + 1]; DWORD size = sizeof(compName); GetComputerNameA(compName, &size);
    DWORD volSerial = 0; GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);
    char id[256]; sprintf_s(id, "%s-%X", compName, volSerial); return string(id);
}

void SaveSessionData() { 
    ofstream f(GetSessionFilePath()); 
    if(f.is_open()) { f << (isSessionActive?1:0) << "\n" << currentSessionPass << "\n" << eyeBrightness << "\n" << eyeWarmth << endl; f.close(); } 
}

void ClearSessionData() { 
    isSessionActive = false; currentSessionPass = ""; 
    SaveSessionData();
    if(w_ptr) w_ptr->eval("window.updateStatusFromCpp(false, 'Ready')");
}

void CloseActiveTabAndMinimize(HWND hBrowser) { 
    keybd_event(VK_CONTROL,0,0,0); keybd_event('W',0,0,0); 
    keybd_event('W',0,KEYEVENTF_KEYUP,0); keybd_event(VK_CONTROL,0,KEYEVENTF_KEYUP,0); 
    ShowWindow(hBrowser, SW_MINIMIZE); 
}

// --- Blocking Logic ---
void CALLBACK FastLoop(HWND, UINT, UINT_PTR, DWORD) {
    if (!isSessionActive) return;
    HWND hActive = GetForegroundWindow();
    if (hActive) {
        char title[512];
        if (GetWindowTextA(hActive, title, 512) > 0) {
            string sTitle = title; transform(sTitle.begin(), sTitle.end(), sTitle.begin(), ::tolower);
            if (sTitle.find("facebook") != string::npos && blockReels) CloseActiveTabAndMinimize(hActive);
        }
    }
}

// --- Keyboard Hook ---
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

// ==========================================
// MAIN
// ==========================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInst, 0);
    SetTimer(NULL, 1, 200, FastLoop);

    webview::webview w(false, nullptr);
    w_ptr = &w;
    w.set_title("RasFocus Pro");
    w.set_size(1050, 700, WEBVIEW_HINT_NONE);

    w.bind("cppStartFocus", [&](string pass, string hr, string min) -> string {
        currentSessionPass = pass; isSessionActive = true;
        SaveSessionData();
        w.eval("window.updateStatusFromCpp(true, 'Active')");
        return "OK";
    });

    w.bind("cppStopFocus", [&](string pass) -> string {
        if (pass == currentSessionPass) { ClearSessionData(); return "OK"; }
        return "Wrong Password";
    });

    w.bind("cppSyncSettings", [&](string jsonStr) -> string {
        try {
            auto j = json::parse(jsonStr);
            blockReels = j["reels"];
            isAdblockActive = j["adblock"];
            ToggleAdBlock(isAdblockActive);
        } catch(...) {}
        return "OK";
    });

    char cpath[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, cpath);
    string htmlPath = "file:///" + string(cpath) + "\\index.html";
    w.navigate(htmlPath);
    w.run();

    return 0;
}
