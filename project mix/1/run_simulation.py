import subprocess
import time
import os
import re
import atexit
import sys

# <--- 新增這行：強制 Python 輸出為 UTF-8，這樣印 🚫 和 🛡️ 就不會當機了！
sys.stdout.reconfigure(encoding='utf-8')

# 1. 切換工作目錄
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# 定義顏色
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    PURPLE = '\033[95m'
    CYAN = '\033[96m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

# 全域變數，用於清理
attack_process = None
defend_process = None

def cleanup_processes():
    """強制清理殘留的程序，避免 .exe 檔被鎖定無法編譯"""
    global attack_process, defend_process
    
    if attack_process and attack_process.poll() is None:
        try:
            attack_process.terminate()
            # 如果 terminate 無效，等待一下再 kill
            attack_process.wait(timeout=0.5)
        except:
            if attack_process: attack_process.kill()
            
    if defend_process and defend_process.poll() is None:
        try:
            defend_process.terminate()
            defend_process.wait(timeout=0.5)
        except:
            if defend_process: defend_process.kill()
            
    # 確保釋放變數
    attack_process = None
    defend_process = None

# 註冊：當 Python 結束時(無論正常或崩潰)，都會執行 cleanup_processes
atexit.register(cleanup_processes)

def compile_cpp():
    print(f"{Colors.YELLOW}[SYSTEM] 正在準備環境...{Colors.RESET}")
    print(f"{Colors.YELLOW}[SYSTEM] 工作目錄: {os.getcwd()}{Colors.RESET}")
    
    # 檢查檔案
    if not os.path.exists("attack_1.cpp") or not os.path.exists("defend_1.cpp"):
        print(f"{Colors.RED}[ERROR] 找不到 cpp 原始碼！{Colors.RESET}")
        return False

    # 嘗試預先刪除舊的 exe，確認沒有被鎖定
    exe_ext = ".exe" if os.name == 'nt' else ""
    for f in ["attack_1" + exe_ext, "defend_1" + exe_ext]:
        if os.path.exists(f):
            try:
                os.remove(f)
            except PermissionError:
                print(f"{Colors.RED}[ERROR] 無法刪除舊的 {f}，檔案被佔用中。{Colors.RESET}")
                print(f"{Colors.RED}請手動執行: taskkill /F /IM {f} /T{Colors.RESET}")
                return False

    print(f"{Colors.YELLOW}[SYSTEM] 正在編譯紅藍隊程式碼...{Colors.RESET}")
    
    shell_cmd = True if os.name == 'nt' else False
    
    if subprocess.call(["g++", "attack_1.cpp", "-o", "attack_1"], shell=shell_cmd) != 0:
        print(f"{Colors.RED}[ERROR] attack_1.cpp 編譯失敗。{Colors.RESET}")
        return False
        
    if subprocess.call(["g++", "defend_1.cpp", "-o", "defend_1"], shell=shell_cmd) != 0:
        print(f"{Colors.RED}[ERROR] defend_1.cpp 編譯失敗。{Colors.RESET}")
        return False
        
    print(f"{Colors.GREEN}[SYSTEM] 編譯完成！準備開始演練...{Colors.RESET}\n")
    return True

