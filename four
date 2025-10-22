#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

// 攻擊模組模擬：竄改防禦端儲存的加密資料
const string DATA_FILE = "encrypted_data.bin";

// 從檔案讀取加密後的資料
vector<unsigned char> readEncryptedData() {
    ifstream file(DATA_FILE, ios::binary);
    if (!file) {
        cerr << "找不到加密資料檔案，請先在防禦端執行儲存功能！" << endl;
        exit(1);
    }

    vector<unsigned char> buffer((istreambuf_iterator<char>(file)),
                                  istreambuf_iterator<char>());
    file.close();
    return buffer;
}

// 將竄改後的資料寫回檔案
void writeTamperedData(const vector<unsigned char>& data) {
    ofstream file(DATA_FILE, ios::binary | ios::trunc);
    if (!file) {
        cerr << "寫入失敗，請檢查檔案權限！" << endl;
        exit(1);
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
}

// 隨機竄改資料內容
void tamperData(vector<unsigned char>& data, int tamperCount) {
    srand(time(nullptr));
    for (int i = 0; i < tamperCount; ++i) {
        int index = rand() % data.size();
        unsigned char original = data[index];
        unsigned char tampered = original ^ (1 << (rand() % 8)); // 翻轉其中一個bit
        data[index] = tampered;

        cout << "[!] 攻擊成功：位元組索引 " << index
             << " 被從 0x" << hex << (int)original
             << " 改成 0x" << (int)tampered << dec << endl;
    }
}

// 顯示選單
void showMenu() {
    cout << "\n=== 機敏資料竄改攻擊模擬系統 ===" << endl;
    cout << "1. 讀取現有加密資料" << endl;
    cout << "2. 執行竄改攻擊" << endl;
    cout << "3. 顯示修改後資料內容" << endl;
    cout << "4. 離開" << endl;
    cout << "請輸入選項: ";
}

int main() {
    vector<unsigned char> encryptedData;
    bool hasData = false;
    int choice;

    while (true) {
        showMenu();
        cin >> choice;

        switch (choice) {
            case 1:
                encryptedData = readEncryptedData();
                hasData = true;
                cout << "[Step] 成功讀取加密資料，共 " << encryptedData.size() << " bytes" << endl;
                break;

            case 2:
                if (!hasData) {
                    cout << "請先讀取加密資料！" << endl;
                    break;
                }
                int tamperCount;
                cout << "請輸入要竄改的位元組數量: ";
                cin >> tamperCount;
                tamperData(encryptedData, tamperCount);
                writeTamperedData(encryptedData);
                cout << "[Step] 已將竄改後資料覆蓋寫回檔案。" << endl;
                break;

            case 3:
                if (!hasData) {
                    cout << "尚未讀取資料！" << endl;
                    break;
                }
                cout << "加密資料 (前16 bytes 預覽): ";
                for (size_t i = 0; i < min<size_t>(encryptedData.size(), 16); ++i) {
                    cout << hex << (int)encryptedData[i] << " ";
                }
                cout << dec << endl;
                break;

            case 4:
                cout << "退出程式。" << endl;
                return 0;

            default:
                cout << "無效選項，請重新輸入！" << endl;
                break;
        }
    }
}
