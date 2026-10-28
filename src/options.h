#pragma once
#include "common.h"
struct Options {
    wchar_t language[128],theme[128],themePath[2048],font[64];
    float fontSize,spacing;bool spacingPoints;
    int background,tableRead;DWORD backgroundColor;
    bool lineNumbers,merge,replace,themeStyles,basicCleanup;
    bool borderVertical,borderHorizontal,borderOuter,logging;
    int firstLine;
    int outputMode,windowFill; // 0: native table / theme; 1: grouped text boxes / custom.
    bool windowFrame,windowButtons,windowShadow;
    float windowPadding,windowRadius;DWORD windowColor;
    Options(){copystr(language,128,L"cpp");copystr(theme,128,L"github-light");themePath[0]=0;copystr(font,64,L"Consolas");fontSize=18;spacing=1.2f;spacingPoints=false;background=0;backgroundColor=0xFFFFFF;tableRead=0;borderVertical=true;borderHorizontal=false;borderOuter=false;logging=false;lineNumbers=false;merge=false;replace=false;basicCleanup=true;themeStyles=false;firstLine=1;outputMode=0;windowFrame=false;windowButtons=true;windowShadow=true;windowPadding=14;windowRadius=8;windowFill=0;windowColor=0x282C34;}
};
