#include "menubar_scan.h"

#include <algorithm>
#include <vector>

// Bytes from 0x80 up are parts of UTF-8 characters, which TJS allows in names.
static bool isNameCharacter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           (unsigned char)c >= 0x80;
}

static bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

// The operator at `at`, longest first.
static std::string_view operatorAt(std::string_view line, size_t at)
{
    for (std::string_view op : {"===", "!==", "==", "!="}) {
        if (line.substr(at, op.size()) == op)
            return op;
    }
    return {};
}

static bool meansHidden(std::string_view op, std::string_view value)
{
    if (op == "==" || op == "===")
        return value == "false" || value == "0" || value == "void" || value == "null";
    if (op == "!=" || op == "!==")
        return value == "true" || value == "1";
    return false;
}

// What the `menu.visible` that runs from `start` to `end` in `line` is tested for, or nullptr when it isn't tested
// for false.
static const char *hiddenTest(std::string_view line, size_t start, size_t end)
{
    // Back over the object it belongs to, such as `kag.` or `global.kag.`.
    size_t before = start;
    while (before > 0 && (isNameCharacter(line[before - 1]) || line[before - 1] == '.'))
        before--;
    size_t back = before;
    while (back > 0 && (isSpace(line[back - 1]) || line[back - 1] == '('))
        back--;
    if (back > 0 && line[back - 1] == '!')
        return "negation";

    // `menu.visible == false` and the like.
    size_t after = end;
    while (after < line.size() && isSpace(line[after]))
        after++;
    if (std::string_view op = operatorAt(line, after); !op.empty()) {
        size_t from = after + op.size();
        while (from < line.size() && isSpace(line[from]))
            from++;
        size_t to = from;
        while (to < line.size() && isNameCharacter(line[to]))
            to++;
        if (meansHidden(op, line.substr(from, to - from)))
            return "compares with false";
    }

    // `false == menu.visible`.
    back = before;
    while (back > 0 && isSpace(line[back - 1]))
        back--;
    for (std::string_view op : {"===", "!==", "==", "!="}) {
        if (back < op.size() || line.substr(back - op.size(), op.size()) != op)
            continue;
        size_t to = back - op.size();
        while (to > 0 && isSpace(line[to - 1]))
            to--;
        size_t from = to;
        while (from > 0 && isNameCharacter(line[from - 1]))
            from--;
        if (meansHidden(op, line.substr(from, to - from)))
            return "compares with false";
        break;
    }
    return nullptr;
}

// The rule a test of `menu.visible` for false in `code` matches, or nullptr.
static const char *hiddenTestIn(std::string_view code)
{
    for (size_t at = code.find("menu"); at != std::string_view::npos; at = code.find("menu", at + 1)) {
        if (at > 0 && isNameCharacter(code[at - 1]))
            continue;
        size_t p = at + 4;
        while (p < code.size() && isSpace(code[p]))
            p++;
        if (p >= code.size() || code[p] != '.')
            continue;
        p++;
        while (p < code.size() && isSpace(code[p]))
            p++;
        if (code.substr(p, 7) != "visible" || (p + 7 < code.size() && isNameCharacter(code[p + 7])))
            continue;
        if (const char *rule = hiddenTest(code, at, p + 7))
            return rule;
    }
    return nullptr;
}

// Blanks out TJS comments and string literals in one line, so only code is left. A block comment that runs past the
// line's end carries over through `inComment`.
static std::string codeOf(std::string_view line, bool &inComment)
{
    std::string code(line);
    for (size_t i = 0; i < code.size();) {
        if (inComment) {
            if (code.compare(i, 2, "*/") == 0) {
                code[i] = code[i + 1] = ' ';
                inComment = false;
                i += 2;
            } else {
                code[i++] = ' ';
            }
        } else if (code.compare(i, 2, "/*") == 0) {
            inComment = true;
        } else if (code.compare(i, 2, "//") == 0) {
            std::fill(code.begin() + i, code.end(), ' ');
            break;
        } else if (code[i] == '"' || code[i] == '\'') {
            const char quote = code[i];
            code[i++] = ' ';
            while (i < code.size() && code[i] != quote) {
                if (code[i] == '\\' && i + 1 < code.size())
                    code[i++] = ' ';
                code[i++] = ' ';
            }
            if (i < code.size())
                code[i++] = ' ';
        } else {
            i++;
        }
    }
    return code;
}

