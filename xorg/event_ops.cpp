#include "event_ops.h"

bool
release_p(const InternalEvent* event)
{
   assert(event);
   return (event->any.type == ET_KeyRelease);
}

bool
press_p(const InternalEvent* event)
{
   assert(event);
   return (event->any.type == ET_KeyPress);
}

Time
time_of(const InternalEvent* event)
{
   return event->any.time;
}

KeyCode
detail_of(const InternalEvent* event)
{
   return (event->device_event.detail.key);
}
