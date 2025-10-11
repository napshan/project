#include <stdio.h>
#include <string.h>
#include <unistd.h> // for sleep()

// 模擬的登入請求 (實際可連接防禦端API，這裡用模擬函式代替)
int login_attempt(const char* username, const char* password) {
    // 假設正確密碼是 "correct_pw"
    if (strcmp(password, "correct_pw") == 0) {
        printf("[?] 成功登入！密碼為：%s\n", password);
        return 1; // 登入成功
    } else {
        printf("[?] 登入失敗，密碼嘗試：%s\n", password);
        return 0; // 登入失敗
    }
}

int main() {
    const char* username = "student1";

    // 模擬常見密碼字典
    const char* password_list[] = {
        "123456", "password", "admin", "student", "student1", 
        "1234", "0000", "1111", "test123", "correct_pw"
    };

    int total = sizeof(password_list) / sizeof(password_list[0]);
    int success = 0;

    printf("=== 模擬暴力破解登入攻擊程式 ===\n");
    printf("目標帳號：%s\n", username);
    printf("---------------------------------\n");

    for (int i = 0; i < total; i++) {
        printf("[攻擊中] 嘗試第 %d 個密碼...\n", i + 1);
        int result = login_attempt(username, password_list[i]);
        sleep(1); // 模擬人為間隔（避免過快）
        if (result) {
            success = 1;
            break;
        }
    }

    if (!success) {
        printf("\n?? 所有密碼皆嘗試失敗，帳號可能已被防禦模組封鎖。\n");
    }

    printf("\n=== 攻擊模擬結束 ===\n");
    return 0;
}

