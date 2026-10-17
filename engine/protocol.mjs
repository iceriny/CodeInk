/** Binary bridge protocol: little-endian u32 fields + length-prefixed UTF-16LE.
 * Code is data, never a shell argument, source module, or evaluated expression.
 */
export const REQUEST_MAGIC = 0x32514943; // CIQ2
export const RESPONSE_MAGIC = 0x32524943; // CIR2
export const MAX_TEXT = 50000;
export const MAX_TOTAL = 400000;
export const MAX_ITEMS = 32;
export const MAX_RESPONSE = 16 * 1024 * 1024;

export class Reader {
  constructor(buffer) { this.buffer = buffer; this.offset = 0; }
  u32() {
    if (this.offset + 4 > this.buffer.length) throw new Error('请求数据不完整。');
    const value = this.buffer.readUInt32LE(this.offset); this.offset += 4; return value;
  }
  text(max = MAX_TEXT) {
    const length = this.u32();
    if (length > max || this.offset + length * 2 > this.buffer.length) throw new Error('请求文字长度无效。');
    const value = this.buffer.toString('utf16le', this.offset, this.offset + length * 2);
    this.offset += length * 2; return value;
  }
  end() { if (this.offset !== this.buffer.length) throw new Error('请求格式版本不匹配。'); }
}
export class Writer {
  constructor() { this.parts = []; this.length = 0; }
  bytes(b) { this.parts.push(b); this.length += b.length; if (this.length > MAX_RESPONSE) throw new Error('高亮结果过大，请拆分代码。'); return this; }
  u32(n) { const b = Buffer.alloc(4); b.writeUInt32LE(n >>> 0); return this.bytes(b); }
  text(t) { return this.u32(t.length).bytes(Buffer.from(t, 'utf16le')); }
  finish() { return Buffer.concat(this.parts, this.length); }
}
export function parseRequest(buffer) {
  const r = new Reader(buffer);
  if (r.u32() !== REQUEST_MAGIC) throw new Error('插件与高亮引擎版本不匹配。');
  const operation = r.u32();
  if (operation !== 1) throw new Error('不支持的高亮请求。');
  const language = r.text(256), theme = r.text(256), themePath = r.text(4096);
  const count = r.u32();
  if (count < 1 || count > MAX_ITEMS) throw new Error('单次最多处理 32 个对象。');
  const texts = [];
  let total = 0;
  for (let i = 0; i < count; i++) {
    const t = r.text(); total += t.length;
    if (total > MAX_TOTAL) throw new Error('这一批代码过长，请分批处理。');
    texts.push(t);
  }
  r.end(); return { language, theme, themePath, texts };
}
export function errorResponse(message) {
  return new Writer().u32(RESPONSE_MAGIC).u32(1).text(String(message).slice(0, 1000)).finish();
}
export function successResponse(items, colors) {
  const w = new Writer().u32(RESPONSE_MAGIC).u32(0)
    .u32(colors.fg).u32(colors.bg).u32(colors.gutter).u32(items.length);
  for (const item of items) {
    w.u32(item.length).u32(item.tokens.length);
    for (const t of item.tokens) w.u32(t.start).u32(t.length).u32(t.color).u32(t.style);
  }
  return w.finish();
}
/** Shiki parses LF text; map every UTF-16 boundary back to the original BSTR.
 * A CRLF maps to one LF, a PPT soft break (VT) maps to LF, emoji stay two units.
 * Tabs are deliberately not expanded here: the renderer controls text rewriting.
 */
export function normalizeForParser(raw) {
  const chars = [], map = [0];
  for (let i = 0; i < raw.length;) {
    const c = raw[i++];
    if (c === '\r') { if (raw[i] === '\n') ++i; chars.push('\n'); }
    else chars.push((c === '\v' || c === '\u2028' || c === '\u2029') ? '\n' : c);
    map.push(i);
  }
  return { text: chars.join(''), map };
}
export function colorNumber(value, fallback = 0x24292f, background = 0xffffff) {
  if (typeof value !== 'string') return fallback;
  let hex = value.trim().replace(/^#/, '');
  if (/^[\da-f]{3}$/i.test(hex)) hex = hex.replace(/./g, c => c + c);
  if (!/^[\da-f]{6}([\da-f]{2})?$/i.test(hex)) return fallback;
  const rgb = Number.parseInt(hex.slice(0, 6), 16);
  if (hex.length === 6) return rgb;
  const a = Number.parseInt(hex.slice(6), 16) / 255;
  let result = 0;
  for (const shift of [16, 8, 0]) result |= Math.round(((rgb >> shift) & 255) * a + ((background >> shift) & 255) * (1 - a)) << shift;
  return result >>> 0;
}
/** Accept Shiki's line-token arrays, validate content, and coalesce adjacent runs.
 * Validation rejects silent offset drift instead of coloring the wrong character.
 */
export function mapTokens(raw, normalized, lines, fg, bg) {
  const output = [];
  let lineStart = 0;
  for (let row = 0; row < lines.length; ++row) {
    let cursor = lineStart;
    for (const t of lines[row]) {
      if (typeof t.content !== 'string') throw new Error('高亮引擎返回了无效文字片段。');
      if (normalized.text.slice(cursor, cursor + t.content.length) !== t.content)
        throw new Error('高亮引擎的字符偏移校验失败；原文未改动。');
      const end = cursor + t.content.length;
      const startRaw = normalized.map[cursor], endRaw = normalized.map[end];
      if (startRaw === undefined || endRaw === undefined) throw new Error('字符偏移超出原文范围。');
      const color = colorNumber(t.color, fg, bg), style = (Number.isInteger(t.fontStyle) && t.fontStyle >= 0 ? t.fontStyle : 0) & 7;
      if (endRaw > startRaw) {
        const last = output.at(-1);
        if (last && last.start + last.length === startRaw && last.color === color && last.style === style) last.length += endRaw - startRaw;
        else output.push({ start: startRaw, length: endRaw - startRaw, color, style });
      }
      cursor = end;
    }
    // An engine line must consume exactly one logical line (including blank lines).
    const newline = normalized.text.indexOf('\n', lineStart);
    const expected = newline < 0 ? normalized.text.length : newline;
    if (cursor !== expected) throw new Error('高亮引擎的行边界校验失败。');
    lineStart = expected + 1;
  }
  if (lineStart < normalized.text.length) throw new Error('高亮引擎没有处理全部文字。');
  return { length: raw.length, tokens: output };
}
