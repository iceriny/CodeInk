// Run the actual Ribbon IDispatch callbacks and preference/logger code against
// small Win32 stubs. This is not a Windows/Office UI rendering test.
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "../src/addin.cpp"
using U=std::u16string;
static U u(const wchar_t*s){return s?U((const char16_t*)s,(const char16_t*)s+slen(s)):U();}
static const wchar_t*w(const U&s){return (const wchar_t*)s.c_str();}
static std::map<U,std::vector<BYTE>> reg;
static std::map<U,DWORD> types;
static int logOpens=0,logWrites=0,environmentCalls=0;
static U lastMessage,lastTitle;
static std::string logBody;
extern "C" HANDLE GetProcessHeap(){return (HANDLE)1;}
extern "C" void*HeapAlloc(HANDLE,DWORD,SIZE_T n){return std::calloc(1,n);}
extern "C" BOOL HeapFree(HANDLE,DWORD,void*p){std::free(p);return TRUE;}
extern "C" BSTR SysAllocStringLen(LPCWSTR s,UINT n){auto*p=(UINT*)std::malloc(4+(n+1)*2);assert(p);*p=n;BSTR b=(BSTR)(p+1);if(s)std::memcpy(b,s,n*2);else std::memset(b,0,n*2);b[n]=0;return b;}
extern "C" BSTR SysAllocString(LPCWSTR s){return SysAllocStringLen(s,slen(s));}
extern "C" UINT SysStringLen(BSTR s){return s?((UINT*)s)[-1]:0;}
extern "C" void SysFreeString(BSTR s){if(s)std::free((UINT*)s-1);}
extern "C" HRESULT VariantClear(VARIANT*v){if(v->vt==VT_BSTR)SysFreeString(v->bstrVal);else if(v->vt==VT_DISPATCH&&v->pdispVal)v->pdispVal->Release();std::memset(v,0,sizeof(*v));return S_OK;}
extern "C" HRESULT VariantCopy(VARIANT*d,const VARIANT*s){VariantClear(d);*d=*s;if(s->vt==VT_BSTR)d->bstrVal=SysAllocStringLen(s->bstrVal,SysStringLen(s->bstrVal));else if(s->vt==VT_DISPATCH&&s->pdispVal)s->pdispVal->AddRef();return S_OK;}
extern "C" LONG RegOpenKeyExW(HKEY,LPCWSTR,DWORD,DWORD,HKEY*out){*out=(HKEY)2;return 0;}
extern "C" LONG RegCreateKeyExW(HKEY,LPCWSTR,DWORD,LPWSTR,DWORD,DWORD,void*,HKEY*out,DWORD*){*out=(HKEY)2;return 0;}
extern "C" LONG RegCloseKey(HKEY){return 0;}
extern "C" LONG RegQueryValueExW(HKEY,LPCWSTR name,DWORD*,DWORD*type,BYTE*bytes,DWORD*size){auto i=reg.find(u(name));if(i==reg.end())return 2;*type=types[u(name)];if(*size<i->second.size())return 234;*size=i->second.size();std::memcpy(bytes,i->second.data(),*size);return 0;}
extern "C" LONG RegSetValueExW(HKEY,LPCWSTR name,DWORD,DWORD type,const BYTE*bytes,DWORD size){reg[u(name)]=std::vector<BYTE>(bytes,bytes+size);types[u(name)]=type;return 0;}
extern "C" DWORD GetEnvironmentVariableW(LPCWSTR,LPWSTR dst,DWORD cap){++environmentCalls;U value=u"C:\\Users\\Test\\AppData\\Local";assert(cap>value.size());copystr(dst,cap,w(value));return value.size();}
extern "C" HANDLE CreateFileW(LPCWSTR name,DWORD access,DWORD,void*,DWORD,DWORD,HANDLE){if(access==4){++logOpens;assert(u(name).find(u"CodeInk.log")!=U::npos);return (HANDLE)3;}return INVALID_HANDLE_VALUE;}
extern "C" BOOL WriteFile(HANDLE h,const void*b,DWORD n,DWORD*written,void*){assert(h==(HANDLE)3);++logWrites;logBody.append((const char*)b,n);*written=n;return TRUE;}
extern "C" BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*){return FALSE;}
extern "C" DWORD GetFileSize(HANDLE,DWORD*high){*high=0;return 0;}
extern "C" BOOL CloseHandle(HANDLE){return TRUE;}
extern "C" DWORD GetLastError(){return 2;}
extern "C" DWORD GetFileAttributesW(LPCWSTR){return 0xFFFFFFFF;}
extern "C" BOOL DeleteFileW(LPCWSTR){return TRUE;}
extern "C" int WideCharToMultiByte(UINT,DWORD,LPCWSTR s,int n,char*out,int cap,const char*,BOOL*){assert(n<cap);for(int i=0;i<n;++i)out[i]=(char)(s[i]<128?s[i]:'?');return n;}
extern "C" HWND GetForegroundWindow(){return nullptr;}
extern "C" int MessageBoxW(HWND,LPCWSTR body,LPCWSTR title,UINT){lastMessage=u(body);lastTitle=u(title);return IDYES;}
extern "C" HINSTANCE ShellExecuteW(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,int){return (HINSTANCE)33;}
extern "C" BOOL GetOpenFileNameW(OPENFILENAMEW*){return FALSE;}
Progress::Progress(HWND,const wchar_t*):window(nullptr),label(nullptr),owner(nullptr),stopped(false),ownDisable(false){}
Progress::~Progress(){}
void Progress::text(const wchar_t*){}
bool Progress::pump(){return true;}
HRESULT invokeEngine(const Options&,const BSTR*,int,HighlightResult&,Progress*){return E_FAIL;}
static int settingsOpened=0;static bool settingsAppearance=false;
bool editSettings(HWND,HINSTANCE,Options& o,bool appearance){++settingsOpened;settingsAppearance=appearance;o.windowPadding=18;o.basicCleanup=false;return true;}
static Action lastAction=Action::Plain;static int actions=0;
HRESULT executeAction(IDispatch*,const Options&,Action action,wchar_t*status,int sc,wchar_t*,int,Progress&){lastAction=action;++actions;copystr(status,sc,L"Mock action completed");return S_OK;}
struct App:IDispatch{
    HRESULT QueryInterface(REFIID,void**out)override{*out=this;return S_OK;}ULONG AddRef()override{return 1;}ULONG Release()override{return 1;}
    HRESULT GetTypeInfoCount(UINT*n)override{*n=0;return S_OK;}HRESULT GetTypeInfo(UINT,LCID,ITypeInfo**)override{return E_NOTIMPL;}
    HRESULT GetIDsOfNames(REFIID,LPWSTR*,UINT,LCID,DISPID*)override{return E_FAIL;}
    HRESULT Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*)override{return E_FAIL;}
};
static DISPID method(Addin&a,const wchar_t*name){DISPID id;LPWSTR s=const_cast<LPWSTR>(name);assert(a.GetIDsOfNames(IID_Unknown,&s,1,0,&id)==S_OK);return id;}
static Value get(Addin&a,const wchar_t*name){Value value;assert(a.Invoke(method(a,name),IID_Unknown,0,DISPATCH_METHOD,nullptr,&value.v,nullptr,nullptr)==S_OK);return value;}
static void action(Addin&a,const wchar_t*name){assert(a.Invoke(method(a,name),IID_Unknown,0,DISPATCH_METHOD,nullptr,nullptr,nullptr,nullptr)==S_OK);}
static void change(Addin&a,const wchar_t*name,Value val,bool dropdown=false){VARIANT args[3]={};VariantCopy(&args[0],&val.v);DISPPARAMS params={args,nullptr,(UINT)(dropdown?3:2),0};assert(a.Invoke(method(a,name),IID_Unknown,0,DISPATCH_METHOD,&params,nullptr,nullptr,nullptr)==S_OK);VariantClear(&args[0]);}
static bool checked(Addin&a,const wchar_t*name){Value v=get(a,name);assert(v.v.vt==VT_BOOL);return v.v.boolVal!=0;}
static int passed=0;static void pass(const char*s){std::printf("PASS %02d: %s\n",++passed,s);}
int main(){
    regNumber(CONFIG_KEY,L"AutoFit",1);regNumber(CONFIG_KEY,L"AutoFitMin100",1200);regNumber(CONFIG_KEY,L"AutoFitMax100",1400);
    App app;Addin a;assert(a.OnConnection(&app,0,nullptr,nullptr)==S_OK);
    assert(checked(a,L"GetBorderVertical")&&!checked(a,L"GetBorderHorizontal")&&!checked(a,L"GetBorderOuter")&&!checked(a,L"GetLogging"));
    assert(logWrites==0&&logOpens==0);pass("fresh install defaults: vertical-only borders and zero diagnostic log writes");
    {
        BSTR xml=nullptr;assert(a.GetCustomUI(nullptr,&xml)==S_OK);U source=u(xml);SysFreeString(xml);
        assert(source.find(u"toggleButton")!=U::npos&&source.find(u"外观设置")!=U::npos&&source.find(u"label=\"关于\"")!=U::npos);
        assert(source.find(u"版本与诊断")==U::npos&&source.find(u"CodeInkWindowGeometry")==U::npos&&source.find(u"CodeInkWindowColors")==U::npos);
        pass("actual GetCustomUI supplies consolidated window toggle and the renamed About control");
    }
    assert(!checked(a,L"GetWindowEnabled")&&checked(a,L"GetBackgroundEnabled"));
    change(a,L"OnWindowFrame",Value::boolean(true));assert(checked(a,L"GetWindowEnabled")&&!checked(a,L"GetBackgroundEnabled"));
    {Value v=get(a,L"GetWindowLabel");assert(u(v.v.bstrVal)==u"模拟窗口");}
    change(a,L"OnWindowPadding",Value::text(L"22.5"));change(a,L"OnWindowRadius",Value::text(L"12"));
    change(a,L"OnWindowFrame",Value::boolean(false));assert(!checked(a,L"GetWindowEnabled"));change(a,L"OnWindowFrame",Value::boolean(true));
    {Value v=get(a,L"GetWindowPadding");assert(u(v.v.bstrVal)==u"22.5");}
    pass("window dependent controls gate correctly; off/on retains padding and radius");
    change(a,L"OnBorderHorizontal",Value::boolean(true));change(a,L"OnBorderVertical",Value::boolean(false));change(a,L"OnBorderOuter",Value::boolean(true));
    change(a,L"OnOutputMode",Value::integer(1),true);assert(!checked(a,L"GetBorderEnabled"));
    change(a,L"OnOutputMode",Value::integer(0),true);assert(checked(a,L"GetBorderEnabled"));assert(!checked(a,L"GetBorderVertical")&&checked(a,L"GetBorderHorizontal")&&checked(a,L"GetBorderOuter"));
    pass("border toggles stay independent and survive disabling in text-box output mode");
    {
        int oldCalls=environmentCalls;logEvent(L"Test while off",E_FAIL);assert(logWrites==0&&logOpens==0&&environmentCalls==oldCalls);
        change(a,L"OnLogging",Value::boolean(true));assert(checked(a,L"GetLogging")&&diagnosticLogging()&&logWrites==1);
        logEvent(L"Generic renderer event",E_FAIL);assert(logWrites==2&&logBody.find("0x80004005")!=std::string::npos);
        change(a,L"OnLogging",Value::boolean(false));int before=logWrites;oldCalls=environmentCalls;logEvent(L"Suppressed failure",E_FAIL);
        assert(!diagnosticLogging()&&logWrites==before&&environmentCalls==oldCalls);pass("real native logger never opens files when off; opt-in and immediate disable work");
    }
    {
        Addin reloaded;assert(reloaded.OnConnection(&app,0,nullptr,nullptr)==S_OK);
        assert(!checked(reloaded,L"GetLogging")&&!checked(reloaded,L"GetBorderVertical")&&checked(reloaded,L"GetBorderHorizontal")&&checked(reloaded,L"GetWindowFrame"));
        Value v=get(reloaded,L"GetWindowPadding");assert(u(v.v.bstrVal)==u"22.5");pass("preferences round-trip through the registry code after reconnecting");
    }
    action(a,L"OnAbout");assert(lastTitle==u"关于 CodeInk"&&lastMessage.find(u"作者：Iceriny")!=U::npos&&lastMessage.find(u"1.0.1")!=U::npos&&lastMessage.find(u"首次安装")==U::npos&&lastMessage.find(u"新增")==U::npos&&lastMessage.find(u"日志")==U::npos);
    pass("About shows product/version/author without setup notes or runtime diagnostics");
    action(a,L"OnBorders");assert(actions==1&&lastAction==Action::Borders);pass("border-only Ribbon action dispatches the dedicated operation, not format/rebuild");
    action(a,L"OnOpenLog");assert(lastMessage.find(u"没有可打开")!=U::npos&&!diagnosticLogging());
    action(a,L"OnClearLog");assert(lastMessage.find(u"已清理")!=U::npos&&!diagnosticLogging());pass("open/clear log actions do not enable logging or write new log records");
    action(a,L"OnStatus");assert(lastTitle==u"运行状态 · CodeInk"&&lastMessage.find(u"Mock action completed")!=U::npos&&lastMessage.find(u"已关闭")!=U::npos&&lastMessage.find(u"完整列表暂不可用")!=U::npos);
    change(a,L"OnLogging",Value::boolean(true));action(a,L"OnStatus");assert(lastMessage.find(u"Mock action completed")!=U::npos&&lastMessage.find(u"已开启")!=U::npos);
    pass("runtime status reports fallback catalogs and current logging without overwriting last action");
    assert(!checked(a,L"GetNumbersEnabled"));change(a,L"OnNumbers",Value::boolean(true));assert(checked(a,L"GetNumbersEnabled"));
    change(a,L"OnFontSize",Value::text(L"200"));assert(lastMessage.find(u"字号")!=U::npos&&lastMessage.find(u"圆角")==U::npos);
    pass("line-number edit gates on line numbers and validation messages describe only the changed field");
    assert(readNumber(CONFIG_KEY,L"BasicCleanup",9)==1);
    change(a,L"OnBasicCleanup",Value::boolean(false));assert(!checked(a,L"GetBasicCleanup")&&readNumber(CONFIG_KEY,L"BasicCleanup",9)==0);
    change(a,L"OnBasicCleanup",Value::boolean(true));assert(checked(a,L"GetBasicCleanup"));
    change(a,L"OnReplace",Value::boolean(true));assert(checked(a,L"GetReplace"));
    pass("direct Ribbon cleanup/replacement toggles save and invalidate without opening settings");
    action(a,L"OnAppearanceSettings");assert(settingsOpened==1&&settingsAppearance);
    action(a,L"OnLayoutSettings");assert(settingsOpened==2&&!settingsAppearance);
    {Value v=get(a,L"GetWindowPadding");assert(u(v.v.bstrVal)==u"18");}
    assert(checked(a,L"GetEnabled"));pass("settings commands commit preferences and restore Ribbon availability");
    Addin reconnected;assert(reconnected.OnConnection(&app,0,nullptr,nullptr)==S_OK);
    change(reconnected,L"OnFontSize",Value::text(L"60"));
    {Value v=get(reconnected,L"GetFontSize");assert(u(v.v.bstrVal)==u"60");}
    assert(readNumber(CONFIG_KEY,L"FontSize100",0)==6000);
    assert(readNumber(CONFIG_KEY,L"BasicCleanup",9)==0);
    pass("explicit font size persists after reconnect despite legacy automatic-size preferences");
    std::printf("%d Ribbon/preference/logging simulations passed. NOT a Windows/PowerPoint UI test.\n",passed);
}
