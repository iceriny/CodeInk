from pathlib import Path
import subprocess, shutil, sys
root=Path(__file__).resolve().parents[1]
build=root/'build';build.mkdir(exist_ok=True)
# Keep the embedded Ribbon in sync even when this test is run without a PE build.
sys.path.insert(0,str(root));from build import ribbon
ribbon()
cmd=[shutil.which('clang++') or 'clang++','-std=c++17','-O1','-g','-DCODEINK_HOST_TEST','-fshort-wchar','-ffunction-sections','-fdata-sections','-fno-builtin','-I'+str(root/'src'),'-I'+str(build),str(root/'tests/addin_mock_tests.cpp'),str(root/'src/com.cpp'),str(root/'src/common.cpp'),str(root/'src/textutil.cpp'),str(root/'src/catalog.cpp'),'-Wl,--gc-sections','-o',str(build/'addin_mock_tests')]
if '--sanitize' in sys.argv:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
subprocess.run(cmd,check=True)
subprocess.run([str(build/'addin_mock_tests')],check=True)
