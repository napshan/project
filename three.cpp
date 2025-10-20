// mitm_http_proxy.cpp
// Compile: g++ mitm_http_proxy.cpp -o mitm_http_proxy -std=c++17 -pthread
// Usage: ./mitm_http_proxy [listen_port] [upstream_host] [upstream_port]
// Example: ./mitm_http_proxy 8888 127.0.0.1 5000

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

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

static const int BUFFER_SIZE = 8192;
static const string LOG_FILE = "mitm_log.txt";

// Helper: append to log file + stdout
void log_line(const string &s) {
    cout << s << endl;
    ofstream f(LOG_FILE, ios::app);
    if (f.is_open()) {
        f << s << "\n";
        f.close();
    }
}

// Read exactly until "\r\n\r\n" or timeout; returns full header bytes (may contain part of body)
string recv_until_double_crlf(int sock) {
    string acc;
    char buf[BUFFER_SIZE];
    fd_set readfds;
    struct timeval tv;
    // we'll just loop with blocking recv and break when header end found
    while (true) {
        ssize_t r = recv(sock, buf, sizeof(buf), 0);
        if (r <= 0) break;
        acc.append(buf, buf + r);
        if (acc.find("\r\n\r\n") != string::npos) break;
        // note: for demo this simple loop is fine
    }
    return acc;
}

// Parse request/response start-line + headers into components
void parse_headers(const string &head_block, string &start_line, map<string,string> &headers_out) {
    headers_out.clear();
    stringstream ss(head_block);
    string line;
    bool first = true;
    while (getline(ss, line)) {
        // strip possible trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first) {
            start_line = line;
            first = false;
        } else {
            auto pos = line.find(':');
            if (pos != string::npos) {
                string k = line.substr(0, pos);
                string v = line.substr(pos+1);
                // trim
                auto trim = [](string &s) {
                    size_t a = s.find_first_not_of(" \t");
                    size_t b = s.find_last_not_of(" \t");
                    if (a == string::npos) { s = ""; return; }
                    s = s.substr(a, b - a + 1);
                };
                trim(k); trim(v);
                // normalize header name to canonical form (case-insensitive via lowercase key)
                string lk = k;
                transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
                headers_out[lk] = v;
            }
        }
    }
}

// Rebuild header block from start_line + headers map
string build_head_block(const string &start_line, const map<string,string> &headers) {
    string out = start_line + "\r\n";
    for (auto &p : headers) {
        // write header name as-is from map key (lowercase)
        out += p.first + ": " + p.second + "\r\n";
    }
    out += "\r\n";
    return out;
}

