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
    // An odd number of `!` before it tests for false. `!!menu.visible` tests for true.
    size_t back = before;
    int negations = 0;
    while (back > 0 && (isSpace(line[back - 1]) || line[back - 1] == '(' || line[back - 1] == '!')) {
        negations += line[back - 1] == '!';
        back--;
    }
    if (negations % 2)
        return "negation";
    if (negations)
        return nullptr;

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

// What carries over from one line of TJS to the next: open block comments, which nest in TJS, and an open string.
struct TJSState {
    int comments = 0;
    char quote = 0;
};

// Blanks out TJS comments and string literals in one line, so only code is left.
static std::string codeOf(std::string_view line, TJSState &state)
{
    std::string code(line);
    auto blank = [&](size_t i, size_t n) {
        for (size_t k = i; k < i + n && k < code.size(); k++)
            code[k] = ' ';
    };
    for (size_t i = 0; i < code.size();) {
        if (state.comments) {
            if (code.compare(i, 2, "/*") == 0) {
                state.comments++;
                blank(i, 2);
                i += 2;
            } else if (code.compare(i, 2, "*/") == 0) {
                state.comments--;
                blank(i, 2);
                i += 2;
            } else {
                blank(i++, 1);
            }
        } else if (state.quote) {
            if (code[i] == '\\') {
                blank(i, 2);
                i += 2;
            } else {
                if (code[i] == state.quote)
                    state.quote = 0;
                blank(i++, 1);
            }
        } else if (code.compare(i, 2, "/*") == 0) {
            state.comments = 1;
            blank(i, 2);
            i += 2;
        } else if (code.compare(i, 2, "//") == 0) {
            blank(i, code.size() - i);
            break;
        } else if (code[i] == '"' || code[i] == '\'') {
            state.quote = code[i];
            blank(i++, 1);
        } else {
            i++;
        }
    }
    return code;
}

static bool sameName(std::string_view name, std::string_view lower)
{
    if (name.size() != lower.size())
        return false;
    for (size_t i = 0; i < name.size(); i++) {
        if ((name[i] >= 'A' && name[i] <= 'Z' ? name[i] + ('a' - 'A') : name[i]) != lower[i])
            return false;
    }
    return true;
}

// Reads one tag's name and attributes from `text`, which starts just after its `[` or `@`, up to `]`, or the end for
// an `@` line. Returns the values of its `exp` and `cond` attributes, unescaped: a backtick in a quoted value keeps the
// character after it. `end` is set past the tag.
static std::vector<std::string> expressionsOfTag(std::string_view text, bool bracketed, size_t &end)
{
    std::vector<std::string> found;
    size_t p = 0;
    while (p < text.size() && isNameCharacter(text[p]))
        p++;
    for (;;) {
        while (p < text.size() && isSpace(text[p]))
            p++;
        if (p >= text.size() || (bracketed && text[p] == ']'))
            break;
        size_t nameStart = p;
        while (p < text.size() && !isSpace(text[p]) && text[p] != '=' && !(bracketed && text[p] == ']'))
            p++;
        std::string_view name = text.substr(nameStart, p - nameStart);
        size_t q = p;
        while (q < text.size() && isSpace(text[q]))
            q++;
        if (q >= text.size() || text[q] != '=') {
            if (p == nameStart)
                p++;
            continue;  // an attribute with no value
        }
        p = q + 1;
        while (p < text.size() && isSpace(text[p]))
            p++;
        std::string value;
        if (p < text.size() && (text[p] == '"' || text[p] == '\'')) {
            const char quote = text[p++];
            while (p < text.size() && text[p] != quote) {
                if (text[p] == '`' && p + 1 < text.size())
                    p++;
                value += text[p++];
            }
            if (p < text.size())
                p++;
        } else {
            while (p < text.size() && !isSpace(text[p]) && !(bracketed && text[p] == ']'))
                value += text[p++];
        }
        if (sameName(name, "exp") || sameName(name, "cond"))
            found.push_back(std::move(value));
    }
    end = p < text.size() ? p + 1 : p;
    return found;
}

// The expressions a scenario line evaluates: the `exp` and `cond` attributes of its tags, whether the whole line is an
// `@` tag or tags sit in `[` and `]` among its text. `[[` is a literal bracket.
static std::vector<std::string> expressionsOf(std::string_view line)
{
    std::vector<std::string> found;
    size_t end = 0;
    if (line.substr(0, 1) == "@")
        return expressionsOfTag(line.substr(1), false, end);
    for (size_t at = line.find('['); at != std::string_view::npos; at = line.find('[', at)) {
        if (line.substr(at, 2) == "[[") {
            at += 2;
            continue;
        }
        auto tag = expressionsOfTag(line.substr(at + 1), true, end);
        found.insert(found.end(), tag.begin(), tag.end());
        at += 1 + end;
    }
    return found;
}

static bool startsTag(std::string_view line, std::string_view name)
{
    for (std::string_view open : {"[", "@"}) {
        if (line.substr(0, open.size()) != open)
            continue;
        std::string_view rest = line.substr(open.size());
        size_t n = 0;
        while (n < rest.size() && isNameCharacter(rest[n]))
            n++;
        if (sameName(rest.substr(0, n), name))
            return true;
    }
    return false;
}

std::optional<VNKKMenuBarTest> VNKKFindHiddenMenuBarTest(std::string_view script, bool kag)
{
    size_t number = 0;
    bool inScript = !kag;
    TJSState state;
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
                state = TJSState();
                continue;
            }
            if (trimmed.substr(0, 1) == ";" || trimmed.substr(0, 1) == "*")
                continue;
            for (const std::string &expression : expressionsOf(trimmed)) {
                TJSState fresh;
                if ((rule = hiddenTestIn(codeOf(expression, fresh))))
                    break;
            }
        } else {
            if (kag && !state.comments && !state.quote && startsTag(trimmed, "endscript")) {
                inScript = false;
                continue;
            }
            rule = hiddenTestIn(codeOf(line, state));
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
