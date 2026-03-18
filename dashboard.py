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
        
        self.processes = [] # 用來記錄所有開起來的背景程式
        self.protocol("WM_DELETE_WINDOW", self.on_closing) # 關閉視窗時自動清場
        
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
        
        ctk.CTkButton(btn_frame, text="▶ 啟動模組一 (Auth)", command=self.run_mod1, width=180).grid(row=0, column=0, padx=5, pady=5)
        ctk.CTkButton(btn_frame, text="▶ 啟動模組二 (Data)", command=self.run_mod2, width=180).grid(row=0, column=1, padx=5, pady=5)
        ctk.CTkButton(btn_frame, text="▶ 啟動 Server+Proxy", command=self.run_mod3, width=180).grid(row=1, column=0, padx=5, pady=5)
        
        # 發送攻擊的紅色按鈕
        self.btn_send = ctk.CTkButton(btn_frame, text="✉ 發送測試封包", command=self.send_api_request, width=180, fg_color="#993333", hover_color="#AA4444")
        self.btn_send.grid(row=1, column=1, padx=5, pady=5)
        
        self.txt_global_log = ctk.CTkTextbox(self.frame_m4, width=450, height=130, font=("Consolas", 14))
        self.txt_global_log.pack(pady=5)
        self.log_global("系統初始化完成。等待指揮官下達指令。")

    def log_global(self, msg):
        self.txt_global_log.insert("end", f"[*] {msg}\n")
        self.txt_global_log.see("end")
# 通用背景執行引擎 (支援 exe 和 python)
    def run_cmd_in_background(self, folder_name, cmd_list, textbox, update_ui_callback=None):
        def task():
            try:
                flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
                process = subprocess.Popen(
                    cmd_list, cwd=folder_name, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, 
                    text=True, encoding='utf-8', errors='replace', bufsize=1, creationflags=flags
                )
                self.processes.append(process) 
                
                for line in process.stdout:
                    clean_line = strip_ansi(line)
                    self.after(0, lambda l=clean_line: [textbox.insert("end", l), textbox.see("end")])
                    if update_ui_callback:
                        self.after(0, lambda l=clean_line: update_ui_callback(l))
            except Exception as e:
                # [修復] 先把錯誤轉成字串，再傳給介面
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

        self.run_cmd_in_background("1", ["python", "-u", "run_simulation.py"], self.txt_auth_log, check_auth_status)

    def run_mod2(self):
        self.txt_data_log.delete("0.0", "end")
        self.log_global("啟動模組二：資料竄改與自癒測試...")
        
        def start_mod2():
            current_dir = os.getcwd()
            # 明確指定去 4 資料夾裡面找 simulation.py
            script_path = os.path.join(current_dir, "4", "simulation.py")
            
            # 雷達掃描：確認檔案存在
            if not os.path.exists(script_path):
                error_msg = f"雷達警告！找不到檔案：\n{script_path}"
                self.after(0, lambda m=error_msg: self.log_global(m))
                return
                
            # 使用 -u 強制即時輸出，並指定 cwd="4"
            self.run_cmd_in_background("4", ["python", "-u", script_path], self.txt_data_log)
            
        threading.Thread(target=start_mod2, daemon=True).start()
    def run_mod3(self):
        self.txt_mitm_log.delete("0.0", "end")
        self.log_global("啟動模組三：Secure Server 與 MITM Proxy 上線...")
        
        # 1. 啟動安全的 Flask 伺服器
        self.run_cmd_in_background("API", ["python", "-u", "secure_server.py"], self.txt_mitm_log)
        
        # 2. 等待 2 秒讓伺服器開好，再啟動 C++ Proxy
        def start_proxy():
            time.sleep(2)
            current_dir = os.getcwd()
            exe_path = os.path.join(current_dir, "API", "mitm_http_proxy.exe")
            
            # === [新增] 雷達掃描：在啟動前先確認檔案是否真的存在 ===
            if not os.path.exists(exe_path):
                error_msg = f"雷達警告！找不到檔案：\n{exe_path}"
                self.after(0, lambda m=error_msg: self.log_global(m))
                return # 找不到就停止啟動，避免崩潰
            # ========================================================
                
            self.run_cmd_in_background("API", [exe_path, "8888", "127.0.0.1", "5000"], self.txt_mitm_log)
            
        threading.Thread(target=start_proxy, daemon=True).start()

# 虛擬受害者：送出封包給 Proxy
    def send_api_request(self):
        self.log_global("受害者發送合法 Token 封包到 Proxy...")
        self.txt_mitm_log.insert("end", "\n[Client] 發送 GET 請求 (挾帶 NORMAL_USER_TOKEN)...\n")
        self.txt_mitm_log.see("end")
        
        def task():
            req = urllib.request.Request("http://127.0.0.1:8888/api/data")
            req.add_header("Authorization", "Bearer NORMAL_USER_TOKEN")
            try:
                response = urllib.request.urlopen(req, timeout=3)
                res_text = response.read().decode('utf-8')
                self.after(0, lambda r=res_text: self.txt_mitm_log.insert("end", f"[Client] 收到伺服器回應: {r}\n\n"))
            except urllib.error.HTTPError as e:
                err_code = e.code
                err_msg = e.read().decode('utf-8')
                self.after(0, lambda c=err_code, m=err_msg: self.txt_mitm_log.insert("end", f"[Client] 請求被拒絕 ({c}): {m}\n\n"))
            except Exception as e:
                err_str = str(e)
                self.after(0, lambda m=err_str: self.txt_mitm_log.insert("end", f"[Client] 連線失敗: {m}\n\n"))
            finally:
                self.after(0, lambda: self.txt_mitm_log.see("end"))

        threading.Thread(target=task, daemon=True).start()

    # 關閉視窗時，把背景所有的 python 和 exe 全部砍掉，釋放 Port
    def on_closing(self):
        for p in self.processes:
            try: p.terminate()
            except: pass
        self.destroy()

if __name__ == "__main__":
    app = SOCDashboard()
    app.mainloop()