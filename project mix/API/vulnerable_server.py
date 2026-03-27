# 檔名: vulnerable_server.py
import sys
sys.stdout.reconfigure(encoding='utf-8')
import logging
from flask import Flask, request, jsonify

app = Flask(__name__)

# [優化] 關閉 Flask 預設的繁雜日誌
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

SECRET_DATA = {
    "admin": "FLAG{CRITICAL_SYSTEM_ACCESS_GRANTED}",
    "user": "Hello normal user, standard permission."
}

@app.route('/api/data', methods=['GET'])
def get_data():
    auth_header = request.headers.get('Authorization')
    print("\n==================================================")
    print(f"[Server - 脆弱模式] 收到 API 請求")
    
    if not auth_header:
        print(" └─ [拒絕] 缺少 Authorization 標頭\n")
        return jsonify({"error": "Missing Authorization header"}), 401

    print(f" ├─ 解析 Token: {auth_header}")

    # [漏洞點] 不安全的權限驗證邏輯
    if auth_header == "Bearer FORGED_BY_MITM_DEMO":
        print(" └─ [危險] 伺服器被欺騙！接受了偽造的 Token，核發 Admin 權限！")
        return jsonify({
            "status": "success", 
            "role": "admin (BYPASSED)", 
            "data": SECRET_DATA["admin"]
        })
    elif auth_header == "Bearer NORMAL_USER_TOKEN":
        print(" └─ [正常] 辨識為一般用戶。")
        return jsonify({
            "status": "success", 
            "role": "user", 
            "data": SECRET_DATA["user"]
        })
    else:
        print(" └─ [拒絕] 無效的 Token。")
        return jsonify({"error": "Invalid Token"}), 403

if __name__ == '__main__':
    print("[Server] 啟動中... (Port: 5000) [無簽章驗證]")
    app.run(host='127.0.0.1', port=5000)