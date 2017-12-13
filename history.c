#include "config.h"
#include "violet/all.h"
#include "action.h"
#include "history.h"

void history_push(struct history *hist, enum action action, u32 num_clones)
{
	struct history_event *event =   hist->end < HISTORY_EVENT_MAX
	                              ? &hist->events[hist->end] : &hist->events[0];
	event->action     = action;
	event->num_clones = num_clones;
	hist->end = (hist->end + 1) % HISTORY_EVENT_MAX;
	if (hist->end == hist->begin)
		hist->begin = (hist->begin + 1) % HISTORY_EVENT_MAX;
}

b32 history_pop(struct history *hist, enum action *action, u32 *num_clones)
{
	if (hist->begin == hist->end) {
		return false;
	} else if (hist->end == 0) {
		struct history_event *event = &hist->events[HISTORY_EVENT_MAX - 1];
		*action     = event->action;
		*num_clones = event->num_clones;
		hist->end = HISTORY_EVENT_MAX - 1;
		return true;
	} else {
		struct history_event *event = &hist->events[--hist->end];
		*action     = event->action;
		*num_clones = event->num_clones;
		return true;
	}
}

void history_clear(struct history *hist)
{
	hist->begin = 0;
	hist->end = 0;
}
