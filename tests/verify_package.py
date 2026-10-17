#!/usr/bin/env python3
"""Static PE/payload/Ribbon checks. Does NOT execute Windows binaries."""
from pathlib import Path
import struct, hashlib, json, re, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
DIST=ROOT/'dist'
VERSION=(ROOT/'VERSION').read_text(encoding='utf-8').strip()
assert json.loads((ROOT/'engine/package.json').read_text(encoding='utf-8'))['version']==VERSION
assert f'VERSION=L"{VERSION}"' in (ROOT/'src/common.cpp').read_text(encoding='utf-8')
U16=lambda b,p:struct.unpack_from('<H',b,p)[0]
U32=lambda b,p:struct.unpack_from('<I',b,p)[0]
U64=lambda b,p:struct.unpack_from('<Q',b,p)[0]
class PE:
    def __init__(self,p):
        self.data=p.read_bytes();b=self.data
        assert b[:2]==b'MZ',p.name
        h=U32(b,60);assert b[h:h+4]==b'PE\0\0'
        self.machine=U16(b,h+4);count=U16(b,h+6);size=U16(b,h+20);self.characteristics=U16(b,h+22)
        o=h+24;magic=U16(b,o);self.x64=magic==0x20b;assert magic in (0x10b,0x20b)
        self.dllflags=U16(b,o+70);self.entry=U32(b,o+16);assert self.entry>0
        dd=o+(112 if self.x64 else 96);self.directories=[(U32(b,dd+i*8),U32(b,dd+i*8+4)) for i in range(16)]
        self.sections=[]
        for i in range(count):
            q=o+size+40*i
            self.sections.append((b[q:q+8].split(b'\0')[0].decode(),U32(b,q+12),U32(b,q+8),U32(b,q+20),U32(b,q+16),U32(b,q+36)))
        for name,rva,vsize,raw,rawsize,flags in self.sections:
            assert not (flags&0x80000000 and flags&0x20000000),f'Unexpected RWX section: {name}'
        assert self.dllflags&0x40 and self.dllflags&0x100,'ASLR and NX should be set'
        assert self.directories[4]==(0,0),'Package is explicitly unsigned'
        assert self.directories[5][0]>0,'Relocations required for ASLR'
    def offset(self,rva):
        for _,v,n,p,z,_ in self.sections:
            if v<=rva<v+max(n,z):return p+rva-v
        raise AssertionError(f'Unmapped RVA {rva:x}')
    def string(self,rva):
        p=self.offset(rva);return self.data[p:self.data.index(b'\0',p)].decode('ascii')
    def imports(self):
        result={};p=self.offset(self.directories[1][0]);b=self.data
        while U32(b,p+12):
            name=self.string(U32(b,p+12));table=U32(b,p) or U32(b,p+16);q=self.offset(table);members=[];step=8 if self.x64 else 4
            while True:
                v=U64(b,q) if self.x64 else U32(b,q)
                if not v:break
                assert not v&(1<<(step*8-1)),'No opaque ordinal imports'
                members.append(self.string(v+2));q+=step
            result[name]=members;p+=20
        return result
    def exports(self):
        p=self.offset(self.directories[0][0]);n=U32(self.data,p+24);names=self.offset(U32(self.data,p+32))
        return [self.string(U32(self.data,names+4*i)) for i in range(n)]
results={}
for filename,arch,dll in [('CodeInk.x86.dll',0x14c,True),('CodeInk.x64.dll',0x8664,True),('CodeInk.Check32.exe',0x14c,False),('CodeInk.Check64.exe',0x8664,False),(f'CodeInk-Setup-{VERSION}-online.exe',0x14c,False)]:
    p=DIST/filename;pe=PE(p);assert pe.machine==arch
    assert bool(pe.characteristics&0x2000)==dll
    im=pe.imports();allowed={'KERNEL32.dll','USER32.dll','ADVAPI32.dll','OLEAUT32.dll','OLE32.dll','SHELL32.dll','GDI32.dll','COMDLG32.dll'}
    assert set(im)<=allowed,'Native code must import only the reviewed system DLLs'
    assert all('@' not in fn for funcs in im.values() for fn in funcs),'Import name decoration must not leak to PE table'
    if dll:assert set(pe.exports())=={'DllCanUnloadNow','DllGetClassObject','DllRegisterServer','DllUnregisterServer'}
    else:assert b'level="asInvoker"' in pe.data or b"level='asInvoker'" in pe.data,'Executable must not request elevation'
    if pe.x64:assert pe.directories[3][0]>0,'x64 stack unwind metadata must be present'
    results[filename]={'machine':hex(pe.machine),'bytes':p.stat().st_size,'sha256':hashlib.sha256(pe.data).hexdigest(),'imports':im}
setup=(DIST/f'CodeInk-Setup-{VERSION}-online.exe').read_bytes()
for name in ('CodeInk.x86.dll','CodeInk.x64.dll','CodeInk.Check32.exe','CodeInk.Check64.exe'):
    assert (DIST/name).read_bytes() in setup,f'Installer must embed exact current binary: {name}'
for p in (ROOT/'LICENSE.txt',ROOT/'THIRD-PARTY-NOTICES.txt',ROOT/'README.md',ROOT/'docs/使用说明.html',*(ROOT/'engine').glob('*.mjs'),ROOT/'engine/package.json'):
    assert p.read_bytes() in setup,f'Installer missing current documentation: {p.name}'
