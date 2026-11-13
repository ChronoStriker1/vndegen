#include "tjsCommHead.h"
#include "tjs.h"

#include <cstdio>
#include <string>

using namespace TJS;

static int failures;

static void check(tTJS &tjs, const char *expression, const std::string &expected)
{
    tTJSVariant result;
    tjs.EvalExpression(ttstr(expression), &result);
    std::string got = ttstr(result).AsStdString();
    bool ok = got == expected;
    printf("%s: %s gives %s\n", ok ? "pass" : "FAIL", expression, got.c_str());
    if (!ok)
        failures++;
}

int main()
{
    setvbuf(stdout, nullptr, _IOLBF, 0);
    tTJS *tjs = new tTJS();
    check(*tjs, "\"？？？＠レリア\".split(\"＠\").join(\"|\")", "？？？|レリア");
    check(*tjs, "\"？？？＠レリア\".split('\\＠')[0]", "？？？");
    check(*tjs, "[].split(\"＠\", \"？？？＠レリア/mkn0001\").join(\"|\")", "？？？|レリア/mkn0001");
    check(*tjs, "\"？？？＠レリア/mkn0001\".split(\"／＠/\").join(\"|\")", "？？？|レリア|mkn0001");
    check(*tjs, "\"清子/sei0001\".split(\"/\").join(\"|\")", "清子|sei0001");
    check(*tjs, "\"a,b;;c\".split(\",;\").join(\"|\")", "a|b||c");
    check(*tjs, "\"a,b;;c\".split(\",;\",,true).join(\"|\")", "a|b|c");
    check(*tjs, "\",a,\".split(\",\").count", "3");
    check(*tjs, "\"abc\".split(\"\").join(\"|\")", "abc");
    printf("%s\n", failures ? "split checks failed" : "split checks passed");
    return failures != 0;
}
