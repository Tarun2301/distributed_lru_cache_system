const NODE_COLORS = ['#178AD4', '#0F7060', '#A34B2D', '#7c3aed', '#0891b2', '#d97706', '#be185d'];
let selectedLRUNode = null;
let light = false;

function $(id) { return document.getElementById(id); }

function showToast(msg, type = 'blue', duration = 2500) {
  const t = $('toast');
  t.textContent = msg;
  t.className = `toast show toast-${type}`;
  setTimeout(() => { t.className = 'toast'; }, duration);
}

function escHtml(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

//  Stats 
function refreshStats() {
  $('statHits').textContent = dc.totalHits;
  $('statMisses').textContent = dc.totalMisses;
  $('statEvictions').textContent = dc.totalEvictions;
  const hr = dc.hitRate;
  $('statHitRate').textContent = hr > 0 ? (hr * 100).toFixed(1) + '%' : '—';
  $('hitRateBar').style.width = (hr * 100) + '%';
  $('hitRateLabel').textContent = hr > 0 ? (hr * 100).toFixed(1) + '%' : '0%';
}

//  Nodes 
function getNodeColor(name) {
  const n = dc.ringNodes.get(name);
  return n ? n.color : '#888';
}

function refreshNodes() {
  const list = $('nodeList');
  list.innerHTML = '';
  const nodeNames = dc.ringNodeNames;
  $('nodeCountBadge').textContent = nodeNames.length;
  $('statusText').textContent = `${nodeNames.length} node${nodeNames.length !== 1 ? 's' : ''} online`;

  nodeNames.forEach(name => {
    const info = dc.ringNodes.get(name);
    const item = document.createElement('div');
    item.className = 'node-item';
    item.innerHTML = `
      <span class="node-dot" style="background:${info.color}"></span>
      <span class="node-name">${name}</span>
      <span class="node-keys">${info.keyCount} key${info.keyCount !== 1 ? 's' : ''}</span>
      <button class="node-remove" title="Remove node" data-name="${name}">✕</button>
    `;
    list.appendChild(item);
  });

  list.querySelectorAll('.node-remove').forEach(btn => {
    btn.addEventListener('click', async () => {
      const name = btn.dataset.name;
      try {
        await dc.removeNode(name);
        showToast(`Node "${name}" removed`, 'blue');
        refreshAll();
      } catch (e) { showToast(e.message, 'red'); }
    });
  });

  refreshLRUNodeBtns();
}

//  Key Table 
function refreshKeyTable() {
  const wrap = $('keyTable');
  const keys = dc.allKeys();
  if (keys.length === 0) {
    wrap.innerHTML = '<div class="empty-state">No keys. Run a SET or DEMO first.</div>';
    return;
  }
  let html = `<table class="key-table"><thead><tr><th>#</th><th>Key</th><th>Value</th><th>Node</th><th></th></tr></thead><tbody>`;
  keys.forEach((item, i) => {
    const color = getNodeColor(item.node);
    html += `<tr>
      <td style="color:var(--txt3)">${i + 1}</td>
      <td><strong>${escHtml(item.key)}</strong></td>
      <td>${escHtml(item.val)}</td>
      <td class="node-cell"><span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:${color};margin-right:6px"></span>${escHtml(item.node)}</td>
      <td><button class="del-key-btn" data-key="${escHtml(item.key)}" title="Delete">🗑</button></td>
    </tr>`;
  });
  html += `</tbody></table>`;
  wrap.innerHTML = html;

  wrap.querySelectorAll('.del-key-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
      await dc.del(btn.dataset.key);
      showToast(`Deleted "${btn.dataset.key}"`, 'blue');
      await dc.syncAll();
      refreshAll();
    });
  });
}

function refreshAll() {
  refreshStats();
  refreshNodes();
  if (document.querySelector('#tab-ring.active')) drawRing();
  if (document.querySelector('#tab-lru.active')) drawLRU();
  refreshKeyTable();
}

