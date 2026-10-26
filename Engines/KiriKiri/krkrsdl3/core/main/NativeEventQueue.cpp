#include "tjsCommHead.h"
#include "NativeEventQueue.h"
#include "TVPApplication.h"

void NativeEventQueueImplement::PostEvent(const NativeEvent& ev)
{
    // The message number lets Clear remove one kind of event.
    Application->PostUserMessage([this, ev]() { Dispatch(*const_cast<NativeEvent*>(&ev)); }, this,
                                 ev.Message);
}

void NativeEventQueueImplement::Clear(int msg)
{
    if (Application)
        Application->CancelUserMessages(this, msg);
}
