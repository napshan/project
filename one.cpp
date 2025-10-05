#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace std;
using clockt = std::chrono::steady_clock;

static vector<string> default_wordlist() {
    return {
        "password", "12345678", "qwerty123", "admin1234", "letmein01",
        "default_fallback_secret", "jwtkey", "secretKey1", "token2024",
        "passw0rd!", "welcome1", "iloveyou", "changeme", "admin", "rootpass"
    };
}

static string make_token_from_word(const string &w) {
    // In your defender, a "token" can be any string. For demo we just return the candidate.
    // You can modify this to apply base64 or other transforms if desired.
    return w;
}

int main(int argc, char* argv[]) {
    cout << "=== Attacker simulator (safe demo) ===\n";
    vector<string> words;
    string target = "";
    bool use_file = false;
    string wordfile = "";

    // parse args
    for (int i=1;i<argc;i++) {
        string a = argv[i];
        if (a == "-f" && i+1 < argc) { use_file = true; wordfile = argv[++i]; }
        else target = a;
    }

    if (use_file) {
        ifstream fin(wordfile);
        if (!fin.is_open()) {
            cerr << "無法打開字典檔: " << wordfile << "\n";
            return 1;
        }
        string line;
        while (getline(fin, line)) {
            if (!line.empty()) words.push_back(line);
        }
        fin.close();
        if (words.empty()) {
            cerr << "字典檔為空或只含空白行\n";
            return 1;
        }
    } else {
        words = default_wordlist();
    }

    cout << "載入 " << words.size() << " 個候選 token 。\n";
    if (!target.empty()) cout << "目標 (target) 模式：會自動停止在命中 target\n";
    else cout << "互動模式：程式只列出每個嘗試，請將候選 token 貼到防禦系統輸入框進行測試\n";

    // simple attack loop
    auto start = clockt::now();
    int attempts = 0;
    for (size_t i = 0; i < words.size(); ++i) {
        string candidate = make_token_from_word(words[i]);
        ++attempts;
        // print attempt info
        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        cout << "[" << attempts << "] "
             << put_time(localtime(&now), "%F %T") << " try: " << candidate << "\n";

        // If user provided a target token as arg, auto-compare (simulate success)
        if (!target.empty() && candidate == target) {
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(clockt::now() - start).count();
            cout << "\n>>> SUCCESS: guessed target token: " << candidate << "\n";
            cout << "Attempts: " << attempts << ", elapsed ms: " << elapsed << "\n";
            // log to file
            ofstream logf("attack_attempts_log.txt", ios::app);
            if (logf.is_open()) {
                logf << "SUCCESS token=" << candidate << " attempts=" << attempts << " elapsed_ms=" << elapsed << "\n";
                logf.close();
            }
            return 0;
        }

        // If interactive demo, pause small amount so you can copy/paste from console easily
        std::this_thread::sleep_for(std::chrono::milliseconds(350));

        // simple rate-limit or stop if you want to avoid too fast attempts
    }

    auto elapsed = chrono::duration_cast<chrono::milliseconds>(clockt::now() - start).count();
    cout << "\nFinished all candidates. Attempts: " << attempts << ", elapsed ms: " << elapsed << "\n";
    ofstream logf("attack_attempts_log.txt", ios::app);
    if (logf.is_open()) {
        logf << "FINISHED attempts=" << attempts << " elapsed_ms=" << elapsed << "\n";
        logf.close();
    }
    cout << "提示：將列出的某個 token 貼到防守端輸入，看防守端是否記錄為攻擊或接受。\n";
    return 0;
}

