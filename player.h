void player_init(struct player *player, u32 idx);
b32  player_desires_action(struct player *player, enum action action,
                           const gui_t *gui);
enum action player_desired_action(struct player *player, const gui_t *gui);
