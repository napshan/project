#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace std;
using clockt = std::chrono::steady_clock;

/*
    預設字典：如果攻擊者沒有提供字典檔 .txt，
    就會使用這裡的內建常見弱密碼 / Token 字串來進行爆破攻擊。
*/
static vector<string> default_wordlist() {
    return {
        "password", "12345678", "qwerty123", "admin1234", "letmein01",
        "default_fallback_secret", "jwtkey", "secretKey1", "token2024",
        "passw0rd!", "welcome1", "iloveyou", "changeme", "admin", "rootpass"
    };
}

/*
    將字典中的字串轉成 Token。
    這裡為示範，直接回傳原字串。
    要做更真實的攻擊情境，可以在這裡加上 base64、hash、簽章偽造等動作。
*/
static string make_token_from_word(const string &w) {
    return w;
}

int main(int argc, char* argv[]) {

    cout << "=== Attacker simulator (safe demo) ===\n";

    vector<string> words;     // 字典內容
    string target = "";       // 若使用者輸入要爆破的目標 token
    bool use_file = false;    // 是否指定字典檔案
    string wordfile = "";     // 字典檔路徑

    // 解析命令列參數
    // 用法示例：
    //    attacker.exe -f wordlist.txt target_token
    //    attacker.exe target_token
    //    attacker.exe   (進入互動模式)
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-f" && i+1 < argc) {
            use_file = true;
            wordfile = argv[++i];  // 讀取字典檔名稱
        } else {
            target = a;            // 對目標 token 進行爆破
        }
    }

    // 如果有讀取字典檔案
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
    } 
    else {
        // 若沒有指定字典檔，就使用預設字典
        words = default_wordlist();
    }

    cout << "載入 " << words.size() << " 個候選 token。\n";

    if (!target.empty())
        cout << "目標模式：會自動比對 candidate 是否命中 target。\n";
    else
        cout << "互動模式：每個候選會顯示，你可以手動把 token 貼到防守端測試。\n";

    // 記錄開始時間
    auto start = clockt::now();
    int attempts = 0; // 記錄攻擊次數

    // 攻擊迴圈：逐一嘗試字典中的每個字串
    for (size_t i = 0; i < words.size(); ++i) {

        string candidate = make_token_from_word(words[i]);
        ++attempts;

        // 印出攻擊時間與嘗試內容
        auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
        cout << "[" << attempts << "] "
             << put_time(localtime(&now), "%F %T")
             << " try: " << candidate << "\n";

        // 若有指定 target，則自動判斷是否猜中
        if (!target.empty() && candidate == target) {

            auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                               clockt::now() - start).count();

            cout << "\n>>> SUCCESS: guessed target token: " << candidate << "\n";
            cout << "Attempts: " << attempts << ", elapsed ms: " << elapsed << "\n";

            // 將攻擊結果寫入 log
            ofstream logf("attack_attempts_log.txt", ios::app);
            if (logf.is_open()) {
                logf << "SUCCESS token=" << candidate
                     << " attempts=" << attempts
                     << " elapsed_ms=" << elapsed << "\n";
                logf.close();
            }

            return 0;
        }

        // 若是互動模式，需要讓你有時間複製 token
        this_thread::sleep_for(chrono::milliseconds(350));
    }

    // 若所有字典都試完沒有命中 target
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                       clockt::now() - start).count();

    cout << "\n完成所有候選 token，未命中目標。\n";
    cout << "總嘗試次數: " << attempts << ", 耗時(ms): " << elapsed << "\n";

    // 最後記錄於 log 檔
    ofstream logf("attack_attempts_log.txt", ios::app);
    if (logf.is_open()) {
        logf << "FINISHED attempts=" << attempts
             << " elapsed_ms=" << elapsed << "\n";
        logf.close();
    }

    cout << "提示：你可以把上面列出的 token 貼到防守端測試異常登入偵測效果。\n";
    return 0;
}

