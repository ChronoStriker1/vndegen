#include "menubar_scan.h"

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

std::optional<VNKKMenuBarTest> VNKKFindHiddenMenuBarTest(std::string_view script)
{
    size_t number = 0;
    while (!script.empty()) {
        number++;
        size_t newline = script.find('\n');
        std::string_view line = script.substr(0, newline);
        script = newline == std::string_view::npos ? std::string_view() : script.substr(newline + 1);

        size_t first = 0;
        while (first < line.size() && isSpace(line[first]))
            first++;
        if (line.substr(first, 1) == ";" || line.substr(first, 2) == "//")
            continue;

        for (size_t at = line.find("menu"); at != std::string_view::npos; at = line.find("menu", at + 1)) {
            if (at > 0 && isNameCharacter(line[at - 1]))
                continue;
            size_t p = at + 4;
            while (p < line.size() && isSpace(line[p]))
                p++;
            if (p >= line.size() || line[p] != '.')
                continue;
            p++;
            while (p < line.size() && isSpace(line[p]))
                p++;
            if (line.substr(p, 7) != "visible" || (p + 7 < line.size() && isNameCharacter(line[p + 7])))
                continue;
            if (const char *rule = hiddenTest(line, at, p + 7)) {
                std::string_view excerpt = line.substr(first);
                while (!excerpt.empty() && isSpace(excerpt.back()))
                    excerpt.remove_suffix(1);
                if (excerpt.size() > 160)
                    excerpt = excerpt.substr(0, 160);
                return VNKKMenuBarTest{number, rule, std::string(excerpt)};
            }
        }
    }
    return std::nullopt;
}
