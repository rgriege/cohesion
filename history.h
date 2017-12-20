void history_push(struct history *hist, enum action action, u32 num_clones);
b32  history_pop(struct history *hist, enum action *action, u32 *num_clones);
void history_clear(struct history *hist);
