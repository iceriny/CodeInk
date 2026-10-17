#!/usr/bin/env python3
"""Reproducible freestanding Windows build with LLVM clang/lld; no downloaded SDK.
Run `python build.py` on Linux/macOS/Windows with clang++, lld-link and Python 3.
"""
from pathlib import Path
import subprocess, shutil, json, hashlib, sys, re
from dev import configure_tools
configure_tools()
ROOT=Path(__file__).resolve().parent
VERSION=(ROOT/'VERSION').read_text(encoding='utf-8').strip()
SRC=ROOT/'src'; BUILD=ROOT/'build'; DIST=ROOT/'dist'
BUILD.mkdir(exist_ok=True); DIST.mkdir(exist_ok=True)
CLANG=shutil.which('clang++'); LLD=shutil.which('lld-link')
COMPILE_COMMANDS=[]
if not CLANG or not LLD: raise SystemExit('Install LLVM clang++ and lld-link, and add them to PATH.')
def run(args):
    print(' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),check=True,cwd=ROOT)
def ribbon():
    text=(SRC/'ribbon.xml').read_text(encoding='utf-8')
    escaped=text.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n"\nL"')
    (BUILD/'ribbon.generated.h').write_text('static const wchar_t* const RIBBON_XML = L"'+escaped+'";\n',encoding='utf-8')
def libraries(arch):
    apis=re.findall(r'^API\([^,]+,([A-Za-z0-9_]+),', (SRC/'winapi.h').read_text(encoding='utf-8'),re.M)
    groups={'USER32':['MessageBoxW','GetForegroundWindow','RegisterClassW','UnregisterClassW','CreateWindowExW','DefWindowProcW','DestroyWindow','ShowWindow','UpdateWindow','EnableWindow','SetWindowTextW','PeekMessageW','TranslateMessage','DispatchMessageW','SendMessageW','OpenClipboard','GetClipboardData','CloseClipboard'],
            'GDI32':['GetStockObject'],
            'COMDLG32':['GetOpenFileNameW'],
            'URLMON':['URLDownloadToFileW'],
            'ADVAPI32':[x for x in apis if x.startswith('Reg')],
            'OLEAUT32':['SysAllocString','SysAllocStringLen','SysStringLen','SysFreeString','VariantClear','VariantCopy','SafeArrayCreateVector','SafeArrayPutElement'],
            'OLE32':['CoCreateInstance','CoInitializeEx','CoUninitialize'],
            'SHELL32':['ShellExecuteW']}
    groups['USER32'] += ['DialogBoxIndirectParamW','EndDialog','GetDlgItem','GetDlgItemTextW','SetDlgItemTextW','CheckDlgButton','IsDlgButtonChecked','SendDlgItemMessageW','SetPropW','GetPropW','RemovePropW','SetFocus']
    used={x for group in groups.values() for x in group}
    groups['KERNEL32']=[x for x in apis if x not in used]
    files=[]
    for dll,names in groups.items():
        definition=BUILD/(dll+'.def'); definition.write_text('LIBRARY '+dll+'.dll\nEXPORTS\n'+'\n'.join('  '+x for x in names)+'\n')
        lib=BUILD/(dll+'-'+arch+'.lib')
        run([LLD,'/lib','/def:'+str(definition),'/machine:'+arch,'/out:'+str(lib)])
        files.append(lib)
    return files
def compile_file(filename,arch):
    target='x86_64-pc-windows-msvc' if arch=='x64' else 'i686-pc-windows-msvc'
    obj=BUILD/(Path(filename).stem+'-'+arch+'.obj')
    args=[CLANG,'--target='+target,'-std=c++17','-O2','-ffreestanding','-fno-builtin','-fno-exceptions','-funwind-tables','-fno-rtti','-fno-threadsafe-statics','-fno-stack-protector','-Wall','-Wextra','-Wframe-larger-than=3800','-I'+str(BUILD),'-I'+str(SRC),'-c',SRC/filename,'-o',obj]
    COMPILE_COMMANDS.append({'directory':str(ROOT),'file':str(SRC/filename),'arguments':list(map(str,args))})
    # Separate databases prevent IntelliSense from choosing the wrong ABI.
    entries=[entry for entry in COMPILE_COMMANDS if '--target='+target in entry['arguments']]
    (BUILD/('compile_commands-'+arch+'.json')).write_text(json.dumps(entries,indent=2)+'\n',encoding='utf-8')
    run(args)
    return obj
def link(output,objects,libs,arch,dll=False):
    flags=['/nodefaultlib','/machine:'+arch,'/subsystem:windows,6.1','/dynamicbase','/nxcompat','/opt:ref','/opt:icf','/incremental:no','/timestamp:1789261200']
    if dll:
        definition=BUILD/('addin-'+arch+'.def')
        exp=[('DllGetClassObject',12),('DllCanUnloadNow',0),('DllRegisterServer',0),('DllUnregisterServer',0)]
        definition.write_text('LIBRARY CodeInk\nEXPORTS\n'+'\n'.join(' '+n+('=_'+n+'@'+str(k) if arch=='x86' else '') for n,k in exp)+'\n')
        flags+=['/dll','/entry:DllMain','/def:'+str(definition)]
    else: flags+=['/entry:EntryPoint']
    # LLVM generates the asInvoker manifest without an external manifest tool.
    if not dll: flags+=['/manifest:embed',"/manifestuac:level='asInvoker' uiAccess='false'"]
    run([LLD,*flags,'/out:'+str(output),*objects,*libs])
def main():
    ribbon()
    for arch in ['x86','x64']:
        libs=libraries(arch)
        objects=[compile_file(f,arch) for f in ['common.cpp','textutil.cpp','basicformat.cpp','com.cpp','progress.cpp','engine.cpp','catalog.cpp','render.cpp','settings.cpp','addin.cpp']]
        link(DIST/('CodeInk.'+arch+'.dll'),objects,libs,arch,True)
        if (SRC/'check.cpp').exists():
            chk=[BUILD/('common-'+arch+'.obj'),BUILD/('progress-'+arch+'.obj'),BUILD/('engine-'+arch+'.obj'),compile_file('check.cpp',arch)]
            link(DIST/('CodeInk.Check'+('64' if arch=='x64' else '32')+'.exe'),chk,libs,arch)
    if (SRC/'installer.cpp').exists() and '--dll-only' not in sys.argv:
        from make_payload import make_payload
        make_payload(ROOT)
        objs=[BUILD/'common-x86.obj',BUILD/'progress-x86.obj',compile_file('installer.cpp','x86')]
        link(DIST/f'CodeInk-Setup-{VERSION}-online.exe',objs,libraries('x86'),'x86')
    release_files=[DIST/name for name in ['CodeInk.x86.dll','CodeInk.x64.dll','CodeInk.Check32.exe','CodeInk.Check64.exe',f'CodeInk-Setup-{VERSION}-online.exe']]
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in release_files if p.exists()}
    (DIST/'SHA256SUMS.txt').write_text(''.join(v+'  '+k+'\n' for k,v in sorted(hashes.items())),encoding='utf-8')
    print(json.dumps(hashes,indent=2))
if __name__=='__main__': main()
