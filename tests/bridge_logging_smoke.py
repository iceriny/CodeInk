#!/usr/bin/env python3
"""Execute the real bridge entry point without Shiki to test opt-in error logging.
This does not exercise actual highlighting or Windows process invocation.
"""
from pathlib import Path
import shutil, struct, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
node = shutil.which('node')
assert node, 'Node is required'
u32 = lambda n: struct.pack('<I', n)
def text(value):
    data = value.encode('utf-16le')
    return u32(len(data) // 2) + data
with tempfile.TemporaryDirectory(prefix='codeink-bridge-log-') as folder:
    folder = Path(folder)
    for src in (root / 'engine').glob('*.mjs'):
        shutil.copy2(src, folder / src.name)
    log = folder / 'engine-error.txt'
    def invoke(*args):
        return subprocess.run([node, str(folder/'bridge.mjs'), *map(str,args)], cwd=folder, capture_output=True, timeout=20)
    result = invoke('--invalid')
    assert result.returncode == 1 and not log.exists()
    print('PASS 1: real bridge outer failure does not create a diagnostic file by default')
    result = invoke('--invalid', '--diagnostics')
    assert result.returncode == 1 and 'bridge ERROR' in log.read_text()
    old = log.read_bytes()
    assert invoke('--invalid').returncode == 1 and log.read_bytes() == old
    print('PASS 2: explicit enable logs outer failure; disabling stops further writes')
    log.unlink()
    request = (u32(0x32514943)+u32(1)+text('cpp')+text('github-light')+text('')+
               u32(1)+text('const secretCode = 123; // 中文😀'))
    (folder/'request.bin').write_bytes(request)
    result = invoke('--request',folder/'request.bin',folder/'response.bin')
    response = (folder/'response.bin').read_bytes()
    assert result.returncode == 0 and struct.unpack_from('<II',response)==(0x32524943,1)
    assert '本地 Shiki' in response[12:].decode('utf-16le') and not log.exists()
    print('PASS 3: missing-dependency request returns the normal error response with diagnostics off')
    result = invoke('--request',folder/'request.bin',folder/'response.bin','--diagnostics')
    data = log.read_text()
    assert result.returncode == 0 and 'request ERR_MODULE_NOT_FOUND' in data
    assert all(part not in data for part in ['secretCode','中文','bridge.mjs',str(folder)])
    print('PASS 4: request diagnostics opt in without leaking source, paths or stack')
print('4 real bridge CLI error-path checks passed. Shiki highlighting and Windows were NOT executed.')
