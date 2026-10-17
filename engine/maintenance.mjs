/** Track only installation-owned trees, so the native uninstaller does not
 * recursively delete arbitrary files that the user later adds to CodeInk. */
import { readdirSync, lstatSync, existsSync, writeFileSync } from 'node:fs';
import { join, resolve, relative, sep } from 'node:path';
const [operation, rootInput] = process.argv.slice(2);
if (operation !== '--manifest' || !rootInput) throw new Error('Unsupported maintenance operation.');
const root = resolve(rootInput), records = [];
function walk(dir) {
  if (!existsSync(dir)) return;
  const stat = lstatSync(dir);
  if (stat.isSymbolicLink()) throw new Error('Refusing to track an installation symlink.');
  const rel = relative(root, dir);
  if (rel.startsWith('..') || rel.includes('\t') || /[\r\n]/.test(rel)) throw new Error('Invalid installation-relative path.');
  if (stat.isDirectory()) {
    for (const entry of readdirSync(dir)) walk(join(dir, entry));
    records.push(`D\t${rel.split(sep).join('\\')}`); // children before parents
  } else if (stat.isFile()) records.push(`F\t${rel.split(sep).join('\\')}`);
}
for (const owned of ['runtime', 'engine/node_modules', 'cache', '_downloads']) walk(join(root, owned));
writeFileSync(join(root, 'managed-files.tsv'), Buffer.concat([Buffer.from([255,254]), Buffer.from(records.join('\r\n'), 'utf16le')]));