function setResult(html) {
  $('opResult').innerHTML = `<div class="result-row">${html}</div>`;
}

//  Tabs 
document.querySelectorAll('.tab').forEach(tab => {
  tab.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    tab.classList.add('active');
    const target = $('tab-' + tab.dataset.tab);
    target.classList.add('active');
    if (tab.dataset.tab === 'ring') drawRing();
    if (tab.dataset.tab === 'lru') { dc.fetchLRU().then(() => drawLRU()); }
  });
});

//  SET 
$('setBtn').addEventListener('click', async () => {
  const key = $('setKey').value.trim();
  const val = $('setValue').value.trim();
  if (!key || !val) { showToast('Key and value required', 'red'); return; }
  const r = await dc.set(key, val);
  if (!r.node) { setResult(`<span class="result-tag tag-err">ERR</span><span>No nodes available</span>`); return; }
  let html = `<span class="result-tag tag-ok">OK</span>
              <span class="result-key">${escHtml(key)}</span>
              <span style="color:var(--txt3)">→</span>
              <span class="result-node">${r.node}</span>`;
  if (r.evicted) html += `<span class="result-tag tag-evict">EVICTED: ${escHtml(r.evicted)}</span>`;
  setResult(html);
  $('setKey').value = ''; $('setValue').value = '';
  showToast(`SET ${key} → ${r.node}`, 'blue');
  await dc.syncAll();
  refreshAll();
  animateKey(key, 'set');
});

// GET 
$('getBtn').addEventListener('click', async () => {
  const key = $('getKey').value.trim();
  if (!key) { showToast('Key required', 'red'); return; }
  const r = await dc.get(key);
  if (!r.node) { setResult(`<span class="result-tag tag-err">ERR</span><span>No nodes available</span>`); return; }
  if (r.hit) {
    setResult(`<span class="result-tag tag-hit">HIT</span>
               <span class="result-key">"${escHtml(key)}"</span>
               <span>=</span>
               <span class="result-val">${escHtml(r.value)}</span>
               <span class="result-node">(${r.node})</span>`);
    showToast(`HIT: ${key} = ${r.value}`, 'blue');
    animateKey(key, 'get');
  } else {
    setResult(`<span class="result-tag tag-miss">MISS</span>
               <span class="result-key">"${escHtml(key)}"</span>
               <span class="result-node">not found  (${r.node})</span>`);
    showToast(`MISS: ${key} not found`, 'blue');
  }
  $('getKey').value = '';
  refreshStats();
});

// DEL 
$('delBtn').addEventListener('click', async () => {
  const key = $('delKey').value.trim();
  if (!key) { showToast('Key required', 'red'); return; }
  const ok = await dc.del(key);
  setResult(`<span class="result-tag ${ok ? 'tag-del' : 'tag-miss'}">${ok ? 'DELETED' : 'MISS'}</span>
             <span class="result-key">"${escHtml(key)}"</span>`);
  showToast(ok ? `Deleted "${key}"` : `Key "${key}" not found`, ok ? 'blue' : 'red');
  $('delKey').value = '';
  await dc.syncAll();
  refreshAll();
});

// FLUSH 
$('flushBtn').addEventListener('click', async () => {
  await dc.flush();
  setResult(`<span class="result-tag tag-ok">FLUSH</span><span>All caches cleared, stats reset</span>`);
  showToast('All caches flushed', 'blue');
  refreshAll();
});

//  DEMO 
$('demoBtn').addEventListener('click', async () => {
  const r = await dc.demo();
  setResult(`<span class="result-tag tag-ok">DEMO</span>
             <span>16 keys inserted${r.evictions > 0 ? `, ${r.evictions} eviction${r.evictions > 1 ? 's' : ''}` : ''}</span>
             <span class="result-node">— try: get user:1 | stats</span>`);
  showToast('Demo data loaded!', 'blue');
  refreshAll();
});

