import os
import struct
import random
import binascii
import shutil # 新增：用於檔案操作

class RobustDataProtection:
    def __init__(self):
        self.key = self._generate_key(16)
        self.stored_crc = 0
        self.filename = "encrypted_data.bin"
        self.backup_filename = "encrypted_data.bak" # 新增：備份檔案路徑

    def _generate_key(self, length):
        return bytearray(random.getrandbits(8) for _ in range(length))

    def _calculate_crc32(self, data):
        return binascii.crc32(data) & 0xFFFFFFFF

    def _xor_encrypt(self, data):
        output = bytearray(len(data))
        for i in range(len(data)):
            output[i] = data[i] ^ self.key[i % len(self.key)]
        return output

    def store_data(self, input_str):
        print(f"[Blue Team] 正在加密並儲存資料...")
        data = bytearray(input_str.encode('utf-8'))
        
        # 1. 計算並記憶 CRC (為了偵測)
        self.stored_crc = self._calculate_crc32(data)
        
        # 2. 加密
        encrypted_data = self._xor_encrypt(data)
        
        # 3. 寫入主要檔案 (Target)
        with open(self.filename, 'wb') as f:
            f.write(encrypted_data)
            
        # 4. [防禦升級] 同步寫入備份檔案 (隱藏目錄或受保護區域)
        with open(self.backup_filename, 'wb') as f:
            f.write(encrypted_data)
            
        print(f"[Blue Team] 資料已雙重備份。原始 CRC32: {hex(self.stored_crc)}")

    def verify_and_recover(self):
        print("\n[Blue Team] 啟動資料完整性驗證程序...")
        
        # 1. 嘗試讀取主要檔案
        if not os.path.exists(self.filename):
            print("[Error] 主要檔案遺失！")
            return self._recover_from_backup()

        with open(self.filename, 'rb') as f:
            encrypted_data = bytearray(f.read())

        # 2. 解密並驗證
        decrypted_data = self._xor_encrypt(encrypted_data)
        current_crc = self._calculate_crc32(decrypted_data)

        if current_crc == self.stored_crc:
            print(f"[Success] 驗證成功！資料完好無損: {decrypted_data.decode('utf-8')}")
        else:
            # 3. [防禦升級] 偵測到攻擊，觸發自動修復
            print(f"[ALERT] 警告！偵測到資料竄改！(CRC Mismatch)")
            
            # --- 新增：從備份中讀取並解密原始資料供對比展示 ---
            original_text = "[備份遺失，無法顯示原始內容]"
            if os.path.exists(self.backup_filename):
                with open(self.backup_filename, 'rb') as f:
                    backup_data = bytearray(f.read())
                original_text = self._xor_encrypt(backup_data).decode('utf-8', errors='replace')
            
            # 輸出對比
            print(f"[Info] 原始內容: {original_text}")
            print(f"[Info] 竄改後內容: {decrypted_data.decode('utf-8', errors='replace')}")
            print(f"--------------------------------------------------")
            
            print(f"[Defense] 正在啟動自動修復機制 (Self-Healing)...")
            self._recover_from_backup()

    def _recover_from_backup(self):
        # 從備份還原的邏輯
        if not os.path.exists(self.backup_filename):
            print("[Critical] 嚴重錯誤：備份檔案也被摧毀，資料無法復原！")
            return

        print(f"[Defense] 讀取備份檔案: {self.backup_filename}")
        with open(self.backup_filename, 'rb') as f:
            backup_data = bytearray(f.read())
            
        # 驗證備份是否乾淨
        decrypted_backup = self._xor_encrypt(backup_data)
        backup_crc = self._calculate_crc32(decrypted_backup)
        
        if backup_crc == self.stored_crc:
            # 還原主要檔案
            shutil.copy(self.backup_filename, self.filename)
            print(f"[Success] 修復成功！已使用備份覆蓋受損檔案。")
            print(f"[Result] 還原後的資料: {decrypted_backup.decode('utf-8')}")
        else:
            print("[Critical] 備份檔案也遭到汙染！防禦失敗。")

# ==========================================
# 攻擊模組 (保持不變)
# ==========================================
def attack_module(tamper_count=1):
    filename = "encrypted_data.bin"
    print(f"\n[Red Team] 攻擊主要資料庫: {filename} ...")
    if os.path.exists(filename):
        with open(filename, 'rb') as f:
            data = bytearray(f.read())
        
        # 隨機破壞
        random.seed()
        for _ in range(tamper_count):
            index = random.randint(0, len(data) - 1)
            data[index] ^= (1 << random.randint(0, 7))
            
        with open(filename, 'wb') as f:
            f.write(data)
        print("[Red Team] 攻擊完成，資料已竄改。")

# ==========================================
# 執行
# ==========================================
if __name__ == "__main__":
    # --- 加上這兩行防止中文亂碼 ---
    import sys
    sys.stdout.reconfigure(encoding='utf-8')

    protector = RobustDataProtection()
    
    # 1. 正常儲存 (把 input 拿掉，直接給它一段假資料)
    print("請輸入機敏資料: [自動化測試] 這是一份極機密的客戶金融憑證！")
    user_input = "[自動化測試] 這是一份極機密的客戶金融憑證！"
    protector.store_data(user_input)

    # 2. 模擬攻擊 (把 input 拿掉，直接發動攻擊)
    import time
    time.sleep(1) # 稍微停頓 1 秒讓畫面有動態感
    print("\n是否進行攻擊 (y/n)? y")
    attack_module(tamper_count=3)
    
    # 3. 驗證與修復
    time.sleep(1)
    protector.verify_and_recover()