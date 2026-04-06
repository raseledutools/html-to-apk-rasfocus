#include <windows.h>
#include <string>

using namespace std;

void ToggleAdBlock(bool enable) {
    HKEY hKey;
    
    // Chrome Registry Path for Force Install
    string chromePath = "SOFTWARE\\Policies\\Google\\Chrome\\ExtensionInstallForcelist";
    // AdGuard Chrome Extension ID & Update URL
    string adGuardChrome = "bgnkhhnnamicmpeenaelnjfhikgbkllg;https://clients2.google.com/service/update2/crx";
    
    // Edge Registry Path for Force Install
    string edgePath = "SOFTWARE\\Policies\\Microsoft\\Edge\\ExtensionInstallForcelist";
    // AdGuard Edge Extension ID & Update URL
    string adGuardEdge = "pdffkfellgipmhklpdmokmckkkfcopbh;https://edge.microsoft.com/extensionwebstorebase/v1/crx";

    if (enable) {
        // ক্রোম ব্রাউজারে AdGuard ফোর্স ইনস্টল করা
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, chromePath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "1", 0, REG_SZ, (const BYTE*)adGuardChrome.c_str(), adGuardChrome.length() + 1);
            RegCloseKey(hKey);
        }
        
        // এজ ব্রাউজারে AdGuard ফোর্স ইনস্টল করা
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, edgePath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "1", 0, REG_SZ, (const BYTE*)adGuardEdge.c_str(), adGuardEdge.length() + 1);
            RegCloseKey(hKey);
        }
    } else {
        // টিক তুলে নিলে AdGuard ডিলিট করে দেওয়া (Value Delete)
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, chromePath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueA(hKey, "1");
            RegCloseKey(hKey);
        }
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, edgePath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueA(hKey, "1");
            RegCloseKey(hKey);
        }
    }
}