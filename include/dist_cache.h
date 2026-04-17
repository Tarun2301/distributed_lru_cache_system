#pragma once
#include "lru_cache.h"
#include "consistent_hash.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <random>

// DistCache — ties ConsistentHash + per-node LRUCache into one system

struct GetResult {
    bool        hit;
    std::string value;
    std::string node;
};

struct SetResult {
    std::string node;
    std::string evicted;   
};

struct BenchmarkResult {
    int    ops;
    double durationMs;
    double opsPerMs;
    int    hits;
    int    misses;
    double hitRate;       
    int    evictions;
    double avgLatencyUs;
};



class DistCache {
public:
    explicit DistCache(int cacheCapPerNode = 6, int vnodes = 40);

    // Node management
    void addNode(const std::string& name, const std::string& color = "#178AD4");
    void removeNode(const std::string& name);
    void setVNodes(int v);
    int  nodeCount() const;

    // Cache operations
    GetResult get(const std::string& key);
    SetResult set(const std::string& key, const std::string& value);
    bool      del(const std::string& key);
    void      flush();

    // Resize all node caches
    void setCapacity(int cap);
    int  capacity() const { return capPerNode_; }

    // Statistics
    int    totalHits()      const { return hits_; }
    int    totalMisses()    const { return misses_; }
    int    totalEvictions() const { return evictions_; }
    double hitRate()        const;

    
    BenchmarkResult benchmark(int ops, int keyspace = 60, double zipfSkew = 0.5);

    const ConsistentHash& ring()   const { return ring_; }
    LRUCache&             cache(const std::string& nodeName) { return *caches_.at(nodeName); }

private:
    double zipfSample(int n, double s);

    int              capPerNode_;
    ConsistentHash   ring_;
    std::unordered_map<std::string, LRUCache*> caches_;

    int hits_      = 0;
    int misses_    = 0;
    int evictions_ = 0;

    std::mt19937 rng_{ std::random_device{}() };
};
