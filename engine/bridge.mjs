import { readFileSync, writeFileSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseRequest, successResponse, errorResponse, normalizeForParser, colorNumber, mapTokens } from './protocol.mjs';
import { loadThemeFile } from './theme-loader.mjs';
import { diagnosticEvent } from './diagnostics.mjs';

const here = dirname(fileURLToPath(import.meta.url));
async function library() {
  // Packages and the Oniguruma WASM are installed locally. No CDN, telemetry,
  // sockets, language server, code evaluation, or runtime downloads are used.
  const shiki = await import('shiki');
  const { parse } = await import('jsonc-parser');
  const { createOnigurumaEngine } = await import('shiki/engine/oniguruma');
  return { shiki, parse, createOnigurumaEngine };
}
function resolveLanguage(input, shiki) {
  const q = input.trim().toLowerCase();
  if (['text', 'txt', 'plaintext', '纯文本'].includes(q)) return 'text';
  if (q === 'arduino' || q === 'arduino / c++') return 'cpp';
  const item = shiki.bundledLanguagesInfo.find(x => x.id === q || x.name.toLowerCase() === q || x.aliases?.some(a => a.toLowerCase() === q));
  if (!item) throw new Error('未找到该语言。请从语言列表选择，或输入 Shiki 语言 ID / 常用别名。');
  return item.id;
}
export async function highlight(request) {
  const { shiki, parse, createOnigurumaEngine } = await library();
  const lang = resolveLanguage(request.language, shiki);
  let theme = request.theme.trim().toLowerCase();
  if (theme === '__custom__') {
    if (!request.themePath) throw new Error('请先导入一个 VS Code JSON / JSONC 主题。');
    theme = loadThemeFile(request.themePath, parse);
  } else {
    const item = shiki.bundledThemesInfo.find(x => x.id === theme || x.displayName?.toLowerCase() === theme);
    if (!item) throw new Error('未找到该主题。请从主题列表选择，或使用“导入主题”。');
    theme = item.id;
  }
  const highlighter = await shiki.createHighlighter({ themes: [theme], langs: lang === 'text' ? [] : [lang], engine: createOnigurumaEngine(import('shiki/wasm')) });
  try {
    const themeName = typeof theme === 'string' ? theme : theme.name;
    const actual = highlighter.getTheme(themeName);
    const bg = colorNumber(actual.bg, actual.type === 'dark' ? 0x1e1e1e : 0xffffff);
    const fg = colorNumber(actual.fg, actual.type === 'dark' ? 0xd4d4d4 : 0x24292f, bg);
    const gutter = colorNumber(actual.colors?.['editorLineNumber.foreground'], actual.type === 'dark' ? 0x858585 : 0x6e7781, bg);
    const items = request.texts.map(raw => {
      const normalized = normalizeForParser(raw);
      const result = highlighter.codeToTokens(normalized.text, { lang, theme: themeName });
      return mapTokens(raw, normalized, result.tokens, fg, bg);
    });
    return { items, colors: { fg, bg, gutter } };
  } finally { highlighter.dispose(); }
}
function writeUtf16(path, text) { writeFileSync(path, Buffer.concat([Buffer.from([255, 254]), Buffer.from(text, 'utf16le')])); }
async function catalog() {
  const { shiki } = await library();
  const priorities = ['cpp', 'python', 'javascript', 'typescript', 'c', 'csharp', 'java', 'rust', 'go', 'html', 'css', 'json', 'yaml', 'markdown', 'sql', 'powershell', 'bash'];
  const langs = [...shiki.bundledLanguagesInfo].sort((a,b) => {
    const x = priorities.indexOf(a.id), y = priorities.indexOf(b.id);
    return (x < 0 ? 999 : x) - (y < 0 ? 999 : y) || a.id.localeCompare(b.id);
  });
  const themePriority = ['github-light', 'light-plus', 'github-dark', 'dark-plus', 'one-dark-pro', 'dracula', 'nord', 'monokai', 'catppuccin-latte', 'catppuccin-mocha'];
  const themes = [...shiki.bundledThemesInfo].sort((a,b) => {
    const x = themePriority.indexOf(a.id), y = themePriority.indexOf(b.id);
    return (x < 0 ? 999 : x) - (y < 0 ? 999 : y) || a.id.localeCompare(b.id);
  });
  writeUtf16(join(here, 'languages.tsv'), [...langs.map(x => `${x.id}\t${x.name}`), 'arduino\tArduino / C++', 'text\t纯文本'].join('\r\n'));
  writeUtf16(join(here, 'themes.tsv'), themes.map(x => `${x.id}\t${x.displayName || x.id}`).join('\r\n'));
  writeFileSync(join(here, 'catalog-summary.json'), JSON.stringify({ shiki: '4.4.3', languages: langs.length, themes: themes.length }, null, 2));
}
async function selfTest() {
  for (const [name, version] of [['shiki', '4.4.3'], ['jsonc-parser', '3.3.1']]) {
    const metadata = JSON.parse(readFileSync(join(here, 'node_modules', name, 'package.json'), 'utf8'));
    if (metadata.version !== version) throw new Error('Installed highlighter dependency version does not match.');
  }
  // This test runs during installation on the user's actual Windows runtime.
  for (const [language, text] of [['cpp', '#include <iostream>\r\nint main(){/*中文😀*/return 0;}'], ['python', 'x = f"value={1 + 2}"\rprint(x)'], ['javascript', 'const text = `hello ${1 + 2}`;'], ['rust', 'fn main() { println!("Hello"); }']]) {
    const result = await highlight({ language, theme: 'github-light', themePath: '', texts: [text] });
    if (!result.items[0]?.tokens.length || new Set(result.items[0].tokens.map(t => t.color)).size < 2) throw new Error('高亮引擎自检失败。');
  }
  const folder = mkdtempSync(join(tmpdir(), 'CodeInk-theme-test-'));
  try {
    writeFileSync(join(folder, 'base.json'), JSON.stringify({ type: 'light', colors: { 'editor.background': '#ffffff', 'editor.foreground': '#222222' }, tokenColors: [{ scope: 'keyword', settings: { foreground: '#0033aa' } }] }));
    writeFileSync(join(folder, 'theme.jsonc'), '// JSONC comments and trailing comma\n{ "include": "./base.json", "tokenColors": [ { "scope": "constant.numeric", "settings": { "foreground": "#aa3300" } }, ], }');
    const custom = await highlight({ language: 'cpp', theme: '__custom__', themePath: join(folder, 'theme.jsonc'), texts: ['int x = 42;'] });
    if (!custom.items[0]?.tokens.length) throw new Error('JSONC theme integration self-test failed.');
  } finally { rmSync(folder, { recursive: true, force: true }); }
  await catalog();
  writeFileSync(join(here, 'self-test-passed.txt'), 'CodeInk 1.0.1: Shiki integration self-test passed\n');
}
const [command, input, output] = process.argv.slice(2);
const diagnostics = process.argv.slice(2).includes('--diagnostics');
try {
  if (command === '--self-test') await selfTest();
  else if (command === '--catalog') await catalog();
  else if (command === '--request' && input && output) {
    let response;
    try {
      const request = parseRequest(readFileSync(input));
      const result = await highlight(request);
      response = successResponse(result.items, result.colors);
    } catch (e) {
      diagnosticEvent(here, e, diagnostics, 'request');
      // Error text may include a theme file path, but never a source-code snippet.
      const text = e?.code === 'ERR_MODULE_NOT_FOUND' ? '本地 Shiki 依赖不完整，请关闭 PowerPoint 后重新运行安装程序。' : (e?.message || '本地高亮引擎失败。');
      response = errorResponse(text);
    }
    writeFileSync(output, response);
  } else throw new Error('Unknown bridge invocation.');
} catch (e) {
  // Deliberately omit stack traces and source code from the persistent log.
  diagnosticEvent(here, e, diagnostics, command === '--self-test' ? 'setup' : 'bridge');
  process.exitCode = 1;
}
