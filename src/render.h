#pragma once
#include "engine.h"
#include "textutil.h"
enum class Action { Color, Format, Table, Plain, Clipboard, Borders };
struct Segment {
    Ref shape,range;Value original,text;int start,length,row,col;bool number;
    Segment():start(0),length(0),row(0),col(0),number(false){}
};
struct Capture {
    Ref shape,table,decoration;Value text;Items<Segment> segments;bool gutter,grouped;int rows,cols;float left,top;
    Capture():gutter(false),grouped(false),rows(0),cols(0),left(48),top(48){}
};
struct CaptureSet { Items<Capture> objects;int count,skipped;CaptureSet():count(0),skipped(0){} };
HRESULT captureSelection(IDispatch* app,CaptureSet& result,const Options& opts,bool format,const wchar_t** message);
HRESULT executeAction(IDispatch* app,const Options& opts,Action action,wchar_t* status,int sc,wchar_t* error,int ec,Progress& progress);
