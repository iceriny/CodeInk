// Standalone Windows visual check. Does not load Office or save preferences.
#include "settings.h"
extern "C" void EntryPoint() {
    auto* options = (Options*)alloc(sizeof(Options));
    if (!options) ExitProcess(1);
    new(options)Options;
    editSettings(nullptr, GetModuleHandleW(nullptr), *options, true);
    editSettings(nullptr, GetModuleHandleW(nullptr), *options, false);
    options->~Options(); dealloc(options); ExitProcess(0);
}
