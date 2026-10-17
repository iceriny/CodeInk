"""Run the real settings dialog logic on Linux with simulated Win32 controls."""
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = root / 'build'
build.mkdir(exist_ok=True)
cmd = [shutil.which('clang++') or 'clang++', '-std=c++17', '-O1', '-g',
       '-DCODEINK_HOST_TEST', '-fshort-wchar', '-ffunction-sections', '-fdata-sections',
       '-fno-builtin', '-I' + str(root / 'src'), str(root / 'tests/settings_mock_tests.cpp'),
       str(root / 'src/common.cpp'), str(root / 'src/textutil.cpp'),
       '-Wl,--gc-sections', '-o', str(build / 'settings_mock_tests')]
if '--sanitize' in sys.argv:
    cmd[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
subprocess.run(cmd, check=True)
subprocess.run([str(build / 'settings_mock_tests')], check=True)
