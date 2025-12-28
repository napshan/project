// mitm_http_proxy.cpp
// Compile: g++ mitm_http_proxy.cpp -o mitm_http_proxy -std=c++17 -pthread
// Usage: ./mitm_http_proxy [listen_port] [upstream_host] [upstream_port]
// Example: ./mitm_http_proxy 8888 127.0.0.1 5000

// 以下 include 為 POSIX / BSD socket 與一般 C++ 標準庫
#include <arpa/inet.h>    // inet_pton, sockaddr_in
#include <netinet/in.h>   // sockaddr_in
#include <sys/socket.h>   // socket, bind, accept, recv, send
#include <unistd.h>       // close

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

// 全域常數：緩衝大小與 log 檔名
static const int BUFFER_SIZE = 8192;
static const string LOG_FILE = "mitm_log.txt";

// ----------------------------------------------------------------------------
// log_line
// 目的：把一行訊息同時輸出到 stdout 與 log 檔（append 模式）
// 註：方便 demo 時在畫面看到訊息，同時保留紀錄供報告與檢視
// ----------------------------------------------------------------------------
void log_line(const string &s) {
    cout << s << endl;
    ofstream f(LOG_FILE, ios::app);
    if (f.is_open()) {
        f << s << "\n";
        f.close();
    }
}

// ----------------------------------------------------------------------------
// recv_until_double_crlf
// 目的：從 socket 讀取資料直到遇到 HTTP header 與 body 的分隔 "\r\n\r\n"
// 備註：
//  - 回傳值包含 header 區塊，且可能包含 body 的一部分（若 client 已經同時送了 body）
//  - 為了教學簡單性，此實作使用 blocking recv 迴圈，不處理超時或 chunked transfer
//  - 如果要更健壯，應設置 select/poll 與 timeout 處理，或解析 Transfer-Encoding: chunked
// ----------------------------------------------------------------------------
string recv_until_double_crlf(int sock) {
    string acc;
    char buf[BUFFER_SIZE];

    // 這裡簡單使用阻塞式 recv 迴圈，實務上要小心 DoS 或長時間等待
    while (true) {
        ssize_t r = recv(sock, buf, sizeof(buf), 0);
        if (r <= 0) break;                     // client 關閉或錯誤
        acc.append(buf, buf + r);              // 累加接收到的 bytes
        if (acc.find("\r\n\r\n") != string::npos) break; // 找到 header 結尾則停止
        // demo 用簡單迴圈即可
    }
    return acc;
}

