#include <nlohmann/json.hpp>
#include "webview.h"
#include "json.hpp"
#include <nlohmann/json.hpp>
#include <windows.h>
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

using namespace std;
using json = nlohmann::json;

// ==========================================
// GLOBALS & DATA
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
string pendingBroadcastMsg = "", currentBroadcastMsg = "", lastSeenBroadcastMsg = "";
string pendingAdminChatStr = "", lastAdminChat = "", globalKeyBuffer = "";

vector<string> blockedApps, blockedWebs, allowedApps, allowedWebs;
vector<string> systemApps = {"explorer.exe", "svchost.exe", "taskmgr.exe", "cmd.exe", "conhost.exe", "csrss.exe", "dwm.exe", "lsass.exe", "services.exe", "smss.exe", "wininit.exe", "winlogon.exe", "spoolsv.exe", "fontdrvhost.exe", "searchui.exe", "searchindexer.exe", "sihost.exe", "taskhostw.exe", "ctfmon.exe", "applicationframehost.exe", "system", "registry", "audiodg.exe", "searchapp.exe", "startmenuexperiencehost.exe", "shellexperiencehost.exe", "textinputhost.exe"};
vector<string> commonThirdPartyApps = {"chrome.exe", "msedge.exe", "firefox.exe", "brave.exe", "opera.exe", "vivaldi.exe", "yandex.exe", "safari.exe", "waterfox.exe", "code.exe", "pycharm64.exe", "python.exe", "idea64.exe", "studio64.exe", "vlc.exe", "telegram.exe", "whatsapp.exe", "discord.exe", "zoom.exe", "skype.exe", "obs64.exe", "steam.exe", "epicgameslauncher.exe", "winword.exe", "excel.exe", "powerpnt.exe", "notepad.exe", "spotify.exe"};
vector<string> explicitKeywords = {"porn", "xxx", "sex", "nude", "nsfw", "xvideos", "pornhub", "xnxx", "xhamster", "brazzers", "onlyfans", "playboy", "mia khalifa", "bhabi", "chudai", "bangla choti", "magi", "sexy"};
vector<string> safeBrowserTitles = {"new tab", "start", "blank page", "allowed websites focus mode", "loading", "untitled", "connecting", "pomodoro break", "premium upgrade"};

// Notun Firebase URL (Alada Dashboard er jonno)
const string FB_COLLECTION = "rasfocus_dashboard_v2";

extern void ToggleAdBlock(bool enable); 

// ==========================================
// UTILITY FUNCTIONS (Ager logic 100% same)
// ==========================================
string GetSecretDir() {
    string dir = "C:\\ProgramData\\SysCache_Ras";
    CreateDirectoryA(dir.c_str(), NULL);
    SetFileAttributesA(dir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    return dir + "\\";
}

string GetSessionFilePath() { return GetSecretDir() + "session.dat"; }

string GetDeviceID() {
    char compName[MAX_COMPUTERNAME_LENGTH + 1]; DWORD size = sizeof(compName); GetComputerNameA(compName, &size);
    DWORD volSerial = 0; GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);
    char id[256]; sprintf(id, "%s-%X", compName, volSerial); return string(id);
}

void SetupAutoStart() { 
    char p[MAX_PATH]; GetModuleFileNameA(NULL, p, MAX_PATH); 
    string pathWithArg = "\"" + string(p) + "\" -autostart";
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "RasFocusPro", 0, REG_SZ, (const BYTE*)pathWithArg.c_str(), pathWithArg.length() + 1);
        RegCloseKey(hKey);
    }
}

