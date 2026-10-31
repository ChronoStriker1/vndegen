// KiriKiriMenuBarScanTest: checks which script lines count as a test for a hidden menu bar. It needs nothing from the
// engine, so it also builds on its own:
//   c++ -std=c++17 vndegen/menubar_scan.cpp vndegen/menubar_scan_test.cpp -o menubar_scan_test && ./menubar_scan_test
#include "menubar_scan.h"

#include <cstdio>
#include <cstring>

static int failures;

static void expect(bool kag, const char *script, const char *rule, size_t line = 1)
{
    auto hit = VNKKFindHiddenMenuBarTest(script, kag);
    bool ok = rule ? hit && !strcmp(hit->rule, rule) && hit->line == line : !hit;
    if (!ok) {
        failures++;
        printf("FAIL (%s): %s\n  expected %s, got %s", kag ? "ks" : "tjs", script, rule ? rule : "no test",
               hit ? hit->rule : "no test");
        if (hit)
            printf(" at line %zu", hit->line);
        printf("\n");
    }
}

static void ks(const char *script, const char *rule, size_t line = 1) { expect(true, script, rule, line); }
static void tjs(const char *script, const char *rule, size_t line = 1) { expect(false, script, rule, line); }

int main()
{
    // Test Game 71 puts its message window buttons on screen when the menu bar is hidden.
    ks("[if exp=\"kag.menu.visible == false\"]", "compares with false");
    ks("\t[if exp=\"kag.menu.visible == false && sf.message_window_button==true\"]", "compares with false");
    // Tawawa only builds its menu when the menu bar is shown, and shows nothing in its place.
    ks(";//メニューバーの切り替え\n[if exp=\"kag.menu.visible == true\"]\n\t[call storage=\"customMenu.ks\"]\n[endif]",
       nullptr);
    tjs(";menu.visible = true;", nullptr);
    tjs("menu.visible = true;", nullptr);

    // Other forms of the same test, in TJS files, in [iscript], and in tag attributes.
    tjs("if(!kag.menu.visible) showButtons();", "negation");
    tjs("if (!menu.visible) x();", "negation");
    tjs("if(!( kag.menu.visible )) x();", "negation");
    tjs("if(! global.kag.menu.visible) x();", "negation");
    tjs("if(fullScreened && global.kag.fore.base.cursorY <= 30 && !menu.visible){", "negation");
    ks("[if exp=\"kag.menu.visible===false\"]", "compares with false");
    ks("[if exp='kag.menu.visible != true']", "compares with false");
    ks("@if exp=\"kag.menu.visible !== true\"", "compares with false");
    ks("[jump target=*buttons cond=\"kag.menu.visible == 0\"]", "compares with false");
    ks("[eval exp=\"f.x = 1\"][if exp=\"!kag.menu.visible\"]", "negation");
    tjs("if (false == kag.menu.visible) x();", "compares with false");
    tjs("if(w.menu . visible == false) x();", "compares with false");
    tjs("a = 1;\nb = 2;\n  if(!kag.menu.visible) x();", "negation", 3);
    ks("*start\n[iscript]\nif (!kag.menu.visible)\n\tshowButtons();\n[endscript]", "negation", 3);

    // Not a test for a hidden menu bar.
    tjs("if(kag.menu.visible) x();", nullptr);
    ks("[if exp=\"kag.menu.visible != false\"]", nullptr);
    tjs("if(!w.fullScreen && w.menu.visible) {", nullptr);
    tjs("if(!storeMenu.visible) x();", nullptr);
    tjs("if(submenu.visible == false) x();", nullptr);
    tjs("if(!kag.menu.visibleItems) x();", nullptr);
    tjs("if(!kag.menuButton.visible) x();", nullptr);
    ks("; [if exp=\"kag.menu.visible == false\"]", nullptr);
    tjs("  // if(!kag.menu.visible) x();", nullptr);
    tjs("x = a != (menu.visible);", nullptr);
    // Comments and strings.
    tjs("/* if (!kag.menu.visible) {} */ x();", nullptr);
    tjs("/* start\nif (!kag.menu.visible) {}\nend */ x();", nullptr);
    tjs("x(); // if (!kag.menu.visible)", nullptr);
    tjs("Debug.message('!menu.visible');", nullptr);
    tjs("Debug.message(\"kag.menu.visible == false \\\" still a string\");", nullptr);
    ks("[if exp=\"Debug.message('!menu.visible')\"]", nullptr);
    // Scenario text and other attributes aren't code.
    ks("Is !menu.visible a thing?[p]", nullptr);
    ks("[link text=\"!kag.menu.visible\" target=*a]", nullptr);
    ks("[iscript]\nx();\n[endscript]\nWhen menu.visible == false, nothing happens.", nullptr);

    // KAG attribute syntax: names in any case, backtick escapes, values of other attributes.
    ks("[if EXP=\"!kag.menu.visible\"]", "negation");
    ks("[if exp=\"f.name == `\"x`\" && !kag.menu.visible\"]", "negation");
    ks("[link text=\"cond='!kag.menu.visible'\" target=*help]", nullptr);
    ks("Press [[if exp=!kag.menu.visible] to see it.", nullptr);
    ks("*label|cond !kag.menu.visible", nullptr);
    ks("@iscript\nvar a = 1;\nif (!kag.menu.visible) x();\n@endscript", "negation", 3);
    // TJS strings that span lines, nested comments, and double negation.
    tjs("var s = \"first line\n!kag.menu.visible in the string\";", nullptr);
    tjs("/* outer /* inner */ if (!kag.menu.visible) fallback(); */ x();", nullptr);
    tjs("if (!!kag.menu.visible) makeMenu();", nullptr);
    tjs("if (!!!kag.menu.visible) fallback();", "negation");
    ks("[iscript]\nvar s = 'a\n[endscript]\n';\nif (!kag.menu.visible) x();\n[endscript]", "negation", 5);

    if (failures) {
        printf("%d failed\n", failures);
        return 1;
    }
    printf("menu bar scan: all passed\n");
    return 0;
}
