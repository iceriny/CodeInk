from pathlib import Path
import json

def make_payload(root: Path):
    files=[
        ('CodeInk.x86.dll',root/'dist/CodeInk.x86.dll',False),
        ('CodeInk.x64.dll',root/'dist/CodeInk.x64.dll',False),
        ('CodeInk.Check32.exe',root/'dist/CodeInk.Check32.exe',False),
        ('CodeInk.Check64.exe',root/'dist/CodeInk.Check64.exe',False),
        ('使用说明.html',root/'docs/使用说明.html',False),
        ('README.txt',root/'README.md',False),
        ('LICENSE.txt',root/'LICENSE.txt',False),
        ('THIRD-PARTY-NOTICES.txt',root/'THIRD-PARTY-NOTICES.txt',False),
    ]
    files += [('engine\\'+p.name,p,True) for p in sorted((root/'engine').iterdir()) if p.suffix=='.mjs' or p.name=='package.json']
    pieces=['struct Payload { const wchar_t* name; const unsigned char* bytes; unsigned long length; bool engine; };']
    items=[]
    for i,(name,path,engine) in enumerate(files):
        data=path.read_bytes()
        if name=='README.txt':data=b'\xef\xbb\xbf'+data
        pieces.append(f'static const unsigned char PAYLOAD_{i}[] = {{')
        pieces.extend(','.join(f'0x{x:02x}' for x in data[j:j+24])+',' for j in range(0,len(data),24))
        pieces.append('};')
        items.append('{L'+json.dumps(name,ensure_ascii=False)+f',PAYLOAD_{i},sizeof(PAYLOAD_{i}),'+str(engine).lower()+'}')
    pieces.append('static const Payload PAYLOADS[] = {'+','.join(items)+'};')
    pieces.append('static const unsigned int PAYLOAD_COUNT = sizeof(PAYLOADS)/sizeof(PAYLOADS[0]);')
    (root/'build/payload.generated.h').write_text('\n'.join(pieces)+'\n',encoding='utf-8')
    text=(root/'setup/download-runtime.ps1').read_text(encoding='utf-8')
    escaped=text.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n"\nL"')
    (root/'build/bootstrap.generated.h').write_text('static const wchar_t*const BOOTSTRAP_SCRIPT=L"'+escaped+'";\n',encoding='utf-8')