void CreateDesktopShortcut() {
    char exePath[MAX_PATH]; GetModuleFileName(NULL, exePath, MAX_PATH);
    char desktopPath[MAX_PATH]; SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath);
    string shortcutPath = string(desktopPath) + "\\RasFocus Pro.lnk";
    ifstream f(shortcutPath.c_str());
    if (!f.good()) {
        CoInitialize(NULL); IShellLink* psl;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl))) {
            psl->SetPath(exePath); psl->SetDescription("Launch RasFocus Pro"); psl->SetIconLocation(exePath, 0); 
            IPersistFile* ppf;
            if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {
                WCHAR wsz[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, shortcutPath.c_str(), -1, wsz, MAX_PATH); ppf->Save(wsz, TRUE); ppf->Release();
            } psl->Release();
        } CoUninitialize();
    }
}

bool CheckMatch(string url, string sTitle) { string t=sTitle; t.erase(remove_if(t.begin(), t.end(), ::isspace), t.end()); string s=url; transform(s.begin(), s.end(), s.begin(), ::tolower); string exact=s; exact.erase(remove_if(exact.begin(), exact.end(), ::isspace), exact.end()); if (!exact.empty() && t.find(exact) != string::npos) return true; replace(s.begin(), s.end(), '.', ' '); replace(s.begin(), s.end(), '/', ' '); replace(s.begin(), s.end(), ':', ' '); replace(s.begin(), s.end(), '-', ' '); stringstream ss(s); string word; while(ss >> word) { if (word=="https"||word=="http"||word=="www"||word=="com"||word=="org"||word=="net"||word=="html"||word=="github") continue; if (word.length()>=3 && t.find(word) != string::npos) return true; } return false; }
string GenerateDisplayURL(string url) { string s=url; string e[]={"https://","http://","www.","/*"}; for(const string& p:e){ size_t pos=s.find(p); if(pos!=string::npos) s.erase(pos,p.length()); } return s; }
string EnsureExe(string n) { if(n.length()<4 || n.substr(n.length()-4)!=".exe") return n+".exe"; return n; }

void SaveSessionData() { 
    ofstream f(GetSessionFilePath()); 
    if(f.is_open()) { f << (isSessionActive?1:0) << endl << (isTimeMode?1:0) << endl << (isPassMode?1:0) << endl << currentSessionPass << endl << focusTimeTotalSeconds << endl << timerTicks << endl << (useAllowMode?1:0) << endl << (isPomodoroMode?1:0) << endl << (isPomodoroBreak?1:0) << endl << pomoTicks << endl << eyeBrightness << endl << eyeWarmth << endl << (blockReels?1:0) << endl << (blockShorts?1:0) << endl << (isAdblockActive?1:0) << endl << pomoCurrentSession << endl << userProfileName << endl; f.close(); } 
}

void LoadSessionData() { 
    ifstream f(GetSessionFilePath()); 
    if(f.is_open()) { 
        int a=0, tm=0, pm=0, ua=0, po=0, pb=0, br=0, bs=0, ad=0, pc=1; 
        f >> a >> tm >> pm >> currentSessionPass >> focusTimeTotalSeconds >> timerTicks >> ua >> po >> pb >> pomoTicks; 
        if(f >> eyeBrightness >> eyeWarmth >> br >> bs >> ad >> pc) { blockReels=(br==1); blockShorts=(bs==1); isAdblockActive=(ad==1); pomoCurrentSession = pc; } 
        f >> ws; getline(f, userProfileName); 
        if(a==1){isSessionActive=true; isTimeMode=(tm==1); isPassMode=(pm==1); useAllowMode=(ua==1); isPomodoroMode=(po==1); isPomodoroBreak=(pb==1); } f.close(); 
    } 
}

void ClearSessionData() { 
    isSessionActive=false; isTimeMode=false; isPassMode=false; isPomodoroMode=false; isPomodoroBreak=false; 
    currentSessionPass=""; focusTimeTotalSeconds=0; timerTicks=0; pomoTicks=0; pomoCurrentSession=1; 
    SaveSessionData();
    if(w_ptr) w_ptr->eval("window.updateStatusFromCpp(false, 'Ready')");
}

