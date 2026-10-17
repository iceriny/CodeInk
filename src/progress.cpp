#include "progress.h"
// At most one modal operation per process; Ribbon commands are re-entry guarded.
static Progress* active=nullptr;
static LRESULT WINAPI proc(HWND w,UINT m,WPARAM a,LPARAM b){
    if(m==0x10||(m==0x111&&(a&0xFFFF)==2)){if(active)active->cancel();return 0;}
    return DefWindowProcW(w,m,a,b);
}
Progress::Progress(HWND o,const wchar_t* title):window(nullptr),label(nullptr),owner(o),stopped(false),ownDisable(false){
    HINSTANCE module=GetModuleHandleW(nullptr);WNDCLASSW c={};c.lpfnWndProc=proc;c.hInstance=module;c.hbrBackground=(void*)16;c.lpszClassName=L"CodeInkProgressWindow02";RegisterClassW(&c);
    active=this;
    window=CreateWindowExW(0x00000100,c.lpszClassName,title,0x00C80000,0x80000000,0x80000000,520,170,owner,nullptr,module,nullptr);
    if(window){
        label=CreateWindowExW(0,L"STATIC",L"准备中…",0x50000000,22,18,465,58,window,nullptr,module,nullptr);
        HWND button=CreateWindowExW(0,L"BUTTON",L"取消",0x50010000,390,86,100,30,window,(void*)2,module,nullptr);
        void* font=GetStockObject(17);SendMessageW(label,0x0030,(WPARAM)font,1);SendMessageW(button,0x0030,(WPARAM)font,1);
        if(owner){EnableWindow(owner,FALSE);ownDisable=true;}ShowWindow(window,5);UpdateWindow(window);
    }
}
Progress::~Progress(){if(ownDisable)EnableWindow(owner,TRUE);if(window){DestroyWindow(window);UnregisterClassW(L"CodeInkProgressWindow02",GetModuleHandleW(nullptr));}if(active==this)active=nullptr;}
void Progress::text(const wchar_t* s){if(label){SetWindowTextW(label,s);UpdateWindow(window);}pump();}
bool Progress::pump(){
    MSG msg;int count=0;
    while(count++<100&&PeekMessageW(&msg,nullptr,0,0,1)){
        if(msg.message==0x100&&msg.wParam==27){stopped=true;continue;}
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    return !stopped;
}
bool runChild(const wchar_t*exe,wchar_t*cmd,DWORD timeout,Progress*p,DWORD& code,bool hidden){
    STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};code=0xFFFFFFFF;
    // Explicit executable: never use shell lookup and never interpolate source code.
    if(!CreateProcessW(exe,cmd,nullptr,nullptr,FALSE,hidden?0x08000000:0,nullptr,nullptr,&si,&pi)){code=GetLastError();return false;}
    CloseHandle(pi.hThread);DWORD start=GetTickCount();bool ok=true;
    for(;;){DWORD wait=WaitForSingleObject(pi.hProcess,60);if(wait==0)break;
        if(wait==0xFFFFFFFF||GetTickCount()-start>timeout||(p&&!p->pump())){TerminateProcess(pi.hProcess,0x80004004);WaitForSingleObject(pi.hProcess,5000);ok=false;break;}}
    if(!GetExitCodeProcess(pi.hProcess,&code))ok=false;CloseHandle(pi.hProcess);return ok&&code==0;
}
