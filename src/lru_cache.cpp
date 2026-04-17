#include "lru_cache.h"
#include <stdexcept>

//  Constructor / Destructor 

LRUCache::LRUCache(int capacity)
    : capacity_(capacity), evictions_(0)
{
    if (capacity <= 0)
        throw std::invalid_argument("LRUCache capacity must be > 0");

    
    head_ = new LRUNode("__HEAD__", "");
    tail_ = new LRUNode("__TAIL__", "");
    head_->next = tail_;
    tail_->prev = head_;
}

LRUCache::~LRUCache() {
    flush();
    delete head_;
    delete tail_;
}

//  Private helpers 

void LRUCache::removeNode(LRUNode* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

void LRUCache::insertAfterHead(LRUNode* node) {
    node->next       = head_->next;
    node->prev       = head_;
    head_->next->prev = node;
    head_->next      = node;
}

//  Public API 

std::string LRUCache::get(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return "";          // cache miss

    LRUNode* node = it->second;
    removeNode(node);
    insertAfterHead(node);                    // mark as most recently used
    return node->value;
}

std::string LRUCache::set(const std::string& key, const std::string& value) {
    std::string evicted = "";

    auto it = map_.find(key);
    if (it != map_.end()) {
        // Key already exists: update value and move to head
        LRUNode* node = it->second;
        node->value   = value;
        removeNode(node);
        insertAfterHead(node);
    } else {
        // New key: evict LRU if at capacity
        if ((int)map_.size() >= capacity_) {
            LRUNode* lru = tail_->prev;      
            evicted      = lru->key;
            removeNode(lru);
            map_.erase(lru->key);
            delete lru;
            evictions_++;
        }
        LRUNode* node = new LRUNode(key, value);
        map_[key]     = node;
        insertAfterHead(node);
    }
    return evicted;
}

bool LRUCache::del(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return false;

    removeNode(it->second);
    delete it->second;
    map_.erase(it);
    return true;
}

void LRUCache::flush() {
    LRUNode* cur = head_->next;
    while (cur != tail_) {
        LRUNode* next = cur->next;
        delete cur;
        cur = next;
    }
    map_.clear();
    head_->next = tail_;
    tail_->prev = head_;
}

void LRUCache::resize(int newCap) {
    capacity_ = newCap;
    while ((int)map_.size() > capacity_) {
        LRUNode* lru = tail_->prev;
        map_.erase(lru->key);
        removeNode(lru);
        delete lru;
        evictions_++;
    }
}

std::vector<std::pair<std::string,std::string>> LRUCache::toVector() const {
    std::vector<std::pair<std::string,std::string>> result;
    LRUNode* cur = head_->next;
    while (cur != tail_) {
        result.emplace_back(cur->key, cur->value);
        cur = cur->next;
    }
    return result;
}
