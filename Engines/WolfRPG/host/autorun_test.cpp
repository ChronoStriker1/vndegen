#include "game.h"

#include <cassert>
#include <cstdio>

namespace {

wolf::MapPage page(int trigger, int commands) {
    wolf::MapPage pg;
    pg.trigger = uint8_t(trigger);
    pg.commands.resize(size_t(commands));
    return pg;
}

wolf::MapEventState state(const wolf::MapEvent &data, int active) {
    wolf::MapEventState e;
    e.data = &data;
    e.page = active;
    return e;
}

}  // namespace

int main() {
    wolf::MapEvent parallel, idle, autorun;
    parallel.pages = {page(2, 3)};
    idle.pages = {page(1, 1)};
    autorun.pages = {page(0, 3), page(1, 3)};

    std::deque<wolf::MapEventState> events{state(parallel, 0), state(idle, 0), state(autorun, 1)};
    assert(wolf::firstAutorun(events) == 2);
    assert(wolf::firstAutorun(events) == 2);

    events[2].page = 0;
    assert(wolf::firstAutorun(events) == -1);

    events[2].page = -1;
    assert(wolf::firstAutorun(events) == -1);

    std::puts("autorun: ok");
}
