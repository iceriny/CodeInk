#pragma once
// Minimal Windows ABI declarations. No Windows SDK redistribution or CRT dependency.
// Sizes and calling conventions follow the Windows SDK / MS-DTYP ABI.
#ifdef CODEINK_HOST_TEST
// Test-only Linux COM simulator; Windows builds keep the original ABI below.
#define WINAPI
#define STDMETHODCALLTYPE
#define API(ret,name,args) extern "C" ret name args
using BYTE=unsigned char;using WORD=unsigned short;using DWORD=__UINT32_TYPE__;
using LONG=__INT32_TYPE__;using ULONG=__UINT32_TYPE__;using UINT=unsigned int;using BOOL=int;
using HRESULT=__INT32_TYPE__;using LCID=__UINT32_TYPE__;using DISPID=__INT32_TYPE__;
#else
#define WINAPI __stdcall
#define STDMETHODCALLTYPE __stdcall
#if defined(_M_IX86) || defined(__i386__)
#define IMPORT_NAME(name) __asm__("_" #name)
#else
#define IMPORT_NAME(name)
#endif
#define API(ret,name,args) extern "C" __declspec(dllimport) ret WINAPI name args IMPORT_NAME(name)
using BYTE=unsigned char; using WORD=unsigned short; using DWORD=unsigned long;
using LONG=long; using ULONG=unsigned long; using UINT=unsigned int; using BOOL=int;
using HRESULT=long; using LCID=unsigned long; using DISPID=long;
#endif
using ULONG_PTR=__UINTPTR_TYPE__; using LONG_PTR=__INTPTR_TYPE__; using SIZE_T=__SIZE_TYPE__;
using HANDLE=void*; using HMODULE=void*; using HINSTANCE=void*; using HWND=void*;
using HKEY=void*; using LPWSTR=wchar_t*; using LPCWSTR=const wchar_t*; using BSTR=wchar_t*;
using VARIANT_BOOL=short; using LRESULT=LONG_PTR; using WPARAM=ULONG_PTR; using LPARAM=LONG_PTR;
struct GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; };
using REFIID=const GUID&; using REFCLSID=const GUID&;
struct IUnknown; struct IDispatch; struct ITypeInfo; struct SAFEARRAY;
struct VARIANT {
    WORD vt,wReserved1,wReserved2,wReserved3;
    union { long long llVal; LONG lVal; BYTE bVal; short iVal; float fltVal; double dblVal;
        VARIANT_BOOL boolVal; HRESULT scode; BSTR bstrVal; IUnknown* punkVal; IDispatch* pdispVal;
        SAFEARRAY* parray; void* byref; VARIANT* pvarVal; BSTR* pbstrVal; IDispatch** ppdispVal;
        struct {void* pvRecord; void* pRecInfo;} record;
    };
};
struct DISPPARAMS { VARIANT* rgvarg; DISPID* rgdispidNamedArgs; UINT cArgs,cNamedArgs; };
struct EXCEPINFO { WORD wCode,wReserved; BSTR bstrSource,bstrDescription,bstrHelpFile; DWORD dwHelpContext; void* pvReserved; void* pfnDeferredFillIn; HRESULT scode; };
struct IUnknown { virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)=0; virtual ULONG STDMETHODCALLTYPE AddRef()=0; virtual ULONG STDMETHODCALLTYPE Release()=0; };
struct IDispatch: IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPWSTR*,UINT,LCID,DISPID*)=0;
    virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*)=0;
};
struct IClassFactory: IUnknown { virtual HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown*,REFIID,void**)=0; virtual HRESULT STDMETHODCALLTYPE LockServer(BOOL)=0; };
struct IDTExtensibility2: IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnConnection(IDispatch*,LONG,IDispatch*,SAFEARRAY**)=0;
    virtual HRESULT STDMETHODCALLTYPE OnDisconnection(LONG,SAFEARRAY**)=0;
    virtual HRESULT STDMETHODCALLTYPE OnAddInsUpdate(SAFEARRAY**)=0;
    virtual HRESULT STDMETHODCALLTYPE OnStartupComplete(SAFEARRAY**)=0;
    virtual HRESULT STDMETHODCALLTYPE OnBeginShutdown(SAFEARRAY**)=0;
};
struct IRibbonExtensibility: IDispatch { virtual HRESULT STDMETHODCALLTYPE GetCustomUI(BSTR,BSTR*)=0; };
constexpr HRESULT S_OK=0,S_FALSE=1,E_NOINTERFACE=(HRESULT)0x80004002L,E_POINTER=(HRESULT)0x80004003L,E_FAIL=(HRESULT)0x80004005L,E_OUTOFMEMORY=(HRESULT)0x8007000EL,E_NOTIMPL=(HRESULT)0x80004001L;
constexpr HRESULT CLASS_E_NOAGGREGATION=(HRESULT)0x80040110L,CLASS_E_CLASSNOTAVAILABLE=(HRESULT)0x80040111L;
constexpr HRESULT DISP_E_UNKNOWNNAME=(HRESULT)0x80020006L,DISP_E_MEMBERNOTFOUND=(HRESULT)0x80020003L,DISP_E_BADPARAMCOUNT=(HRESULT)0x8002000EL;
constexpr WORD VT_EMPTY=0,VT_I2=2,VT_I4=3,VT_R4=4,VT_R8=5,VT_BSTR=8,VT_DISPATCH=9,VT_BOOL=11,VT_VARIANT=12,VT_UNKNOWN=13,VT_UI4=19,VT_INT=22,VT_BYREF=0x4000;
constexpr WORD DISPATCH_METHOD=1,DISPATCH_PROPERTYGET=2,DISPATCH_PROPERTYPUT=4;
constexpr BOOL TRUE=1,FALSE=0;
constexpr DWORD KEY_READ=0x20019,KEY_WRITE=0x20006,KEY_WOW64_64KEY=0x100,KEY_WOW64_32KEY=0x200;
#define HKCU ((HKEY)(LONG_PTR)(LONG)0x80000001)
#define HKLM ((HKEY)(LONG_PTR)(LONG)0x80000002)
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
constexpr DWORD REG_SZ=1,REG_DWORD=4;
constexpr UINT MB_OK=0,MB_OKCANCEL=1,MB_YESNO=4,MB_ICONERROR=16,MB_ICONQUESTION=32,MB_ICONWARNING=48,MB_ICONINFORMATION=64,MB_DEFBUTTON2=256;
constexpr int IDOK=1,IDCANCEL=2,IDYES=6;
inline bool failed(HRESULT hr) { return hr<0; }
struct FILETIME { DWORD dwLowDateTime,dwHighDateTime; };
struct PROCESSENTRY32W { DWORD dwSize,cntUsage,th32ProcessID; ULONG_PTR th32DefaultHeapID; DWORD th32ModuleID,cntThreads,th32ParentProcessID; LONG pcPriClassBase; DWORD dwFlags; wchar_t szExeFile[260]; };
struct STARTUPINFOW { DWORD cb; LPWSTR lpReserved,lpDesktop,lpTitle; DWORD dwX,dwY,dwXSize,dwYSize,dwXCountChars,dwYCountChars,dwFillAttribute,dwFlags; WORD wShowWindow,cbReserved2; BYTE* lpReserved2; HANDLE hStdInput,hStdOutput,hStdError; };
struct PROCESS_INFORMATION { HANDLE hProcess,hThread; DWORD dwProcessId,dwThreadId; };
API(HANDLE,GetProcessHeap,());
API(void*,HeapAlloc,(HANDLE,DWORD,SIZE_T));
API(BOOL,HeapFree,(HANDLE,DWORD,void*));
API(DWORD,GetLastError,());
API(DWORD,GetEnvironmentVariableW,(LPCWSTR,LPWSTR,DWORD));
API(DWORD,GetModuleFileNameW,(HMODULE,LPWSTR,DWORD));
API(BOOL,CreateDirectoryW,(LPCWSTR,void*));
API(HANDLE,CreateFileW,(LPCWSTR,DWORD,DWORD,void*,DWORD,DWORD,HANDLE));
API(BOOL,WriteFile,(HANDLE,const void*,DWORD,DWORD*,void*));
API(BOOL,CloseHandle,(HANDLE));
API(BOOL,DeleteFileW,(LPCWSTR));
API(BOOL,RemoveDirectoryW,(LPCWSTR));
API(BOOL,CopyFileW,(LPCWSTR,LPCWSTR,BOOL));
API(DWORD,GetFileAttributesW,(LPCWSTR));
API(BOOL,IsWow64Process,(HANDLE,BOOL*));
API(HANDLE,GetCurrentProcess,());
API(DWORD,GetCurrentProcessId,());
API(DWORD,GetTempPathW,(DWORD,LPWSTR));
API(LPWSTR,GetCommandLineW,());
API(BOOL,CreateProcessW,(LPCWSTR,LPWSTR,void*,void*,BOOL,DWORD,void*,LPCWSTR,STARTUPINFOW*,PROCESS_INFORMATION*));
API(HANDLE,OpenProcess,(DWORD,BOOL,DWORD));
API(DWORD,WaitForSingleObject,(HANDLE,DWORD));
API(void,Sleep,(DWORD));
API(void,ExitProcess,(UINT));
API(BOOL,DisableThreadLibraryCalls,(HMODULE));
inline LONG InterlockedIncrement(volatile LONG* p){return __atomic_add_fetch(p,(LONG)1,__ATOMIC_SEQ_CST);}
inline LONG InterlockedDecrement(volatile LONG* p){return __atomic_sub_fetch(p,(LONG)1,__ATOMIC_SEQ_CST);}
API(HANDLE,CreateToolhelp32Snapshot,(DWORD,DWORD));
API(BOOL,Process32FirstW,(HANDLE,PROCESSENTRY32W*));
API(BOOL,Process32NextW,(HANDLE,PROCESSENTRY32W*));
API(int,WideCharToMultiByte,(UINT,DWORD,LPCWSTR,int,char*,int,const char*,BOOL*));
API(int,MessageBoxW,(HWND,LPCWSTR,LPCWSTR,UINT));
API(HWND,GetForegroundWindow,());
API(LONG,RegCreateKeyExW,(HKEY,LPCWSTR,DWORD,LPWSTR,DWORD,DWORD,void*,HKEY*,DWORD*));
API(LONG,RegOpenKeyExW,(HKEY,LPCWSTR,DWORD,DWORD,HKEY*));
API(LONG,RegSetValueExW,(HKEY,LPCWSTR,DWORD,DWORD,const BYTE*,DWORD));
API(LONG,RegQueryValueExW,(HKEY,LPCWSTR,DWORD*,DWORD*,BYTE*,DWORD*));
API(LONG,RegCloseKey,(HKEY));
API(LONG,RegDeleteTreeW,(HKEY,LPCWSTR));
API(LONG,RegDeleteKeyExW,(HKEY,LPCWSTR,DWORD,DWORD));
API(BSTR,SysAllocString,(LPCWSTR));
API(BSTR,SysAllocStringLen,(LPCWSTR,UINT));
API(UINT,SysStringLen,(BSTR));
API(void,SysFreeString,(BSTR));
API(HRESULT,VariantClear,(VARIANT*));
API(HRESULT,VariantCopy,(VARIANT*,const VARIANT*));
API(HRESULT,CoCreateInstance,(REFCLSID,IUnknown*,DWORD,REFIID,void**));
API(HRESULT,CoInitializeEx,(void*,DWORD));
API(void,CoUninitialize,());
API(HINSTANCE,ShellExecuteW,(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,int));
static_assert(sizeof(wchar_t)==2,"Windows WCHAR must be 16-bit");
static_assert(sizeof(LONG)==4,"Windows LONG must be 32-bit");
static_assert(sizeof(GUID)==16,"GUID ABI");
static_assert(sizeof(VARIANT)==(sizeof(void*)==8?24:16),"VARIANT ABI");
struct SYSTEM_INFO { union {DWORD dwOemId;struct {WORD wProcessorArchitecture,wReserved;};}; DWORD dwPageSize;void* lpMinimumApplicationAddress;void* lpMaximumApplicationAddress;ULONG_PTR dwActiveProcessorMask;DWORD dwNumberOfProcessors,dwProcessorType,dwAllocationGranularity;WORD wProcessorLevel,wProcessorRevision;};
API(void,GetNativeSystemInfo,(SYSTEM_INFO*));
API(BOOL,GetExitCodeProcess,(HANDLE,DWORD*));
// Additional Windows APIs used by the v0.2 local-process bridge and UI.
struct POINT { LONG x,y; };
struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; DWORD lPrivate; };
using WNDPROC=LRESULT (WINAPI*)(HWND,UINT,WPARAM,LPARAM);
struct WNDCLASSW { UINT style; WNDPROC lpfnWndProc; int cbClsExtra,cbWndExtra; HINSTANCE hInstance; void* hIcon;void* hCursor;void* hbrBackground;LPCWSTR lpszMenuName,lpszClassName; };
struct OPENFILENAMEW { DWORD lStructSize;HWND hwndOwner;HINSTANCE hInstance;LPCWSTR lpstrFilter;LPWSTR lpstrCustomFilter;DWORD nMaxCustFilter,nFilterIndex;LPWSTR lpstrFile;DWORD nMaxFile;LPWSTR lpstrFileTitle;DWORD nMaxFileTitle;LPCWSTR lpstrInitialDir,lpstrTitle;DWORD Flags;WORD nFileOffset,nFileExtension;LPCWSTR lpstrDefExt;LPARAM lCustData;void* lpfnHook;LPCWSTR lpTemplateName;void* pvReserved;DWORD dwReserved,FlagsEx; };
API(BOOL,ReadFile,(HANDLE,void*,DWORD,DWORD*,void*));
API(DWORD,GetFileSize,(HANDLE,DWORD*));
API(DWORD,SetFilePointer,(HANDLE,LONG,LONG*,DWORD));
API(UINT,GetTempFileNameW,(LPCWSTR,LPCWSTR,UINT,LPWSTR));
API(DWORD,GetTickCount,());
API(BOOL,TerminateProcess,(HANDLE,UINT));
API(HMODULE,GetModuleHandleW,(LPCWSTR));
API(WORD,RegisterClassW,(const WNDCLASSW*));
API(HWND,CreateWindowExW,(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,void*,HINSTANCE,void*));
API(LRESULT,DefWindowProcW,(HWND,UINT,WPARAM,LPARAM));
API(BOOL,DestroyWindow,(HWND));
API(BOOL,ShowWindow,(HWND,int));
API(BOOL,UpdateWindow,(HWND));
API(BOOL,EnableWindow,(HWND,BOOL));
API(BOOL,SetWindowTextW,(HWND,LPCWSTR));
API(BOOL,PeekMessageW,(MSG*,HWND,UINT,UINT,UINT));
API(BOOL,TranslateMessage,(const MSG*));
API(LRESULT,DispatchMessageW,(const MSG*));
API(LRESULT,SendMessageW,(HWND,UINT,WPARAM,LPARAM));
API(void*,GetStockObject,(int));
API(BOOL,GetOpenFileNameW,(OPENFILENAMEW*));
API(BOOL,OpenClipboard,(HWND));
API(HANDLE,GetClipboardData,(UINT));
API(BOOL,CloseClipboard,());
API(void*,GlobalLock,(HANDLE));
API(BOOL,GlobalUnlock,(HANDLE));
API(SIZE_T,GlobalSize,(HANDLE));
API(HRESULT,URLDownloadToFileW,(IUnknown*,LPCWSTR,LPCWSTR,DWORD,IUnknown*));

