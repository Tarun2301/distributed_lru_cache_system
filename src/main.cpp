
#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601  
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include "dist_cache.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>

static DistCache   dc(6, 40);
static CRITICAL_SECTION dcCS;

struct CSLock {
    CSLock()  { EnterCriticalSection(&dcCS); }
    ~CSLock() { LeaveCriticalSection(&dcCS); }
};

static std::string jsonStr(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += (char)c;
    }
    out += '"';
    return out;
}
static std::string jsonBool(bool b)  { return b ? "true" : "false"; }
static std::string jsonNum(int n)    { return std::to_string(n); }
static std::string jsonNum(double d) {
    std::ostringstream ss; ss << d; return ss.str();
}

static std::string parseJsonStr(const std::string& body, const std::string& field) {
    std::string needle = "\"" + field + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    while (pos < body.size() && (body[pos]==' '||body[pos]==':'||body[pos]=='\t')) pos++;
    if (pos >= body.size() || body[pos] != '"') return "";
    pos++;
    std::string val;
    while (pos < body.size() && body[pos] != '"') {
        if (body[pos]=='\\' && pos+1 < body.size()) {
            pos++;
            if      (body[pos]=='"')  val+='"';
            else if (body[pos]=='\\') val+='\\';
            else if (body[pos]=='n')  val+='\n';
            else if (body[pos]=='r')  val+='\r';
            else if (body[pos]=='t')  val+='\t';
            else val+=body[pos];
        } else { val+=body[pos]; }
        pos++;
    }
    return val;
}
static double parseJsonNum(const std::string& body, const std::string& field, double def=0) {
    std::string needle = "\"" + field + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return def;
    pos += needle.size();
    while (pos < body.size() && (body[pos]==' '||body[pos]==':'||body[pos]=='\t')) pos++;
    if (pos >= body.size()) return def;
    try { return std::stod(body.substr(pos)); } catch(...) { return def; }
}

//  URL decode 
static std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ) {
        if (s[i]=='%' && i+2 < s.size()) {
            int h = std::stoi(s.substr(i+1,2), nullptr, 16);
            out += (char)h; i += 3;
        } else if (s[i]=='+') { out += ' '; i++; }
        else { out += s[i++]; }
    }
    return out;
}

//  HTTP Request 
struct Req {
    std::string method, path, query, body;
    std::string getParam(const std::string& k) const {
        std::string search = k + "=";
        auto pos = query.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        auto end = query.find('&', pos);
        return urlDecode(end==std::string::npos ? query.substr(pos) : query.substr(pos,end-pos));
    }
    bool hasParam(const std::string& k) const {
        return query.find(k+"=") != std::string::npos || query == k;
    }
    std::string pathSegment(int n) const {
        int seg = 0; size_t i = 0;
        while (i < path.size()) {
            if (path[i]=='/') { i++; continue; }
            if (seg == n) {
                auto end = path.find('/', i);
                return end==std::string::npos ? path.substr(i) : path.substr(i,end-i);
            }
            seg++;
            auto end = path.find('/', i);
            if (end==std::string::npos) break;
            i = end;
        }
        return "";
    }
};

//  HTTP Response sender 
static void sendResponse(SOCKET sock, int status, const std::string& body,
                         const std::string& ct="application/json") {
    std::string statusLine;
    if      (status==200) statusLine="200 OK";
    else if (status==204) statusLine="204 No Content";
    else if (status==400) statusLine="400 Bad Request";
    else if (status==404) statusLine="404 Not Found";
    else if (status==409) statusLine="409 Conflict";
    else if (status==503) statusLine="503 Service Unavailable";
    else                  statusLine=std::to_string(status)+" Error";

    std::ostringstream resp;
    resp << "HTTP/1.1 " << statusLine << "\r\n"
         << "Content-Type: " << ct << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
         << "Access-Control-Allow-Headers: Content-Type\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << body;
    std::string r = resp.str();
    send(sock, r.c_str(), (int)r.size(), 0);
}

static void sendJSON(SOCKET sock, int status, const std::string& body) {
    sendResponse(sock, status, body, "application/json");
}