// Add Node 
$('addNodeBtn').addEventListener('click', async () => {
  const name = $('addNodeInput').value.trim();
  if (!name) { showToast('Node name required', 'red'); return; }
  const colorIdx = dc.ringNodeNames.length % NODE_COLORS.length;
  try {
    await dc.addNode(name, NODE_COLORS[colorIdx]);
    showToast(`Node "${name}" added`, 'blue');
    $('addNodeInput').value = '';
    refreshAll();
    if (document.querySelector('[data-tab="ring"].active')) drawRing();
  } catch (e) { showToast(e.message, 'red'); }
});
$('addNodeInput').addEventListener('keydown', e => { if (e.key === 'Enter') $('addNodeBtn').click(); });

//  Capacity 
$('capBtn').addEventListener('click', async () => {
  const cap = parseInt($('capInput').value);
  if (!cap || cap <= 0) { showToast('Capacity must be > 0', 'red'); return; }
  await dc.setCapacity(cap);
  showToast(`Capacity set to ${cap} per node`, 'blue');
  refreshAll();
});

//  Refresh Keys 
$('refreshKeysBtn').addEventListener('click', async () => {
  await dc.fetchKeys();
  refreshKeyTable();
  showToast('Key table refreshed', 'blue');
});

// Ring Canvas 
function drawRing() {
  const canvas = $('ringCanvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height, cx = W / 2, cy = H / 2;
  const R = Math.min(W, H) / 2 - 40;
  const RING_SCALE = 1000;

  ctx.clearRect(0, 0, W, H);
  ctx.beginPath(); ctx.arc(cx, cy, R, 0, Math.PI * 2);
  ctx.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue('--border') || '#2e3655';
  ctx.lineWidth = 2; ctx.stroke();

  const grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, R);
  grad.addColorStop(0, 'rgba(30,36,64,0.6)'); grad.addColorStop(1, 'rgba(13,15,26,0)');
  ctx.beginPath(); ctx.arc(cx, cy, R, 0, Math.PI * 2); ctx.fillStyle = grad; ctx.fill();

  const nodeNames = dc.ringNodeNames;
  if (!nodeNames.length) return;

  const sortedPos = dc.sortedRingPositions;
  const ringMap = dc.ringMap;
  const ringNodes = dc.ringNodes;

  sortedPos.forEach(({ pos, name }) => {
    const angle = (pos / RING_SCALE) * Math.PI * 2 - Math.PI / 2;
    const info = ringNodes.get(name);
    const color = info ? info.color : '#888';
    const x = cx + R * Math.cos(angle), y = cy + R * Math.sin(angle);
    ctx.beginPath(); ctx.arc(x, y, 4, 0, Math.PI * 2);
    ctx.fillStyle = color; ctx.globalAlpha = 0.8; ctx.fill(); ctx.globalAlpha = 1;
  });

  const nodePositions = {};
  nodeNames.forEach(n => nodePositions[n] = []);
  sortedPos.forEach(({ pos, name }) => { if (nodePositions[name]) nodePositions[name].push(pos); });

  const naturalAngles = {};
  nodeNames.forEach(name => {
    const positions = nodePositions[name];
    if (!positions.length) { naturalAngles[name] = 0; return; }
    let sx = 0, sy = 0;
    positions.forEach(p => { const a = (p / RING_SCALE) * Math.PI * 2; sx += Math.cos(a); sy += Math.sin(a); });
    naturalAngles[name] = Math.atan2(sy, sx);
  });

  const MIN_SEP = (Math.PI * 2) / Math.max(nodeNames.length, 1) * 0.72;
  const labelAngles = { ...naturalAngles };
  for (let iter = 0; iter < 120; iter++) {
    let moved = false;
    for (let i = 0; i < nodeNames.length; i++) {
      for (let j = i + 1; j < nodeNames.length; j++) {
        const na = nodeNames[i], nb = nodeNames[j];
        let diff = labelAngles[nb] - labelAngles[na];
        while (diff > Math.PI) diff -= Math.PI * 2;
        while (diff < -Math.PI) diff += Math.PI * 2;
        const gap = Math.abs(diff);
        if (gap < MIN_SEP) {
          const push = (MIN_SEP - gap) / 2 + 0.001;
          const dir = diff >= 0 ? 1 : -1;
          labelAngles[na] -= dir * push; labelAngles[nb] += dir * push; moved = true;
        }
      }
    }
    if (!moved) break;
  }

  nodeNames.forEach(name => {
    const info = ringNodes.get(name);
    const angle = labelAngles[name] - Math.PI / 2;
    const labelR = R * 0.56;
    const x = cx + labelR * Math.cos(angle), y = cy + labelR * Math.sin(angle);
    ctx.beginPath(); ctx.arc(x, y, 24, 0, Math.PI * 2); ctx.fillStyle = info.color + '22'; ctx.fill();
    ctx.beginPath(); ctx.arc(x, y, 24, 0, Math.PI * 2); ctx.strokeStyle = info.color; ctx.lineWidth = 2; ctx.stroke();
    ctx.fillStyle = info.color; ctx.font = 'bold 10px JetBrains Mono, monospace';
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText(name, x, y - 5);
    ctx.font = '9px Inter, sans-serif'; ctx.fillStyle = 'rgba(200,211,245,0.75)';
    ctx.fillText(`${info.keyCount} keys`, x, y + 7);
  });

  ctx.font = 'bold 13px Inter, sans-serif';
  ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--txt2') || '#8b96c4';
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText(`${dc.ringSize} vnodes`, cx, cy - 8);
  ctx.font = '11px Inter, sans-serif';
  ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--txt3') || '#5a6490';
  ctx.fillText(`${nodeNames.length} nodes`, cx, cy + 10);

  const legend = $('ringLegend');
  legend.innerHTML = nodeNames.map(name => {
    const info = ringNodes.get(name);
    return `<div class="legend-item">
      <span class="legend-color" style="background:${info.color}"></span>
      <span class="legend-label">${escHtml(name)}</span>
      <span class="legend-count">${info.keyCount} key${info.keyCount !== 1 ? 's' : ''} · ${(nodePositions[name] || []).length} vnodes</span>
    </div>`;
  }).join('');

  $('ringStats').innerHTML = `
    <strong>Total vnodes:</strong> ${dc.ringSize}<br>
    <strong>Physical nodes:</strong> ${nodeNames.length}<br>
    <strong>Hash fn:</strong> FNV-1a 32-bit % 1000<br>
    <strong>Total keys:</strong> ${dc.allKeys().length}
  `;
}