void ApplyEyeFilters() {
    int dimAlpha = (100 - eyeBrightness) * 2.55; if (dimAlpha < 0) dimAlpha = 0; if (dimAlpha > 240) dimAlpha = 240; 
    if (dimAlpha > 0) { ShowWindow(hDimFilter, SW_SHOWNOACTIVATE); SetLayeredWindowAttributes(hDimFilter, 0, dimAlpha, LWA_ALPHA); } else { ShowWindow(hDimFilter, SW_HIDE); }
    int warmAlpha = eyeWarmth * 1.5; if (warmAlpha < 0) warmAlpha = 0; if (warmAlpha > 180) warmAlpha = 180;
    if (warmAlpha > 0) { ShowWindow(hWarmFilter, SW_SHOWNOACTIVATE); SetLayeredWindowAttributes(hWarmFilter, 0, warmAlpha, LWA_ALPHA); } else { ShowWindow(hWarmFilter, SW_HIDE); }
}

void CloseActiveTabAndMinimize(HWND hBrowser) { 
    if (GetForegroundWindow() == hBrowser) {
        keybd_event(VK_CONTROL,0,0,0); keybd_event('W',0,0,0); 
        keybd_event('W',0,KEYEVENTF_KEYUP,0); keybd_event(VK_CONTROL,0,KEYEVENTF_KEYUP,0); 
        Sleep(50); 
    }
    ShowWindow(hBrowser, SW_MINIMIZE); 
}