def run_demo():
    global attack_process, defend_process
    print(f"{Colors.YELLOW}[SYSTEM] 啟動紫隊演練腳本 (v2.2 Safe-Close)...{Colors.RESET}")
    TARGET_PASSWORD = "admin1234"

    if os.name == 'nt':
        attack_exec = "attack_1.exe"
        defend_exec = "defend_1.exe"
        SYS_ENCODING = 'utf-8' 
    else:
        attack_exec = "./attack_1"
        defend_exec = "./defend_1"
        SYS_ENCODING = 'utf-8'

    try:
        # 1. 啟動藍隊
        defend_process = subprocess.Popen(
            [defend_exec],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding=SYS_ENCODING,
            errors='replace',
            bufsize=0,
            shell=False 
        )
    except Exception as e:
        print(f"{Colors.RED}[ERROR] 啟動 Defender 失敗: {e}{Colors.RESET}")
        return

    print(f"{Colors.BLUE}[BLUE TEAM] 初始化安全閘道器...{Colors.RESET}")
    time.sleep(1)
    print(f"{Colors.CYAN}[ADMIN] 設定管理者金鑰: {TARGET_PASSWORD}{Colors.RESET}")
    
    try:
        defend_process.stdin.write(TARGET_PASSWORD + "\n")
        defend_process.stdin.write(TARGET_PASSWORD + "\n")
    except Exception as e:
        print(f"{Colors.RED}[ERROR] 初始化寫入失敗: {e}{Colors.RESET}")
        return
    
    # 等待系統上線
    while True:
        try:
            line = defend_process.stdout.readline()
            if "系統初始化完成" in line:
                print(f"{Colors.BLUE}[BLUE TEAM] {line.strip()}{Colors.RESET}")
                break
            if not line: break
        except: break

    print("-" * 60)
    print(f"{Colors.PURPLE}{Colors.BOLD}>>> 紫隊演練開始：自動化攻擊與防禦模擬 <<<{Colors.RESET}")
    print("-" * 60)
    time.sleep(2)

    # 3. 啟動紅隊
    try:
        attack_process = subprocess.Popen(
            [attack_exec],
            stdout=subprocess.PIPE,
            text=True,
            encoding=SYS_ENCODING,
            errors='replace',
            bufsize=0,
            shell=False
        )
    except Exception as e:
        print(f"{Colors.RED}[ERROR] 啟動 Attacker 失敗: {e}{Colors.RESET}")
        return

    # 4. 攻防迴圈 (使用 try...finally 確保一定會清理)
    try:
        while True:
            # A. 讀取攻擊者輸出
            try:
                attack_line = attack_process.stdout.readline()
                if not attack_line: break 
            except: continue

            match = re.search(r"try:\s*(.+)", attack_line)
            if match:
                token_attempt = match.group(1).strip()
                print(f"{Colors.RED}[RED TEAM] 發送惡意 Payload: '{token_attempt}'{Colors.RESET}")

                # B. 等待防禦者準備好
                while True:
                    def_line = defend_process.stdout.readline()
                    if "WAITING_FOR_TOKEN" in def_line:
                        break
                    if def_line.strip():
                        print(f"{Colors.BLUE}    └─ [LOG] {def_line.strip()}{Colors.RESET}")

                # C. 將攻擊送往防禦者
                try:
                    defend_process.stdin.write(token_attempt + "\n")
                except: break
                
                # D. 讀取防禦結果
                result_line = defend_process.stdout.readline()
                
                if "[ALLOW]" in result_line:
                    print(f"{Colors.GREEN}{Colors.BOLD}    └─ [DEFENSE] ⚠ 警報：攻擊穿透防禦！ {result_line.strip()}{Colors.RESET}")
                elif "[BLOCK]" in result_line or "鎖定" in result_line:
                    print(f"{Colors.BLUE}{Colors.BOLD}    └─ [DEFENSE] 🛡️ {result_line.strip()}{Colors.RESET}")
                elif "[DENY]" in result_line:
                    print(f"{Colors.BLUE}    └─ [DEFENSE] 🚫 {result_line.strip()}{Colors.RESET}")
                else:
                    print(f"{Colors.BLUE}    └─ [DEFENSE] {result_line.strip()}{Colors.RESET}")
                print("") 

            else:
                if attack_line.strip():
                    print(f"{Colors.YELLOW}[RED LOG] {attack_line.strip()}{Colors.RESET}")

    except KeyboardInterrupt:
        print("\n[SYSTEM] 演練手動終止。")
    except Exception as e:
        print(f"\n[SYSTEM] 發生錯誤: {e}")
    finally:
        # 這裡是最重要的修改：無論發生什麼事，最後一定執行清理
        cleanup_processes()
        print("-" * 60)
        print(f"{Colors.PURPLE}演練結束。程序已釋放。{Colors.RESET}")

if __name__ == "__main__":
    # 先清理一次可能的殘留
    cleanup_processes()
    if compile_cpp():
        run_demo()