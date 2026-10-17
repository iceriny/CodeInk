#pragma once
#include "common.h"
struct Value {
    VARIANT v;
    Value(){memset(&v,0,sizeof(v));}
    ~Value(){VariantClear(&v);}
    Value(const Value& other):Value(){VariantCopy(&v,&other.v);}
    Value& operator=(const Value& other){if(this!=&other){VariantClear(&v);VariantCopy(&v,&other.v);}return *this;}
    void clear(){VariantClear(&v);memset(&v,0,sizeof(v));}
    static Value integer(LONG x){Value r;r.v.vt=VT_I4;r.v.lVal=x;return r;}
    static Value real(float x){Value r;r.v.vt=VT_R4;r.v.fltVal=x;return r;}
    static Value boolean(bool x){Value r;r.v.vt=VT_BOOL;r.v.boolVal=x?-1:0;return r;}
    static Value text(const wchar_t* s){Value r;r.v.vt=VT_BSTR;r.v.bstrVal=SysAllocString(s?s:L"");return r;}
    static Value textLen(const wchar_t* s,int n){Value r;r.v.vt=VT_BSTR;r.v.bstrVal=SysAllocStringLen(s,n);return r;}
    LONG integerValue(LONG fallback=0)const{
        if(v.vt==VT_I4||v.vt==VT_UI4||v.vt==VT_INT)return v.lVal;
        if(v.vt==VT_I2)return v.iVal;if(v.vt==VT_BOOL)return v.boolVal;
        if(v.vt==VT_R4)return (LONG)v.fltVal;if(v.vt==VT_R8)return (LONG)v.dblVal;return fallback;
    }
    float realValue(float fallback=0)const{if(v.vt==VT_R4)return v.fltVal;if(v.vt==VT_R8)return (float)v.dblVal;return (float)integerValue((LONG)fallback);}
};
struct Ref {
    IDispatch* p;
    Ref():p(nullptr){}
    ~Ref(){if(p)p->Release();}
    Ref(const Ref& r):p(r.p){if(p)p->AddRef();}
    Ref& operator=(const Ref& r){set(r.p);return *this;}
    void set(IDispatch* x){if(x)x->AddRef();if(p)p->Release();p=x;}
    void take(IDispatch* x){if(p)p->Release();p=x;}
    operator IDispatch*()const{return p;}
    explicit operator bool()const{return p!=nullptr;}
};
inline const VARIANT* unwrap(const VARIANT* v){for(int i=0;v&&i<4;++i){if(v->vt==(VT_VARIANT|VT_BYREF))v=v->pvarVal;else return v;}return v;}
inline IDispatch* asDispatch(const VARIANT* v){v=unwrap(v);if(!v)return nullptr;if(v->vt==VT_DISPATCH)return v->pdispVal;if(v->vt==(VT_DISPATCH|VT_BYREF)&&v->ppdispVal)return *v->ppdispVal;return nullptr;}
inline LONG asLong(const VARIANT* v,LONG fallback=0){v=unwrap(v);if(!v)return fallback;if(v->vt==VT_I4||v->vt==VT_INT)return v->lVal;if(v->vt==VT_I2)return v->iVal;if(v->vt==VT_BOOL)return v->boolVal;return fallback;}
inline BSTR asText(const VARIANT* v){v=unwrap(v);if(!v)return nullptr;if(v->vt==VT_BSTR)return v->bstrVal;if(v->vt==(VT_BSTR|VT_BYREF)&&v->pbstrVal)return *v->pbstrVal;return nullptr;}
HRESULT invoke(IDispatch* object,const wchar_t* member,WORD flags,int count,const Value* args,Value* result=nullptr);
HRESULT object(IDispatch* parent,const wchar_t* member,Ref& output,int count=0,const Value* args=nullptr);
HRESULT put(IDispatch* parent,const wchar_t* member,const Value& value);
LONG getInt(IDispatch* parent,const wchar_t* member,LONG fallback=0);
float getFloat(IDispatch* parent,const wchar_t* member,float fallback=0);
HRESULT getText(IDispatch* parent,const wchar_t* member,Value& value);