function animateKey(key, type) {
  const canvas = $('ringCanvas');
  if (!canvas.parentElement.closest('#tab-ring.active')) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height, cx = W / 2, cy = H / 2;
  const R = Math.min(W, H) / 2 - 40;
  const pos = fnv1a(key) % 1000;
  const angle = (pos / 1000) * Math.PI * 2 - Math.PI / 2;
  const x = cx + R * Math.cos(angle), y = cy + R * Math.sin(angle);
  let alpha = 1, radius = 4;
  const color = type === 'set' ? '#34d399' : '#60a5fa';
  const anim = () => {
    drawRing();
    ctx.beginPath(); ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fillStyle = color; ctx.globalAlpha = alpha; ctx.fill(); ctx.globalAlpha = 1;
    radius += 1.5; alpha -= 0.05;
    if (alpha > 0) requestAnimationFrame(anim);
  };
  requestAnimationFrame(anim);
}

//  LRU Viewer 
function refreshLRUNodeBtns() {
  const btns = $('lruNodeBtns');
  btns.innerHTML = '';
  const nodeNames = dc.ringNodeNames;
  if (selectedLRUNode !== '__ALL__' && selectedLRUNode && !nodeNames.includes(selectedLRUNode) && nodeNames.length > 0)
    selectedLRUNode = nodeNames[0];
  else if (!selectedLRUNode && nodeNames.length > 0)
    selectedLRUNode = nodeNames[0];

  nodeNames.forEach(name => {
    const btn = document.createElement('button');
    btn.className = 'lru-node-btn' + (name === selectedLRUNode ? ' active' : '');
    btn.textContent = name;
    btn.addEventListener('click', async () => { selectedLRUNode = name; await dc.fetchLRU(name); refreshLRUNodeBtns(); drawLRU(); });
    btns.appendChild(btn);
  });
  const allBtn = document.createElement('button');
  allBtn.className = 'lru-node-btn' + (selectedLRUNode === '__ALL__' ? ' active' : '');
  allBtn.textContent = 'All Nodes';
  allBtn.addEventListener('click', async () => { selectedLRUNode = '__ALL__'; await dc.fetchLRU(); refreshLRUNodeBtns(); drawLRU(); });
  btns.appendChild(allBtn);
}