// Forward request_bytes to upstream and get full response bytes
string forward_to_upstream(const string &upstream_host, int upstream_port, const vector<char> &request_bytes) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_line("[MITM] socket() failed for upstream");
        return "";
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(upstream_port);
    inet_pton(AF_INET, upstream_host.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_line("[MITM] connect() failed to upstream " + upstream_host + ":" + to_string(upstream_port));
        close(sock);
        return "";
    }

    // send all bytes
    size_t sent = 0;
    while (sent < request_bytes.size()) {
        ssize_t n = send(sock, request_bytes.data() + sent, request_bytes.size() - sent, 0);
        if (n <= 0) break;
        sent += n;
    }

    // receive response until socket closed or small chunk
    vector<char> resp;
    char buf[BUFFER_SIZE];
    while (true) {
        ssize_t r = recv(sock, buf, sizeof(buf), 0);
        if (r <= 0) break;
        resp.insert(resp.end(), buf, buf + r);
        // heuristic: if r < BUF, might be end; but continue until close for demo
        if (r < (ssize_t)sizeof(buf)) {
            // continue to read a bit, then break if no data
            // small sleep to allow remaining bytes
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
    close(sock);
    return string(resp.begin(), resp.end());
}

// Inject banner into HTML body and fix Content-Length header
string modify_response_html_and_fix(const string &raw_response) {
    // split header and body
    size_t pos = raw_response.find("\r\n\r\n");
    if (pos == string::npos) return raw_response;
    string head = raw_response.substr(0, pos);
    string body = raw_response.substr(pos + 4);

    // parse headers
    string start_line;
    map<string,string> headers;
    parse_headers(head + "\r\n", start_line, headers);

    string content_type = "";
    if (headers.count("content-type")) content_type = headers["content-type"];
    // only modify HTML
    if (content_type.find("text/html") == string::npos) return raw_response;

    // injection HTML banner before </body>
    string injection = "\n<!-- MITM DEMO -->\n<div style='background:#fee;border:1px solid #f00;padding:8px;'>MODIFIED BY MITM DEMO</div>\n";
    string lower_body = body;
    transform(lower_body.begin(), lower_body.end(), lower_body.begin(), ::tolower);
    size_t idx = lower_body.rfind("</body>");
    if (idx != string::npos) {
        body.insert(idx, injection);
    } else {
        body = injection + body;
    }

    // update content-length header if present
    headers["content-length"] = to_string(body.size());

    // rebuild response bytes
    string new_head = start_line + "\r\n";
    for (auto &h : headers) {
        new_head += h.first + ": " + h.second + "\r\n";
    }
    new_head += "\r\n";
    return new_head + body;
}

// Modify request headers (detect Authorization and replace)
void modify_request_headers_for_demo(map<string,string> &headers) {
    if (headers.count("authorization")) {
        string orig = headers["authorization"];
        log_line("[MITM] Detected Authorization header: " + orig);
        headers["authorization"] = "Bearer FORGED_BY_MITM_DEMO";
        log_line("[MITM] Replaced Authorization header with forged value.");
    }
}

// Per-client handler thread
void handle_client(int client_sock, string client_addr, const string upstream_host, int upstream_port) {
    // Receive request header+maybe part body
    string header_block = recv_until_double_crlf(client_sock);
    if (header_block.empty()) {
        close(client_sock);
        return;
    }

    // Separate head and possible body bytes already read
    size_t hdr_end = header_block.find("\r\n\r\n");
    string head = header_block.substr(0, hdr_end);
    string remainder = header_block.substr(hdr_end + 4); // may be empty or start of body

    // parse request start-line and headers
    string request_line;
    map<string,string> req_headers;
    parse_headers(head + "\r\n", request_line, req_headers);

    log_line("[MITM] " + client_addr + " -> " + request_line);

    // Detect possible credential leakage (simple)
    string lower_all = head;
    transform(lower_all.begin(), lower_all.end(), lower_all.begin(), ::tolower);
    if (lower_all.find("authorization:") != string::npos || lower_all.find("password") != string::npos) {
        log_line("[MITM] Possible credential-containing header detected.");
    }

    // Modify request headers for demo
    modify_request_headers_for_demo(req_headers);

    // Rebuild request bytes to send upstream
    string req_head_text = request_line + "\r\n";
    for (auto &p : req_headers) req_head_text += p.first + ": " + p.second + "\r\n";
    req_head_text += "\r\n";

    // Determine if we need to read more body bytes (Content-Length)
    size_t content_length = 0;
    if (req_headers.count("content-length")) {
        content_length = stoul(req_headers["content-length"]);
    }

    vector<char> req_bytes;
    req_bytes.insert(req_bytes.end(), req_head_text.begin(), req_head_text.end());
    // append already-read remainder of body
    if (!remainder.empty()) {
        req_bytes.insert(req_bytes.end(), remainder.begin(), remainder.end());
    }
    // if more body to read, read the rest
    while (req_bytes.size() < req_head_text.size() + content_length) {
        char buf[BUFFER_SIZE];
        ssize_t r = recv(client_sock, buf, sizeof(buf), 0);
        if (r <= 0) break;
        req_bytes.insert(req_bytes.end(), buf, buf + r);
    }

    // Forward to upstream and get response
    string resp = forward_to_upstream(upstream_host, upstream_port, req_bytes);
    if (resp.empty()) {
        log_line("[MITM] Empty response from upstream");
        close(client_sock);
        return;
    }

    // Try to modify response (inject banner if html)
    string resp_modified = modify_response_html_and_fix(resp);

    // Send back to client
    ssize_t sent = send(client_sock, resp_modified.data(), resp_modified.size(), 0);
    (void)sent;
    log_line("[MITM] Sent response back to client (" + to_string(resp_modified.size()) + " bytes).");

    close(client_sock);
}

// main: listen loop
int main(int argc, char* argv[]) {
    int listen_port = 8888;
    string upstream_host = "127.0.0.1";
    int upstream_port = 5000;
    if (argc >= 2) listen_port = stoi(argv[1]);
    if (argc >= 3) upstream_host = argv[2];
    if (argc >= 4) upstream_port = stoi(argv[3]);

    // clear log
    ofstream f(LOG_FILE, ios::trunc);
    f.close();

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

    while (true) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int cs = accept(srv, (struct sockaddr*)&cli, &clen);
        if (cs < 0) {
            perror("accept");
            continue;
        }
        char cbuf[64];
        inet_ntop(AF_INET, &cli.sin_addr, cbuf, sizeof(cbuf));
        string client_addr = string(cbuf) + ":" + to_string(ntohs(cli.sin_port));
        // handle each client in a new thread
        thread t(handle_client, cs, client_addr, upstream_host, upstream_port);
        t.detach();
    }

    close(srv);
    return 0;
}
