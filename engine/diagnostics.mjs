import { appendFileSync } from 'node:fs';
import { join } from 'node:path';

// Opt-in only. Never persist source code, error messages, paths or stack traces.
export function diagnosticEvent(directory, error, enabled = false, operation = 'request') {
  if (!enabled) return false;
  const rawCode = error?.code;
  const code = typeof rawCode === 'string' && /^[A-Z][A-Z0-9_]{0,47}$/.test(rawCode) ? rawCode : 'ERROR';
  const action = ['request', 'setup', 'catalog'].includes(operation) ? operation : 'bridge';
  try {
    appendFileSync(join(directory, 'engine-error.txt'), `${new Date().toISOString()} ${action} ${code}\n`, 'utf8');
    return true;
  } catch {
    // A diagnostic write failure must never prevent the actual response/error UI.
    return false;
  }
}