//  Route handlers 
static std::string handleStats() {
    CSLock lk;
    return "{"
        "\"hits\":"      + jsonNum(dc.totalHits())      + ","
        "\"misses\":"    + jsonNum(dc.totalMisses())    + ","
        "\"evictions\":" + jsonNum(dc.totalEvictions()) + ","
        "\"hitRate\":"   + jsonNum(dc.hitRate())         +
    "}";
}

static std::string handleGetNodes() {
    CSLock lk;
    const auto& nodes = dc.ring().nodes();
    std::string arr = "[";
    bool first = true;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        if (!first) arr += ",";
        first = false;
        arr += "{\"name\":"     + jsonStr(it->first)              + ","
                "\"color\":"    + jsonStr(it->second.color)       + ","
                "\"keyCount\":" + jsonNum((int)it->second.keys.size()) + "}";
    }
    return arr + "]";
}

static std::string handlePostNode(const std::string& body) {
    std::string name  = parseJsonStr(body, "name");
    std::string color = parseJsonStr(body, "color");
    if (color.empty()) color = "#178AD4";
    if (name.empty()) return "";  
    CSLock lk;
    dc.addNode(name, color);
    return "{\"ok\":true,\"name\":" + jsonStr(name) + "}";
}

static std::string handleDeleteNode(const std::string& name) {
    CSLock lk;
    dc.removeNode(name);
    return "{\"ok\":true,\"removed\":" + jsonStr(name) + "}";
}

static std::string handleGetKeys() {
    CSLock lk;
    const auto& nodes = dc.ring().nodes();
    std::string arr = "[";
    bool first = true;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        const std::string& nodeName = it->first;
        try {
            auto& cache = dc.cache(nodeName);
            auto  items = cache.toVector();
            for (size_t i = 0; i < items.size(); ++i) {
                if (!first) arr += ",";
                first = false;
                arr += "{\"key\":" + jsonStr(items[i].first)  +
                       ",\"val\":" + jsonStr(items[i].second) +
                       ",\"node\":" + jsonStr(nodeName) + "}";
            }
        } catch(...) {}
    }
    return arr + "]";
}

static std::string handleGetLRU(const std::string& filterNode) {
    CSLock lk;
    const auto& nodes = dc.ring().nodes();
    std::string result = "[";
    bool firstNode = true;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        const std::string& nodeName = it->first;
        if (!filterNode.empty() && nodeName != filterNode) continue;
        try {
            auto& cache = dc.cache(nodeName);
            auto  items = cache.toVector();
            if (!firstNode) result += ",";
            firstNode = false;
            std::string itemArr = "[";
            bool firstItem = true;
            for (size_t i = 0; i < items.size(); ++i) {
                if (!firstItem) itemArr += ",";
                firstItem = false;
                itemArr += "{\"key\":" + jsonStr(items[i].first) +
                           ",\"val\":" + jsonStr(items[i].second) + "}";
            }
            itemArr += "]";
            result += "{\"node\":"     + jsonStr(nodeName)         +
                      ",\"color\":"    + jsonStr(it->second.color)  +
                      ",\"capacity\":" + jsonNum(cache.capacity())  +
                      ",\"items\":"    + itemArr + "}";
        } catch(...) {}
    }
    return result + "]";
}

static std::string handleGet(const std::string& key) {
    CSLock lk;
    GetResult r = dc.get(key);
    if (r.node.empty()) return "";
    return "{\"hit\":"   + jsonBool(r.hit)  +
           ",\"value\":" + jsonStr(r.value) +
           ",\"node\":"  + jsonStr(r.node)  + "}";
}

static std::string handleSet(const std::string& body) {
    std::string key = parseJsonStr(body, "key");
    std::string val = parseJsonStr(body, "value");
    if (key.empty()) return "";
    CSLock lk;
    SetResult r = dc.set(key, val);
    if (r.node.empty()) return "";
    return "{\"node\":"    + jsonStr(r.node)    +
           ",\"evicted\":" + jsonStr(r.evicted)  + "}";
}

static std::string handleDel(const std::string& key) {
    CSLock lk;
    bool ok = dc.del(key);
    return "{\"ok\":" + jsonBool(ok) + ",\"key\":" + jsonStr(key) + "}";
}