root=ET.parse(ROOT/'src/ribbon.xml').getroot();assert root.tag=='{http://schemas.microsoft.com/office/2006/01/customui}customUI'
ids=[];callbacks=set()
source=(ROOT/'src/addin.cpp').read_text(encoding='utf-8')
for element in root.iter():
    if 'id' in element.attrib:ids.append(element.attrib['id'])
    for key,value in element.attrib.items():
        if key in ('onAction','onChange','onLoad') or key.startswith('get'):
            callbacks.add(value);assert 'L"'+value+'"' in source,f'Missing COM callback {value}'
assert len(ids)==len(set(ids))
enum = re.search(r'enum Callback \{(.*?)\};',source,re.S).group(1)
enum = [x.strip().split('=')[0] for x in enum.split(',') if x.strip()]
methods = re.findall(r'L"([^"]+)"', re.search(r'METHODS\[\]=\{(.*?)\};',source,re.S).group(1))
assert len(enum)==len(methods)
exceptions={'CConnect':'OnConnection','CDisconnect':'OnDisconnection','CAddins':'OnAddInsUpdate','CStartup':'OnStartupComplete','CShutdown':'OnBeginShutdown','CCustomUI':'GetCustomUI','GLanguageCount':'LanguageCount','GLanguageLabel':'LanguageLabel','GThemeCount':'ThemeCount','GThemeLabel':'ThemeLabel'}
for key,method in zip(enum,methods):
    expected=exceptions.get(key,('On' if key.startswith('C') else 'Get')+key[1:])
    assert method==expected,f'Callback enum and DISPID list mismatch: {key} -> {method}'

# Release-specific layout and logging wiring checks, separate from UI rendering.
ns={'r':'http://schemas.microsoft.com/office/2006/01/customui'}
groups=root.findall('./r:ribbon/r:tabs/r:tab/r:group',ns)
assert [g.get('id') for g in groups]==['CodeInkApply','CodeInkSyntax','CodeInkType','CodeInkAppearance','CodeInkNumbers','CodeInkHelp']
controls={e.get('id'):e for e in root.iter() if e.get('id')}
window=controls['CodeInkWindowFrame']
assert window.tag.endswith('}toggleButton') and window.get('label')=='模拟窗口'
assert window.get('getPressed')=='GetWindowFrame'
assert controls['CodeInkAppearanceSettings'].get('onAction')=='OnAppearanceSettings'
assert controls['CodeInkLayoutSettings'].get('onAction')=='OnLayoutSettings'
assert controls['CodeInkFirstLine'].get('getEnabled')=='GetNumbersEnabled'
assert controls['CodeInkBasicCleanup'].get('getPressed')=='GetBasicCleanup'
assert controls['CodeInkReplace'].get('getPressed')=='GetReplace'
assert controls['CodeInkClipboard'].get('size')=='large'
assert controls['CodeInkHelpMenu'].find('r:menu[@id="CodeInkDiagnostics"]',ns) is not None
for key,label in [('Vertical','行号分隔线'),('Horizontal','行间横线'),('Outer','表格外框')]:
    assert controls['CodeInkBorder'+key].get('label')==label
    assert controls['CodeInkBorder'+key].get('getEnabled')=='GetEnabled'
# Menus cannot contain edit boxes / combo boxes in RibbonX 2006.
for menu in root.findall('.//r:menu',ns):
    assert not menu.findall('.//r:editBox',ns) and not menu.findall('.//r:comboBox',ns)
about=source[source.index('    void about(){'):source.index('\n\n};')]
assert all(word not in about for word in ['首次安装','新增','日志','最近状态','实机回归'])
assert controls['CodeInkAboutButton'].get('label')=='关于'
assert 'Iceriny' in source and 'Iceriny' in (ROOT/'src/installer.cpp').read_text(encoding='utf-8')
assert not root.findall('.//r:labelControl',ns), 'Avoid restoring the unbounded Ribbon status label'
for e in root.iter():
    for attr in ['label','pressed','enabled','visible','supertip','screentip','text']:
        assert not (attr in e.attrib and 'get'+attr[0].upper()+attr[1:] in e.attrib), f'Mutually exclusive Ribbon attributes on {e.get("id")}'
installer=(ROOT/'src/installer.cpp').read_text(encoding='utf-8')
assert all(flag in installer for flag in ['--logs-max=0','--loglevel=silent','--timing=false'])
assert 'if ($Diagnostics)' in (ROOT/'setup/download-runtime.ps1').read_text(encoding='utf-8')
assert 'diagnostics' in (ROOT/'engine/bridge.mjs').read_text(encoding='utf-8')
assert 'if (!enabled) return false;' in (ROOT/'engine/diagnostics.mjs').read_text(encoding='utf-8')

assert not list((ROOT/'src').glob('lexer*')), 'The old handwritten lexer must not ship'
results['static_checks']={'payloads':'all exact embedded binaries and documentation match','ribbon_ids':len(ids),'ribbon_callbacks':len(callbacks),'dispatch_methods':len(methods),'ribbon_groups':len(groups),'release_checks':'compact Ribbon / settings callbacks / contextual About and status / opt-in logging','authenticode':'unsigned','windows_runtime':'NOT TESTED'}
report_dir=ROOT/'build/reports'
report_dir.mkdir(parents=True,exist_ok=True)
(report_dir/'pe-validation.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('PASS: PE architectures / system-only imports / COM exports / manifests / exact payloads / Ribbon callback mapping')
print(json.dumps(results['static_checks'],ensure_ascii=False))
