#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <limits>
#include <algorithm> 

// Windows 專用：設定編碼
#ifdef _WIN32
#include <windows.h>
#endif

class TokenManager {
private:
    std::unordered_map<std::string, bool> validTokens;
    int attackCount = 0;
    const int attackThreshold = 5; 
    const std::string logFile = "defense_log.txt";

    void logAttack(const std::string& detail) {
        std::ofstream logStream(logFile, std::ios::app);
        if (logStream.is_open()) {
            logStream << detail << std::endl;
            logStream.close();
        }
    }

public:
    void addToken(const std::string& token) {
        validTokens[token] = true;
    }

    int validateToken(const std::string& token) {
        if (validTokens.find(token) != validTokens.end() && validTokens[token]) {
            return 0; 
        } else {
            attackCount++;
            logAttack("[警告] 惡意攻擊: 使用 token " + token + " | 攻擊次數: " + std::to_string(attackCount));
            if (attackCount >= attackThreshold) return 2;
            return 1;
        }
    }

    bool isBlocked() { return attackCount >= attackThreshold; }
    int getAttackCount() { return attackCount; }
    int getThreshold() { return attackThreshold; }
};

// 移除 Windows 的 \r 換行符
std::string cleanString(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

std::string inputValidatedKey(const std::string& prompt) {
    std::string key;
    while (true) {
        std::cout << prompt << std::flush; 
        std::getline(std::cin, key);
        key = cleanString(key);

        if (key.length() >= 8 && key.length() <= 16) {
            break;
        }
        std::cout << "長度不符 (" << key.length() << ")，請重新輸入 (8-16字元)\n" << std::flush;
    }
    return key;
}

struct ApiRequest {
    std::string token;
    std::string payload;
};

int main() {
    // 1. 強制設定 Windows 控制台輸出為 UTF-8
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    #endif

    // 2. 關閉緩衝，讓 Python 即時讀取
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    TokenManager tokenManager;

    // 3. 初始化
    std::string key1 = inputValidatedKey("請輸入設定密鑰 (8-16字元): ");
    std::string key2 = inputValidatedKey("請再次輸入以確認: ");
    
    if (key1 != key2) {
        std::cout << "錯誤：兩次輸入的密鑰不一致。\n";
        return 1;
    }
    
    tokenManager.addToken(key1);
    std::cout << ">>> 系統初始化完成，防禦系統啟動 <<<\n";

    // 4. 主循環
    std::string inputToken;
    while (true) {
        std::cout << "WAITING_FOR_TOKEN\n" << std::flush; // 關鍵訊號
        
        std::getline(std::cin, inputToken);
        inputToken = cleanString(inputToken);
        
        if (inputToken.empty()) continue;
        if (inputToken == "exit") break;

        if (tokenManager.isBlocked()) {
            std::cout << "[BLOCK] 防禦系統已鎖定，拒絕來自 " << inputToken << " 的請求\n";
            continue; 
        }

        ApiRequest req{inputToken, "Payload_Data"};
        int result = tokenManager.validateToken(req.token);

        if (result == 0) {
            std::cout << "[ALLOW] 驗證成功! 允許存取機敏資料\n";
        } else if (result == 2) {
            std::cout << "[ALERT] 偵測到暴力破解，達到閾值 (" 
                      << tokenManager.getAttackCount() << "/" << tokenManager.getThreshold() 
                      << ")，系統鎖定\n";
        } else {
            std::cout << "[DENY] 驗證失敗 (Token無效)，累計攻擊次數 " 
                      << tokenManager.getAttackCount() << "\n";
        }
    }

    return 0;
}
