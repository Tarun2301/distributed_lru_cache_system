#include "dist_cache.h"
#include <cmath>
#include <stdexcept>

// ─── Constructor / Destructor ────────────────────────────────────────────────

DistCache::DistCache(int capPerNode, int vnodes)
    : capPerNode_(capPerNode), ring_(vnodes) {}

// ─── Node management ─────────────────────────────────────────────────────────

void DistCache::addNode(const std::string& name, const std::string& color) {
    if (caches_.count(name))
        throw std::invalid_argument("Node already exists: " + name);

    caches_[name] = new LRUCache(capPerNode_);
    ring_.addNode(name, color);
}

void DistCache::removeNode(const std::string& name) {
    ring_.removeNode(name);
    delete caches_.at(name);
    caches_.erase(name);
}

int DistCache::nodeCount() const { return (int)caches_.size(); }

// ─── Cache operations ────────────────────────────────────────────────────────

GetResult DistCache::get(const std::string& key) {
    std::string nodeName = ring_.lookup(key);
    if (nodeName.empty() || !caches_.count(nodeName))
        return { false, "", "" };

    std::string val = caches_[nodeName]->get(key);
    if (!val.empty()) {
        hits_++;
        ring_.nodeInfo(nodeName).keys.insert(key);
        return { true, val, nodeName };
    }
    misses_++;
    return { false, "", nodeName };
}

SetResult DistCache::set(const std::string& key, const std::string& value) {
    std::string nodeName = ring_.assignKey(key);
    if (nodeName.empty() || !caches_.count(nodeName))
        return { "", "" };

    std::string evicted = caches_[nodeName]->set(key, value);
    if (!evicted.empty()) evictions_++;
    return { nodeName, evicted };
}

bool DistCache::del(const std::string& key) {
    std::string nodeName = ring_.lookup(key);
    if (nodeName.empty() || !caches_.count(nodeName)) return false;
    ring_.nodeInfo(nodeName).keys.erase(key);
    return caches_[nodeName]->del(key);
}

void DistCache::flush() {
    for (auto& [name, cache] : caches_) cache->flush();
    for (auto& [name, info]  : ring_.nodes())
        const_cast<NodeInfo&>(info).keys.clear();
    hits_ = misses_ = evictions_ = 0;
}

void DistCache::setCapacity(int cap) {
    capPerNode_ = cap;
    for (auto& [name, cache] : caches_) cache->resize(cap);
}

double DistCache::hitRate() const {
    int total = hits_ + misses_;
    return total > 0 ? (double)hits_ / total : 0.0;
}

// ─── Zipf sampling ───────────────────────────────────────────────────────────
// Returns a rank in [1..n] with probability proportional to 1/rank^s.
// s=0 → uniform; s=1 → classic Zipf (hot-key heavy).

double DistCache::zipfSample(int n, double s) {
    if (s == 0.0) {
        std::uniform_int_distribution<int> dist(1, n);
        return dist(rng_);
    }
    double denom = 0.0;
    for (int k = 1; k <= n; ++k) denom += 1.0 / std::pow(k, s);

    std::uniform_real_distribution<double> u(0.0, 1.0);
    double r   = u(rng_);
    double cum = 0.0;
    for (int k = 1; k <= n; ++k) {
        cum += (1.0 / std::pow(k, s)) / denom;
        if (r <= cum) return k;
    }
    return n;
}

// ─── Benchmark ───────────────────────────────────────────────────────────────

BenchmarkResult DistCache::benchmark(int ops, int keyspace, double zipfSkew) {
    // Flush for a clean measurement
    flush();

    int    bHits = 0, bMisses = 0, bEvict = 0;
    double totalLatUs = 0.0;

    std::uniform_real_distribution<double> coin(0.0, 1.0);
    auto t0 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ops; ++i) {
        std::string key = "k" + std::to_string((int)zipfSample(keyspace, zipfSkew));

        auto lt0 = std::chrono::high_resolution_clock::now();

        if (coin(rng_) < 0.65) {
            // Read-heavy: 65% get, 35% set
            auto r = get(key);
            if (r.hit) bHits++;
            else { bMisses++; set(key, "val-" + key); }
        } else {
            auto r = set(key, "val-" + key);
            if (!r.evicted.empty()) bEvict++;
        }

        auto lt1 = std::chrono::high_resolution_clock::now();
        totalLatUs += std::chrono::duration<double, std::micro>(lt1 - lt0).count();
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    int total = bHits + bMisses;
    return {
        ops,
        ms,
        ops / ms,
        bHits,
        bMisses,
        total > 0 ? (double)bHits / total : 0.0,
        bEvict,
        totalLatUs / ops
    };
}
