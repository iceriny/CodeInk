#pragma once
#include "textutil.h"
namespace codeink {
// Whitespace-only cleanup, with conservative language guards. Keeps line breaks
// and opaque literals/comments intact. Returns -1 if the output would exceed cap.
int basicFormat(const Ch* source, int length, const Ch* language, Ch* output, int cap);
}
