#pragma once
#include "common.h"
class Progress {
    HWND window,label,owner;bool stopped;bool ownDisable;
public:
    Progress(HWND owner,const wchar_t* title);~Progress();
    void text(const wchar_t* value);bool pump();void cancel(){stopped=true;}
};
bool runChild(const wchar_t* executable,wchar_t* command,DWORD timeout,Progress* progress,DWORD& exitCode,bool hidden=true);
