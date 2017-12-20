void actor_init(struct actor *actor, s32 x, s32 y, struct level *level);
void actor_entered_tile(struct actor *actor, struct level *level,
                        u32 *num_clones_attached);
b32  actor_can_act(const struct actor *actor, const struct level *level,
                   enum action action);
b32  actor_can_undo(const struct actor *actor, const struct level *level,
                    enum action action, u32 num_clones_acquired);