static std::string handleFlush() {
    CSLock lk;
    dc.flush();
    return "{\"ok\":true}";
}

static std::string handleCapacity(const std::string& body) {
    int cap = (int)parseJsonNum(body, "capacity", 0);
    if (cap <= 0) return "";
    CSLock lk;
    dc.setCapacity(cap);
    return "{\"ok\":true,\"capacity\":" + jsonNum(cap) + "}";
}

static std::string handleDemo() {
    std::vector<std::pair<std::string,std::string>> data = {
        {"user:1","alice"},    {"user:2","bob"},       {"user:3","charlie"},
        {"user:4","diana"},    {"sess:tok1","active"}, {"sess:tok2","expired"},
        {"product:101","Widget A"}, {"product:102","Widget B"},
        {"config:ttl","3600"}, {"config:env","production"},
        {"user:1","alice_v2"},{"product:101","Widget A"},
        {"user:5","eve"},      {"sess:tok3","active"}, {"cache:warmup","done"},
        {"user:2","bob"},
    };
    CSLock lk;
    int evictions = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        SetResult r = dc.set(data[i].first, data[i].second);
        if (!r.evicted.empty()) evictions++;
    }
    return "{\"ok\":true,\"inserted\":16,\"evictions\":" + jsonNum(evictions) + "}";
}

static std::string handleBenchmark(const std::string& body) {
    int    ops  = (int)parseJsonNum(body, "ops",      2000);
    int    ks   = (int)parseJsonNum(body, "keyspace", 60);
    double skew =      parseJsonNum(body, "skew",     0.5);
    if (ops<=0) ops=2000; if (ks<=0) ks=60;
    CSLock lk;
    BenchmarkResult r = dc.benchmark(ops, ks, skew);
    return "{\"ops\":"          + jsonNum(r.ops)          +
           ",\"durationMs\":"   + jsonNum(r.durationMs)   +
           ",\"opsPerMs\":"     + jsonNum(r.opsPerMs)     +
           ",\"hits\":"         + jsonNum(r.hits)         +
           ",\"misses\":"       + jsonNum(r.misses)       +
           ",\"hitRate\":"      + jsonNum(r.hitRate)      +
           ",\"evictions\":"    + jsonNum(r.evictions)    +
           ",\"avgLatencyUs\":" + jsonNum(r.avgLatencyUs)  + "}";
}

