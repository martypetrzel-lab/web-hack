const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const port = Number(process.env.PORT) || 3000;
const apiKey = process.env.DEVICE_API_KEY || '';
const publicDir = path.join(__dirname, 'public');
const types = { '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.json': 'application/json; charset=utf-8' };
let latest = { status: 'Čekám na ESP32…', running: false, mode: 'idle', updated_at: null };
let pending = { command: '', arg: 0, created_at: null };

function json(res, code, value) {
  res.writeHead(code, { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', 'access-control-allow-origin': '*' });
  res.end(JSON.stringify(value));
}
function authorized(req) {
  if (!apiKey) return false;
  const supplied = String(req.headers['x-api-key'] || req.headers['x-control-key'] || '');
  const a = Buffer.from(apiKey), b = Buffer.from(supplied);
  return a.length === b.length && crypto.timingSafeEqual(a, b);
}
function body(req, max = 32_768) {
  return new Promise((resolve, reject) => { let data = '';
    req.on('data', chunk => { data += chunk; if (data.length > max) req.destroy(); });
    req.on('end', () => { try { resolve(JSON.parse(data || '{}')); } catch { reject(new Error('Invalid JSON')); } });
    req.on('error', reject);
  });
}
function api(req, res, pathname) {
  if (!authorized(req)) return json(res, 401, { error: 'Neplatný API klíč' });
  if (pathname === '/api/data' && req.method === 'GET') return json(res, 200, latest);
  if (pathname === '/api/data' && req.method === 'POST') return body(req).then(data => {
    latest = { ...data, updated_at: new Date().toISOString() };
    const command = pending; pending = { command: '', arg: 0, created_at: null };
    json(res, 200, command);
  }).catch(() => json(res, 400, { error: 'Neplatný JSON' }));
  if (pathname === '/api/command' && req.method === 'POST') return body(req, 2048).then(data => {
    const allowed = ['scan', 'lorascan', 'stop'];
    if (!allowed.includes(data.command)) return json(res, 400, { error: 'Příkaz není v bezpečném cloudovém režimu povolen' });
    pending = { command: data.command, arg: Number(data.arg) || 0, created_at: new Date().toISOString() };
    json(res, 202, { queued: true, ...pending });
  }).catch(() => json(res, 400, { error: 'Neplatný JSON' }));
  return json(res, 404, { error: 'API endpoint nenalezen' });
}

const server = http.createServer((req, res) => {
  const pathname = new URL(req.url, 'http://localhost').pathname;
  if (pathname === '/health') return json(res, 200, { ok: true, configured: Boolean(apiKey) });
  if (pathname.startsWith('/api/')) return api(req, res, pathname);
  const relative = pathname === '/' ? 'index.html' : pathname.replace(/^\/+/, '');
  const file = path.resolve(publicDir, relative);
  if (!file.startsWith(publicDir + path.sep)) { res.writeHead(403); return res.end('Forbidden'); }
  fs.readFile(file, (error, content) => {
    if (error) { res.writeHead(error.code === 'ENOENT' ? 404 : 500); return res.end(error.code === 'ENOENT' ? 'Not found' : 'Server error'); }
    res.writeHead(200, { 'content-type': types[path.extname(file)] || 'application/octet-stream', 'cache-control': pathname === '/' ? 'no-cache' : 'public, max-age=3600', 'x-content-type-options': 'nosniff', 'referrer-policy': 'no-referrer', 'permissions-policy': 'serial=(self)' });
    res.end(content);
  });
});

server.listen(port, '0.0.0.0', () => console.log(`Controller listening on ${port}`));
