#pragma once
#include "lru_cache.h"
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// Consistent Hash Ring — FNV-1a hash, virtual nodes, clockwise lookup

struct NodeInfo {
    std::string              name;
    std::string              color;
    std::unordered_set<std::string> keys;  
};

class ConsistentHash {
public:
    explicit ConsistentHash(int vnodes = 40);

    void addNode(const std::string& name, const std::string& color);

    void removeNode(const std::string& name);

    void setVNodes(int n);

    std::string lookup(const std::string& key) const;

    std::string assignKey(const std::string& key);

    int nodeCount() const { return (int)nodes_.size(); }

    const std::unordered_map<std::string, NodeInfo>& nodes() const { return nodes_; }
    NodeInfo& nodeInfo(const std::string& name) { return nodes_.at(name); }

    int ringSize() const { return (int)ring_.size(); }

    const std::map<uint32_t, std::string>& ring() const { return ring_; }

private:
    uint32_t fnv1a(const std::string& s) const;
    void     rebalance();

    int vnodes_;
    std::map<uint32_t, std::string>          ring_;  
    std::unordered_map<std::string, NodeInfo> nodes_; 
};