static bool startsTag(std::string_view line, std::string_view name)
{
    for (std::string_view open : {"[", "@"}) {
        if (line.substr(0, open.size()) != open)
            continue;
        std::string_view rest = line.substr(open.size());
        if (rest.size() < name.size())
            continue;
        bool same = true;
        for (size_t i = 0; i < name.size() && same; i++)
            same = (rest[i] | 0x20) == name[i];
        if (same && (rest.size() == name.size() || !isNameCharacter(rest[name.size()])))
            return true;
    }
    return false;
}

// The expressions a KAG tag line evaluates: the values of its `exp` and `cond` attributes.
static std::vector<std::string_view> expressionsOf(std::string_view line)
{
    std::vector<std::string_view> found;
    for (std::string_view name : {"exp", "cond"}) {
        for (size_t at = line.find(name); at != std::string_view::npos; at = line.find(name, at + 1)) {
            if (at == 0 || isNameCharacter(line[at - 1]) ||
                (at + name.size() < line.size() && isNameCharacter(line[at + name.size()])))
                continue;
            size_t p = at + name.size();
            while (p < line.size() && isSpace(line[p]))
                p++;
            if (p >= line.size() || line[p] != '=')
                continue;
            p++;
            while (p < line.size() && isSpace(line[p]))
                p++;
            if (p < line.size() && (line[p] == '"' || line[p] == '\'')) {
                size_t end = line.find(line[p], p + 1);
                found.push_back(line.substr(p + 1, end == std::string_view::npos ? std::string_view::npos : end - p - 1));
            } else {
                size_t end = p;
                while (end < line.size() && !isSpace(line[end]) && line[end] != ']')
                    end++;
                found.push_back(line.substr(p, end - p));
            }
        }
    }
    return found;
}

std::optional<VNKKMenuBarTest> VNKKFindHiddenMenuBarTest(std::string_view script, bool kag)
{
    size_t number = 0;
    bool inScript = !kag, inComment = false;
    while (!script.empty()) {
        number++;
        size_t newline = script.find('\n');
        std::string_view line = script.substr(0, newline);
        script = newline == std::string_view::npos ? std::string_view() : script.substr(newline + 1);

        size_t first = 0;
        while (first < line.size() && isSpace(line[first]))
            first++;
        std::string_view trimmed = line.substr(first);

        const char *rule = nullptr;
        if (kag && !inScript) {
            // Outside [iscript], a scenario line is a comment, a label, text, or tags whose expressions run.
            if (startsTag(trimmed, "iscript")) {
                inScript = true;
                inComment = false;
                continue;
            }
            if (trimmed.substr(0, 1) == ";")
                continue;
            for (std::string_view expression : expressionsOf(trimmed)) {
                bool unused = false;
                if ((rule = hiddenTestIn(codeOf(expression, unused))))
                    break;
            }
        } else {
            if (kag && startsTag(trimmed, "endscript")) {
                inScript = false;
                continue;
            }
            rule = hiddenTestIn(codeOf(line, inComment));
        }
        if (rule) {
            std::string_view excerpt = trimmed;
            while (!excerpt.empty() && isSpace(excerpt.back()))
                excerpt.remove_suffix(1);
            if (excerpt.size() > 160)
                excerpt = excerpt.substr(0, 160);
            return VNKKMenuBarTest{number, rule, std::string(excerpt)};
        }
    }
    return std::nullopt;
}
