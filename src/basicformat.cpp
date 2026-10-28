#include "basicformat.h"
namespace codeink {
namespace {
bool equal(const Ch* a, const Ch* b) {
    while (*a && *b) { Ch c = *a++; if (c >= 'A' && c <= 'Z') c += 32; if (c != *b++) return false; }
    return !*a && !*b;
}
bool space(Ch c) { return c == ' ' || c == '\t'; }
bool line(Ch c) { return c == '\r' || c == '\n' || c == '\v' || c == 0x2028 || c == 0x2029; }
bool word(Ch c) { return c >= 128 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$'; }
bool op(Ch c) { return c == '=' || c == '!' || c == '<' || c == '>' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '&' || c == '|' || c == '^' || c == '~' || c == '?' || c == ':'; }
bool tokenIs(const Ch* s, int begin, int end, const Ch* text) {
    int k = 0; while (text[k] && begin + k < end && text[k] == s[begin + k]) ++k;
    return !text[k] && begin + k == end;
}
bool oneOf(const Ch* s, int begin, int end, const Ch* const* words, int count) {
    for (int j = 0; j < count; ++j) if (tokenIs(s, begin, end, words[j])) return true;
    return false;
}
// Recognize simple balanced type argument lists before classifying << / >>.
// This is deliberately bounded and rejects expression operators. Ambiguous
// compact a<b>c is left looking like a template rather than guessing its AST.
int templateEnd(const Ch* s, int n, int begin) {
    if (begin + 1 >= n || s[begin + 1] == '<' || s[begin + 1] == '=') return -1;
    int depth = 1, round = 0, square = 0, limit = begin + 4096; if (limit > n) limit = n;
    bool content = false;
    for (int j = begin + 1; j < limit; ++j) {
        Ch c = s[j];
        if (c == '(') { ++round; continue; }
        if (c == ')' && round) { --round; continue; }
        if (c == '[') { ++square; continue; }
        if (c == ']' && square) { --square; continue; }
        if (round || square) { if (c == ';' || c == '{' || c == '}' || c == '"' || c == '\'') return -1; continue; }
        if (word(c)) { content = true; continue; }
        if (space(c) || line(c) || c == ',' || c == ':' || c == '*' || c == '=' || c == '.') continue;
        if (c == '&') { int next = j + 1; while (next < limit && (space(s[next]) || s[next] == '&')) ++next;
            if (next < limit && (s[next] == '>' || s[next] == ',')) continue; return -1; }
        if (c == '<') { if (j + 1 < limit && (s[j + 1] == '<' || s[j + 1] == '=')) return -1; ++depth; continue; }
        if (c == '>' && content) { if (!--depth) return j + 1; continue; }
        return -1;
    }
    return -1;
}
// Independent preprocessing-token check: whitespace edits must not join/split
// operators, numeric tokens, literal prefixes/suffixes, or digraphs.
int cppTokenEnd(const Ch* s, int n, int i) {
    int j = i;
    if (s[j] == '#' || (s[j] == '/' && j + 1 < n && s[j + 1] == '/')) {
        while (j < n && !line(s[j])) ++j; return j;
    }
    if (s[j] == '/' && j + 1 < n && s[j + 1] == '*') {
        j += 2; while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '/')) ++j;
        return j + 1 < n ? j + 2 : n;
    }
    if (s[j] == 'u' && j + 1 < n && s[j + 1] == '8') j += 2;
    else if (s[j] == 'u' || s[j] == 'U' || s[j] == 'L') ++j;
    bool raw = j + 1 < n && s[j] == 'R' && s[j + 1] == '"';
    if (raw) ++j;
    if (j < n && (s[j] == '"' || s[j] == '\'')) {
        Ch quote = s[j++];
        if (raw) {
            int delimiter = j; while (j < n && s[j] != '(' && j - delimiter <= 16) ++j;
            if (j == n || s[j] != '(') return n;
            int length = j++ - delimiter;
            while (j < n) {
                if (s[j] == ')') { int k = 0; while (k < length && j + 1 + k < n && s[j + 1 + k] == s[delimiter + k]) ++k;
                    if (k == length && j + k + 1 < n && s[j + k + 1] == '"') { j += k + 2; break; } }
                ++j;
            }
        } else {
            while (j < n) { if (s[j] == '\\') { j += j + 1 < n ? 2 : 1; continue; } if (s[j++] == quote) break; }
        }
        while (j < n && word(s[j])) ++j; return j;
    }
    j = i;
    if ((s[j] >= '0' && s[j] <= '9') || (s[j] == '.' && j + 1 < n && s[j + 1] >= '0' && s[j + 1] <= '9')) {
        ++j;
        while (j < n) {
            if (word(s[j]) || s[j] == '.' || (s[j] == '\'' && j + 1 < n && word(s[j + 1]))) { ++j; continue; }
            if ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E' || s[j - 1] == 'p' || s[j - 1] == 'P')) { ++j; continue; }
            break;
        }
        return j;
    }
    if (word(s[j])) { while (j < n && word(s[j])) ++j; return j; }
    const Ch* punct[] = {u"%:%:",u"<<=",u">>=",u"<=>",u"->*",u"...",u"##",u"::",u".*",u"->",u"++",u"--",u"<<",u">>",u"<=",u">=",u"==",u"!=",u"&&",u"||",u"*=",u"/=",u"%=",u"+=",u"-=",u"&=",u"^=",u"|=",u"<:",u":>",u"<%",u"%>",u"%:"};
    for (const Ch* token : punct) { int k = 0; while (token[k] && i + k < n && s[i + k] == token[k]) ++k;
        if (!token[k]) return i + k; }
    return i + 1;
}
bool sameCppTokens(const Ch* a, int an, const Ch* b, int bn) {
    int i = 0, j = 0;
    for (;;) {
        while (i < an && (space(a[i]) || line(a[i]))) ++i;
        while (j < bn && (space(b[j]) || line(b[j]))) ++j;
        if (i == an || j == bn) return i == an && j == bn;
        int ae = cppTokenEnd(a, an, i), be = cppTokenEnd(b, bn, j);
        if (ae - i != be - j) return false;
        while (i < ae) if (a[i++] != b[j++]) return false;
    }
}
// Macro arguments may be stringified (#) or token-pasted (##). Treat common
// uppercase macro calls as opaque; changing their spaces can change output.
int macroEnd(const Ch* s, int n, int begin) {
    int depth = 0; Ch quote = 0; bool block = false, comment = false;
    for (int j = begin; j < n; ++j) {
        Ch c = s[j];
        if (comment) { if (line(c)) comment = false; continue; }
        if (block) { if (c == '*' && j + 1 < n && s[j + 1] == '/') { block = false; ++j; } continue; }
        if (quote) { if (c == '\\' && j + 1 < n) ++j; else if (c == quote) quote = 0; continue; }
        if (c == '/' && j + 1 < n && s[j + 1] == '*') { block = true; ++j; continue; }
        if (c == '/' && j + 1 < n && s[j + 1] == '/') { comment = true; ++j; continue; }
        // Raw/continued macro arguments need a parser; retain the remainder.
        if (c == '\\' || (c == 'R' && j + 1 < n && s[j + 1] == '"')) return n;
        if (c == '"' || c == '\'') { quote = c; continue; }
        if (c == '(') ++depth;
        if (c == ')' && !--depth) return j + 1;
    }
    return n;
}
struct Writer {
    Ch* out; int cap, used = 0, floor = 0; bool failed = false;
    void put(Ch c) { if (used >= cap - 1) { failed = true; return; } out[used++] = c; if (line(c)) floor = used; }
    void trim() { while (used > floor && space(out[used - 1])) --used; }
    void gap() { trim(); if (used > floor) put(' '); }
    void copy(const Ch* s, int a, int b) { for (int j = a; j < b; ++j) put(s[j]); }
};
}
int basicFormat(const Ch* s, int n, const Ch* language, Ch* out, int cap, int indentWidth) {
    if (!s || n < 0 || !language || !out || cap < 1) return -1;
    if (indentWidth < 1 || indentWidth > 8) indentWidth = 4;
    bool python = equal(language, u"python") || equal(language, u"py");
    bool js = equal(language, u"javascript") || equal(language, u"js") || equal(language, u"typescript") || equal(language, u"ts");
    bool json = equal(language, u"json") || equal(language, u"jsonc");
    bool cpp = equal(language, u"cpp") || equal(language, u"c++") || equal(language, u"c") || equal(language, u"arduino");
    bool cs = equal(language, u"csharp") || equal(language, u"c#") || equal(language, u"cs");
    bool go = equal(language, u"go"), java = equal(language, u"java");
    bool known = python || js || json || cpp || cs || go || java;
    Writer w{out, cap};
    // Physical line splicing happens before tokenization, including in comments.
    // Retain these snippets instead of changing that preprocessing boundary.
    if (cpp) for (int j = 0; j + 1 < n; ++j) if (s[j] == '\\') {
        int end = j + 1; while (end < n && space(s[end])) ++end;
        if (end < n && line(s[end])) { w.copy(s, 0, n); out[w.used] = 0; return w.failed ? -1 : w.used; }
    }
    // Spaces can change shell arguments, Markdown breaks, YAML block scalars,
    // Make recipes, JSX text, etc. Unknown grammars are intentionally untouched.
    if (!known) { w.copy(s, 0, n); out[w.used] = 0; return w.failed ? -1 : w.used; }
    int depth = 0, round = 0, square = 0, base = -1;
    bool start = true, value = false, afterSpace = false, directive = false, opaqueTail = false;
    bool type = false, control = false, keyword = false, operatorName = false;
    int angleEnd = -1, forRound = -1;
    struct Conditional { int round, square, depth; };
    Conditional conditional[64]; int conditions = 0;
    bool annotatedParameter[128] = {};
    struct Name { int begin, end; } macros[64]; int macroCount = 0;
    if (cpp) for (int j = 0; j < n; ++j) if (s[j] == '#') {
        int k = j + 1; while (k < n && space(s[k])) ++k;
        int end = k; while (end < n && word(s[end])) ++end;
        if (!tokenIs(s, k, end, u"define")) continue;
        k = end; while (k < n && space(s[k])) ++k; end = k; while (end < n && word(s[end])) ++end;
        if (end > k && macroCount < 64) macros[macroCount++] = {k, end};
    }
    bool continuation = false;
    for (int i = 0; i < n && !w.failed;) {
        if (start) {
            int columns = 0;
            while (i < n && space(s[i])) { columns += s[i] == '\t' ? (python ? 8 : indentWidth) - columns % (python ? 8 : indentWidth) : 1; ++i; }
            if (base < 0 && i < n && !line(s[i])) base = columns;
            // Preserve continuation alignment and indentation-sensitive syntax.
            int indent = columns;
            if (cpp && !directive && i < n && !line(s[i]) && s[i] != '#') {
                bool closer = s[i] == ')' || s[i] == ']' || (i < angleEnd && s[i] == '>');
                bool leadingOperator = value && (s[i] == '+' || s[i] == '-' || s[i] == '<' || s[i] == '>' || s[i] == '&' || s[i] == '|');
                int extra = (round || square || i < angleEnd || continuation || leadingOperator) && !closer && s[i] != '}' ? 1 : 0;
                indent = (base < 0 ? 0 : base) + (depth - (s[i] == '}' && depth ? 1 : 0) + extra) * indentWidth;
            }
            if (!cpp && !python && !directive && !round && (!square || json) && i < n && !line(s[i]) && s[i] != '#') {
                int level = depth + (json ? square : 0) - ((s[i] == '}' && depth > 0) || (json && s[i] == ']' && square > 0) ? 1 : 0);
                indent = (base < 0 ? 0 : base) + level * indentWidth;
            }
            if (i == n || line(s[i])) indent = 0;
            for (int j = 0; j < indent; ++j) w.put(' ');
            w.floor = w.used; start = false; afterSpace = false;
            if (i == n) break;
            // Preprocessor directives and their continuation lines are opaque.
            if ((cpp || cs) && (s[i] == '#' || directive)) {
                int end = i; while (end < n && !line(s[end])) ++end;
                directive = end > i && s[end - 1] == '\\'; w.copy(s, i, end); i = end; opaqueTail = true;
            }
            if (i == n) break;
        }
        Ch c = s[i];
        if (line(c)) {
            int last = i - 1; while (last >= 0 && space(s[last])) --last;
            continuation = !opaqueTail && last >= 0 && last + 1 != angleEnd && (op(s[last]) || s[last] == ',') && s[last] != '!' && s[last] != '~';
            if (!opaqueTail) w.trim(); w.put(c); ++i; opaqueTail = false;
            if (c == '\r' && i < n && s[i] == '\n') w.put(s[i++]);
            start = true; afterSpace = false; continue;
        }
        if (space(c)) { opaqueTail = false; if (!afterSpace) w.put(' '); ++i; continue; }
        bool comment = (python && c == '#') || (!python && c == '/' && i + 1 < n && (s[i + 1] == '/' || s[i + 1] == '*'));
        if (comment) {
            int end = i + 1;
            if (!python && s[i + 1] == '*') { end = i + 2; while (end + 1 < n && !(s[end] == '*' && s[end + 1] == '/')) ++end; end = end + 1 < n ? end + 2 : n; }
            else { while (end < n && !line(s[end])) ++end; }
            w.copy(s, i, end); i = end; afterSpace = false; opaqueTail = true; continue;
        }
        // C++ raw strings can use arbitrary delimiters and contain quotes/newlines.
        if (cpp && c == 'R' && i + 1 < n && s[i + 1] == '"') {
            int open = i + 2; while (open < n && open - i <= 18 && s[open] != '(' && !line(s[open])) ++open;
            int end = n;
            if (open < n && s[open] == '(') for (int j = open + 1; j < n; ++j) if (s[j] == ')') {
                int k = 0, count = open - i - 2;
                while (k < count && j + 1 + k < n && s[j + 1 + k] == s[i + 2 + k]) ++k;
                if (k == count && j + 1 + count < n && s[j + 1 + count] == '"') { end = j + count + 2; break; }
            }
            w.copy(s, i, end); i = end; value = true; afterSpace = false; opaqueTail = true; continue;
        }
        if (js && c == '`') { w.copy(s, i, n); i = n; opaqueTail = true; continue; }
        if (c == '\'' || c == '"' || c == '`') {
            int end = i + 1;
            bool triple = (python || cs || equal(language, u"java")) && i + 2 < n && s[i + 1] == c && s[i + 2] == c;
            bool verbatim = cs && i > 0 && s[i - 1] == '@';
            if (cs && triple) { w.copy(s, i, n); i = n; opaqueTail = true; continue; }
            if (triple) end = i + 3;
            while (end < n) {
                if (!verbatim && c != '`' && s[end] == '\\') { end += end + 1 < n ? 2 : 1; continue; }
                if (c == '`' && !go && s[end] == '\\') { end += end + 1 < n ? 2 : 1; continue; }
                if (s[end] == c) {
                    if (triple) { if (end + 2 < n && s[end + 1] == c && s[end + 2] == c) { end += 3; break; } }
                    else if (verbatim && end + 1 < n && s[end + 1] == c) { end += 2; continue; }
                    else { ++end; break; }
                }
                ++end;
            }
            w.copy(s, i, end); i = end; value = true; afterSpace = false; opaqueTail = true; continue;
        }
        // Slash is ambiguous in JS (regex vs division). Preserve the remainder
        // of that line, including character classes and regex-internal spaces.
        if (js && c == '/') { int end = i; while (end < n && !line(s[end])) ++end; w.copy(s, i, end); i = end; afterSpace = false; opaqueTail = true; continue; }
        opaqueTail = false;
        if ((c >= '0' && c <= '9') || (c == '.' && i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '9')) {
            int end = i + 1; bool hex = c == '0' && end < n && (s[end] == 'x' || s[end] == 'X');
            while (end < n) {
                Ch d = s[end];
                if (word(d) || d == '.' || (cpp && d == '\'' && end + 1 < n && word(s[end + 1]))) { ++end; continue; }
                bool exponent = hex ? s[end - 1] == 'p' || s[end - 1] == 'P' : s[end - 1] == 'e' || s[end - 1] == 'E';
                if ((d == '+' || d == '-') && exponent && end + 1 < n && s[end + 1] >= '0' && s[end + 1] <= '9') { ++end; continue; }
                break;
            }
            w.copy(s, i, end); i = end; value = true; afterSpace = false; type = control = keyword = operatorName = false; continue;
        }
        if (word(c)) {
            int end = i + 1;
            while (end < n && word(s[end])) {
                if (cpp && s[end] == 'R' && end + 1 < n && s[end + 1] == '"') break;
                ++end;
            }
            bool uppercase = c >= 'A' && c <= 'Z';
            for (int k = i; k < end; ++k) if (s[k] >= 'a' && s[k] <= 'z') uppercase = false;
            int call = end;
            while (call < n) {
                if (space(s[call]) || (cpp && line(s[call]))) { ++call; continue; }
                if (cpp && s[call] == '/' && call + 1 < n && (s[call + 1] == '/' || s[call + 1] == '*')) {
                    call = cppTokenEnd(s, n, call); continue;
                }
                break;
            }
            bool macro = uppercase;
            for (int k = 0; cpp && !macro && k < macroCount; ++k) if (end - i == macros[k].end - macros[k].begin) {
                int t = 0; while (i + t < end && s[i + t] == s[macros[k].begin + t]) ++t; macro = i + t == end;
            }
            if (cpp && macro && call < n && s[call] == '(') {
                end = macroEnd(s, n, call); w.copy(s, i, end); i = end;
                value = true; type = control = keyword = operatorName = false; afterSpace = false; opaqueTail = true; continue;
            }
            bool interpolated = false;
            for (int k = i; k < end; ++k) if ((python && (s[k] == 'f' || s[k] == 'F')) || (cs && s[k] == '$')) interpolated = true;
            if (interpolated && end < n && (s[end] == '\'' || s[end] == '"' || (cs && s[end] == '@'))) {
                w.copy(s, i, n); i = n; opaqueTail = true; continue;
            }
            const Ch* controls[] = {u"if", u"for", u"while", u"switch", u"catch", u"foreach", u"with", u"lock", u"using", u"synchronized"};
            const Ch* prefixes[] = {u"return", u"throw", u"yield", u"new", u"delete", u"case", u"else", u"do", u"try", u"finally", u"await", u"not", u"and", u"or", u"in", u"is"};
            const Ch* types[] = {u"void", u"bool", u"char", u"short", u"int", u"long", u"float", u"double", u"signed", u"unsigned", u"auto", u"const", u"volatile", u"wchar_t", u"char8_t", u"char16_t", u"char32_t", u"string", u"decimal", u"object", u"var", u"byte"};
            control = oneOf(s, i, end, controls, sizeof(controls)/sizeof(*controls));
            keyword = oneOf(s, i, end, prefixes, sizeof(prefixes)/sizeof(*prefixes));
            bool mixedCase = false; for (int k = i + 1; k < end; ++k) if (s[k] >= 'a' && s[k] <= 'z') mixedCase = true;
            type = oneOf(s, i, end, types, sizeof(types)/sizeof(*types)) ||
                (c >= 'A' && c <= 'Z' && (mixedCase || tokenIs(s, i, end, u"T"))) || (end - i > 2 && s[end - 2] == '_' && s[end - 1] == 't');
            operatorName = tokenIs(s, i, end, u"operator");
            if (tokenIs(s, i, end, u"for")) forRound = round + 1;
            if (keyword && w.used > w.floor && w.out[w.used - 1] == '}') w.gap();
            w.copy(s, i, end); i = end; value = !keyword && !control; afterSpace = false; opaqueTail = false; continue;
        }
        bool ternaryColon = c == ':' && (i + 1 == n || s[i + 1] != ':') && (i == 0 || s[i - 1] != ':') && conditions > 0 && conditional[conditions - 1].round == round && conditional[conditions - 1].square == square && conditional[conditions - 1].depth == depth;
        bool ordinaryColon = c == ':' && !ternaryColon && (i + 1 == n || (s[i + 1] != '=' && s[i + 1] != ':')) && (i == 0 || s[i - 1] != ':');
        if (c == ',' || c == ';' || (ordinaryColon && (!python || !square))) {
            if (python && round > 0 && round < 128) {
                if (c == ':' && !square) annotatedParameter[round] = true;
                if (c == ',') annotatedParameter[round] = false;
            }
            if (c == ':' && round == forRound) w.gap(); else w.trim();
            w.put(c); ++i; opaqueTail = false; int next = i; while (next < n && space(s[next])) ++next;
            if (next < n && !line(s[next]) && s[next] != ')' && s[next] != ']' && s[next] != '}' && s[next] != ';') w.put(' ');
            afterSpace = true; value = false; type = control = keyword = operatorName = false; continue;
        }
        if (op(c)) {
            if (c == '<' && i >= angleEnd && value && !operatorName && (cpp || java || cs || js)) angleEnd = templateEnd(s, n, i);
            if (i < angleEnd) { w.put(c); ++i; afterSpace = false; value = c == '>'; type = value; continue; }
            int end = i + 1;
            const Ch* operators[] = {u">>>=", u"->*", u"===", u"!==", u"<=>", u">>>", u"**=", u"//=", u"<<=", u">>=", u"&&=", u"||=", (u"?" u"?="), u"++", u"--", u"->", u"::", u"**", u"//", u"&&", u"||", u"??", u"?.", u"<<", u">>", u"==", u"!=", u"<=", u">=", u"+=", u"-=", u"*=", u"/=", u"%=", u"&=", u"|=", u"^=", u"=>", u":="};
            for (const Ch* token : operators) {
                int k = 0; while (token[k] && i + k < n && token[k] == s[i + k]) ++k;
                if (!token[k]) { end = i + k; break; }
            }
            int next = end; while (next < n && space(s[next])) ++next;
            if (python && c == '=' && end == i + 1 && round > 0 && round < 128 && !annotatedParameter[round]) {
                w.trim(); w.put('='); i = end; afterSpace = true; value = false;
                type = control = keyword = operatorName = false; continue;
            }
            bool conditionalOp = end - i == 1 && c == '?' && !python && !go && value &&
                !(cs && type) && next < n && s[next] != ':' && s[next] != ';' && conditions < 64;
            if (conditionalOp) conditional[conditions++] = {round, square, depth};
            if (ternaryColon) --conditions;
            bool pointer = (cpp || cs) && type && (c == '*' || c == '&');
            bool shift = (end - i == 2 && ((c == '<' && s[i + 1] == '<') || (c == '>' && s[i + 1] == '>'))) ||
                (end - i == 3 && c == '>' && s[i + 1] == '>' && s[i + 2] == '>');
            bool binary = end - i == 1 && value && (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '&' || c == '|' || c == '^' || c == '<' || c == '>');
            bool padded = !operatorName && !pointer && (
                c == '=' || (end - i >= 2 && s[end - 1] == '=') || shift || binary ||
                (end - i == 3 && c == '<' && s[i + 1] == '=' && s[i + 2] == '>') ||
                (end - i == 2 && ((c == '&' && s[i + 1] == '&') || (c == '|' && s[i + 1] == '|') || (c == '?' && s[i + 1] == '?') || (c == '*' && s[i + 1] == '*' && value) || (python && c == '/' && s[i + 1] == '/'))) ||
                conditionalOp || ternaryColon);
            bool postfix = value && end - i == 2 && ((c == '+' && s[i + 1] == '+') || (c == '-' && s[i + 1] == '-'));
            if (padded) { w.gap(); w.copy(s, i, end); if (next < n && !line(s[next])) w.put(' '); }
            else w.copy(s, i, end);
            i = end; afterSpace = padded; value = postfix; type = pointer; control = keyword = operatorName = false; continue;
        }
        if (c == '(') {
            int next = i + 1; while (next < n && space(s[next])) ++next;
            bool declarator = cpp && type && next < n && (s[next] == '*' || s[next] == '&');
            if (control || declarator) w.gap(); else if (value && !keyword) w.trim();
            ++round; if (round < 128) annotatedParameter[round] = false;
            w.put(c); ++i; afterSpace = true; value = false; type = control = keyword = operatorName = false; continue;
        }
        if (c == '[') {
            if (value && !keyword) w.trim();
            ++square; w.put(c); ++i; afterSpace = true; value = false; type = control = keyword = operatorName = false; continue;
        }
        if (c == ')' || c == ']') {
            w.trim();
            if (c == ')' && round) { if (round == forRound) forRound = -1; --round; }
            if (c == ']' && square) --square;
            w.put(c); ++i; afterSpace = false; value = true; type = control = keyword = operatorName = false; continue;
        }
        if (c == '{') {
            if (w.used > w.floor && (w.out[w.used - 1] == ')' || keyword)) w.gap();
            ++depth; w.put(c); ++i; int next = i; while (next < n && space(s[next])) ++next;
            if (next < n && !line(s[next]) && s[next] != '}') w.put(' ');
            afterSpace = true; value = false; type = control = keyword = operatorName = false; continue;
        }
        if (c == '}') {
            w.trim(); if (w.used > w.floor && w.out[w.used - 1] != '{') w.gap();
            if (depth) --depth;
            w.put(c); ++i; afterSpace = false; value = true; type = control = keyword = operatorName = false; continue;
        }
        w.put(c); ++i; value = c == ')' || c == ']'; afterSpace = false;
    }
    if (!opaqueTail) w.trim(); out[w.used] = 0;
    if (!w.failed && cpp && !sameCppTokens(s, n, out, w.used)) {
        if (n >= cap) return -1; for (int j = 0; j < n; ++j) out[j] = s[j]; out[n] = 0; return n;
    }
    return w.failed ? -1 : w.used;
}
}