void ShowAllowedWebsitesPage() {
    static DWORD lastTime = 0; if (GetTickCount()-lastTime<3000) return; lastTime=GetTickCount();
    string hPath = GetSecretDir() + "allowed_sites.html"; ofstream html(hPath);
    html<<"<!DOCTYPE html><html><head><style>body{font-family:sans-serif;text-align:center;padding-top:50px;}a{background:#007bff;color:white;padding:10px;margin:5px;display:inline-block;text-decoration:none;border-radius:5px;}</style></head><body><h2>Focus Mode Active!</h2><p>Only these sites are allowed:</p>";
    for (const auto& w:allowedWebs) html<<"<a href='https://"<<GenerateDisplayURL(w)<<"'>"<<w<<"</a>"; html<<"</body></html>"; html.close(); 
    ShellExecute(NULL, "open", hPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void ShowPomodoroBreakPage() {
    string hPath=GetSecretDir()+"pomodoro_break.html"; ofstream html(hPath);
    html<<"<!DOCTYPE html><html><head><style>body{margin:0;height:100vh;display:flex;flex-direction:column;justify-content:center;align-items:center;background:linear-gradient(to bottom, #1e3c72, #2a5298);color:white;font-family:'Segoe UI',sans-serif;}h1{font-size:50px;margin-bottom:10px;}p{font-size:20px;color:#a0c4ff;}</style></head><body><h1>Time to Relax!</h1><p>Break Started.</p></body></html>";
    html.close(); ShellExecute(NULL, "open", hPath.c_str(), NULL, NULL, SW_SHOWMAXIMIZED);
}

// ==========================================
// FIREBASE SYNC FUNCTIONS (Notun Path E)
// ==========================================
void SyncProfileNameToFirebase(string name) {
    string dId = GetDeviceID(); string url = "https://firestore.googleapis.com/v1/projects/mywebtools-f8d53/databases/(default)/documents/" + FB_COLLECTION + "/" + dId + "?updateMask.fieldPaths=profileName&key=AIzaSyDGd3KAo45UuqmeGFALziz_oKm3htEASHY";
    string params = "-WindowStyle Hidden -Command \"$body = @{ fields = @{ profileName = @{ stringValue = '" + name + "' } } } | ConvertTo-Json -Depth 5; Invoke-RestMethod -Uri '" + url + "' -Method Patch -Body $body -ContentType 'application/json'\"";
    SHELLEXECUTEINFOA sei = { sizeof(sei) }; sei.lpVerb = "open"; sei.lpFile = "powershell.exe"; sei.lpParameters = params.c_str(); sei.nShow = SW_HIDE; ShellExecuteExA(&sei);
}

void SyncLiveTrackerToFirebase() {
    string dId = GetDeviceID(); string mode = "None"; string timeL = "00:00"; string activeStr = isSessionActive ? "$true" : "$false";
    if (isSessionActive) {
        if(isPomodoroMode) { mode = "Pomodoro"; int l = (pomoLengthMin*60) - pomoTicks; if(isPomodoroBreak) l = (2*60) - pomoTicks; if(l<0) l=0; char buf[20]; sprintf(buf, "%02d:%02d", l/60, l%60); timeL = buf; }
        else if(isTimeMode) { mode = "Timer"; int l = focusTimeTotalSeconds - timerTicks; if(l<0) l=0; char buf[20]; sprintf(buf, "%02d:%02d", l/3600, (l%3600)/60); timeL = buf; }
        else if(isPassMode) { mode = "Password"; timeL = "Manual Lock"; }
    }
    string url = "https://firestore.googleapis.com/v1/projects/mywebtools-f8d53/databases/(default)/documents/" + FB_COLLECTION + "/" + dId + "?updateMask.fieldPaths=isSelfControlActive&updateMask.fieldPaths=activeModeType&updateMask.fieldPaths=timeRemaining&key=AIzaSyDGd3KAo45UuqmeGFALziz_oKm3htEASHY";
    string params = "-WindowStyle Hidden -Command \"$body = @{ fields = @{ isSelfControlActive = @{ booleanValue = " + activeStr + " }; activeModeType = @{ stringValue = '" + mode + "' }; timeRemaining = @{ stringValue = '" + timeL + "' } } } | ConvertTo-Json -Depth 5; Invoke-RestMethod -Uri '" + url + "' -Method Patch -Body $body -ContentType 'application/json'\"";
    SHELLEXECUTEINFOA sei = { sizeof(sei) }; sei.lpVerb = "open"; sei.lpFile = "powershell.exe"; sei.lpParameters = params.c_str(); sei.nShow = SW_HIDE; ShellExecuteExA(&sei);
}

void RegisterDeviceToFirebase(string dId) {
    string params = "-WindowStyle Hidden -Command \"$url='https://firestore.googleapis.com/v1/projects/mywebtools-f8d53/databases/(default)/documents/" + FB_COLLECTION + "/" + dId + "?key=AIzaSyDGd3KAo45UuqmeGFALziz_oKm3htEASHY'; $body = @{ fields = @{ deviceID = @{ stringValue = '" + dId + "' }; status = @{ stringValue = 'TRIAL' }; package = @{ stringValue = '7 Days Trial' }; adminMessage = @{ stringValue = '' }; adminCmd = @{ stringValue = 'NONE' }; liveChatAdmin = @{ stringValue = '' }; profileName = @{ stringValue = '' } } } | ConvertTo-Json -Depth 5; Invoke-RestMethod -Uri $url -Method Patch -Body $body -ContentType 'application/json'\"";
    SHELLEXECUTEINFOA sei = { sizeof(sei) }; sei.lpVerb = "open"; sei.lpFile = "powershell.exe"; sei.lpParameters = params.c_str(); sei.nShow = SW_HIDE; ShellExecuteExA(&sei);
}

void ValidateLicenseAndTrial() {
    string dId = GetDeviceID(); 
    HINTERNET hInternet = InternetOpenA("RasFocus", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        DWORD timeout = 4000; InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        string url = "https://firestore.googleapis.com/v1/projects/mywebtools-f8d53/databases/(default)/documents/" + FB_COLLECTION + "/" + dId + "?key=AIzaSyDGd3KAo45UuqmeGFALziz_oKm3htEASHY";
        HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hConnect) {
            char buffer[1024]; DWORD bytesRead; string response = "";
            while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) { buffer[bytesRead] = '\0'; response += buffer; }
            InternetCloseHandle(hConnect);

            string fbPackage = "7 Days Trial";
            size_t pkgPos = response.find("\"package\"");
            if (pkgPos != string::npos) { size_t valPos = response.find("\"stringValue\": \"", pkgPos); if (valPos != string::npos) { valPos += 16; size_t endPos = response.find("\"", valPos); if (endPos != string::npos) fbPackage = response.substr(valPos, endPos - valPos); } }

            string trialFile = GetSecretDir() + "sys_lic.dat"; ifstream in(trialFile); time_t activationTime = 0; string savedPackage = "7 Days Trial";
            if (in >> activationTime) { getline(in, savedPackage); if (!savedPackage.empty() && savedPackage[0] == ' ') savedPackage = savedPackage.substr(1); } 
            else { activationTime = time(0); savedPackage = fbPackage; ofstream out(trialFile); out << activationTime << " " << savedPackage; out.close(); RegisterDeviceToFirebase(dId); }

            int totalDays = GetTotalDaysForPackage(savedPackage); double daysPassed = difftime(time(0), activationTime) / 86400.0; trialDaysLeft = totalDays - (int)daysPassed;

            bool explicitlyRevoked = (response.find("\"stringValue\": \"REVOKED\"") != string::npos);
            bool explicitlyApproved = (response.find("\"stringValue\": \"APPROVED\"") != string::npos);

            if (explicitlyRevoked) { isLicenseValid = false; isTrialExpired = true; trialDaysLeft = 0; } 
            else { if (trialDaysLeft <= 0) { isTrialExpired = true; trialDaysLeft = 0; isLicenseValid = false; } else { isTrialExpired = false; isLicenseValid = explicitlyApproved; } }

            size_t msgPos = response.find("\"adminMessage\""); 
            if (msgPos != string::npos) { size_t valPos = response.find("\"stringValue\": \"", msgPos); if (valPos != string::npos) { valPos += 16; size_t endPos = response.find("\"", valPos); safeAdminMsg = response.substr(valPos, endPos - valPos); } }

            size_t cmdPos = response.find("\"adminCmd\"");
            if (cmdPos != string::npos) {
                size_t vPos = response.find("\"stringValue\": \"", cmdPos);
                if (vPos != string::npos) { vPos += 16; size_t ePos = response.find("\"", vPos); string cmd = response.substr(vPos, ePos - vPos);
                    if (cmd == "START_FOCUS" && !isSessionActive) { pendingAdminCmd = 1; } else if (cmd == "STOP_FOCUS" && isSessionActive) { pendingAdminCmd = 2; }
                }
            }

            size_t chatPos = response.find("\"liveChatAdmin\"");
            if (chatPos != string::npos) {
                size_t cvPos = response.find("\"stringValue\": \"", chatPos);
                if (cvPos != string::npos) { cvPos += 16; size_t cePos = response.find("\"", cvPos); string adminChatStr = response.substr(cvPos, cePos - cvPos);
                    if (!adminChatStr.empty() && adminChatStr != lastAdminChat) { lastAdminChat = adminChatStr; pendingAdminChatStr = adminChatStr; }
                }
            }
        } InternetCloseHandle(hInternet);
    } else { isLicenseValid = !isTrialExpired; }
}