//  Request parser + dispatcher 
static void handleClient(SOCKET sock) {
    char buf[65536] = {};
    int  received   = recv(sock, buf, sizeof(buf)-1, 0);
    if (received <= 0) { closesocket(sock); return; }

    std::string raw(buf, received);

    // Parse request line
    Req req;
    std::istringstream ss(raw);
    std::string requestLine;
    std::getline(ss, requestLine);
    if (!requestLine.empty() && requestLine.back()=='\r') requestLine.pop_back();

    std::istringstream rl(requestLine);
    std::string fullPath;
    rl >> req.method >> fullPath;

    auto q = fullPath.find('?');
    if (q != std::string::npos) {
        req.path  = fullPath.substr(0, q);
        req.query = fullPath.substr(q+1);
    } else {
        req.path = fullPath;
    }

    // Find body (after blank line)
    auto bodyStart = raw.find("\r\n\r\n");
    if (bodyStart != std::string::npos) req.body = raw.substr(bodyStart+4);

    //  Routing 
    // OPTIONS pre-flight (CORS)
    if (req.method == "OPTIONS") {
        sendJSON(sock, 204, "");
        closesocket(sock); return;
    }

    const std::string& m = req.method;
    const std::string& p = req.path;

    if (m=="GET" && p=="/api/stats") {
        sendJSON(sock, 200, handleStats());
    }
    else if (m=="GET" && p=="/api/nodes") {
        sendJSON(sock, 200, handleGetNodes());
    }
    else if (m=="POST" && p=="/api/nodes") {
        std::string name = parseJsonStr(req.body, "name");
        if (name.empty()) { sendJSON(sock,400,"{\"error\":\"name required\"}"); closesocket(sock); return; }
        try {
            sendJSON(sock, 200, handlePostNode(req.body));
        } catch(std::exception& e) {
            sendJSON(sock, 409, "{\"error\":" + jsonStr(e.what()) + "}");
        }
    }
    else if (m=="DELETE" && p.substr(0,11)=="/api/nodes/") {
        std::string name = urlDecode(p.substr(11));
        try {
            sendJSON(sock, 200, handleDeleteNode(name));
        } catch(std::exception& e) {
            sendJSON(sock, 404, "{\"error\":" + jsonStr(e.what()) + "}");
        }
    }
    else if (m=="GET" && p=="/api/keys") {
        sendJSON(sock, 200, handleGetKeys());
    }
    else if (m=="GET" && p=="/api/lru") {
        std::string node = req.getParam("node");
        sendJSON(sock, 200, handleGetLRU(node));
    }
    else if (m=="GET" && p=="/api/get") {
        std::string key = req.getParam("key");
        if (key.empty()) { sendJSON(sock,400,"{\"error\":\"key required\"}"); closesocket(sock); return; }
        std::string r = handleGet(key);
        if (r.empty()) sendJSON(sock,503,"{\"error\":\"no nodes available\"}");
        else sendJSON(sock,200,r);
    }
    else if (m=="POST" && p=="/api/set") {
        std::string key = parseJsonStr(req.body,"key");
        if (key.empty()) { sendJSON(sock,400,"{\"error\":\"key required\"}"); closesocket(sock); return; }
        std::string r = handleSet(req.body);
        if (r.empty()) sendJSON(sock,503,"{\"error\":\"no nodes available\"}");
        else sendJSON(sock,200,r);
    }
    else if (m=="DELETE" && p=="/api/del") {
        std::string key = req.getParam("key");
        if (key.empty()) { sendJSON(sock,400,"{\"error\":\"key required\"}"); closesocket(sock); return; }
        sendJSON(sock, 200, handleDel(key));
    }
    else if (m=="POST" && p=="/api/flush") {
        sendJSON(sock, 200, handleFlush());
    }
    else if (m=="POST" && p=="/api/capacity") {
        std::string r = handleCapacity(req.body);
        if (r.empty()) sendJSON(sock,400,"{\"error\":\"capacity must be > 0\"}");
        else sendJSON(sock,200,r);
    }
    else if (m=="POST" && p=="/api/demo") {
        sendJSON(sock, 200, handleDemo());
    }
    else if (m=="POST" && p=="/api/vnodes") {
        int v = (int)parseJsonNum(req.body, "vnodes", 0);
        if (v < 1) { 
            sendJSON(sock, 400, "{\"error\":\"vnodes must be >= 1\"}"); 
        } else {
            CSLock lk;
            dc.setVNodes(v);
            sendJSON(sock, 200, "{\"ok\":true,\"vnodes\":" + jsonNum(v) + "}");
        }
    }
    else if (m=="POST" && p=="/api/benchmark") {
        sendJSON(sock, 200, handleBenchmark(req.body));
    }
    else {
        sendJSON(sock, 404, "{\"error\":\"not found\"}");
    }

    closesocket(sock);
}

//  Thread entry point (__stdcall required by CreateThread on Windows) 
static DWORD WINAPI clientThreadProc(LPVOID arg) {
    SOCKET s = (SOCKET)(uintptr_t)arg;
    handleClient(s);
    return 0;
}

//   Main 
int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket() failed\n"; WSACleanup(); return 1;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(8080);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed — is port 8080 in use?\n";
        closesocket(listenSock); WSACleanup(); return 1;
    }
    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed\n";
        closesocket(listenSock); WSACleanup(); return 1;
    }

    InitializeCriticalSection(&dcCS);

    dc.addNode("node-a", "#178AD4");
    dc.addNode("node-b", "#0F7060");
    dc.addNode("node-c", "#A34B2D");

    std::cout << "\n  DistCache HTTP Server v1.0\n";
    std::cout << "  Listening on http://localhost:8080\n";
    std::cout << "  3 nodes online: node-a, node-b, node-c\n";
    std::cout << "  Press Ctrl+C to stop.\n\n";

    while (true) {
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;
        HANDLE hThread = CreateThread(NULL, 0, clientThreadProc,
                                      (LPVOID)(uintptr_t)clientSock, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}
