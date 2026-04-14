#include "dist_cache.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// DistCache CLI — interactive command-line interface
// ─────────────────────────────────────────────────────────────────────────────
//
// Commands:
//   set   <key> <value>   — store a key-value pair
//   get   <key>           — retrieve a value (HIT or MISS)
//   del   <key>           — delete a key
//   flush                 — clear all caches
//   stats                 — print hit/miss/eviction statistics
//   nodes                 — list all nodes and their key counts
//   add   <name>          — add a cache node at runtime
//   remove <name>         — remove a cache node
//   bench [ops] [skew]    — run a benchmark
//   lru   [node]          — show LRU state for a node
//   cap   <n>             — resize cache capacity on all nodes
//   demo                  — populate with sample data
//   help                  — show this message
//   quit / exit           — exit

static const std::string RESET  = "\033[0m";
static const std::string GREEN  = "\033[32m";
static const std::string YELLOW = "\033[33m";
static const std::string RED    = "\033[31m";
static const std::string BLUE   = "\033[36m";
static const std::string BOLD   = "\033[1m";
static const std::string DIM    = "\033[2m";

void printHelp() {
    std::cout << "\n" << BOLD << "  DistCache CLI — commands\n" << RESET;
    std::cout << DIM  << "  ─────────────────────────────────────────────\n" << RESET;
    auto row = [](const char* cmd, const char* desc) {
        std::cout << "  " << BOLD << std::left << std::setw(24) << cmd << RESET
                  << DIM  << desc << RESET << "\n";
    };
    row("set <key> <value>",  "store or update a key");
    row("get <key>",          "retrieve a value (HIT / MISS)");
    row("del <key>",          "delete a key");
    row("flush",              "clear all caches");
    row("stats",              "hit / miss / eviction summary");
    row("nodes",              "list nodes and key counts");
    row("lru [node]",         "show LRU order for a node");
    row("add <name>",         "add a new cache node");
    row("remove <name>",      "remove a cache node");
    row("cap <n>",            "resize per-node capacity");
    row("bench [ops [skew]]", "run benchmark (default: 2000 ops, 0.5 skew)");
    row("demo",               "insert sample data");
    row("help",               "show this message");
    row("quit / exit",        "exit");
    std::cout << "\n";
}

void printStats(const DistCache& dc) {
    int total = dc.totalHits() + dc.totalMisses();
    double hr  = dc.hitRate() * 100.0;
    std::cout << BLUE << "\n  [stats]\n" << RESET;
    std::cout << "  hits      : " << GREEN  << dc.totalHits()      << RESET << "\n";
    std::cout << "  misses    : " << YELLOW << dc.totalMisses()    << RESET << "\n";
    std::cout << "  hit rate  : " << BOLD   << std::fixed << std::setprecision(1) << hr << "%" << RESET << "\n";
    std::cout << "  evictions : " << RED    << dc.totalEvictions() << RESET << "\n";
    std::cout << "  total ops : " << total  << "\n\n";
}

void printNodes(const DistCache& dc) {
    std::cout << BLUE << "\n  [nodes]\n" << RESET;
    for (const auto& [name, info] : dc.ring().nodes()) {
        std::cout << "  " << BOLD << std::left << std::setw(12) << name << RESET
                  << "  keys=" << std::setw(4) << info.keys.size()
                  << "  vnodes=" << 40 << "\n";
    }
    std::cout << "\n";
}

void printLRU(DistCache& dc, const std::string& nodeName) {
    try {
        auto& cache = dc.cache(nodeName);
        auto  items = cache.toVector();
        std::cout << BLUE << "\n  [lru: " << nodeName << "]  "
                  << items.size() << "/" << cache.capacity() << "\n" << RESET;
        for (int i = 0; i < (int)items.size(); ++i) {
            std::string tag = (i == 0) ? "MRU" : (i == (int)items.size()-1 ? "LRU" : "   ");
            std::cout << "  " << DIM << tag << "  " << RESET
                      << BOLD << std::left << std::setw(20) << items[i].first << RESET
                      << "  " << items[i].second << "\n";
        }
        if (items.empty()) std::cout << "  (empty)\n";
        std::cout << "\n";
    } catch (...) {
        std::cout << RED << "  node not found: " << nodeName << RESET << "\n\n";
    }
}

void runDemo(DistCache& dc) {
    std::vector<std::pair<std::string,std::string>> data = {
        {"user:1","alice"},    {"user:2","bob"},       {"user:3","charlie"},
        {"user:4","diana"},    {"sess:tok1","active"}, {"sess:tok2","expired"},
        {"product:101","Widget A"}, {"product:102","Widget B"},
        {"config:ttl","3600"}, {"config:env","production"},
        {"user:1","alice_v2"},{"product:101","Widget A"},
        {"user:5","eve"},      {"sess:tok3","active"}, {"cache:warmup","done"},
        {"user:2","bob"},
    };
    for (const auto& [k, v] : data) {
        auto r = dc.set(k, v);
        std::cout << DIM << "  set " << std::left << std::setw(20) << k
                  << "→ " << r.node;
        if (!r.evicted.empty())
            std::cout << RED << "  [evicted: " << r.evicted << "]" << RESET;
        std::cout << RESET << "\n";
    }
    std::cout << GREEN << "\n  demo done — try: get user:1 | stats\n\n" << RESET;
}


