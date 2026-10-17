#pragma once
#include "options.h"
// Commits settings only when the user saves a fully valid dialog.
bool editSettings(HWND owner, HINSTANCE module, Options& options, bool appearance);