function drawLRU() {
  const viewer = $('lruViewer');
  viewer.innerHTML = '';
  const data = dc.lruData;
  const filter = selectedLRUNode === '__ALL__' ? null : selectedLRUNode;
  const toShow = filter ? data.filter(d => d.node === filter) : data;

  toShow.forEach(({ node, color, capacity, items }) => {
    const panel = document.createElement('div');
    panel.className = 'lru-node-panel';
    let chainHtml;
    if (!items || items.length === 0) {
      chainHtml = `<div class="lru-empty">Cache is empty</div>`;
    } else {
      chainHtml = `<div class="lru-chain">`;
      items.forEach((item, i) => {
        const cls = i === 0 ? 'mru' : (i === items.length - 1 ? 'lru-end' : '');
        const pos = i === 0 ? 'MRU' : (i === items.length - 1 ? 'LRU' : `${i + 1}`);
        chainHtml += `<div class="lru-slot ${cls}" style="${cls ? `border-color:${color}40` : ''}">
          <span class="lru-slot-pos">${pos}</span>
          <span class="lru-slot-key" title="${escHtml(item.key)}">${escHtml(item.key.length > 10 ? item.key.slice(0, 10) + '…' : item.key)}</span>
          <span class="lru-slot-val" title="${escHtml(item.val)}">${escHtml(item.val)}</span>
        </div>`;
        if (i < items.length - 1) chainHtml += `<span class="lru-arrow">→</span>`;
      });
      chainHtml += `</div>`;
    }
    panel.innerHTML = `
      <div class="lru-node-header">
        <span class="node-dot" style="background:${color}"></span>
        <span class="lru-node-title">${escHtml(node)}</span>
        <span class="lru-cap-badge">${items ? items.length : 0} / ${capacity}</span>
      </div>${chainHtml}`;
    viewer.appendChild(panel);
  });

  if (viewer.children.length === 0)
    viewer.innerHTML = `<div style="color:var(--txt3);font-style:italic">No nodes to display.</div>`;
}

// Benchmark 
const benchSkewSlider = $('benchSkew');
const benchSkewVal = $('benchSkewVal');
benchSkewSlider.addEventListener('input', () => { benchSkewVal.textContent = parseFloat(benchSkewSlider.value).toFixed(2); });

