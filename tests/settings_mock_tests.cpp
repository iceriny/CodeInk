// Real dialog construction and callback logic, with Win32 controls simulated.
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "../src/settings.cpp"
using U = std::u16string;
static U u(const wchar_t* s) { return U((const char16_t*)s, (const char16_t*)s + slen(s)); }
static std::map<int, U> texts;
static std::map<int, int> values;
static std::map<int, bool> enabled;
static std::map<int, int> itemCounts;
static HANDLE property;
static LONG_PTR ended;
static int scenario, warnings, focusId;
static U warningText;
extern "C" HANDLE GetProcessHeap() { return (HANDLE)1; }
extern "C" void* HeapAlloc(HANDLE, DWORD, SIZE_T n) { return std::calloc(1, n); }
extern "C" BOOL HeapFree(HANDLE, DWORD, void* p) { std::free(p); return TRUE; }
extern "C" BOOL SetPropW(HWND, LPCWSTR, HANDLE p) { property = p; return TRUE; }
extern "C" HANDLE GetPropW(HWND, LPCWSTR) { return property; }
extern "C" HANDLE RemovePropW(HWND, LPCWSTR) { auto p = property; property = nullptr; return p; }
extern "C" BOOL EndDialog(HWND, LONG_PTR n) { ended = n; return TRUE; }
extern "C" HWND GetDlgItem(HWND, int id) { return (HWND)(LONG_PTR)id; }
extern "C" BOOL EnableWindow(HWND w, BOOL on) { enabled[(int)(LONG_PTR)w] = on; return TRUE; }
extern "C" HWND SetFocus(HWND w) { focusId = (int)(LONG_PTR)w; return nullptr; }
extern "C" BOOL CheckDlgButton(HWND, int id, UINT value) { values[id] = value; return TRUE; }
extern "C" UINT IsDlgButtonChecked(HWND, int id) { return values[id]; }
extern "C" BOOL SetDlgItemTextW(HWND, int id, LPCWSTR value) { texts[id] = u(value); return TRUE; }
extern "C" UINT GetDlgItemTextW(HWND, int id, LPWSTR out, int count) {
    const U& value = texts[id]; assert((int)value.size() < count);
    std::memcpy(out, value.data(), value.size() * 2); out[value.size()] = 0; return value.size();
}
extern "C" LRESULT SendDlgItemMessageW(HWND, int id, UINT message, WPARAM param, LPARAM) {
    if (message == AddString) ++itemCounts[id];
    if (message == SetSelection) values[id] = (int)param;
    if (message == GetSelection) return values[id];
    return 0;
}
extern "C" int MessageBoxW(HWND, LPCWSTR message, LPCWSTR, UINT) { ++warnings; warningText = u(message); return 1; }

