# 檔名: secure_server.py
# 執行指令: python3 secure_server.py
import sys
sys.stdout.reconfigure(encoding='utf-8')
import hmac
import hashlib
from flask import Flask, request, jsonify

app = Flask(__name__)

# [防禦關鍵] 只有伺服器知道的密鑰，MITM 攻擊者不知道
SECRET_KEY = b"SUPER_SECRET_KEY_THAT_MITM_DOES_NOT_KNOW"

def verify_token(token_str):
    """
    驗證 Token 格式: "USER_ID.SIGNATURE"
    """
    try:
        parts = token_str.split('.')
        if len(parts) != 2: return False, None
        
        user_id = parts[0]
        provided_sig = parts[1]
        
        # 重新計算簽章 (HMAC-SHA256)
        expected_sig = hmac.new(SECRET_KEY, user_id.encode(), hashlib.sha256).hexdigest()
        
        # [安全比對] 使用 compare_digest 防止時序攻擊
        if hmac.compare_digest(expected_sig, provided_sig):
            return True, user_id
        else:
            return False, None
    except Exception:
        return False, None

@app.route('/api/data', methods=['GET'])
def get_secure_data():
    auth_header = request.headers.get('Authorization')
    
    if not auth_header or not auth_header.startswith("Bearer "):
        return jsonify({"error": "Missing or invalid Authorization header"}), 401

    token = auth_header.split(" ")[1]
    print(f"[Server] Verifying Token: {token}")

    # [防禦實作] 驗證簽章
    is_valid, user_role = verify_token(token)

    if is_valid:
        if user_role == "admin":
             return jsonify({"status": "success", "data": "FLAG{SECURE_ACCESS_VERIFIED}"})
        else:
             return jsonify({"status": "success", "data": "Normal user data"})
    else:
        # MITM 篡改後的 Token 簽章不符，進入此處
        print(f"[Alert] Token signature mismatch! Potential MITM attack.")
        return jsonify({"error": "Integrity Check Failed - Token Tampered"}), 403

if __name__ == '__main__':
    print("[Server] Secure API running on port 5000 (HMAC Enabled)...")
    
    # 產生一組合法的 admin token 供測試使用
    valid_sig = hmac.new(SECRET_KEY, b"admin", hashlib.sha256).hexdigest()
    print(f"\n[*] 用於測試的合法 Admin Token:\n    Bearer admin.{valid_sig}\n")
    
    app.run(host='127.0.0.1', port=5000)