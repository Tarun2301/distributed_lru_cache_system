#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// LRU Cache — O(1) get and set using HashMap + Doubly Linked List
// ─────────────────────────────────────────────────────────────────────────────

struct LRUNode {
    std::string key;
    std::string value;
    LRUNode*    prev;
    LRUNode*    next;

    LRUNode(const std::string& k, const std::string& v)
        : key(k), value(v), prev(nullptr), next(nullptr) {}
};

class LRUCache {
public:
    explicit LRUCache(int capacity);
    ~LRUCache();

    // Returns the value for key, or "" if not found. Marks key as MRU.
    std::string get(const std::string& key);

    // Inserts or updates key. Returns evicted key ("" if none).
    std::string set(const std::string& key, const std::string& value);

    // Removes a key. Returns true if it existed.
    bool del(const std::string& key);

    // Remove all entries.
    void flush();

    // Resize capacity. Evicts LRU entries if needed.
    void resize(int newCap);

    int  size()       const { return (int)map_.size(); }
    int  capacity()   const { return capacity_; }
    int  evictions()  const { return evictions_; }

    // Returns keys from MRU to LRU order.
    std::vector<std::pair<std::string,std::string>> toVector() const;

private:
    void removeNode(LRUNode* node);
    void insertAfterHead(LRUNode* node);

    int  capacity_;
    int  evictions_;
    std::unordered_map<std::string, LRUNode*> map_;
    LRUNode* head_;   // sentinel — most recently used side
    LRUNode* tail_;   // sentinel — least recently used side
};
