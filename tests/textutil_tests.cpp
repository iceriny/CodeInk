#include "textutil.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
using namespace codeink;
static std::u16string norm(const std::u16string&s){int len=normalizedLength(s.data(),s.size());std::u16string out(len+1,u'\0');int wrote=normalize(s.data(),s.size(),out.data(),out.size());assert(wrote==len);out.resize(len);return out;}
int main(){
    assert(norm(u"a\tb")==u"a    b"); // four literal spaces, not a tab stop
    assert(norm(u"\t\tx")==u"        x");
    assert(norm(u"a\r\nb\nc\vd\r")==u"a\rb\rc\rd\r");
    assert(norm(u"// 中文😀\t注释")==u"// 中文😀    注释");
    assert(norm(u"")==u"");
    const std::u16string escaped=u"const char* s=\"\\t\\n\";";assert(norm(escaped)==escaped);
    char16_t small[2];assert(normalize(u"\t",1,small,2)==-1);assert(normalizedLength(nullptr,2)==-1);
    assert(lineCount(u"a\r\nb",4)==2);assert(lineCount(u"a\rb\rc",5)==3);assert(lineCount(u"",0)==1);
    assert(lineCount(u"\r\n",2)==2);assert(lineCount(u"x\v",2)==2);
    assert(displayColumns(u"// 中文😀",7)==9);assert(displayColumns(u"a\tb",3)==6);
    assert(displayColumns(u"abc\rx",5)==3);
    int expected=-1,count=0;assert(numberSequence(u"98\r99",5,expected,count)&&expected==100&&count==2);
    assert(numberSequence(u"100\r",4,expected,count)&&expected==101&&count==3);
    expected=-1;count=0;assert(!numberSequence(u"1\r3",3,expected,count));
    expected=-1;count=0;assert(!numberSequence(u"0",1,expected,count));
    expected=-1;count=0;assert(!numberSequence(u"1x",2,expected,count));
    expected=-1;count=0;assert(!numberSequence(u"1\r\r2",4,expected,count));
    expected=-1;count=0;assert(numberSequence(u" 1 \r\n2\t",7,expected,count));
    float value=0;int end=0;assert(parseDecimal(u" 1.25 倍",value,end)&&std::abs(value-1.25f)<1e-6&&end==6);
    assert(parseDecimal(u"24 pt",value,end)&&value==24&&end==3);
    assert(parseDecimal(u"999999",value,end)&&value==999999&&end==6);
    assert(!parseDecimal(u"-1",value,end));assert(!parseDecimal(u"abc",value,end));assert(!parseDecimal(nullptr,value,end));
    Rect occupied[]={{12,12,250,160}};float x=0,y=0;
    assert(choosePosition(occupied,1,200,100,960,540,12,190,x,y)&&x==12&&y==190);
    assert(choosePosition(occupied,1,200,100,960,540,12,12,x,y)&&x>=272);
    assert(!choosePosition(occupied,1,936,516,960,540,12,12,x,y));
    assert(choosePosition(nullptr,0,936,516,960,540,12,12,x,y));
    assert(!choosePosition(nullptr,0,1000,100,960,540,12,12,x,y));
    // Row spans preserve interior blanks, terminal blank and original UTF-16 positions.
    LineSpan spans[10];std::u16string rows=u"a\r\n\r中😀\v";
    int parts=splitLines(rows.data(),rows.size(),spans,10);assert(parts==4);
    assert(spans[0].start==0&&spans[0].length==1);
    assert(spans[1].start==3&&spans[1].length==0);
    assert(spans[2].start==4&&spans[2].length==3);
    assert(spans[3].start==8&&spans[3].length==0);
    assert(splitLines(u"",0,spans,10)==1&&spans[0].length==0);
    assert(splitLines(u"a\nb",3,spans,1)==-1);
    assert(norm(u"a\u2028b\u2029")==u"a\rb\r");
    assert(lineNumberDigits(1,9)==1&&lineNumberDigits(1,10)==2);
    assert(lineNumberDigits(98,2)==2&&lineNumberDigits(98,3)==3);
    assert(lineNumberDigits(998,3)==4&&lineNumberDigits(1000,1)==4);
    assert(std::abs(narrowGutterWidth(98,3,18,27)-39)<0.001);
    assert(narrowGutterWidth(98,3,18)>narrowGutterWidth(98,2,18));
    assert(narrowGutterWidth(1,3,24)>narrowGutterWidth(1,3,12));
    auto window=windowGeometry(200,100,18,14,true);
    assert(window.width==228&&std::abs(window.height-157.7)<0.01);
    assert(window.contentX==14&&std::abs(window.contentY-43.7)<0.01);
    assert(window.dotSize==9);
    auto bare=windowGeometry(200,100,18,0,false);
    assert(bare.width==200&&bare.height==100&&bare.contentY==0&&bare.headerHeight==0);
    // Deterministic mixed Unicode/newline/tab cases, using an independent string oracle.
    std::u16string symbols[]={u"中",u"😀",u"\t",u"x",u"\r\n",u"\v",u"\n"};
    for(int a=0;a<7;++a)for(int b=0;b<7;++b){auto src=symbols[a]+u"Q"+symbols[b];std::u16string expectedText;
        for(size_t i=0;i<src.size();++i){char16_t c=src[i];if(c=='\t')expectedText+=u"    ";else if(c=='\r'){expectedText+=u'\r';if(i+1<src.size()&&src[i+1]=='\n')++i;}else if(c=='\n'||c=='\v')expectedText+=u'\r';else expectedText+=c;}
        assert(norm(src)==expectedText);
    }
    std::cout<<"PASS: UTF-16 normalization, four-space tabs, line count/width, number-column detection, decimal settings, line spans, dynamic gutters, window geometry; 49 mixed-text cases.\n";
}
