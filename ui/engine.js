const API_BASE = 'http://localhost:8080/api';

async function apiFetch(method, path, body = null) {
  const opts = { method, headers: { 'Content-Type': 'application/json' } };
  if (body !== null) opts.body = JSON.stringify(body);
  const res = await fetch(API_BASE + path, opts);
  const text = await res.text();
  try { return JSON.parse(text); }
  catch { throw new Error('Invalid JSON: ' + text); }
}

function fnv1a(str) {
  let h = 2166136261;
  for (let i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h;
}

class DistCacheClient {
  constructor() {
    this._stats = { hits: 0, misses: 0, evictions: 0, hitRate: 0 };
    this._nodes = [];
    this._keys = [];
    this._lru = [];
    this._vnodes = 40;
  }

  get totalHits() { return this._stats.hits; }
  get totalMisses() { return this._stats.misses; }
  get totalEvictions() { return this._stats.evictions; }
  get hitRate() { return this._stats.hitRate; }

  async fetchStats() { this._stats = await apiFetch('GET', '/stats'); return this._stats; }
  async fetchNodes() { this._nodes = await apiFetch('GET', '/nodes'); return this._nodes; }
  async fetchKeys() { this._keys = await apiFetch('GET', '/keys'); return this._keys; }
  async fetchLRU(node = '') {
    const p = node ? `/lru?node=${encodeURIComponent(node)}` : '/lru';
    this._lru = await apiFetch('GET', p);
    return this._lru;
  }

  allKeys() { return this._keys; }

  async syncAll() {
    await Promise.all([this.fetchStats(), this.fetchNodes(), this.fetchKeys(), this.fetchLRU()]);
  }

  async get(key) {
    const r = await apiFetch('GET', `/get?key=${encodeURIComponent(key)}`);
    if (r.error) return { hit: false, value: null, node: null };
    await this.fetchStats();
    return r;
  }

  async set(key, value) {
    const r = await apiFetch('POST', '/set', { key, value });
    if (r.error) return { node: null, evicted: null };
    await this.fetchStats();
    return r;
  }

  async del(key) {
    const r = await apiFetch('DELETE', `/del?key=${encodeURIComponent(key)}`);
    if (r.error) return false;
    await this.fetchStats();
    return r.ok;
  }

  async flush() { await apiFetch('POST', '/flush'); await this.syncAll(); }
  async setCapacity(cap) { const r = await apiFetch('POST', '/capacity', { capacity: cap }); if (r.error) throw new Error(r.error); await this.syncAll(); }

  async setVNodes(v) {
    const r = await apiFetch('POST', '/vnodes', { vnodes: v });
    if (r.error) throw new Error(r.error);
    this._vnodes = v;
    await this.syncAll();
    return r;
  }

  async demo() { const r = await apiFetch('POST', '/demo'); await this.syncAll(); return r; }
  async benchmark(ops, keyspace, skew) { const r = await apiFetch('POST', '/benchmark', { ops, keyspace, skew }); await this.syncAll(); return r; }

  async addNode(name, color = '#178AD4') {
    const r = await apiFetch('POST', '/nodes', { name, color });
    if (r.error) throw new Error(r.error);
    await this.syncAll();
    return r;
  }

  async removeNode(name) {
    const r = await apiFetch('DELETE', `/nodes/${encodeURIComponent(name)}`);
    if (r.error) throw new Error(r.error);
    await this.syncAll();
    return r;
  }
  get ringNodes() { const m = new Map(); for (const n of this._nodes) m.set(n.name, n); return m; }
  get ringNodeNames() { return this._nodes.map(n => n.name); }

  get sortedRingPositions() {
    const pos = [];
    for (const n of this._nodes)
      for (let i = 0; i < this._vnodes; i++)
        pos.push({ pos: fnv1a(`${n.name}#${i}`) % 1000, name: n.name });
    pos.sort((a, b) => a.pos - b.pos);
    return pos;
  }

  get ringMap() { const m = new Map(); for (const p of this.sortedRingPositions) m.set(p.pos, p.name); return m; }
  get ringSize() { return this.sortedRingPositions.length; }
  get lruData() { return this._lru; }
}

const dc = new DistCacheClient();
