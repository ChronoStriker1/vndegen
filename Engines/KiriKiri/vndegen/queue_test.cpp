// KiriKiriQueueTest: checks how the engine's native event queues deliver and cancel events.
//   KiriKiriQueueTest
// Exits nonzero if a check fails.
#include "tjsCommHead.h"
#include "NativeEventQueue.h"
#include "TVPApplication.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

static int failures;

static void check(bool ok, const std::string &what)
{
    printf("%s: %s\n", ok ? "pass" : "FAIL", what.c_str());
    if (!ok)
        failures++;
}

// Logs each event it gets, then runs its next action, if any.
struct Owner {
    Owner(std::vector<int> &log, int id) : log(log), id(id) {}
    void Handle(NativeEvent &event)
    {
        log.push_back(id * 10 + (int)event.Message);
        if (action) {
            auto run = action;
            action = nullptr;
            run();
        }
    }
    std::vector<int> &log;
    int id;
    std::function<void()> action;
    NativeEventQueue<Owner> queue{this, &Owner::Handle};
};

int main()
{
    // Each result reaches the log even if a later check crashes.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    Application = new tTVPApplication;
    std::vector<int> log;

    // VideoOverlay clears its queue when it is invalidated, which an earlier event can do.
    Owner first(log, 1), second(log, 2);
    first.action = [&] { second.queue.Clear(); };
    first.queue.PostEvent(NativeEvent(1));
    second.queue.PostEvent(NativeEvent(1));
    Application->ProcessMessages();
    check(log == std::vector<int>{11}, "an event cleared by an earlier event in its batch is not delivered");

    log.clear();
    Owner kinds(log, 3);
    kinds.action = [&] { kinds.queue.Clear(2); };
    kinds.queue.PostEvent(NativeEvent(1));
    kinds.queue.PostEvent(NativeEvent(2));
    kinds.queue.PostEvent(NativeEvent(3));
    Application->ProcessMessages();
    check(log == std::vector<int>{31, 33}, "clearing one kind of event leaves the others in the batch");

    log.clear();
    Owner poster(log, 4);
    poster.action = [&] { poster.queue.PostEvent(NativeEvent(2)); };
    poster.queue.PostEvent(NativeEvent(1));
    Application->ProcessMessages();
    check(log == std::vector<int>{41}, "an event posted by a handler waits for the next round");
    Application->ProcessMessages();
    check(log == std::vector<int>{41, 42}, "the next round delivers it");

    // TVPBreathe pumps messages from inside a handler.
    log.clear();
    Owner pumper(log, 5), later(log, 6);
    pumper.action = [&] { Application->ProcessMessages(); };
    pumper.queue.PostEvent(NativeEvent(1));
    later.queue.PostEvent(NativeEvent(1));
    later.queue.PostEvent(NativeEvent(2));
    Application->ProcessMessages();
    check(log == std::vector<int>{51, 61, 62}, "a handler that pumps messages delivers the rest of its batch once, in order");

    // A VideoOverlay deleted by an earlier event deallocates its queue; its events must not reach it.
    log.clear();
    Owner deleter(log, 7);
    Owner *victim = new Owner(log, 8);
    deleter.action = [&] {
        victim->queue.Deallocate();
        delete victim;
    };
    deleter.queue.PostEvent(NativeEvent(1));
    victim->queue.PostEvent(NativeEvent(1));
    Application->ProcessMessages();
    check(log == std::vector<int>{71}, "an owner deleted by an earlier event gets none of its events");

    delete Application;
    Application = nullptr;
    printf("%s\n", failures ? "queue test failed" : "queue test passed");
    return failures ? 1 : 0;
}
