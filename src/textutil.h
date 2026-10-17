#pragma once
// Pure text/layout helpers. Syntax analysis is exclusively performed by Shiki.
namespace codeink {
using Ch=char16_t;
int normalizedLength(const Ch* s,int n);
int normalize(const Ch* s,int n,Ch* out,int capacity);
int lineCount(const Ch* s,int n);
int displayColumns(const Ch* s,int n);
bool numberSequence(const Ch* text,int n,int& expected,int& count);
bool parseDecimal(const Ch* s,float& value,int& end);
}
namespace codeink {
struct Rect { float x,y,w,h; };
bool choosePosition(const Rect* occupied,int count,float width,float height,float slideWidth,float slideHeight,float preferredX,float preferredY,float& x,float& y);
}

namespace codeink {
// Ranges into the full normalized source: blank lines, terminal newline and UTF-16
// offsets are preserved. Tokenization remains whole-document, never per line.
struct LineSpan { int start,length; };
int splitLines(const Ch* source,int length,LineSpan* output,int capacity);
int lineNumberDigits(int first,int count);
float narrowGutterWidth(int first,int count,float fontSize,float measuredTextWidth=-1);
struct WindowGeometry { float width,height,contentX,contentY,headerHeight,dotSize; };
WindowGeometry windowGeometry(float contentWidth,float contentHeight,float fontSize,float padding,bool buttons);
}
