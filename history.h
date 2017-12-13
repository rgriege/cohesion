struct history_event
{
	enum action action;
	u32 num_clones;
};

struct history
{
	struct history_event events[HISTORY_EVENT_MAX];
	u32 begin;
	u32 end;
};

void history_push(struct history *hist, enum action action, u32 num_clones);
b32  history_pop(struct history *hist, enum action *action, u32 *num_clones);
void history_clear(struct history *hist);
