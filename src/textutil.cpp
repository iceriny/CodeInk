#include "textutil.h"
namespace codeink {
static bool newline(Ch c){return c=='\r'||c=='\n'||c=='\v'||c==0x2028||c==0x2029;}
int normalizedLength(const Ch*s,int n){if(!s||n<0)return -1;int result=0;for(int i=0;i<n;++i){if(s[i]=='\r'&&i+1<n&&s[i+1]=='\n')++i;result+=s[i]=='\t'?4:1;}return result;}
int normalize(const Ch*s,int n,Ch*out,int cap){int needed=normalizedLength(s,n);if(needed<0||!out||cap<needed+1)return -1;int k=0;for(int i=0;i<n;++i){if(newline(s[i])){if(s[i]=='\r'&&i+1<n&&s[i+1]=='\n')++i;out[k++]='\r';}else if(s[i]=='\t'){for(int j=0;j<4;++j)out[k++]=' ';}else out[k++]=s[i];}out[k]=0;return k;}
int lineCount(const Ch*s,int n){int result=1;for(int i=0;i<n;++i)if(newline(s[i])){++result;if(s[i]=='\r'&&i+1<n&&s[i+1]=='\n')++i;}return result;}
int displayColumns(const Ch*s,int n){int max=0,col=0;for(int i=0;i<n;++i){if(newline(s[i])){if(col>max)max=col;col=0;}else if(s[i]=='\t')col+=4;else if(s[i]>=0xDC00&&s[i]<=0xDFFF&&i&&s[i-1]>=0xD800&&s[i-1]<=0xDBFF){}else col+=s[i]>=0x2E80?2:1;}return col>max?col:max;}
bool numberSequence(const Ch*s,int n,int& expected,int& count){
    int i=0;if(n==0)return false;
    while(i<n){while(i<n&&(s[i]==' '||s[i]=='\t'))++i;int value=0,digits=0;
        while(i<n&&s[i]>='0'&&s[i]<='9'){if(++digits>7)return false;value=value*10+s[i++]-'0';}
        if(!digits||value<1)return false;if(expected<0)expected=value;if(value!=expected)return false;++expected;++count;
        while(i<n&&(s[i]==' '||s[i]=='\t'))++i;if(i==n)break;if(!newline(s[i]))return false;
        if(s[i]=='\r'&&i+1<n&&s[i+1]=='\n')++i;++i;
        // Permit a trailing paragraph mark supplied by Office.
        if(i==n)break;
    }
    return true;
}
bool parseDecimal(const Ch*s,float& value,int& end){
    if(!s)return false;int i=0;while(s[i]==' ')++i;float v=0;int digits=0;
    while(s[i]>='0'&&s[i]<='9'){if(++digits>6)return false;v=v*10+(s[i++]-'0');}
    if(s[i]=='.'){++i;float divisor=10;while(s[i]>='0'&&s[i]<='9'){if(++digits>9)return false;v+=(s[i++]-'0')/divisor;divisor*=10;}}
    if(!digits)return false;while(s[i]==' ')++i;value=v;end=i;return true;
}
}
namespace codeink {
bool choosePosition(const Rect* used,int count,float width,float height,float sw,float sh,float px,float py,float& x,float& y){
    const float margin=12, gap=10;
    if(width<=0||height<=0||width>sw-2*margin||height>sh-2*margin)return false;
    auto free=[&](float a,float b){
        if(a<margin||b<margin||a+width>sw-margin+0.1f||b+height>sh-margin+0.1f)return false;
        for(int i=0;i<count;++i){const Rect&r=used[i];if(a<r.x+r.w+gap&&a+width+gap>r.x&&b<r.y+r.h+gap&&b+height+gap>r.y)return false;}
        x=a;y=b;return true;
    };
    if(free(px,py))return true;
    for(int i=0;i<count;++i){const Rect&r=used[i];if(free(r.x+r.w+gap,r.y)||free(r.x,r.y+r.h+gap))return true;}
    if(free(margin,margin)||free(sw-margin-width,sh-margin-height))return true;
    for(float b=margin;b+height<=sh-margin+0.1f;b+=16)for(float a=margin;a+width<=sw-margin+0.1f;a+=16)if(free(a,b))return true;
    return false;
}
}

namespace codeink {
int splitLines(const Ch*s,int n,LineSpan*out,int capacity){
    if(!s||n<0||!out)return -1;int needed=lineCount(s,n);if(capacity<needed)return -1;
    int row=0,start=0;
    for(int i=0;i<n;++i)if(newline(s[i])){
        out[row++]={start,i-start};if(s[i]=='\r'&&i+1<n&&s[i+1]=='\n')++i;start=i+1;
    }
    out[row++]={start,n-start};return row;
}
int lineNumberDigits(int first,int count){
    int last=first+(count>0?count-1:0),digits=1;while(last>=10){last/=10;++digits;}return digits;
}
float narrowGutterWidth(int first,int count,float fs,float measured){
    // Four points on the left, six on the right, plus two points for rounding.
    // Measure the longest actual number in PowerPoint where possible.
    float textWidth=measured>0?measured:lineNumberDigits(first,count)*fs*0.62f;
    return textWidth+12.0f;
}
WindowGeometry windowGeometry(float w,float h,float fs,float pad,bool buttons){
    float header=buttons?(fs*1.65f>24?fs*1.65f:24):0;
    float dot=fs*0.5f;if(dot<6)dot=6;if(dot>12)dot=12;
    return {w+pad*2,h+pad*2+header,pad,pad+header,header,dot};
}
}
