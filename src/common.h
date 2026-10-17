#pragma once
#include "winapi.h"
extern const GUID IID_Unknown,IID_Dispatch,IID_ClassFactory,IID_Extensibility,IID_Ribbon,CLSID_CodeInk;
extern const wchar_t* const PROGID;
extern const wchar_t* const CLSID_TEXT;
extern const wchar_t* const ADDIN_KEY;
extern const wchar_t* const CONFIG_KEY;
extern const wchar_t* const VERSION;
void* alloc(SIZE_T size); void dealloc(void* p);
int slen(const wchar_t* s);
bool same(const wchar_t* a,const wchar_t* b); bool samei(const wchar_t* a,const wchar_t* b);
bool guidEq(REFIID a,REFIID b);
bool copystr(wchar_t* dst,int cap,const wchar_t* src);
bool catstr(wchar_t* dst,int cap,const wchar_t* src);
void number(wchar_t* dst,int value);
void hexcode(wchar_t* dst,DWORD value);
bool installDir(wchar_t* dst,int cap);
bool path(wchar_t* dst,int cap,const wchar_t* directory,const wchar_t* name);
bool regText(const wchar_t* key,const wchar_t* name,const wchar_t* value,DWORD view=0);
bool regNumber(const wchar_t* key,const wchar_t* name,DWORD value,DWORD view=0);
DWORD readNumber(const wchar_t* key,const wchar_t* name,DWORD fallback);
bool deleteKey(const wchar_t* key,DWORD view=0);
bool writeFile(const wchar_t* filename,const void* bytes,DWORD length);
bool diagnosticLogging();
void logEvent(const wchar_t* event,HRESULT hr=0);
#ifdef CODEINK_HOST_TEST
#include <cstring>
#include <new>
#else
extern "C" void* memset(void*,int,SIZE_T);
extern "C" void* memcpy(void*,const void*,SIZE_T);
extern "C" void* memmove(void*,const void*,SIZE_T);
void* operator new(SIZE_T,void* p) noexcept;
#endif
bool registerCom(const wchar_t* dllPath,DWORD view);
bool unregisterCom(DWORD view);
bool readRegText(const wchar_t* key,const wchar_t* name,wchar_t* output,int capacity,const wchar_t* fallback);
bool quoteArg(wchar_t* command,int capacity,const wchar_t* argument);
bool enginePaths(wchar_t* node,int nc,wchar_t* bridge,int bc);
struct FileBytes {
    BYTE* bytes;DWORD length;
    FileBytes():bytes(nullptr),length(0){}~FileBytes(){dealloc(bytes);}
    FileBytes(const FileBytes&)=delete;FileBytes& operator=(const FileBytes&)=delete;
    bool load(const wchar_t* filename,DWORD maximum);
};
template<class T> class Items {
    T* p;int n;
public:
    Items():p(nullptr),n(0){}~Items(){clear();}
    Items(const Items&)=delete;Items& operator=(const Items&)=delete;
    bool make(int count){clear();if(count<0)return false;if(!count)return true;p=(T*)alloc(sizeof(T)*(SIZE_T)count);if(!p)return false;n=count;for(int i=0;i<n;++i)new(p+i)T();return true;}
    void clear(){if(p){for(int i=0;i<n;++i)p[i].~T();dealloc(p);}p=nullptr;n=0;}
    T& operator[](int i){return p[i];}const T& operator[](int i)const{return p[i];}
    int size()const{return n;}T* data(){return p;}const T* data()const{return p;}
};
