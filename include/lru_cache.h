#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <stdexcept>


// LRU Cache — O(1) get and set using HashMap + Doubly Linked List


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

    std::string get(const std::string& key);

    std::string set(const std::string& key, const std::string& value);

    bool del(const std::string& key);

    void flush();

    void resize(int newCap);

    int  size()       const { return (int)map_.size(); }
    int  capacity()   const { return capacity_; }
    int  evictions()  const { return evictions_; }

    std::vector<std::pair<std::string,std::string>> toVector() const;

private:
    void removeNode(LRUNode* node);
    void insertAfterHead(LRUNode* node);

    int  capacity_;
    int  evictions_;
    std::unordered_map<std::string, LRUNode*> map_;
    LRUNode* head_;  
    LRUNode* tail_;   
};