static void checkTemplate(const WORD* data, bool appearance) {
    // Walk Windows' WORD/DWORD-aligned format and confirm every field exists,
    // every control is inside the client area, and IDs are unique.
    int count = data[4], width = data[7], height = data[8], i = 9;
    assert(width == 354 && count > 10);
    assert(data[i++] == 0 && data[i++] == 0);
    while (data[i++]) {} ++i; while (data[i++]) {} // title, point size, font
    std::map<int, bool> ids;
    for (int item = 0; item < count; ++item) {
        if (i & 1) ++i;
        int x = data[i + 4], y = data[i + 5], w = data[i + 6], h = data[i + 7], id = data[i + 8];
        assert(x >= 0 && x + w <= width && y >= 0 && y + h <= height);
        if (id != 65535) { assert(!ids[id]); ids[id] = true; }
        i += 9; assert(data[i++] == 65535); ++i;
        while (data[i++]) {} assert(data[i++] == 0);
    }
    assert(ids[1] && ids[2]);
    assert(appearance ? ids[Window] && ids[BackgroundColor] && ids[Radius] : ids[ReadTable] && ids[Merge] && !ids[Replace] && !ids[BasicCleanup]);
}
extern "C" LONG_PTR DialogBoxIndirectParamW(HINSTANCE, const void* data, HWND, DLGPROC proc, LPARAM param) {
    auto* state = (State*)param;
    checkTemplate((const WORD*)data, state->appearance);
    texts.clear(); values.clear(); enabled.clear(); itemCounts.clear(); ended = 0;
    HWND window = (HWND)1; proc(window, InitDialog, 0, param);
    if (state->appearance) {
        assert(itemCounts[Background] == 3 && itemCounts[Fill] == 2);
        assert(!enabled[Padding] && enabled[Background] && !enabled[BackgroundColor]);
        values[Window] = 1; proc(window, Command, Window, 0);
        assert(enabled[Padding] && !enabled[Background] && !enabled[WindowColor]);
        values[Fill] = 1; proc(window, Command, Fill, 0); assert(enabled[WindowColor]);
        texts[Padding] = u"22.5"; texts[Radius] = u"12"; texts[WindowColor] = u"#123aBc";
        if (scenario == 0) proc(window, Command, 2, 0);
        else if (scenario == 1) proc(window, Command, 1, 0);
        else if (scenario == 2) {
            texts[WindowColor] = u"#zzzzzz"; proc(window, Command, 1, 0);
            assert(ended == 0 && focusId == WindowColor && warnings > 0);
            proc(window, Close, 0, 0);
        } else if (scenario == 3) {
            texts[Padding] = u"61"; proc(window, Command, 1, 0);
            assert(ended == 0 && focusId == Padding); texts[Padding] = u"60";
            texts[Radius] = u"41"; proc(window, Command, 1, 0);
            assert(ended == 0 && focusId == Radius); texts[Radius] = u"0";
            proc(window, Command, 1, 0);
        } else if (scenario == 4) {
            values[Window] = 0; values[Background] = 2; proc(window, Command, Background, 0);
            assert(enabled[BackgroundColor] && !enabled[Padding]);
            texts[BackgroundColor] = u"#F3F3F6"; texts[Padding] = u"bad";
            proc(window, Command, 1, 0); // irrelevant disabled field must not block save
        }
    } else {
        assert(itemCounts[ReadTable] == 3); values[ReadTable] = 1; values[Merge] = 1; texts[AutoMin] = u"10"; texts[AutoMax] = u"24";
        if (scenario == 2) {
            texts[AutoMin] = u"5"; proc(window, Command, 1, 0); assert(!ended && focusId == AutoMin);
            texts[AutoMin] = u"12"; texts[AutoMax] = u"10"; proc(window, Command, 1, 0); assert(!ended && focusId == AutoMax);
            texts[AutoMax] = u"24"; proc(window, Command, 1, 0);
        } else proc(window, Command, scenario == 0 ? 2 : 1, 0);
    }
    proc(window, Destroy, 0, 0); assert(!property); return ended;
}
int main() {
    for (scenario = 0; scenario <= 4; ++scenario) {
        Options options;
        bool saved = editSettings(nullptr, nullptr, options, true);
        assert(saved == (scenario == 1 || scenario == 3 || scenario == 4));
        if (!saved) assert(!options.windowFrame && options.windowPadding == 14 && options.windowColor == 0x282C34);
        if (scenario == 1) assert(options.windowFrame && options.windowPadding == 22.5f && options.windowRadius == 12 && options.windowColor == 0x123ABC);
        if (scenario == 3) assert(options.windowPadding == 60 && options.windowRadius == 0);
        if (scenario == 4) assert(!options.windowFrame && options.background == 2 && options.backgroundColor == 0xF3F3F6 && options.windowPadding == 14);
    }
    for (scenario = 0; scenario <= 2; ++scenario) {
        Options options; bool saved = editSettings(nullptr, nullptr, options, false);
        assert(saved == (scenario != 0)); assert(!options.replace && options.merge == saved && options.autoFit);
        assert(options.autoFitMin == (scenario == 0 ? 8 : scenario == 1 ? 10 : 12));
        assert(options.autoFitMax == (saved ? 24 : 32));
        assert(options.tableRead == (saved ? 1 : 0));
        assert(options.basicCleanup);
    }
    std::puts("PASS: 8 settings scenarios: aligned templates, cancel, save, validation, disabled fields, generation preferences and automatic size limits");
}
