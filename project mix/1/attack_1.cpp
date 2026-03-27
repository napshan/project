#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

static vector<string> default_wordlist() {
    return {
        "password", "12345678", "qwerty123", "letmein01",
        "default_fallback_secret", "jwtkey", "secretKey1", "token2024",
        "passw0rd!", "welcome1", "iloveyou", "changeme",
        "admin1234", // 正確答案
        "rootpass"
    };
}

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    #endif

    setvbuf(stdout, NULL, _IONBF, 0); // 關閉緩衝

    cout << "=== Attacker simulator (Red Team) ===\n";

    vector<string> words = default_wordlist();
    
    int attempts = 0; 
    for (size_t i = 0; i < words.size(); ++i) {
        string candidate = words[i];
        ++attempts;

        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        
        // 輸出格式：[次數] 時間 try: 密碼
        cout << "[" << attempts << "] "
             << put_time(localtime(&now), "%F %T")
             << " try: " << candidate << "\n";

        // 模擬網路延遲 (稍微加快一點以便觀察，設為 500ms)
        this_thread::sleep_for(chrono::milliseconds(500));
    }

    cout << "字典攻擊結束。\n";
    return 0;
}