API(BOOL,UnregisterClassW,(const wchar_t*,HINSTANCE));
using DLGPROC=LONG_PTR (WINAPI*)(HWND,UINT,WPARAM,LPARAM);
API(LONG_PTR,DialogBoxIndirectParamW,(HINSTANCE,const void*,HWND,DLGPROC,LPARAM));
API(BOOL,EndDialog,(HWND,LONG_PTR));
API(HWND,GetDlgItem,(HWND,int));
API(UINT,GetDlgItemTextW,(HWND,int,LPWSTR,int));
API(BOOL,SetDlgItemTextW,(HWND,int,LPCWSTR));
API(BOOL,CheckDlgButton,(HWND,int,UINT));
API(UINT,IsDlgButtonChecked,(HWND,int));
API(LRESULT,SendDlgItemMessageW,(HWND,int,UINT,WPARAM,LPARAM));
API(BOOL,SetPropW,(HWND,LPCWSTR,HANDLE));
API(HANDLE,GetPropW,(HWND,LPCWSTR));
API(HANDLE,RemovePropW,(HWND,LPCWSTR));
API(HWND,SetFocus,(HWND));

// Automation array helpers used by Shapes.Range(Array(names)).Group.
API(SAFEARRAY*,SafeArrayCreateVector,(WORD,LONG,ULONG));
API(HRESULT,SafeArrayPutElement,(SAFEARRAY*,LONG*,void*));