$('benchBtn').addEventListener('click', async () => {
  const ops = parseInt($('benchOps').value) || 2000;
  const ks = parseInt($('benchKeyspace').value) || 60;
  const skew = parseFloat($('benchSkew').value) || 0.5;
  const btn = $('benchBtn');
  btn.textContent = '⏳ Running…'; btn.disabled = true;

  const r = await dc.benchmark(ops, ks, skew);
  btn.textContent = '▶ Run Benchmark'; btn.disabled = false;

  const hitPct = r.hitRate * 100, missPct = 100 - hitPct;
  $('benchResults').innerHTML = `
    <h2 class="panel-title">📈 Results</h2>
    <div class="bench-result-grid">
      <div class="bench-metric"><span class="bench-metric-label">Throughput</span><span class="bench-metric-val green">${r.opsPerMs.toFixed(2)}</span><span style="font-size:.72rem;color:var(--txt3)">ops / ms</span></div>
      <div class="bench-metric"><span class="bench-metric-label">Hit Rate</span><span class="bench-metric-val blue">${hitPct.toFixed(1)}%</span><span style="font-size:.72rem;color:var(--txt3)">${r.hits} hits / ${r.hits + r.misses} reads</span></div>
      <div class="bench-metric"><span class="bench-metric-label">Avg Latency</span><span class="bench-metric-val yellow">${r.avgLatencyUs.toFixed(3)}</span><span style="font-size:.72rem;color:var(--txt3)">µs per op</span></div>
      <div class="bench-metric"><span class="bench-metric-label">Evictions</span><span class="bench-metric-val red">${r.evictions}</span><span style="font-size:.72rem;color:var(--txt3)">${r.durationMs.toFixed(1)} ms total</span></div>
    </div>
    <div class="bench-bar-section">
      <div class="bench-bar-label">Hit / Miss Distribution</div>
      <div class="bench-dual-bar">
        <div class="bench-bar-hit" style="width:${hitPct}%">${hitPct.toFixed(0)}%</div>
        <div class="bench-bar-miss" style="width:${missPct}%">${missPct.toFixed(0)}%</div>
      </div>
    </div>`;
  refreshAll();
  showToast(`Benchmark done: ${r.opsPerMs.toFixed(1)} ops/ms`, 'blue');
});

// Capacity & VNodes 
$('capBtn').addEventListener('click', async () => {
  const cap = parseInt($('capInput').value);
  if (isNaN(cap) || cap < 1) { showToast('Invalid capacity', 'red'); return; }
  try {
    await dc.setCapacity(cap);
    showToast(`Capacity set to ${cap} per node`, 'blue');
    refreshAll();
  } catch (e) { showToast(e.message, 'red'); }
});

const vnodeSlider = $('vnodeSlider');
const vnodeVal = $('vnodeVal');
vnodeSlider.addEventListener('input', () => { vnodeVal.textContent = vnodeSlider.value; });

$('vnodeBtn').addEventListener('click', async () => {
  const v = parseInt(vnodeSlider.value);
  const btn = $('vnodeBtn');
  btn.disabled = true;
  btn.textContent = '🔄 Reconfiguring Ring...';
  try {
    await dc.setVNodes(v);
    showToast(`Ring updated with ${v} virtual nodes per node`, 'success');
    refreshAll();
  } catch (e) { showToast(e.message, 'red'); }
  btn.disabled = false;
  btn.textContent = 'Apply Ring Change';
});

//  Theme 
$('themeToggle').addEventListener('click', () => {
  light = !light;
  document.body.classList.toggle('light', light);
  $('themeToggle').textContent = light ? '🌙' : '☀️';
  if (document.querySelector('#tab-ring.active')) drawRing();
});

$('setKey').addEventListener('keydown', e => { if (e.key === 'Enter') $('setValue').focus(); });
$('setValue').addEventListener('keydown', e => { if (e.key === 'Enter') $('setBtn').click(); });
$('getKey').addEventListener('keydown', e => { if (e.key === 'Enter') $('getBtn').click(); });
$('delKey').addEventListener('keydown', e => { if (e.key === 'Enter') $('delBtn').click(); });

async function checkBackend() {
  let connected = false;
  let attempts = 0;
  const maxAttempts = 10;

  while (!connected && attempts < maxAttempts) {
    try {
      await dc.syncAll();
      connected = true;
      refreshAll();
      showToast('Connected to backend ✓', 'blue');
    } catch (e) {
      attempts++;
      if (attempts === 1) {
        showToast('Connecting to backend...', 'blue');
        $('statusText').textContent = 'Connecting...';
      }
      await new Promise(r => setTimeout(r, 1000));
    }
  }

  if (!connected) {
    showToast('⚠ Backend offline — start app.exe first', 'red', 6000);
    $('statusText').textContent = 'Backend offline';
  }
}

function initTerminal() { }
checkBackend();
