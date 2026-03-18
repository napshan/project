# 檔名: vulnerable_server.py
# 執行指令: python3 vulnerable_server.py

from flask import Flask, request, jsonify

app = Flask(__name__)

# 模擬資料庫中的機密資料
SECRET_DATA = {
    "admin": "FLAG{CRITICAL_SYSTEM_ACCESS_GRANTED}",
    "user": "Hello normal user, standard permission."
}

@app.route('/api/data', methods=['GET'])
def get_data():
    auth_header = request.headers.get('Authorization')
    
    if not auth_header:
        return jsonify({"error": "Missing Authorization header"}), 401

    print(f"[Server] Received Authorization: {auth_header}")

    # [漏洞點] 不安全的權限驗證邏輯
    # 伺服器只檢查字串內容，沒有驗證簽章，導致攻擊者可以輕易偽造
    if auth_header == "Bearer FORGED_BY_MITM_DEMO":
        # 攻擊成功：伺服器誤以為這是合法的高權限請求
        return jsonify({
            "status": "success", 
            "role": "admin (BYPASSED)", 
            "data": SECRET_DATA["admin"]
        })
    
    elif auth_header == "Bearer NORMAL_USER_TOKEN":
        return jsonify({
            "status": "success", 
            "role": "user", 
            "data": SECRET_DATA["user"]
        })
    
    else:
        return jsonify({"error": "Invalid Token"}), 403

if __name__ == '__main__':
    print("[Server] Vulnerable API running on port 5000 (No Signature Verification)...")
    app.run(host='127.0.0.1', port=5000)