// ==========================================
// CORE BLOCKING LOGIC (Fast Loop 200ms)
// ==========================================
void CALLBACK FastLoop(HWND, UINT, UINT_PTR, DWORD) {
    if (!isSessionActive && !blockReels && !blockShorts && !blockAdult) return;
    HWND hActive = GetForegroundWindow();
    if (hActive) {
        char title[512], cls[256];
        if (GetWindowTextA(hActive, title, 512) > 0) {
            GetClassNameA(hActive, cls, 256);
            string sTitle = title; transform(sTitle.begin(), sTitle.end(), sTitle.begin(), ::tolower);
            string sClass = cls; transform(sClass.begin(), sClass.end(), sClass.begin(), ::tolower);

            if(sTitle.find("appdata")!=string::npos || sTitle.find("roaming")!=string::npos) { CloseActiveTabAndMinimize(hActive); return; } 

            bool isSafe=false; for(const auto& s:safeBrowserTitles){ if(sTitle.find(s)!=string::npos){isSafe=true;break;} } if(isSafe) return;

            bool isBrowser = (sTitle.find("chrome") != string::npos || sTitle.find("edge") != string::npos || sTitle.find("firefox") != string::npos || sTitle.find("brave") != string::npos || sClass.find("chrome") != string::npos || sClass.find("mozilla") != string::npos);

            // Reels & Shorts
            if (blockReels && sTitle.find("facebook") != string::npos && (sTitle.find("reels") != string::npos || sTitle.find("watch") != string::npos)) { CloseActiveTabAndMinimize(hActive); return; }
            if (blockShorts && sTitle.find("youtube") != string::npos && sTitle.find("shorts") != string::npos) { CloseActiveTabAndMinimize(hActive); return; }

            // Website Block Check
            if (isSessionActive && isBrowser) {
                if(useAllowMode){
                    bool isAll=false; for(const auto& w:allowedWebs){ if(CheckMatch(w, sTitle)){isAll=true;break;} }
                    if(!isAll){ CloseActiveTabAndMinimize(hActive); ShowAllowedWebsitesPage(); }
                } else {
                    for(const auto& w:blockedWebs){ if(CheckMatch(w, sTitle)){ CloseActiveTabAndMinimize(hActive); break; } }
                }
            }
        }
    }
}

