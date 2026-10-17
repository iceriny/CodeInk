#include "settings.h"
#include "textutil.h"

namespace {
enum Control {
    Background = 100, BackgroundColor, ThemeStyles,
    Window = 110, Buttons, Shadow, Fill, WindowColor, Padding, Radius,
    ReadTable = 120, Merge, Replace, AutoFit, AutoMin, AutoMax, BasicCleanup
};
constexpr UINT InitDialog = 0x110, Command = 0x111, Close = 0x10, Destroy = 0x82;
constexpr UINT AddString = 0x143, SetSelection = 0x14E, GetSelection = 0x147;
constexpr DWORD Child = 0x50000000, TabStop = 0x10000;
const wchar_t* const StateKey = L"CodeInk.Settings.State";
struct State { Options draft; bool appearance; };

// Standard DLGTEMPLATE, in dialog units. Windows supplies font scaling,
// keyboard navigation, default buttons and the owner's modal message loop.
struct Template {
    WORD* words; int used;
    Template(): words((WORD*)alloc(16384)), used(0) {}
    ~Template() { dealloc(words); }
    void word(WORD v) { words[used++] = v; }
    void dword(DWORD v) { word((WORD)v); word((WORD)(v >> 16)); }
    void text(const wchar_t* value) { do { word((WORD)*value); } while (*value++); }
    void begin(const wchar_t* title, int height) {
        dword(0x80C808C0); // popup, caption, system menu, modal frame, center, font
        dword(0); word(0); word(0); word(0); word(354); word((WORD)height);
        word(0); word(0); text(title); word(9); text(L"Microsoft YaHei UI");
    }
    void item(WORD type, int id, const wchar_t* title, int x, int y, int w, int h, DWORD style = 0, DWORD ex = 0) {
        if (used & 1) word(0);
        dword(Child | style); dword(ex);
        word((WORD)x); word((WORD)y); word((WORD)w); word((WORD)h); word((WORD)id);
        word(0xFFFF); word(type); text(title); word(0); ++words[4];
    }
    void label(const wchar_t* title, int x, int y, int w, int h = 12) { item(0x82, -1, title, x, y, w, h); }
    void check(int id, const wchar_t* title, int x, int y, int w) { item(0x80, id, title, x, y, w, 14, TabStop | 3); }
    void edit(int id, int x, int y, int w) { item(0x81, id, L"", x, y, w, 15, TabStop | 0x80, 0x200); }
    void combo(int id, int x, int y, int w) { item(0x85, id, L"", x, y, w, 80, TabStop | 0x200003); }
    void finish(int y, const wchar_t* note = L"保存设置后，点击“高亮排版”或“生成代码块”应用。") {
        label(note, 12, y - 24, 330, 20);
        item(0x80, 1, L"保存(&S)", 222, y, 58, 18, TabStop | 1);
        item(0x80, 2, L"取消", 286, y, 56, 18, TabStop);
    }
};

void combo(HWND window, int id, const wchar_t* const* items, int count, int selected) {
    for (int i = 0; i < count; ++i) SendDlgItemMessageW(window, id, AddString, 0, (LPARAM)items[i]);
    SendDlgItemMessageW(window, id, SetSelection, selected, 0);
}
int selection(HWND window, int id) { return (int)SendDlgItemMessageW(window, id, GetSelection, 0, 0); }
bool checked(HWND window, int id) { return IsDlgButtonChecked(window, id) == 1; }
void colorText(wchar_t* text, DWORD color) {
    const wchar_t* hex = L"0123456789ABCDEF";
    text[0] = '#'; for (int i = 0; i < 6; ++i) text[i + 1] = hex[(color >> (20 - i * 4)) & 15]; text[7] = 0;
}
void decimalText(wchar_t* text, float value) {
    int scaled = (int)(value * 100 + 0.5f); number(text, scaled / 100);
    if (scaled % 100) {
        wchar_t tail[4] = {'.', (wchar_t)('0' + scaled / 10 % 10), (wchar_t)('0' + scaled % 10), 0};
        if (!tail[2] || tail[2] == '0') tail[2] = 0;
        catstr(text, 32, tail);
    }
}
void enable(HWND window, int id, bool enabled) { EnableWindow(GetDlgItem(window, id), enabled); }
void update(HWND window) {
    bool frame = checked(window, Window);
    enable(window, Background, !frame);
    enable(window, BackgroundColor, !frame && selection(window, Background) == 2);
    const int dependent[] = {Buttons, Shadow, Fill, Padding, Radius};
    for (int id : dependent) enable(window, id, frame);
    enable(window, WindowColor, frame && selection(window, Fill) == 1);
}
void initialize(HWND window, const State& state) {
    const Options& o = state.draft;
    if (!state.appearance) {
        const wchar_t* modes[] = {L"自动识别首列行号", L"读取全部单元格", L"将首列视为行号"};
        combo(window, ReadTable, modes, 3, o.tableRead);
        CheckDlgButton(window, Merge, o.merge); wchar_t limits[32];
        decimalText(limits, o.autoFitMin); SetDlgItemTextW(window, AutoMin, limits);
        decimalText(limits, o.autoFitMax); SetDlgItemTextW(window, AutoMax, limits);
        SendDlgItemMessageW(window, AutoMin, 0xC5, 16, 0); SendDlgItemMessageW(window, AutoMax, 0xC5, 16, 0);
        return;
    }
    const wchar_t* backgrounds[] = {L"无背景", L"跟随主题", L"自定义颜色"};
    const wchar_t* fills[] = {L"跟随主题", L"自定义颜色"};
    combo(window, Background, backgrounds, 3, o.background); combo(window, Fill, fills, 2, o.windowFill);
    CheckDlgButton(window, ThemeStyles, o.themeStyles); CheckDlgButton(window, Window, o.windowFrame);
    CheckDlgButton(window, Buttons, o.windowButtons); CheckDlgButton(window, Shadow, o.windowShadow);
    wchar_t text[32];
    colorText(text, o.backgroundColor); SetDlgItemTextW(window, BackgroundColor, text);
    colorText(text, o.windowColor); SetDlgItemTextW(window, WindowColor, text);
    decimalText(text, o.windowPadding); SetDlgItemTextW(window, Padding, text);
    decimalText(text, o.windowRadius); SetDlgItemTextW(window, Radius, text);
    SendDlgItemMessageW(window, BackgroundColor, 0xC5, 7, 0); SendDlgItemMessageW(window, WindowColor, 0xC5, 7, 0);
    SendDlgItemMessageW(window, Padding, 0xC5, 16, 0); SendDlgItemMessageW(window, Radius, 0xC5, 16, 0);
    update(window);
}
bool invalid(HWND window, int id, const wchar_t* message) {
    MessageBoxW(window, message, L"请检查输入", MB_OK | MB_ICONWARNING);
    SetFocus(GetDlgItem(window, id)); SendDlgItemMessageW(window, id, 0xB1, 0, -1); return false;
}
bool readColor(HWND window, int id, DWORD& value) {
    wchar_t text[16]; GetDlgItemTextW(window, id, text, 16); DWORD color = 0;
    if (slen(text) != 7 || text[0] != '#') return invalid(window, id, L"请输入六位颜色值，例如 #F3F3F6。");
    for (int i = 1; i < 7; ++i) {
        wchar_t c = text[i]; int d = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (d < 0) return invalid(window, id, L"颜色只能包含 # 和六位十六进制数字，例如 #F3F3F6。");
        color = (color << 4) | d;
    }
    value = color; return true;
}
bool readNumber(HWND window, int id, float& value, float maximum, const wchar_t* message) {
    wchar_t text[32]; GetDlgItemTextW(window, id, text, 32); float v = 0; int end = 0;
    if (!codeink::parseDecimal((const codeink::Ch*)text, v, end) || text[end] || v < 0 || v > maximum) return invalid(window, id, message);
    value = v; return true;
}
bool save(HWND window, State& state) {
    Options& o = state.draft;
    if (!state.appearance) {
        o.tableRead = selection(window, ReadTable); o.merge = checked(window, Merge);
        o.autoFit = true;
        float minimum = 0, maximum = 0;
        if (!readNumber(window, AutoMin, minimum, 96, L"自动字号下限请输入 6～96 pt。")) return false;
        if (minimum < 6) return invalid(window, AutoMin, L"自动字号下限不能小于 6 pt。");
        if (!readNumber(window, AutoMax, maximum, 96, L"自动字号上限请输入 6～96 pt。")) return false;
        if (maximum < minimum) return invalid(window, AutoMax, L"自动字号上限不能小于下限。");
        o.autoFitMin = minimum; o.autoFitMax = maximum; return true;
    }
    o.windowFrame = checked(window, Window); o.themeStyles = checked(window, ThemeStyles);
    o.background = selection(window, Background); o.windowFill = selection(window, Fill);
    o.windowButtons = checked(window, Buttons); o.windowShadow = checked(window, Shadow);
    if (!o.windowFrame && o.background == 2 && !readColor(window, BackgroundColor, o.backgroundColor)) return false;
    if (o.windowFrame) {
        if (o.windowFill == 1 && !readColor(window, WindowColor, o.windowColor)) return false;
        if (!readNumber(window, Padding, o.windowPadding, 60, L"内边距请输入 0～60 之间的数值，单位为磅。")) return false;
        if (!readNumber(window, Radius, o.windowRadius, 40, L"圆角请输入 0～40 之间的数值，单位为磅。")) return false;
    }
    return true;
}
LONG_PTR WINAPI dialogProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == InitDialog) {
        auto* state = (State*)lparam;
        if (!SetPropW(window, StateKey, state)) { EndDialog(window, -1); return TRUE; }
        initialize(window, *state); return TRUE;
    }
    auto* state = (State*)GetPropW(window, StateKey);
    if (message == Destroy) { RemovePropW(window, StateKey); return FALSE; }
    if (message == Close) { EndDialog(window, 2); return TRUE; }
    if (!state || message != Command) return FALSE;
    int id = (int)(wparam & 0xFFFF);
    if (id == 2) { EndDialog(window, 2); return TRUE; }
    if (id == 1) { if (save(window, *state)) EndDialog(window, 1); return TRUE; }
    if (state->appearance && (id == Window || id == Background || id == Fill)) { update(window); return TRUE; }
    return FALSE;
}
void appearanceTemplate(Template& t) {
    t.begin(L"外观设置 · CodeInk", 280);
    t.item(0x80, -1, L"代码背景与字形", 12, 10, 330, 68, 7);
    t.label(L"背景", 24, 29, 34); t.combo(Background, 62, 26, 114);
    t.label(L"颜色", 192, 29, 30); t.edit(BackgroundColor, 226, 26, 102);
    t.check(ThemeStyles, L"使用主题字形（粗体、斜体和下划线）", 24, 53, 296);
    t.item(0x80, -1, L"模拟窗口", 12, 88, 330, 134, 7);
    t.check(Window, L"启用模拟窗口", 24, 104, 126);
    t.check(Buttons, L"装饰按钮", 158, 104, 76); t.check(Shadow, L"柔和阴影", 250, 104, 78);
    t.label(L"底色", 24, 132, 34); t.combo(Fill, 62, 129, 114);
    t.label(L"颜色", 192, 132, 30); t.edit(WindowColor, 226, 129, 102);
    t.label(L"内边距", 24, 158, 38); t.edit(Padding, 62, 155, 82); t.label(L"pt", 150, 158, 24);
    t.label(L"圆角", 192, 158, 30); t.edit(Radius, 226, 155, 78); t.label(L"pt", 310, 158, 20);
    t.label(L"更新已有窗口：勾选“替换原对象”，再生成代码块。", 24, 181, 302, 32);
    t.finish(250);
}
void layoutTemplate(Template& t) {
    t.begin(L"生成选项 · CodeInk", 278);
    t.item(0x80, -1, L"读取现有表格", 12, 10, 330, 64, 7);
    t.label(L"读取方式", 24, 30, 54); t.combo(ReadTable, 86, 27, 240);
    t.label(L"首列数字属于代码或数据时，请选择“读取全部单元格”。", 24, 52, 302, 14);
    t.item(0x80, -1, L"多选对象", 12, 84, 330, 44, 7);
    t.check(Merge, L"将多个选中对象合并为一个代码块", 24, 102, 300);
    t.item(0x80, -1, L"自动字号范围", 12, 138, 330, 78, 7);
    t.label(L"下限", 24, 159, 30); t.edit(AutoMin, 62, 156, 72); t.label(L"pt", 140, 159, 20);
    t.label(L"上限", 192, 159, 30); t.edit(AutoMax, 226, 156, 72); t.label(L"pt", 304, 159, 20);
    t.label(L"仅在空间不足时使用，不限制手动字号。\n达到下限仍放不下时直接插入，可随后移动代码块。", 24, 182, 304, 28);
    t.finish(248, L"保存后，下次生成或粘贴时生效。");
}
} // namespace

bool editSettings(HWND owner, HINSTANCE module, Options& options, bool appearance) {
    auto* state = (State*)alloc(sizeof(State)); if (!state) return false;
    new(state)State{options, appearance}; Template t;
    if (!t.words) { state->~State(); dealloc(state); return false; }
    if (appearance) appearanceTemplate(t); else layoutTemplate(t);
    LONG_PTR result = DialogBoxIndirectParamW(module, t.words, owner, dialogProc, (LPARAM)state);
    if (result == 1) options = state->draft;
    if (result == -1) MessageBoxW(owner, L"无法打开设置窗口，请关闭后重试。", L"CodeInk", MB_OK | MB_ICONWARNING);
    state->~State(); dealloc(state); return result == 1;
}
