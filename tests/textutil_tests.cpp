#include "textutil.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
using namespace codeink;
int main(){
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
    // Row spans preserve interior blanks, terminal blank and original UTF-16 positions.
    LineSpan spans[10];std::u16string rows=u"a\r\n\r中😀\v";
    int parts=splitLines(rows.data(),rows.size(),spans,10);assert(parts==4);
    assert(spans[0].start==0&&spans[0].length==1);
    assert(spans[1].start==3&&spans[1].length==0);
    assert(spans[2].start==4&&spans[2].length==3);
    assert(spans[3].start==8&&spans[3].length==0);
    assert(splitLines(u"",0,spans,10)==1&&spans[0].length==0);
    assert(splitLines(u"a\nb",3,spans,1)==-1);
    assert(lineCount(u"a\u2028b\u2029",4)==3);
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
    ColumnLayout layout;
    assert(columnLayout(7,2,0,0,layout)&&layout.columns==2&&layout.rows==4);
    assert(layout.count(7,0)==4&&layout.count(7,1)==3&&layout.first(7,1)==4);
    assert(columnLayout(8,3,0,0,layout)&&layout.count(8,0)==3&&layout.count(8,1)==3&&layout.count(8,2)==2);
    assert(columnLayout(45,0,20,0,layout)&&layout.columns==3&&layout.rows==15);
    assert(columnLayout(80,3,20,0,layout)&&layout.columns==4&&layout.rows==20);
    assert(columnLayout(80,3,20,1,layout)&&layout.columns==3&&layout.rows==27);
    assert(!columnLayout(80,3,20,2,layout)&&layout.columns==3&&layout.rows==27);
    assert(columnLayout(1,5,0,0,layout)&&layout.columns==1);
    assert(columnLayout(200,0,1,0,layout)&&layout.columns==200);
    for(int lines=1;lines<=200;++lines)for(int columns=1;columns<=lines;++columns){
        assert(columnLayout(lines,columns,0,0,layout));int next=0,previous=201;
        for(int c=0;c<columns;++c){assert(layout.first(lines,c)==next);int n=layout.count(lines,c);assert(n>0&&n<=previous&&(!c||previous-n<=1));previous=n;next+=n;}
        assert(next==lines);
    }
    std::cout<<"PASS: UTF-16 line count/width, number-column detection, decimal settings, line spans, dynamic gutters, window geometry and balanced column layouts.\n";
}