// ==========================================
// PROCESS KILLER & TIMER (Slow Loop 1000ms)
// ==========================================
void CALLBACK SlowLoop(HWND, UINT, UINT_PTR, DWORD) {
    if (isSessionActive) {
        if (isTrialExpired) { ClearSessionData(); if(w_ptr) w_ptr->eval("alert('License Expired!');"); return; }
        
        // Timer Logic
        if(isPomodoroMode){ 
            pomoTicks++; if(pomoTicks%5==0) SaveSessionData(); 
            if(!isPomodoroBreak && pomoTicks>=pomoLengthMin*60){ isPomodoroBreak=true; pomoTicks=0; ShowPomodoroBreakPage(); } 
            else if(isPomodoroBreak && pomoTicks>=2*60){ isPomodoroBreak=false; pomoTicks=0; pomoCurrentSession++; if(pomoCurrentSession > pomoTotalSessions) { ClearSessionData(); if(w_ptr) w_ptr->eval("alert('Pomodoro Complete!');"); } } 
        } 
        else if(isTimeMode && focusTimeTotalSeconds>0){ 
            timerTicks++; if(timerTicks%5==0) SaveSessionData(); 
            if(timerTicks>=focusTimeTotalSeconds){ ClearSessionData(); if(w_ptr) w_ptr->eval("alert('Focus Time Over!');"); } 
        }

        // Process Killing
        HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32 pe={sizeof(pe)}; DWORD myPid=GetCurrentProcessId();
        if(Process32First(h,&pe)){ do{
            if(pe.th32ProcessID==myPid) continue; string n=pe.szExeFile; transform(n.begin(), n.end(), n.begin(), ::tolower);
            
            if(n=="taskmgr.exe" || n=="msiexec.exe" || n=="setup.exe"){ HANDLE p_term=OpenProcess(PROCESS_TERMINATE,FALSE,pe.th32ProcessID); if(p_term){TerminateProcess(p_term,1);CloseHandle(p_term);} continue; }
            
            if(useAllowMode){
                bool isSys=(find(systemApps.begin(), systemApps.end(), n)!=systemApps.end()); bool isAll=false;
                for(const auto& a:allowedApps){ if(n==EnsureExe(a)){isAll=true;break;} }
                bool isCommonBrowser = (n=="chrome.exe"||n=="msedge.exe"||n=="firefox.exe"||n=="brave.exe"||n=="opera.exe"); 
                if(!isSys && !isAll && !isCommonBrowser && n != "rasfocus+adultblocker.exe" && n != "rasfocus_pro.exe"){ 
                    HANDLE p_term=OpenProcess(PROCESS_TERMINATE,FALSE,pe.th32ProcessID); if(p_term){TerminateProcess(p_term,1);CloseHandle(p_term);} 
                }
            } else {
                for(const auto& a:blockedApps){ if(n==EnsureExe(a)){ HANDLE p_term=OpenProcess(PROCESS_TERMINATE,FALSE,pe.th32ProcessID); if(p_term){TerminateProcess(p_term,1);CloseHandle(p_term);} } }
            }
        } while(Process32Next(h,&pe)); } CloseHandle(h);
    }
}

