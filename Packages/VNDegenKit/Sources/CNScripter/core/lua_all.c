// Lua 5.1.5 (MIT, see third_party/LICENSE-lua.txt) built as one unit, the way its own etc/all.c builds it. The
// runtime uses it for NScripter's NSLua scripts; nslua.cpp is the bridge.
#define luaall_c
#define LUA_USE_POSIX 1
#define LUA_USE_POPEN 1

// The C library first: ldebug.h defines a getline macro that would otherwise break stdio.h's declaration.
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../third_party/lua/lapi.c"
#include "../third_party/lua/lcode.c"
#include "../third_party/lua/ldebug.c"
#include "../third_party/lua/ldo.c"
#include "../third_party/lua/ldump.c"
#include "../third_party/lua/lfunc.c"
#include "../third_party/lua/lgc.c"
#include "../third_party/lua/llex.c"
#include "../third_party/lua/lmem.c"
#include "../third_party/lua/lobject.c"
#include "../third_party/lua/lopcodes.c"
#include "../third_party/lua/lparser.c"
#include "../third_party/lua/lstate.c"
#include "../third_party/lua/lstring.c"
#include "../third_party/lua/ltable.c"
#include "../third_party/lua/ltm.c"
#include "../third_party/lua/lundump.c"
#include "../third_party/lua/lvm.c"
#include "../third_party/lua/lzio.c"

#include "../third_party/lua/lauxlib.c"
#include "../third_party/lua/lbaselib.c"
#include "../third_party/lua/ldblib.c"
#include "../third_party/lua/liolib.c"
#include "../third_party/lua/linit.c"
#include "../third_party/lua/lmathlib.c"
#include "../third_party/lua/loadlib.c"
#include "../third_party/lua/loslib.c"
#include "../third_party/lua/lstrlib.c"
#include "../third_party/lua/ltablib.c"
