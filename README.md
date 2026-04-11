# Distributed LRU Cache System

## Overview
This project implements a distributed caching system using:
- LRU (Least Recently Used) eviction policy
- Consistent hashing for load balancing
- Multiple cache server simulation

Inspired by systems like Redis and Memcached.

## Features
- O(1) LRU cache operations
- HashMap + Doubly Linked List
- Consistent hashing ring
- Multi-server simulation
- CLI-based interaction

## Tech Stack
- C++
- STL (unordered_map, map)
