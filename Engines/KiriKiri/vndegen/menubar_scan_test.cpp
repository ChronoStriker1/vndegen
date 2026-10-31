// KiriKiriMenuBarScanTest: checks which script lines count as a test for a hidden menu bar. It needs nothing from the
// engine, so it also builds on its own:
//   c++ -std=c++17 vndegen/menubar_scan.cpp vndegen/menubar_scan_test.cpp -o menubar_scan_test && ./menubar_scan_test
#include "menubar_scan.h"

#include <cstdio>
#include <cstring>

static int failures;

static void expect(const char *script, const char *rule, size_t line = 1)
{
    auto hit = VNKKFindHiddenMenuBarTest(script);
    bool ok = rule ? hit && !strcmp(hit->rule, rule) && hit->line == line : !hit;
    if (!ok) {
        failures++;
        printf("FAIL: %s\n  expected %s, got %s", script, rule ? rule : "no test", hit ? hit->rule : "no test");
        if (hit)
            printf(" at line %zu", hit->line);
        printf("\n");
    }
}

int main()
{
    // Test Game 71 puts its message window buttons on screen when the menu bar is hidden.
    expect("[if exp=\"kag.menu.visible == false\"]", "compares with false");
    expect("[if exp=\"kag.menu.visible == false && sf.message_window_button==true\"]", "compares with false");
    // Tawawa only builds its menu when the menu bar is shown, and shows nothing in its place.
    expect(";//メニューバーの切り替え\n[if exp=\"kag.menu.visible == true\"]\n\t[call storage=\"customMenu.ks\"]\n[endif]",
           nullptr);
    expect(";menu.visible = true;", nullptr);
    expect("menu.visible = true;", nullptr);

    // Other forms of the same test.
    expect("if(!kag.menu.visible) showButtons();", "negation");
    expect("if (!menu.visible) x();", "negation");
    expect("if(!( kag.menu.visible )) x();", "negation");
    expect("if(! global.kag.menu.visible) x();", "negation");
    expect("[if exp=\"kag.menu.visible===false\"]", "compares with false");
    expect("[if exp=\"kag.menu.visible != true\"]", "compares with false");
    expect("[if exp=\"kag.menu.visible !== true\"]", "compares with false");
    expect("[if exp=\"kag.menu.visible == 0\"]", "compares with false");
    expect("if (false == kag.menu.visible) x();", "compares with false");
    expect("if(w.menu . visible == false) x();", "compares with false");
    expect("a = 1;\nb = 2;\n  if(!kag.menu.visible) x();", "negation", 3);

    // Not a test for a hidden menu bar.
    expect("if(kag.menu.visible) x();", nullptr);
    expect("[if exp=\"kag.menu.visible != false\"]", nullptr);
    expect("if(!w.fullScreen && w.menu.visible) {", nullptr);
    expect("if(!storeMenu.visible) x();", nullptr);
    expect("if(submenu.visible == false) x();", nullptr);
    expect("if(!kag.menu.visibleItems) x();", nullptr);
    expect("if(!kag.menuButton.visible) x();", nullptr);
    expect("; [if exp=\"kag.menu.visible == false\"]", nullptr);
    expect("  // if(!kag.menu.visible) x();", nullptr);
    expect("x = a != (menu.visible);", nullptr);

    if (failures) {
        printf("%d failed\n", failures);
        return 1;
    }
    printf("menu bar scan: all passed\n");
    return 0;
}
