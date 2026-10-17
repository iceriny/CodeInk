import { readFileSync, statSync } from 'node:fs';
import { resolve, dirname, isAbsolute, extname } from 'node:path';
import { createHash } from 'node:crypto';

/** Load VS Code JSON/JSONC theme data; never execute a VS Code extension.
 * Relative `include` inherits colors and tokenColors. Semantic tokens require a
 * language service and intentionally are not imitated with ad-hoc rules.
 */
export function loadThemeFile(filename, parseJSONC) {
  const active = new Set();
  let bytesRead = 0;
  function read(path, depth) {
    if (depth > 8) throw new Error('主题 include 层数超过 8 层。');
    path = resolve(path);
    if (/^(\\\\|\/\/)/.test(path)) throw new Error('主题必须位于本机文件系统，不能引用网络共享。');
    if (!['.json', '.jsonc'].includes(extname(path).toLowerCase())) throw new Error('仅支持 JSON / JSONC 主题文件。');
    if (active.has(path.toLowerCase())) throw new Error('主题 include 存在循环引用。');
    const size = statSync(path).size; bytesRead += size;
    if (size > 2 * 1024 * 1024 || bytesRead > 8 * 1024 * 1024) throw new Error('主题文件过大。');
    active.add(path.toLowerCase());
    const errors = [];
    const data = parseJSONC(readFileSync(path, 'utf8').replace(/^\ufeff/, ''), errors, { allowTrailingComma: true });
    if (errors.length || !data || typeof data !== 'object' || Array.isArray(data)) throw new Error('主题 JSON / JSONC 解析失败。');
    let base = {};
    if (data.include != null) {
      if (typeof data.include !== 'string' || isAbsolute(data.include) || /^[a-z]+:/i.test(data.include))
        throw new Error('主题 include 必须是相对路径。');
      base = read(resolve(dirname(path), data.include), depth + 1);
    }
    if (data.tokenColors !== undefined && !Array.isArray(data.tokenColors))
      throw new Error('tokenColors 必须是数组；暂不支持外部 .tmTheme / plist 引用。');
    active.delete(path.toLowerCase());
    return { ...base, ...data, colors: { ...(base.colors ?? {}), ...(data.colors ?? {}) },
      tokenColors: [...(base.tokenColors ?? []), ...(data.tokenColors ?? [])] };
  }
  const result = read(filename, 0);
  delete result.include;
  // Stable internal name avoids collisions with bundled themes. This is process
  // state only; no private metadata is attached to any PowerPoint object.
  result.name = 'codeink-custom-' + createHash('sha256').update(JSON.stringify(result)).digest('hex').slice(0, 12);
  if (!result.tokenColors.length) throw new Error('这个文件没有 TextMate tokenColors，不能作为语法染色主题。');
  return result;
}
