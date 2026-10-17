#pragma once
#include "com.h"
#include "options.h"
#include "progress.h"
struct ColorToken { DWORD start,length,color,style; };
struct HighlightItem { DWORD length;Items<ColorToken> tokens;HighlightItem():length(0){} };
struct HighlightResult { DWORD fg,bg,gutter;Items<HighlightItem> items;wchar_t error[1100];HighlightResult():fg(0),bg(0xFFFFFF),gutter(0x6e7781){error[0]=0;} };
HRESULT invokeEngine(const Options& opts,const BSTR* texts,int count,HighlightResult& output,Progress* progress);
