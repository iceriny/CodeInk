#include "common.h"
const GUID IID_Unknown={0,0,0,{0xC0,0,0,0,0,0,0,0x46}};
const GUID IID_Dispatch={0x00020400,0,0,{0xC0,0,0,0,0,0,0,0x46}};
const GUID IID_ClassFactory={1,0,0,{0xC0,0,0,0,0,0,0,0x46}};
const GUID IID_Extensibility={0xB65AD801,0xABAF,0x11D0,{0xBB,0x8B,0,0xA0,0xC9,0x0F,0x27,0x44}};
const GUID IID_Ribbon={0x000C0396,0,0,{0xC0,0,0,0,0,0,0,0x46}};
const GUID CLSID_CodeInk={0x6B524C11,0x6D7C,0x4C67,{0xB9,0x23,0x55,0xDE,0xD6,0x23,0x92,0xA1}};
const wchar_t* const PROGID=L"CodeInk.PowerPointAddin";
const wchar_t* const CLSID_TEXT=L"{6B524C11-6D7C-4C67-B923-55DED62392A1}";
const wchar_t* const ADDIN_KEY=L"Software\\Microsoft\\Office\\PowerPoint\\Addins\\CodeInk.PowerPointAddin";
const wchar_t* const CONFIG_KEY=L"Software\\CodeInk\\PowerPoint";
const wchar_t* const VERSION=L"1.0.2";
extern "C" int _fltused=0;
#ifndef CODEINK_HOST_TEST
extern "C" void* memset(void* d,int c,SIZE_T n) { BYTE* p=(BYTE*)d; for(SIZE_T i=0;i<n;++i)p[i]=(BYTE)c;return d; }
extern "C" void* memcpy(void* d,const void* s,SIZE_T n) { BYTE* p=(BYTE*)d;const BYTE* q=(const BYTE*)s;for(SIZE_T i=0;i<n;++i)p[i]=q[i];return d; }
extern "C" void* memmove(void* d,const void* s,SIZE_T n) { BYTE* p=(BYTE*)d;const BYTE* q=(const BYTE*)s;if(p<q)for(SIZE_T i=0;i<n;++i)p[i]=q[i];else for(SIZE_T i=n;i;--i)p[i-1]=q[i-1];return d; }
void* operator new(SIZE_T,void* p) noexcept{return p;}
#endif
void* alloc(SIZE_T n){return HeapAlloc(GetProcessHeap(),8,n?n:1);}
void dealloc(void* p){if(p)HeapFree(GetProcessHeap(),0,p);}
int slen(const wchar_t* s){int n=0;if(s)while(s[n])++n;return n;}
bool same(const wchar_t*a,const wchar_t*b){if(!a||!b)return a==b;while(*a&&*b){if(*a++!=*b++)return false;}return *a==*b;}
static wchar_t lower(wchar_t c){return c>='A'&&c<='Z'?c+32:c;}
bool samei(const wchar_t*a,const wchar_t*b){if(!a||!b)return a==b;while(*a&&*b){if(lower(*a++)!=lower(*b++))return false;}return *a==*b;}
bool guidEq(REFIID a,REFIID b){const BYTE*p=(const BYTE*)&a,*q=(const BYTE*)&b;for(int i=0;i<16;++i)if(p[i]!=q[i])return false;return true;}
bool copystr(wchar_t*d,int cap,const wchar_t*s){int n=slen(s);if(n>=cap)return false;for(int i=0;i<=n;++i)d[i]=s[i];return true;}
bool catstr(wchar_t*d,int cap,const wchar_t*s){int n=slen(d);return n<cap&&copystr(d+n,cap-n,s);}
void number(wchar_t*d,int value){wchar_t r[16];int k=0;if(value<0){*d++='-';value=-value;}do{r[k++]=(wchar_t)('0'+value%10);value/=10;}while(value);for(int i=0;i<k;++i)d[i]=r[k-i-1];d[k]=0;}
void hexcode(wchar_t*d,DWORD v){const wchar_t*h=L"0123456789ABCDEF";d[0]='0';d[1]='x';for(int i=0;i<8;++i)d[2+i]=h[(v>>(28-4*i))&15];d[10]=0;}
bool installDir(wchar_t*d,int cap){DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",d,cap);return n&&n<(DWORD)cap&&catstr(d,cap,L"\\CodeInk");}
bool path(wchar_t*d,int cap,const wchar_t*dir,const wchar_t*name){return copystr(d,cap,dir)&&catstr(d,cap,L"\\")&&catstr(d,cap,name);}
bool regText(const wchar_t*k,const wchar_t*n,const wchar_t*v,DWORD view){HKEY h=nullptr;if(RegCreateKeyExW(HKCU,k,0,nullptr,0,KEY_WRITE|view,nullptr,&h,nullptr))return false;LONG e=RegSetValueExW(h,n,0,REG_SZ,(const BYTE*)v,(slen(v)+1)*2);RegCloseKey(h);return e==0;}
bool regNumber(const wchar_t*k,const wchar_t*n,DWORD v,DWORD view){HKEY h=nullptr;if(RegCreateKeyExW(HKCU,k,0,nullptr,0,KEY_WRITE|view,nullptr,&h,nullptr))return false;LONG e=RegSetValueExW(h,n,0,REG_DWORD,(const BYTE*)&v,4);RegCloseKey(h);return e==0;}
DWORD readNumber(const wchar_t*k,const wchar_t*n,DWORD fallback){HKEY h=nullptr;DWORD type=0,size=4,val=fallback;if(RegOpenKeyExW(HKCU,k,0,KEY_READ,&h))return fallback;LONG e=RegQueryValueExW(h,n,nullptr,&type,(BYTE*)&val,&size);RegCloseKey(h);return !e&&type==REG_DWORD?val:fallback;}
bool deleteKey(const wchar_t*k,DWORD view){HKEY h=nullptr;LONG e=RegOpenKeyExW(HKCU,k,0,KEY_READ|KEY_WRITE|0x10000|view,&h);if(e==2)return true;if(e)return false;e=RegDeleteTreeW(h,nullptr);RegCloseKey(h);if(e&&e!=2)return false;e=RegDeleteKeyExW(HKCU,k,view,0);return e==0||e==2;}
bool writeFile(const wchar_t*name,const void*bytes,DWORD len){HANDLE f=CreateFileW(name,0x40000000,1,nullptr,2,0x80,nullptr);if(f==INVALID_HANDLE_VALUE)return false;DWORD wrote=0;BOOL ok=WriteFile(f,bytes,len,&wrote,nullptr);CloseHandle(f);return ok&&wrote==len;}
bool diagnosticLogging(){return readNumber(CONFIG_KEY,L"DiagnosticLogging",0)!=0;}
void logEvent(const wchar_t*event,HRESULT hr){if(!diagnosticLogging())return;wchar_t dir[384],name[448],line[256],hex[16];if(!installDir(dir,384)||!path(name,448,dir,L"CodeInk.log"))return;copystr(line,256,event);if(hr){hexcode(hex,(DWORD)hr);catstr(line,256,L" ");catstr(line,256,hex);}catstr(line,256,L"\r\n");char utf8[800];int n=WideCharToMultiByte(65001,0,line,slen(line),utf8,800,nullptr,nullptr);HANDLE f=CreateFileW(name,4,3,nullptr,4,0x80,nullptr);if(f!=INVALID_HANDLE_VALUE){DWORD wrote=0;if(n)WriteFile(f,utf8,n,&wrote,nullptr);CloseHandle(f);}}
bool registerCom(const wchar_t*dll,DWORD view){
    wchar_t key[192];copystr(key,192,L"Software\\Classes\\CLSID\\");catstr(key,192,CLSID_TEXT);
    bool ok=regText(key,nullptr,L"CodeInk PowerPoint Add-in",view);
    catstr(key,192,L"\\InprocServer32");ok=regText(key,nullptr,dll,view)&&ok;ok=regText(key,L"ThreadingModel",L"Apartment",view)&&ok;
    copystr(key,192,L"Software\\Classes\\CLSID\\");catstr(key,192,CLSID_TEXT);catstr(key,192,L"\\ProgID");ok=regText(key,nullptr,PROGID,view)&&ok;
    copystr(key,192,L"Software\\Classes\\");catstr(key,192,PROGID);ok=regText(key,nullptr,L"CodeInk PowerPoint Add-in",view)&&ok;
    catstr(key,192,L"\\CLSID");ok=regText(key,nullptr,CLSID_TEXT,view)&&ok;
    ok=regText(ADDIN_KEY,L"FriendlyName",L"CodeInk · 代码高亮",view)&&ok;
    ok=regText(ADDIN_KEY,L"Description",L"Shiki / TextMate 语法高亮、主题和可编辑代码表格",view)&&ok;
    ok=regNumber(ADDIN_KEY,L"LoadBehavior",3,view)&&ok;return ok;
}
bool unregisterCom(DWORD view){wchar_t key[192];copystr(key,192,L"Software\\Classes\\CLSID\\");catstr(key,192,CLSID_TEXT);bool ok=deleteKey(key,view);copystr(key,192,L"Software\\Classes\\");catstr(key,192,PROGID);ok=deleteKey(key,view)&&ok;return deleteKey(ADDIN_KEY,view)&&ok;}
bool readRegText(const wchar_t*k,const wchar_t*n,wchar_t*out,int cap,const wchar_t*fallback){
    if(cap<1)return false;HKEY h=nullptr;DWORD type=0,size=(DWORD)cap*2;
    if(RegOpenKeyExW(HKCU,k,0,KEY_READ,&h))return copystr(out,cap,fallback);
    LONG e=RegQueryValueExW(h,n,nullptr,&type,(BYTE*)out,&size);RegCloseKey(h);
    if(e||type!=REG_SZ||size<2||size>(DWORD)cap*2)return copystr(out,cap,fallback);
    out[cap-1]=0;return true;
}
bool quoteArg(wchar_t* cmd,int cap,const wchar_t* arg){
    // Windows CommandLineToArgvW / CRT quoting, including backslashes before quotes.
    if(!catstr(cmd,cap,cmd[0]?L" \"":L"\""))return false;
    int slashes=0;
    for(const wchar_t*p=arg;;++p){
        if(*p=='\\'){++slashes;continue;}
        int count=(*p=='"'||!*p)?slashes*2:slashes;
        for(int i=0;i<count;++i)if(!catstr(cmd,cap,L"\\"))return false;
        slashes=0;if(!*p)break;
        if(*p=='"'&&!catstr(cmd,cap,L"\\"))return false;
        wchar_t letter[]={*p,0};if(!catstr(cmd,cap,letter))return false;
    }
    return catstr(cmd,cap,L"\"");
}
bool enginePaths(wchar_t* node,int nc,wchar_t* bridge,int bc){
    wchar_t dir[1024];return installDir(dir,1024)&&path(node,nc,dir,L"runtime\\node-v24.21.0-win-x64\\node.exe")&&path(bridge,bc,dir,L"engine\\bridge.mjs");
}
bool FileBytes::load(const wchar_t*filename,DWORD maximum){
    dealloc(bytes);bytes=nullptr;length=0;HANDLE h=CreateFileW(filename,0x80000000,7,nullptr,3,0x80,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
    DWORD high=0,n=GetFileSize(h,&high);if(high||n>maximum||n==0xFFFFFFFF){CloseHandle(h);return false;}
    bytes=(BYTE*)alloc((SIZE_T)n+2);DWORD got=0;bool ok=bytes&&ReadFile(h,bytes,n,&got,nullptr)&&got==n;CloseHandle(h);
    if(ok){length=n;bytes[n]=bytes[n+1]=0;}else{dealloc(bytes);bytes=nullptr;}return ok;
}
