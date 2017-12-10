void history_push(enum action action, u32 num_clones);
b32  history_pop(enum action *action, u32 *num_clones);
void history_clear(void);

/* Implementation */

#define HISTORY_MAX 512
struct history
{
	enum action action;
	u32 num_clones;
};

struct history g_history[HISTORY_MAX];
u32 g_history_begin = 0;
u32 g_history_end = 0;

void history_push(enum action action, u32 num_clones)
{
	struct history *history =   g_history_end < HISTORY_MAX
	                          ? &g_history[g_history_end] : &g_history[0];
	history->action     = action;
	history->num_clones = num_clones;
	g_history_end = (g_history_end + 1) % HISTORY_MAX;
	if (g_history_end == g_history_begin)
		g_history_begin = (g_history_begin + 1) % HISTORY_MAX;
}

b32 history_pop(enum action *action, u32 *num_clones)
{
	if (g_history_begin == g_history_end) {
		return false;
	} else if (g_history_end == 0) {
		struct history *history = &g_history[HISTORY_MAX - 1];
		*action     = history->action;
		*num_clones = history->num_clones;
		g_history_end = HISTORY_MAX - 1;
		return true;
	} else {
		struct history *history = &g_history[--g_history_end];
		*action     = history->action;
		*num_clones = history->num_clones;
		return true;
	}
}

void history_clear(void)
{
	g_history_begin = 0;
	g_history_end = 0;
}