// ----------------------------------------------------------------------------
// parse_headers
// 目的：把 header block（多行字串）解析成 start_line（request/response 行）
//       與 map<string,string> headers_out（header 名稱轉為小寫 key）
// 注意：header name 以小寫存入 map（便於 case-insensitive 比對）
// ----------------------------------------------------------------------------
void parse_headers(const string &head_block, string &start_line, map<string,string> &headers_out) {
    headers_out.clear();
    stringstream ss(head_block);
    string line;
    bool first = true;
    while (getline(ss, line)) {
        // 去掉行尾可能的 '\r'
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (first) {
            // 第一行是 start-line：例如 "GET /path HTTP/1.1" 或 "HTTP/1.1 200 OK"
            start_line = line;
            first = false;
        } else {
            // 其他行應該是 header: value
            auto pos = line.find(':');
            if (pos != string::npos) {
                string k = line.substr(0, pos);
                string v = line.substr(pos + 1);
                // trim 前後空白（簡單實作）
                auto trim = [](string &s) {
                    size_t a = s.find_first_not_of(" \t");
                    size_t b = s.find_last_not_of(" \t");
                    if (a == string::npos) { s = ""; return; }
                    s = s.substr(a, b - a + 1);
                };
                trim(k); trim(v);
                // 將 header 名稱轉成小寫作為 key（HTTP header 不區分大小寫）
                string lk = k;
                transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
                headers_out[lk] = v;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// build_head_block
// 目的：由 start_line 與 headers map 重建 header block（string）
// 備註：map 的 key 為小寫，輸出也會以小寫 header 名稱（示範用）
// ----------------------------------------------------------------------------
string build_head_block(const string &start_line, const map<string,string> &headers) {
    string out = start_line + "\r\n";
    for (auto &p : headers) {
        out += p.first + ": " + p.second + "\r\n";
    }
    out += "\r\n";
    return out;
}

// ----------------------------------------------------------------------------
// forward_to_upstream
// 目的：把已組好的 request bytes 發送給上游伺服器（upstream_host:upstream_port）
//       並接收上游回應（直到上游關閉連線或沒有更多資料）
// 備註：此函式在 demo 中做同步 blocking I/O，適合本機測試
// ----------------------------------------------------------------------------
string forward_to_upstream(const string &upstream_host, int upstream_port, const vector<char> &request_bytes) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_line("[MITM] socket() failed for upstream");
        return "";
    }

    // 建立上游地址結構
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(upstream_port);
    inet_pton(AF_INET, upstream_host.c_str(), &addr.sin_addr);

    // 連線上游
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_line("[MITM] connect() failed to upstream " + upstream_host + ":" + to_string(upstream_port));
        close(sock);
        return "";
    }

    // 將 request bytes 全部送出（若資料很多可能要 loop）
    size_t sent = 0;
    while (sent < request_bytes.size()) {
        ssize_t n = send(sock, request_bytes.data() + sent, request_bytes.size() - sent, 0);
        if (n <= 0) break;
        sent += n;
    }

    // 接收上游回應（簡單迴圈直到上游關閉）
    vector<char> resp;
    char buf[BUFFER_SIZE];
    while (true) {
        ssize_t r = recv(sock, buf, sizeof(buf), 0);
        if (r <= 0) break;
        resp.insert(resp.end(), buf, buf + r);
        // 若 recv 讀到小於 buffer 的 chunk，那通常表示暫時沒資料；我們小睡一下再繼續讀
        if (r < (ssize_t)sizeof(buf)) {
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
    close(sock);
    return string(resp.begin(), resp.end());
}

// ----------------------------------------------------------------------------
// modify_response_html_and_fix
// 目的：示範如何修改回應的 body（當 Content-Type 包含 text/html）
//       並更新 Content-Length header（若存在）以反映新 body 長度。
// 注意：
//  - 這是簡化實作，並未處理 chunked transfer-encoding 或 gzip/壓縮情況。
//  - 真實情況下需檢查 Content-Encoding，若為壓縮需先解壓再修改，最後再壓縮。
// ----------------------------------------------------------------------------
string modify_response_html_and_fix(const string &raw_response) {
    // 分割 header 與 body
    size_t pos = raw_response.find("\r\n\r\n");
    if (pos == string::npos) return raw_response; // 非標準 response，直接回傳原始

    string head = raw_response.substr(0, pos);
    string body = raw_response.substr(pos + 4);

    // 解析 header 區塊
    string start_line;
    map<string,string> headers;
    parse_headers(head + "\r\n", start_line, headers);

    // 取得 content-type（若有）
    string content_type = "";
    if (headers.count("content-type")) content_type = headers["content-type"];

    // 只在 HTML 回應中注入 banner
    if (content_type.find("text/html") == string::npos) return raw_response;

    // 要插入的 banner（示範用途）
    string injection = "\n<!-- MITM DEMO -->\n<div style='background:#fee;border:1px solid #f00;padding:8px;'>MODIFIED BY MITM DEMO</div>\n";

    // 嘗試在 </body> 前插入（大小寫不敏感）
    string lower_body = body;
    transform(lower_body.begin(), lower_body.end(), lower_body.begin(), ::tolower);
    size_t idx = lower_body.rfind("</body>");
    if (idx != string::npos) {
        body.insert(idx, injection);
    } else {
        // 若找不到 </body>，就 prepend injection（簡化處理）
        body = injection + body;
    }

    // 更新 content-length
    headers["content-length"] = to_string(body.size());

    // 重建 header + body 並回傳
    string new_head = start_line + "\r\n";
    for (auto &h : headers) {
        new_head += h.first + ": " + h.second + "\r\n";
    }
    new_head += "\r\n";
    return new_head + body;
}

// ----------------------------------------------------------------------------
// modify_request_headers_for_demo
// 目的：示範攔截請求 header（例如 Authorization），並將其偽造或替換
// 此處行為：檢查有無 "authorization" header，若有則替換成示範字串
// ----------------------------------------------------------------------------
void modify_request_headers_for_demo(map<string,string> &headers) {
    if (headers.count("authorization")) {
        string orig = headers["authorization"];
        log_line("[MITM] Detected Authorization header: " + orig);
        headers["authorization"] = "Bearer FORGED_BY_MITM_DEMO";
        log_line("[MITM] Replaced Authorization header with forged value.");
    }
}

// ----------------------------------------------------------------------------
// handle_client
// 目的：每個 client 連線進來後由此函式處理（在獨立 thread 中被呼叫）
// 流程：
//  1. 讀取 request header（與可能部份 body）
//  2. 解析並偵測敏感 header（例如 authorization, password）
//  3. 可修改 request headers（示範偽造 Authorization）
//  4. 根據 Content-Length 讀取完整 body（若有）並將完整 request 組起來
//  5. 轉發到 upstream，取得 response
//  6. 修改 response（若為 HTML）並送回 client
// ----------------------------------------------------------------------------
void handle_client(int client_sock, string client_addr, const string upstream_host, int upstream_port) {
    // 先讀 header（與可能同時來的 body 部分）
    string header_block = recv_until_double_crlf(client_sock);
    if (header_block.empty()) {
        close(client_sock);
        return;
    }

    // 將 header 與已讀到的 body 前段分開
    size_t hdr_end = header_block.find("\r\n\r\n");
    string head = header_block.substr(0, hdr_end);
    string remainder = header_block.substr(hdr_end + 4); // 可能為空或為 body 的先行 bytes

    // 解析 start-line 與 headers
    string request_line;
    map<string,string> req_headers;
    parse_headers(head + "\r\n", request_line, req_headers);

    log_line("[MITM] " + client_addr + " -> " + request_line);

    // 簡單偵測是否有可能洩漏憑證（例如 header 中包含 authorization、或 body 有 password）
    string lower_all = head;
    transform(lower_all.begin(), lower_all.end(), lower_all.begin(), ::tolower);
    if (lower_all.find("authorization:") != string::npos || lower_all.find("password") != string::npos) {
        log_line("[MITM] Possible credential-containing header detected.");
    }

    // 對 request header 做示範性修改（例如偽造 Authorization）
    modify_request_headers_for_demo(req_headers);

    // 重建要給 upstream 的 request header 文本
    string req_head_text = request_line + "\r\n";
    for (auto &p : req_headers) req_head_text += p.first + ": " + p.second + "\r\n";
    req_head_text += "\r\n";

    // 如果有 Content-Length，計算尚需讀取的 body 長度
    size_t content_length = 0;
    if (req_headers.count("content-length")) {
        content_length = stoul(req_headers["content-length"]);
    }

    // 建立 request bytes 向上游發送
    vector<char> req_bytes;
    req_bytes.insert(req_bytes.end(), req_head_text.begin(), req_head_text.end());

    // 把已讀到的 remainder（可能是 body 的前段）加入
    if (!remainder.empty()) {
        req_bytes.insert(req_bytes.end(), remainder.begin(), remainder.end());
    }

    // 如果還沒讀完 body，就繼續從 client 讀取剩餘 body
    while (req_bytes.size() < req_head_text.size() + content_length) {
        char buf[BUFFER_SIZE];
        ssize_t r = recv(client_sock, buf, sizeof(buf), 0);
        if (r <= 0) break; // client 提早關閉或錯誤
        req_bytes.insert(req_bytes.end(), buf, buf + r);
    }

    // 把完整 request 轉發到上游，取得上游回應
    string resp = forward_to_upstream(upstream_host, upstream_port, req_bytes);
    if (resp.empty()) {
        log_line("[MITM] Empty response from upstream");
        close(client_sock);
        return;
    }

    // 示範：修改回應（若為 HTML）並修正 Content-Length
    string resp_modified = modify_response_html_and_fix(resp);

    // 傳回給 client
    ssize_t sent = send(client_sock, resp_modified.data(), resp_modified.size(), 0);
    (void)sent; // 忽略回傳值（示範）
    log_line("[MITM] Sent response back to client (" + to_string(resp_modified.size()) + " bytes).");

    close(client_sock);
}

// ----------------------------------------------------------------------------
// main: 監聽迴圈
//  - 解析命令列參數 listen_port, upstream_host, upstream_port（有預設值）
//  - 建立 listening socket，接受 client 連線並為每個 client 建立 thread 處理
// ----------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // 預設參數：監聽 127.0.0.1:8888，轉發到 127.0.0.1:5000
    int listen_port = 8888;
    string upstream_host = "127.0.0.1";
    int upstream_port = 5000;
    if (argc >= 2) listen_port = stoi(argv[1]);
    if (argc >= 3) upstream_host = argv[2];
    if (argc >= 4) upstream_port = stoi(argv[3]);

    // 啟動前清空 log 檔（覆寫）
    ofstream f(LOG_FILE, ios::trunc);
    f.close();

    // 建立 socket
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // bind 到 127.0.0.1:listen_port（只在本機接受連線）
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(listen_port);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv);
        return 1;
    }
    if (listen(srv, 16) < 0) {
        perror("listen");
        close(srv);
        return 1;
    }

    log_line("[MITM] Listening on 127.0.0.1:" + to_string(listen_port)
             + "  -> upstream " + upstream_host + ":" + to_string(upstream_port));

    // 接受循環：每個 client 用一個 thread 處理
    while (true) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int cs = accept(srv, (struct sockaddr*)&cli, &clen);
        if (cs < 0) {
            perror("accept");
            continue;
        }
        // 取得 client IP:port 字串（方便 log）
        char cbuf[64];
        inet_ntop(AF_INET, &cli.sin_addr, cbuf, sizeof(cbuf));
        string client_addr = string(cbuf) + ":" + to_string(ntohs(cli.sin_port));

        // 為每個 client 啟一支 thread 處理
        thread t(handle_client, cs, client_addr, upstream_host, upstream_port);
        t.detach(); // detach：不在主線程等待 thread 結束（demo 模式）
    }

    close(srv);
    return 0;
}


    close(srv);
    return 0;
}