// ==========================================
// FIREBASE SYNC TIMER (4000ms)
// ==========================================
void CALLBACK SyncLoop(HWND, UINT, UINT_PTR, DWORD) {
    ValidateLicenseAndTrial();
    SyncLiveTrackerToFirebase();

    if(w_ptr && !pendingAdminChatStr.empty()) {
        string js = "window.receiveChatFromCpp('" + pendingAdminChatStr + "');";
        w_ptr->eval(js);
        pendingAdminChatStr = "";
    }

    if(pendingAdminCmd == 1 && !isSessionActive) { pendingAdminCmd = 0; currentSessionPass = "12345"; isPassMode = true; isTimeMode = false; isPomodoroMode = false; isSessionActive = true; SaveSessionData(); if(w_ptr) w_ptr->eval("window.updateStatusFromCpp(true, 'Started by Admin')"); }
    else if(pendingAdminCmd == 2 && isSessionActive) { pendingAdminCmd = 0; ClearSessionData(); }
}

// ==========================================
// KEYBOARD HOOK (Adult Blocker)
// ==========================================
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!blockAdult) return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam); 
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam; char c = MapVirtualKey(kbdStruct->vkCode, MAPVK_VK_TO_CHAR);
        if (c >= 32 && c <= 126) {
            globalKeyBuffer += tolower(c); if (globalKeyBuffer.length() > 50) globalKeyBuffer.erase(0, 1);
            for (const auto& kw : explicitKeywords) {
                if (globalKeyBuffer.find(kw) != string::npos) {
                    globalKeyBuffer = ""; HWND hActive = GetForegroundWindow(); if(hActive) SendMessage(hActive, WM_CLOSE, 0, 0); break;
                }
            }
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

// ==========================================
// MAIN ENTRY
// ==========================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "RasFocusPro_Mutex_V55"); (void)hMutex;
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0; 
    
    SetupAutoStart(); CreateDesktopShortcut(); LoadSessionData();
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);

    // Dim & Warm Filters creation
    int vW=GetSystemMetrics(SM_CXVIRTUALSCREEN), vH=GetSystemMetrics(SM_CYVIRTUALSCREEN), vX=GetSystemMetrics(SM_XVIRTUALSCREEN), vY=GetSystemMetrics(SM_YVIRTUALSCREEN);
    WNDCLASS wcD={0}; wcD.lpfnWndProc=DefWindowProc; wcD.hInstance=hInstance; wcD.hbrBackground=CreateSolidBrush(RGB(0,0,0)); wcD.lpszClassName="DimCls"; RegisterClass(&wcD);
    hDimFilter=CreateWindowEx(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT, "DimCls", "", WS_POPUP, vX,vY,vW,vH, NULL, NULL, hInstance, NULL); SetLayeredWindowAttributes(hDimFilter,0,0,LWA_ALPHA);
    
    WNDCLASS wcW={0}; wcW.lpfnWndProc=DefWindowProc; wcW.hInstance=hInstance; wcW.hbrBackground=CreateSolidBrush(RGB(255,130,0)); wcW.lpszClassName="WarmCls"; RegisterClass(&wcW);
    hWarmFilter=CreateWindowEx(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT, "WarmCls", "", WS_POPUP, vX,vY,vW,vH, NULL, NULL, hInstance, NULL); SetLayeredWindowAttributes(hWarmFilter,0,0,LWA_ALPHA);
    
    ApplyEyeFilters();

    // WebView Setup
    webview::webview w(false, nullptr);
    w_ptr = &w;
    w.set_title("RasFocus Pro");
    w.set_size(1050, 700, WEBVIEW_HINT_NONE);

    // Bridge Functions
    w.bind("cppStartFocus", [&](string pass, string hr, string min) -> string {
        currentPass = pass; 
        focusTimeTotalSeconds = (atoi(hr.c_str()) * 3600) + (atoi(min.c_str()) * 60);
        timerTicks = 0;
        isSessionActive = true; 
        if(currentPass.length() > 0) isPassMode = true; else if(focusTimeTotalSeconds > 0) isTimeMode = true;
        SaveSessionData();
        w.eval("window.updateStatusFromCpp(true, 'Active')");
        return "OK";
    });

    w.bind("cppStopFocus", [&](string pass) -> string {
        if (isTimeMode) return "Timer is Active. You must wait!";
        if (pass == currentPass) { ClearSessionData(); return "OK"; }
        return "Wrong Password";
    });

    w.bind("cppSyncSettings", [&](string jsonStr) -> string {
        try {
            auto j = json::parse(jsonStr);
            useAllowMode = j["allowMode"]; blockReels = j["reels"]; blockShorts = j["shorts"];
            bool ab = j["adblock"]; if(ab != isAdblockActive) { isAdblockActive = ab; ToggleAdBlock(isAdblockActive); }
            SaveSessionData();
        } catch(...) {}
        return "OK";
    });

    w.bind("cppSyncEye", [&](string b, string w_val) -> string {
        eyeBrightness = atoi(b.c_str()); eyeWarmth = atoi(w_val.c_str());
        ApplyEyeFilters(); SaveSessionData(); return "OK";
    });

    w.bind("cppSendChat", [&](string msg) -> string {
        string dId = GetDeviceID(); string url = "https://firestore.googleapis.com/v1/projects/mywebtools-f8d53/databases/(default)/documents/" + FB_COLLECTION + "/" + dId + "?updateMask.fieldPaths=liveChatUser&key=AIzaSyDGd3KAo45UuqmeGFALziz_oKm3htEASHY";
        string cmd = "-WindowStyle Hidden -Command \"$body = @{ fields = @{ liveChatUser = @{ stringValue = '" + msg + "' } } } | ConvertTo-Json -Depth 5; Invoke-RestMethod -Uri '" + url + "' -Method Patch -Body $body -ContentType 'application/json'\"";
        SHELLEXECUTEINFOA sei = { sizeof(sei) }; sei.lpVerb = "open"; sei.lpFile = "powershell.exe"; sei.lpParameters = cmd.c_str(); sei.nShow = SW_HIDE; ShellExecuteExA(&sei);
        return "OK";
    });

    w.bind("cppSaveProfile", [&](string name) -> string {
        userProfileName = name; SaveSessionData(); SyncProfileNameToFirebase(name); return "OK";
    });

    // Start Win32 Timers
    SetTimer(NULL, 1, 200, FastLoop);
    SetTimer(NULL, 2, 1000, SlowLoop);
    SetTimer(NULL, 3, 4000, SyncLoop);

    // Load HTML
    char cpath[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, cpath);
    string htmlPath = "file:///" + string(cpath) + "\\index.html";
    w.navigate(htmlPath);

    w.run();

    UnhookWindowsHookEx(hKeyboardHook);
    return 0;
}
