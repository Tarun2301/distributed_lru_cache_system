# Distributed LRU Cache

A distributed cache system with LRU eviction and consistent hashing.

## Overview

This project implements a distributed cache system using:
- **LRU Cache** - evicts least recently used items when capacity is full
- **Consistent Hashing** - distributes keys across multiple virtual nodes using hash ring
- **HTTP REST API** - socket-based API on port 8080 for cache operations
- **Web UI** - dashboard to monitor and manage the cache

## Structure

```
src/          - C++ implementation
include/      - Header files
ui/           - Web dashboard
build.bat     - Build script
start.bat     - Start script
```

## Build & Run (For windows)

```bash  
build.bat    
start.bat   
```


## Key Concepts

### Virtual Nodes
Consistent hashing uses virtual nodes to distribute data evenly across cache nodes. Multiple virtual nodes (replicas) are created for each physical node on the hash ring, reducing data redistribution when nodes are added/removed.

### LRU Eviction
When a cache reaches capacity, the Least Recently Used item is evicted automatically. Each access updates the "recently used" status.

### Consistent Hashing
Keys are mapped to nodes using a hash ring. This ensures minimal data movement when the cache cluster changes size.


## Code Components

- **DistCache** - Main distributed cache, uses consistent hashing to route keys to cache nodes
- **LRUCache** - Individual cache on each node, handles eviction when full
- **ConsistentHash** - Hash ring algorithm for key-to-node mapping
