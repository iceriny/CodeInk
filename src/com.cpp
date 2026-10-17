#include "com.h"
HRESULT invoke(IDispatch* obj,const wchar_t* member,WORD flags,int count,const Value* args,Value* result){
    if(!obj||count<0||count>8||(count&&!args))return E_POINTER;
    GUID nullId={};DISPID id=0;LPWSTR name=(LPWSTR)member;
    HRESULT hr=obj->GetIDsOfNames(nullId,&name,1,0x0409,&id);if(failed(hr))return hr;
    VARIANT reversed[8];for(int i=0;i<count;++i)reversed[i]=args[count-1-i].v;
    DISPID putId=-3;DISPPARAMS params={count?reversed:nullptr,nullptr,(UINT)count,0};
    if(flags&DISPATCH_PROPERTYPUT){params.rgdispidNamedArgs=&putId;params.cNamedArgs=1;}
    EXCEPINFO ex={};UINT argError=0;Value ignored;
    if(result)result->clear();
    hr=obj->Invoke(id,nullId,0x0409,flags,&params,result?&result->v:&ignored.v,&ex,&argError);
    if(ex.bstrSource)SysFreeString(ex.bstrSource);if(ex.bstrDescription)SysFreeString(ex.bstrDescription);if(ex.bstrHelpFile)SysFreeString(ex.bstrHelpFile);
    return hr;
}
HRESULT object(IDispatch* parent,const wchar_t* member,Ref& out,int count,const Value* args){
    Value value;HRESULT hr=invoke(parent,member,DISPATCH_METHOD|DISPATCH_PROPERTYGET,count,args,&value);if(failed(hr))return hr;
    IDispatch* p=asDispatch(&value.v);if(p){out.set(p);return S_OK;}
    if(value.v.vt==VT_UNKNOWN&&value.v.punkVal){IDispatch*d=nullptr;hr=value.v.punkVal->QueryInterface(IID_Dispatch,(void**)&d);if(!failed(hr))out.take(d);return hr;}
    return E_FAIL;
}
HRESULT put(IDispatch* parent,const wchar_t* member,const Value& value){return invoke(parent,member,DISPATCH_PROPERTYPUT,1,&value);}
LONG getInt(IDispatch* parent,const wchar_t* member,LONG fallback){Value v;if(failed(invoke(parent,member,DISPATCH_PROPERTYGET,0,nullptr,&v)))return fallback;return v.integerValue(fallback);}
float getFloat(IDispatch* parent,const wchar_t* member,float fallback){Value v;if(failed(invoke(parent,member,DISPATCH_PROPERTYGET,0,nullptr,&v)))return fallback;return v.realValue(fallback);}
HRESULT getText(IDispatch* parent,const wchar_t* member,Value& v){HRESULT hr=invoke(parent,member,DISPATCH_PROPERTYGET,0,nullptr,&v);if(failed(hr))return hr;return v.v.vt==VT_BSTR?S_OK:E_FAIL;}
