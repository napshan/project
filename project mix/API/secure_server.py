# 檔名: secure_server.py
import sys
sys.stdout.reconfigure(encoding='utf-8')
import hmac
import hashlib
import logging
from flask import Flask, request, jsonify

app = Flask(__name__)

# [優化] 關閉 Flask 預設的繁雜日誌
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

SECRET_KEY = b"SUPER_SECRET_KEY_THAT_MITM_DOES_NOT_KNOW"

def verify_token(token_str):
    try:
        parts = token_str.split('.')
        if len(parts) != 2: return False, None
        
        user_id = parts[0]
        provided_sig = parts[1]
        
        expected_sig = hmac.new(SECRET_KEY, user_id.encode(), hashlib.sha256).hexdigest()
        
        if hmac.compare_digest(expected_sig, provided_sig):
            return True, user_id
        else:
            return False, None
    except Exception:
        return False, None

@app.route('/api/data', methods=['GET'])
def get_secure_data():
    auth_header = request.headers.get('Authorization')
    print("\n==================================================")
    print(f"[Server - 防禦模式] 收到 API 請求")
    
    if not auth_header or not auth_header.startswith("Bearer "):
        print(" └─ [拒絕] 缺少或無效的 Authorization 標頭\n")
        return jsonify({"error": "Missing or invalid Authorization header"}), 401

    token = auth_header.split(" ")[1]
    print(f" ├─ 提取 Token: {token}")
    print(f" ├─ 正在進行 HMAC-SHA256 簽章驗證...")

    is_valid, user_role = verify_token(token)

    if is_valid:
        print(f" └─ [成功] 驗證通過！授權身份: {user_role}")
        if user_role == "admin":
             return jsonify({"status": "success", "data": "FLAG{SECURE_ACCESS_VERIFIED}"})
        else:
             return jsonify({"status": "success", "data": "Normal user data"})
    else:
        print(f" └─ [攔截] 簽章不符！偵測到 Token 遭到竄改 (可能為 MITM 攻擊)！")
        return jsonify({"error": "Integrity Check Failed - Token Tampered"}), 403

if __name__ == '__main__':
    print("[Server] 啟動中... (Port: 5000) [已啟用 HMAC 防禦]")
    valid_sig = hmac.new(SECRET_KEY, b"admin", hashlib.sha256).hexdigest()
    print(f"[*] 測試用合法 Admin Token:\n    Bearer admin.{valid_sig}\n")
    app.run(host='127.0.0.1', port=5000)