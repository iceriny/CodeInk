#include "engine.h"
struct TempFile {
    wchar_t name[260];HANDLE h;
    TempFile():h(INVALID_HANDLE_VALUE){name[0]=0;wchar_t folder[260];DWORD n=GetTempPathW(260,folder);if(!n||n>=260||!GetTempFileNameW(folder,L"CIK",0,name))return;
        h=CreateFileW(name,0xC0000000,7,nullptr,2,0x04000100,nullptr); // delete-on-close, temporary
        if(h==INVALID_HANDLE_VALUE){DeleteFileW(name);name[0]=0;}}
    ~TempFile(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);else if(name[0])DeleteFileW(name);}
    bool write(const void*b,DWORD n){DWORD wrote=0;return h!=INVALID_HANDLE_VALUE&&WriteFile(h,b,n,&wrote,nullptr)&&wrote==n;}
    bool u32(DWORD n){return write(&n,4);}bool text(BSTR b){DWORD n=SysStringLen(b);return u32(n)&&write(b,n*2);}
    bool text(const wchar_t*t){DWORD n=slen(t);return u32(n)&&write(t,n*2);}
};
struct Reader {
    const BYTE* p;DWORD length,at;bool ok;
    Reader(const BYTE*b,DWORD n):p(b),length(n),at(0),ok(true){}
    DWORD u32(){if(at>length||length-at<4){ok=false;return 0;}DWORD x;memcpy(&x,p+at,4);at+=4;return x;}
};
HRESULT invokeEngine(const Options& o,const BSTR* texts,int count,HighlightResult& out,Progress* p){
    copystr(out.error,1100,L"本地高亮引擎未完成。请关闭 PowerPoint 后重新运行安装程序修复依赖。");
    if(count<1||count>32)return E_FAIL;
    TempFile in,output;if(in.h==INVALID_HANDLE_VALUE||output.h==INVALID_HANDLE_VALUE)return E_FAIL;
    bool ok=in.u32(0x32514943)&&in.u32(1)&&in.text(o.language)&&in.text(o.theme)&&in.text(o.themePath)&&in.u32(count);
    int total=0;for(int i=0;i<count&&ok;++i){UINT len=SysStringLen(texts[i]);total+=(int)len;if(len>50000||total>400000){copystr(out.error,1100,L"每个对象最多 50,000 个 UTF-16 字符单位，每批最多 400,000。请分批处理。");return E_FAIL;}ok=in.text(texts[i]);}
    if(!ok)return E_FAIL;
    // Heap buffers avoid a large stack frame inside Office's UI thread.
    wchar_t* paths=(wchar_t*)alloc(12288*2);if(!paths)return E_OUTOFMEMORY;
    wchar_t* node=paths,*bridge=paths+2048,*cmd=paths+4096;cmd[0]=0;
    ok=enginePaths(node,2048,bridge,2048)&&quoteArg(cmd,8192,node)&&quoteArg(cmd,8192,L"--max-old-space-size=512")&&quoteArg(cmd,8192,bridge)&&quoteArg(cmd,8192,L"--request")&&quoteArg(cmd,8192,in.name)&&quoteArg(cmd,8192,output.name);
    if(ok&&o.logging)ok=quoteArg(cmd,8192,L"--diagnostics");
    DWORD code=0;if(ok){if(p)p->text(L"正在本机加载语法与主题并解析代码…\r\n代码不会上传；首次使用某种语言可能稍慢。");ok=runChild(node,cmd,60000,p,code);}
    dealloc(paths);
    if(!ok){logEvent(L"Local highlighting process failed",(HRESULT)code);if(p&&!p->pump())copystr(out.error,1100,L"已取消；还没有开始修改幻灯片。");return (HRESULT)0x80004004;}
    DWORD high=0,bytes=GetFileSize(output.h,&high);if(high||bytes<12||bytes>16*1024*1024)return E_FAIL;
    BYTE* memory=(BYTE*)alloc(bytes);if(!memory)return E_OUTOFMEMORY;DWORD got=0;SetFilePointer(output.h,0,nullptr,0);ok=ReadFile(output.h,memory,bytes,&got,nullptr)&&got==bytes;
    Reader r(memory,ok?bytes:0);DWORD magic=r.u32(),status=r.u32();
    if(magic!=0x32524943){dealloc(memory);return E_FAIL;}
    if(status){DWORD n=r.u32();if(r.ok&&n<1100&&r.at+n*2<=bytes){memcpy(out.error,memory+r.at,n*2);out.error[n]=0;}dealloc(memory);return E_FAIL;}
    out.fg=r.u32();out.bg=r.u32();out.gutter=r.u32();DWORD items=r.u32();
    ok=r.ok&&items==(DWORD)count&&out.fg<=0xFFFFFF&&out.bg<=0xFFFFFF&&out.gutter<=0xFFFFFF&&out.items.make(count);
    for(int i=0;i<count&&ok;++i){HighlightItem& item=out.items[i];item.length=r.u32();DWORD n=r.u32();ok=r.ok&&item.length==SysStringLen(texts[i])&&n<=item.length+1&&n<=50001&&item.tokens.make((int)n);DWORD previous=0;
        for(DWORD j=0;j<n&&ok;++j){ColorToken& t=item.tokens[j];t.start=r.u32();t.length=r.u32();t.color=r.u32();t.style=r.u32();ok=r.ok&&t.start>=previous&&t.start<=item.length&&t.length<=item.length-t.start&&t.color<=0xFFFFFF&&t.style<=7;previous=t.start+t.length;}}
    ok=ok&&r.ok&&r.at==bytes;dealloc(memory);
    if(!ok){copystr(out.error,1100,L"高亮结果的完整性或字符偏移校验失败；原文未改动。");return E_FAIL;}
    out.error[0]=0;return S_OK;
}