int main() {
    std::cout << BOLD << "\n  DistCache v1.0 — Distributed LRU Cache with Consistent Hashing\n" << RESET;
    std::cout << DIM  << "  Type 'help' for commands.\n\n" << RESET;

    DistCache dc(6, 40);
    dc.addNode("node-a", "#178AD4");
    dc.addNode("node-b", "#0F7060");
    dc.addNode("node-c", "#A34B2D");
    std::cout << GREEN << "  3 nodes online: node-a, node-b, node-c\n\n" << RESET;

    std::string line;
    while (true) {
        std::cout << GREEN << "$ " << RESET;
        if (!std::getline(std::cin, line)) break;

        std::istringstream  iss(line);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);
        if (tokens.empty()) continue;

        const std::string& cmd = tokens[0];

        if (cmd == "set" && tokens.size() >= 3) {
            std::string key = tokens[1];
            std::string val = tokens[2];
            for (size_t i = 3; i < tokens.size(); ++i) val += " " + tokens[i];
            auto r = dc.set(key, val);
            if (r.node.empty()) { std::cout << RED << "  ERR: no nodes available\n\n" << RESET; continue; }
            std::cout << GREEN << "  OK" << RESET << "  → " << r.node;
            if (!r.evicted.empty())
                std::cout << RED << "  [evicted: " << r.evicted << "]" << RESET;
            std::cout << "\n\n";
        }

        else if (cmd == "get" && tokens.size() >= 2) {
            auto r = dc.get(tokens[1]);
            if (r.node.empty()) { std::cout << RED << "  ERR: no nodes available\n\n" << RESET; continue; }
            if (r.hit)
                std::cout << GREEN << "  HIT  " << RESET << "\"" << tokens[1] << "\" = " << r.value << "  (" << r.node << ")\n\n";
            else
                std::cout << YELLOW << "  MISS " << RESET << "\"" << tokens[1] << "\" not found  (" << r.node << ")\n\n";
        }

        else if (cmd == "del" && tokens.size() >= 2) {
            bool ok = dc.del(tokens[1]);
            std::cout << (ok ? GREEN : YELLOW)
                      << "  " << (ok ? "DEL OK  " : "DEL MISS") << RESET
                      << " \"" << tokens[1] << "\"\n\n";
        }

        else if (cmd == "flush") {
            dc.flush();
            std::cout << BLUE << "  all caches flushed\n\n" << RESET;
        }
        else if (cmd == "stats") { printStats(dc); }

        else if (cmd == "nodes") { printNodes(dc); }

        else if (cmd == "lru") {
            if (tokens.size() < 2) {
                for (const auto& [name, _] : dc.ring().nodes()) printLRU(dc, name);
            } else {
                printLRU(dc, tokens[1]);
            }
        }

        else if (cmd == "add" && tokens.size() >= 2) {
            try {
                dc.addNode(tokens[1]);
                std::cout << GREEN << "  node added: " << tokens[1] << RESET << "\n\n";
            } catch (std::exception& e) {
                std::cout << RED << "  ERR: " << e.what() << RESET << "\n\n";
            }
        }

        else if (cmd == "remove" && tokens.size() >= 2) {
            try {
                dc.removeNode(tokens[1]);
                std::cout << YELLOW << "  node removed: " << tokens[1] << RESET << "\n\n";
            } catch (std::exception& e) {
                std::cout << RED << "  ERR: " << e.what() << RESET << "\n\n";
            }
        }

        else if (cmd == "cap" && tokens.size() >= 2) {
            try {
                int cap = std::stoi(tokens[1]);
                if (cap <= 0) throw std::invalid_argument("must be > 0");
                dc.setCapacity(cap);
                std::cout << BLUE << "  capacity set to " << cap << " per node\n\n" << RESET;
            } catch (...) {
                std::cout << RED << "  ERR: invalid capacity\n\n" << RESET;
            }
        }

        else if (cmd == "bench") {
            int    ops  = tokens.size() >= 2 ? std::stoi(tokens[1]) : 2000;
            double skew = tokens.size() >= 3 ? std::stod(tokens[2]) : 0.5;
            std::cout << BLUE << "\n  running benchmark: " << ops << " ops, skew=" << skew << "\n" << RESET;
            auto r = dc.benchmark(ops, 60, skew);
            std::cout << GREEN << "  ops/ms     : " << RESET << std::fixed << std::setprecision(2) << r.opsPerMs     << "\n";
            std::cout << GREEN << "  hit rate   : " << RESET << std::setprecision(1) << r.hitRate * 100 << "%\n";
            std::cout << GREEN << "  avg lat μs : " << RESET << std::setprecision(3) << r.avgLatencyUs   << "\n";
            std::cout << YELLOW<< "  evictions  : " << RESET << r.evictions << "\n\n";
        }

        else if (cmd == "demo") { runDemo(dc); }

        else if (cmd == "help") { printHelp(); }

        else if (cmd == "quit" || cmd == "exit") {
            std::cout << DIM << "  goodbye\n\n" << RESET;
            break;
        }

        else {
            std::cout << RED << "  unknown command — type 'help'\n\n" << RESET;
        }
    }
    return 0;
}
