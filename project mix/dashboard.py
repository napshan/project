import customtkinter as ctk
import subprocess
import threading
import urllib.request
import urllib.error
import re
import os
import time

# --- 設定深色科技主題 ---
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")

def strip_ansi(text):
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', text)

class SOCDashboard(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Cybersecurity SOC Dashboard - 專題防禦演練平台")
        self.geometry("1200x800")
        
        self.grid_rowconfigure((0, 1), weight=1)
        self.grid_columnconfigure((0, 1), weight=1)
        
        self.processes = [] # 記錄所有背景程式 (關閉視窗時統一清理)
        self.mod3_processes = [] # [新增] 專門記錄 Module 3 的程式 (為了切換模式時可以先砍掉舊的)
        
        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        self.setup_module_1()
        self.setup_module_2()
        self.setup_module_3()
        self.setup_module_4()

    def setup_module_1(self):
        self.frame_m1 = ctk.CTkFrame(self)
        self.frame_m1.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_m1, text="[ Module 1 ] 暴力破解防禦與速率限制", font=("Microsoft JhengHei", 20, "bold"), text_color="#3399FF").pack(pady=10)
        self.lbl_auth_status = ctk.CTkLabel(self.frame_m1, text="系統狀態：等待啟動...", font=("Microsoft JhengHei", 16), text_color="#AAAAAA")
        self.lbl_auth_status.pack(pady=5)
        self.progress_auth = ctk.CTkProgressBar(self.frame_m1, width=400)
        self.progress_auth.pack(pady=10)
        self.progress_auth.set(0)
        self.txt_auth_log = ctk.CTkTextbox(self.frame_m1, width=450, height=200, font=("Consolas", 14))
        self.txt_auth_log.pack(pady=10)

    def setup_module_2(self):
        self.frame_m2 = ctk.CTkFrame(self)
        self.frame_m2.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_m2, text="[ Module 2 ] CRC 校驗與自動修復", font=("Microsoft JhengHei", 20, "bold"), text_color="#3399FF").pack(pady=10)
        self.txt_data_log = ctk.CTkTextbox(self.frame_m2, width=450, height=250, font=("Consolas", 14))
        self.txt_data_log.pack(pady=10)

    def setup_module_3(self):
        self.frame_m3 = ctk.CTkFrame(self)
        self.frame_m3.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_m3, text="[ Module 3 ] MITM 攔截與 HMAC 簽章", font=("Microsoft JhengHei", 20, "bold"), text_color="#3399FF").pack(pady=10)
        ctk.CTkLabel(self.frame_m3, text="[ Client ] --(Port 8888)--> [ Proxy ] --(Port 5000)--> [ Server ]", font=("Consolas", 14), text_color="#00FF00").pack(pady=5)
        self.txt_mitm_log = ctk.CTkTextbox(self.frame_m3, width=450, height=230, font=("Consolas", 14))
        self.txt_mitm_log.pack(pady=10)

    def setup_module_4(self):
        self.frame_m4 = ctk.CTkFrame(self)
        self.frame_m4.grid(row=1, column=1, padx=10, pady=10, sticky="nsew")
        ctk.CTkLabel(self.frame_m4, text=">>> 全域控制台 (Simulation Control)", font=("Microsoft JhengHei", 20, "bold"), text_color="#FFCC00").pack(pady=5)
        
        btn_frame = ctk.CTkFrame(self.frame_m4, fg_color="transparent")
        btn_frame.pack(pady=5)
        
        # [優化] 第一排：獨立模組
        ctk.CTkButton(btn_frame, text="▶ 啟動模組一 (Auth)", command=self.run_mod1, width=180).grid(row=0, column=0, padx=5, pady=5)
        ctk.CTkButton(btn_frame, text="▶ 啟動模組二 (Data)", command=self.run_mod2, width=180).grid(row=0, column=1, padx=5, pady=5)
        
        # [新增] 第二排：防禦開關與啟動
        self.switch_secure = ctk.CTkSwitch(btn_frame, text="啟用 HMAC 防禦 (Secure)", font=("Microsoft JhengHei", 14, "bold"), onvalue=1, offvalue=0)
        self.switch_secure.grid(row=1, column=0, padx=5, pady=5)
        ctk.CTkButton(btn_frame, text="▶ 啟動 Server+Proxy", command=self.run_mod3, width=180, fg_color="#228B22", hover_color="#2E8B57").grid(row=1, column=1, padx=5, pady=5)
        
        # [優化] 第三排：發送攻擊的紅色按鈕 (橫跨兩格)
        self.btn_send = ctk.CTkButton(btn_frame, text="✉ 模擬受害者發送封包", command=self.send_api_request, width=370, fg_color="#993333", hover_color="#AA4444")
        self.btn_send.grid(row=2, column=0, columnspan=2, padx=5, pady=10)
        
        self.txt_global_log = ctk.CTkTextbox(self.frame_m4, width=450, height=100, font=("Consolas", 14))
        self.txt_global_log.pack(pady=5)
        self.log_global("系統初始化完成。等待指揮官下達指令。")

    def log_global(self, msg):
        self.txt_global_log.insert("end", f"[*] {msg}\n")
        self.txt_global_log.see("end")

    # 通用背景執行引擎 (新增 target_list 參數，方便管理特定群組的 Process)
    def run_cmd_in_background(self, folder_name, cmd_list, textbox, update_ui_callback=None, target_list=None):
        def task():
            try:
                flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
                process = subprocess.Popen(
                    cmd_list, cwd=folder_name, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
                    text=True, encoding='utf-8', errors='replace', bufsize=1, creationflags=flags
                )
                self.processes.append(process)
                if target_list is not None:
                    target_list.append(process)
                
                for line in process.stdout:
                    clean_line = strip_ansi(line)
                    self.after(0, lambda l=clean_line: [textbox.insert("end", l), textbox.see("end")])
                    if update_ui_callback:
                        self.after(0, lambda l=clean_line: update_ui_callback(l))
            except Exception as e:
                err_msg = str(e)
                self.after(0, lambda m=err_msg: self.log_global(f"啟動失敗: {m}"))
        threading.Thread(target=task, daemon=True).start()

    def run_mod1(self):
        self.txt_auth_log.delete("0.0", "end")
        self.lbl_auth_status.configure(text="系統狀態：遭受攻擊中！", text_color="#FFCC00")
        self.progress_auth.set(0)
        self.log_global("啟動模組一：自動化字典攻擊與防禦...")
        
        def check_auth_status(line):
            if "try:" in line or "[DENY]" in line:
                val = self.progress_auth.get()
                if val < 1.0: self.progress_auth.set(val + 0.2)
            if "鎖定" in line or "[BLOCK]" in line or "ALERT" in line:
                self.progress_auth.set(1.0)
                self.lbl_auth_status.configure(text="系統狀態：>> 系統已鎖定！ <<", text_color="#FF3333")

        self.run_cmd_in_background("1", ["python", "-u", "run_simulation.py"], self.txt_auth_log)

    def run_mod2(self):
        self.txt_data_log.delete("0.0", "end")
        self.log_global("啟動模組二：資料竄改與自癒測試...")
        
        def start_mod2():
            current_dir = os.getcwd()
            script_path = os.path.join(current_dir, "4", "simulation.py")
            if not os.path.exists(script_path):
                self.after(0, lambda m=f"雷達警告！找不到檔案：\n{script_path}": self.log_global(m))
                return
            self.run_cmd_in_background("4", ["python", "-u", script_path], self.txt_data_log)
            
        threading.Thread(target=start_mod2, daemon=True).start()

    def run_mod3(self):
        # [防呆機制] 啟動前先砍掉舊的 Server 和 Proxy，避免 Port 衝突
        for p in self.mod3_processes:
            try: p.terminate()
            except: pass
        self.mod3_processes.clear()

        self.txt_mitm_log.delete("0.0", "end")
        
        # [核心邏輯] 判斷開關狀態，決定要跑哪一隻伺服器
        is_secure = self.switch_secure.get() == 1
        server_script = "secure_server.py" if is_secure else "vulnerable_server.py"
        mode_text = "防禦模式 (HMAC)" if is_secure else "脆弱模式 (無驗證)"
        
        self.log_global(f"重新部署環境：目前為【{mode_text}】")
        
        # 1. 啟動選定的 Flask 伺服器
        self.run_cmd_in_background("API", ["python", "-u", server_script], self.txt_mitm_log, target_list=self.mod3_processes)
        
        # 2. 等待 1.5 秒讓伺服器開好，再啟動 C++ Proxy
        def start_proxy():
            time.sleep(1.5)
            current_dir = os.getcwd()
            exe_path = os.path.join(current_dir, "API", "mitm_http_proxy.exe")
            
            if not os.path.exists(exe_path):
                self.after(0, lambda m=f"雷達警告！找不到檔案：\n{exe_path}": self.log_global(m))
                return
                
            self.run_cmd_in_background("API", [exe_path, "8888", "127.0.0.1", "5000"], self.txt_mitm_log, target_list=self.mod3_processes)
            
        threading.Thread(target=start_proxy, daemon=True).start()

    def send_api_request(self):
        mode = "【防禦】" if self.switch_secure.get() == 1 else "【脆弱】"
        self.log_global(f"測試 {mode} 狀態下遭受 MITM 攻擊的反應...")
        
        self.txt_mitm_log.insert("end", "\n--------------------------------------------------\n")
        self.txt_mitm_log.insert("end", "[Client] 一般用戶發送 GET 請求 (挾帶 NORMAL_USER_TOKEN)...\n")
        self.txt_mitm_log.see("end")
        
        def task():
            req = urllib.request.Request("http://127.0.0.1:8888/api/data")
            req.add_header("Authorization", "Bearer NORMAL_USER_TOKEN")
            try:
                response = urllib.request.urlopen(req, timeout=3)
                res_text = response.read().decode('utf-8')
                self.after(0, lambda r=res_text: self.txt_mitm_log.insert("end", f"[Client] 收到回應 (HTTP 200) -> 攻擊成功！資料外洩：\n └─ {r}\n\n"))
            except urllib.error.HTTPError as e:
                err_code = e.code
                err_msg = e.read().decode('utf-8')
                self.after(0, lambda c=err_code, m=err_msg: self.txt_mitm_log.insert("end", f"[Client] 收到回應 (HTTP {c}) -> 防禦成功！連線遭拒：\n └─ {m}\n\n"))
            except Exception as e:
                err_str = str(e)
                self.after(0, lambda m=err_str: self.txt_mitm_log.insert("end", f"[Client] 連線失敗 (請確認伺服器已啟動): {m}\n\n"))
            finally:
                self.after(0, lambda: self.txt_mitm_log.see("end"))

        threading.Thread(target=task, daemon=True).start()

    def on_closing(self):
        for p in self.processes:
            try: p.terminate()
            except: pass
        self.destroy()

if __name__ == "__main__":
    app = SOCDashboard()
    app.mainloop()