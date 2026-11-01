/* The engine_api of M7 (court): the state of an officer, the skill, post,
   rank and item tables, diplomacy, the views of the duel and the debate,
   and the commands of personnel, court, diplomacy, plots, duel and debate.
   Included by engine.cpp, before engine_make_api. */

/* ---- the officer and the tables ------------------------------------------------- */

internal engine_officer_detail
engine_api_officer_detail(void *opaque, u16 officer_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_officer_detail result = {};
    result.officer_id = ENGINE_NO_OFFICER;
    officer_component *officer = sim_find_officer(sim, officer_id);
    if (!officer) return result;
    result.officer_id   = officer_id;
    result.loyalty      = (u8)Minimum((u32)officer->loyalty, 100u);   /* the screen shows 100 for every value of 100 or more (G2-035) */
    result.ambition     = officer->ambition;
    result.justice      = officer->justice;
    result.personality  = officer->personality;
    result.growth       = officer->growth;
    result.health       = officer->health;
    result.stamina      = officer->stamina;
    result.post         = officer->post < sim->content.post_count ? officer->post : (u8)ENGINE_NO_ROW;
    result.female       = officer->female;
    result.captive      = officer->status == SIM_STATUS_CAPTIVE;
    result.former_force = officer->former_force;
    for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot)
    {
        result.liked[slot]    = officer->liked[slot];
        result.disliked[slot] = officer->disliked[slot];
    }
    for (u32 ability = 0; ability < CONTENT_ABILITY_COUNT; ++ability) result.experience[ability] = officer->experience[ability];
    result.age         = sim_officer_age(sim, officer);
    result.soldier_cap = sim_officer_soldier_cap(sim, officer);
    result.training_turns = officer->training_turns;   /* M9 */
    result.training_row   = officer->training_turns ? officer->training_row : (u8)ENGINE_NO_ROW;
    result.created        = officer->created;
    result.portrait_stage = sim_portrait_stage(sim, officer);
    u8 acting = sim_whose_turn(sim);
    if (acting != SIM_NO_FORCE && sim_check_recruit_target(sim, acting, officer) == SIM_REFUSAL_NONE)
    {
        /* The chance with the ruler as the recruiter: a front end shows it in the court panel. */
        officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, acting));
        if (ruler) result.recruit_chance = sim_recruit_chance(sim, ruler, officer);
    }
    if (acting != SIM_NO_FORCE && sim_check_reward_officer(sim, acting, officer) == SIM_REFUSAL_NONE)
    {
        result.reward_rise = sim_reward_rise_of(sim, acting, officer);   /* the 褒賞 list shows it (G2-040) */
    }
    return result;
}

/* The chance of 探索 or 登庸 with the clue of the adviser. */
internal engine_court_advice
engine_api_court_advice(void *opaque, engine_command_kind kind, const engine_court_order *order)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_court_advice advice = {};
    advice.adviser = ENGINE_NO_OFFICER;
    city_component *city = sim_find_city(sim, order->city_id);
    officer_component *officer = sim_find_officer(sim, order->officer);
    if (!city || !officer) return advice;
    u8 force = city->owner_force_id;
    if (kind == ENGINE_COMMAND_SEARCH)
    {
        advice.chance = sim_search_chance(sim, city, officer);
    }
    else if (kind == ENGINE_COMMAND_RECRUIT)
    {
        officer_component *target = sim_find_officer(sim, order->target);
        if (!target || sim_check_recruit_target(sim, force, target) != SIM_REFUSAL_NONE) return advice;
        advice.chance = sim_recruit_chance(sim, officer, target);
    }
    else return advice;
    advice.clue = sim_chance_clue(&sim->content.rules, advice.chance);
    officer_component *adviser = force < SIM_FORCE_CAPACITY ? sim_find_officer(sim, sim->adviser[force]) : 0;
    if (adviser && adviser->force_id == force && sim_officer_is_active(adviser)) advice.adviser = adviser->officer_id;
    return advice;
}

internal u32
engine_api_skill_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.skill_count;
}

internal engine_skill_kind
engine_api_skill_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_skill_kind result = {};
    if (index >= content->skill_count) return result;
    skill_kind *kind = content->skills + index;
    result.number   = kind->number;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.traits   = kind->traits;
    result.category = kind->category;
    result.hooked   = kind->hooked;
    return result;
}

internal u32
engine_api_post_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.post_count;
}

internal engine_post_kind
engine_api_post_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_post_kind result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->post_count) return result;
    post_kind *kind = content->posts + index;
    result.id       = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.merit    = kind->merit;
    result.soldiers = kind->soldiers;
    result.bonus    = kind->bonus;
    result.ability  = kind->ability;
    result.rank     = kind->rank;
    return result;
}

internal u32
engine_api_rank_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.rank_count;
}

internal engine_rank_kind
engine_api_rank_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_rank_kind result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->rank_count) return result;
    rank_kind *kind = content->ranks + index;
    result.id       = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.soldiers = kind->soldiers;
    result.cities   = kind->cities;
    return result;
}

internal u8
engine_api_faction_rank(void *opaque, u8 force_id)
{
    return force_id < SIM_FORCE_CAPACITY ? ((engine *)opaque)->sim->rank[force_id] : (u8)0;
}

internal u16
engine_api_post_holder(void *opaque, u8 force_id, u8 post)
{
    sim_state *sim = ((engine *)opaque)->sim;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->force_id == force_id && sim_officer_is_active(officer) && officer->post == post) return officer->officer_id;
    }
    return ENGINE_NO_OFFICER;
}

internal u32
engine_api_item_count(void *opaque)
{
    (void)opaque;
    return SAVE_ITEM_COUNT;
}

internal engine_item
engine_api_item_at(void *opaque, u32 index)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_item result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= SAVE_ITEM_COUNT || !sim->items[index].exists) return result;
    sim_item *item = sim->items + index;
    result.id       = (u8)index;
    memcpy(result.name, item->name, sizeof(result.name));
    if (item->kind < sim->content.item_count) memcpy(result.key, sim->content.items[item->kind].key, sizeof(result.key));
    result.type     = item->type;
    result.strength = item->strength;
    result.owner    = item->owner;
    result.city     = item->city;
    return result;
}

internal engine_diplomacy
engine_api_diplomacy(void *opaque, u8 force_a, u8 force_b)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_diplomacy result = {};
    result.friendliness = sim_friendliness(sim, force_a, force_b);
    result.level        = sim_friendliness_level(result.friendliness);
    result.relation     = sim_relation(sim, force_a, force_b);
    result.months       = force_a < SIM_DIPLOMACY_FORCES && force_b < SIM_DIPLOMACY_FORCES ? sim->diplomacy.months[force_a][force_b] : 0;
    return result;
}

/* ---- the duel and the debate ------------------------------------------------------- */

internal engine_duel_view
engine_api_duel_view(void *opaque)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim_duel *duel = &sim->duel;
    engine_duel_view view = {};
    view.active = duel->active;
    view.round  = duel->round;
    view.result = duel->result;
    view.winner = duel->winner;
    for (u32 index = 0; index < 2; ++index)
    {
        sim_duel_side *side = duel->side + index;
        engine_duel_side *out = view.side + index;
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            out->officers[place] = side->officers[place];
            officer_component *officer = sim_find_officer(sim, side->officers[place]);
            out->stamina[place] = officer ? officer->stamina : 0;
        }
        out->fighter = side->fighter;
        out->spirit  = side->spirit;
        out->policy  = side->policy;
        out->force   = side->force;
        out->boosted = side->focus_rounds || side->hold_rounds;
        out->guarded = side->guard_rounds != 0;
    }
    for (u32 special = 0; special < DUEL_SPECIAL_COUNT; ++special)
    {
        view.ready[special] = duel->active && sim_duel_special_ready(sim, duel->side + 0, (u8)special);
    }
    return view;
}

internal engine_debate_view
engine_api_debate_view(void *opaque)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim_debate *debate = &sim->debate;
    engine_debate_view view = {};
    view.active   = debate->active;
    view.finished = debate->finished;
    view.round    = debate->round;
    view.topic    = debate->topic;
    view.turn     = debate->turn;
    view.winner   = debate->winner;
    view.purpose  = debate->purpose;
    for (u32 index = 0; index < 2; ++index)
    {
        sim_debate_side *side = debate->side + index;
        engine_debate_side *out = view.side + index;
        out->officer    = side->officer;
        out->hand_count = side->hand_count;
        for (u32 place = 0; place < side->hand_count; ++place) out->hand[place] = side->hand[place];
        out->mind        = side->mind;
        out->rocks       = sim_debate_rocks(sim, side->mind);
        out->anger       = side->anger;
        out->rage_rounds = side->rage_rounds;
        view.last[index] = debate->last[index];
    }
    return view;
}

/* ---- the commands ------------------------------------------------------------------ */

internal sim_command_kind
engine_court_kind(engine_command_kind kind)
{
    switch (kind)
    {
        case ENGINE_COMMAND_SEARCH:       return SIM_COMMAND_SEARCH;
        case ENGINE_COMMAND_RECRUIT:      return SIM_COMMAND_RECRUIT;
        case ENGINE_COMMAND_REWARD:       return SIM_COMMAND_REWARD;
        case ENGINE_COMMAND_GIVE_ITEM:    return SIM_COMMAND_GIVE_ITEM;
        case ENGINE_COMMAND_APPOINT:      return SIM_COMMAND_APPOINT;
        case ENGINE_COMMAND_MATCHMAKE:    return SIM_COMMAND_MATCHMAKE;
        case ENGINE_COMMAND_RELEASE:      return SIM_COMMAND_RELEASE;
        case ENGINE_COMMAND_EXILE:        return SIM_COMMAND_EXILE;
        case ENGINE_COMMAND_DIPLOMACY:    return SIM_COMMAND_DIPLOMACY;
        case ENGINE_COMMAND_FACTION_PLOT: return SIM_COMMAND_FACTION_PLOT;
        default:                          return SIM_COMMAND_NONE;
    }
}

internal engine_check
engine_check_court(engine *e, sim_command_kind kind, const engine_court_order *order)
{
    sim_court_order copy = *order;
    if (kind == SIM_COMMAND_DIPLOMACY)    return sim_check_diplomacy(e->sim, &copy);
    if (kind == SIM_COMMAND_FACTION_PLOT) return sim_check_faction_plot(e->sim, &copy);
    return sim_check_court(e->sim, kind, &copy);
}

internal sim_mini_order
engine_mini_order(u8 action, u8 value)
{
    sim_mini_order order = {};
    order.action    = action;
    order.value     = value;
    order.officer_a = SIM_NO_OFFICER;
    order.officer_b = SIM_NO_OFFICER;
    return order;
}

/* can_issue for the M7 commands; false if the command is not one of them. */
internal b32
engine_can_issue_court(engine *e, const engine_command *command, engine_check *check)
{
    sim_command_kind kind = engine_court_kind(command->kind);
    if (kind != SIM_COMMAND_NONE)
    {
        *check = engine_check_court(e, kind, &command->court);
        return true;
    }
    if (command->kind == ENGINE_COMMAND_DUEL)
    {
        sim_unit_order order = engine_unit_order(command->unit, command->q, command->r, ENGINE_NO_UNIT, 0, 0, 0);
        *check = sim_check_duel(e->sim, &order);
        return true;
    }
    if (command->kind == ENGINE_COMMAND_DUEL_ACTION || command->kind == ENGINE_COMMAND_DEBATE_ACTION)
    {
        sim_mini_order order = engine_mini_order(command->row, command->value);
        *check = command->kind == ENGINE_COMMAND_DUEL_ACTION ? sim_check_duel_action(e->sim, &order) : sim_check_debate_action(e->sim, &order);
        return true;
    }
    return false;
}

/* 移動 and 召喚 (travel.cpp; G2-036, G2-037): one command for the officers
   of the order. A refused order changes nothing. */
internal engine_command_result
engine_run_move_officers(engine *e, sim_command_kind kind, const engine_move_order *order)
{
    engine_command_result result = {};
    sim_move_order move = *order;
    result.refusal = sim_check_move_officers(e->sim, kind, &move).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, kind);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, kind);
    command.move = move;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_move_officers(void *opaque, const engine_move_order *order)
{
    return engine_run_move_officers((engine *)opaque, SIM_COMMAND_MOVE_OFFICERS, order);
}

internal engine_command_result
engine_api_command_summon(void *opaque, const engine_move_order *order)
{
    return engine_run_move_officers((engine *)opaque, SIM_COMMAND_SUMMON, order);
}

/* The days of a journey between two bases (抵達日數). */
internal u32
engine_api_travel_days(void *opaque, u8 from_base, u8 to_base)
{
    return sim_travel_days(((engine *)opaque)->sim, from_base, to_base);
}

internal engine_command_result
engine_api_command_court(void *opaque, engine_command_kind kind, const engine_court_order *order)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_command_kind sim_kind = engine_court_kind(kind);
    if (sim_kind == SIM_COMMAND_NONE) { result.refusal = SIM_REFUSAL_NO_CITY; return result; }
    result.refusal = engine_check_court(e, sim_kind, order).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, sim_kind);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, sim_kind);
    command.court = *order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

/* 俘虜處置: the choice of the player for a captive. */
internal engine_command_result
engine_api_command_captive(void *opaque, u16 officer_id, u8 choice)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_court_order order = {};
    order.target = officer_id;
    order.kind   = choice;
    result.refusal = sim_check_captive(e->sim, &order).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, SIM_COMMAND_CAPTIVE);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_CAPTIVE);
    command.court = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result engine_api_command_duel_officer(void *opaque, u32 unit_id, s32 q, s32 r, u8 slot);

internal engine_command_result
engine_api_command_duel(void *opaque, u32 unit_id, s32 q, s32 r)
{
    return engine_api_command_duel_officer(opaque, unit_id, q, r, 0);
}

/* G3-008: 單挑 with the officer of the slot as the challenger. */
internal engine_command_result
engine_api_command_duel_officer(void *opaque, u32 unit_id, s32 q, s32 r, u8 slot)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_unit_order order = engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0);
    order.kind = slot;
    result.refusal = sim_check_duel(e->sim, &order).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, SIM_COMMAND_DUEL);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_DUEL);
    command.unit = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_run_mini(engine *e, sim_command_kind kind, u8 action, u8 value)
{
    engine_command_result result = {};
    sim_mini_order order = engine_mini_order(action, value);
    result.refusal = kind == SIM_COMMAND_DUEL_ACTION ? sim_check_duel_action(e->sim, &order).refusal : sim_check_debate_action(e->sim, &order).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, kind);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, kind);
    command.mini = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_duel_action(void *opaque, u8 action, u8 value)
{
    return engine_run_mini((engine *)opaque, SIM_COMMAND_DUEL_ACTION, action, value);
}

internal engine_command_result
engine_api_command_debate_action(void *opaque, u8 action, u8 value)
{
    return engine_run_mini((engine *)opaque, SIM_COMMAND_DEBATE_ACTION, action, value);
}

/* ---- test and editor commands ---------------------------------------------------------- */

internal engine_command_result
engine_api_start_debate(void *opaque, u16 officer_a, u16 officer_b)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_mini_order order = engine_mini_order(0, 0);
    order.officer_a = officer_a;
    order.officer_b = officer_b;
    result.refusal = sim_check_start_debate(e->sim, &order).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, SIM_COMMAND_DEBATE);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_DEBATE);
    command.mini = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal void
engine_api_set_officer(void *opaque, u16 officer_id, const engine_officer_edit *edit)
{
    if (engine_setter_locked((engine *)opaque)) return;   /* a shared game: no setter */
    officer_component *officer = sim_find_officer(((engine *)opaque)->sim, officer_id);
    if (!officer || !edit) return;
    officer->loyalty     = (u8)Minimum((u32)edit->loyalty, 100u);
    officer->ambition    = (u8)Minimum((u32)edit->ambition, 4u);
    officer->justice     = (u8)Minimum((u32)edit->justice, 4u);
    officer->personality = edit->personality < PERSONALITY_COUNT ? edit->personality : (u8)PERSONALITY_CAREFUL;
    officer->growth      = edit->growth < GROWTH_COUNT ? edit->growth : (u8)GROWTH_NORMAL;
    officer->female      = edit->female;
    officer->merit       = edit->merit;
    if (edit->set_abilities)
    {
        officer->leadership   = edit->leadership;
        officer->war          = edit->war;
        officer->intelligence = edit->intelligence;
        officer->politics     = edit->politics;
        officer->charm        = edit->charm;
    }
    if (edit->set_aptitudes)
    {
        for (u32 index = 0; index < OFFICER_UNIT_APTITUDE_COUNT; ++index) officer->unit_aptitudes[index] = edit->aptitudes[index];
    }
    if (edit->captor != ENGINE_NO_FACTION && edit->captor != officer->force_id && officer->status != SIM_STATUS_CAPTIVE)
    {
        sim_state *sim = ((engine *)opaque)->sim;
        u8 home = sim_refuge_city(sim, edit->captor, SIM_NO_CITY);
        officer->former_force     = officer->force_id;
        officer->force_id         = edit->captor;
        officer->status           = SIM_STATUS_CAPTIVE;
        officer->current_position = home == SIM_NO_CITY ? 0 : home;
        officer->base_location    = officer->current_position;
    }
}

/* The fields of the editor as they are now: a front end changes some and
   sends them back. */
internal engine_officer_edit
engine_officer_edit_of(engine_officer_detail *detail, engine_officer *officer)
{
    engine_officer_edit edit = {};
    edit.loyalty     = detail->loyalty;
    edit.ambition    = detail->ambition;
    edit.justice     = detail->justice;
    edit.personality = detail->personality;
    edit.growth      = detail->growth;
    edit.female      = detail->female;
    edit.merit       = officer->merit;
    edit.captor      = ENGINE_NO_FACTION;
    return edit;
}

internal void
engine_api_set_item_owner(void *opaque, u8 item, u16 officer_id)
{
    if (engine_setter_locked((engine *)opaque)) return;   /* a shared game: no setter */
    sim_state *sim = ((engine *)opaque)->sim;
    if (item >= SAVE_ITEM_COUNT || !sim->items[item].exists) return;
    sim->items[item].owner = sim_find_officer(sim, officer_id) ? officer_id : (u16)SIM_NO_OFFICER;
    sim->items[item].city  = SIM_NO_CITY;
}

internal void
engine_api_set_diplomacy(void *opaque, u8 force_a, u8 force_b, u32 friendliness, u8 relation, u32 months)
{
    if (engine_setter_locked((engine *)opaque)) return;   /* a shared game: no setter */
    sim_state *sim = ((engine *)opaque)->sim;
    if (force_a >= SIM_DIPLOMACY_FORCES || force_b >= SIM_DIPLOMACY_FORCES || force_a == force_b || relation >= RELATION_COUNT) return;
    u8 value = (u8)Minimum(friendliness, 100u);
    sim->diplomacy.friendliness[force_a][force_b] = sim->diplomacy.friendliness[force_b][force_a] = value;
    sim->diplomacy.relation[force_a][force_b]     = sim->diplomacy.relation[force_b][force_a]     = relation;
    sim->diplomacy.months[force_a][force_b]       = sim->diplomacy.months[force_b][force_a]       = (u8)Minimum(months, 255u);
}

internal void
engine_api_set_stamina(void *opaque, u16 officer_id, u8 stamina)
{
    if (engine_setter_locked((engine *)opaque)) return;   /* a shared game: no setter */
    officer_component *officer = sim_find_officer(((engine *)opaque)->sim, officer_id);
    if (officer) officer->stamina = (u8)Minimum((u32)stamina, 100u);
}

internal void
engine_bind_court_api(engine_api *api)
{
    api->officer_detail        = engine_api_officer_detail;
    api->skill_kind_count      = engine_api_skill_kind_count;
    api->skill_kind_at         = engine_api_skill_kind_at;
    api->post_kind_count       = engine_api_post_kind_count;
    api->post_kind_at          = engine_api_post_kind_at;
    api->rank_kind_count       = engine_api_rank_kind_count;
    api->rank_kind_at          = engine_api_rank_kind_at;
    api->faction_rank          = engine_api_faction_rank;
    api->post_holder           = engine_api_post_holder;
    api->item_count            = engine_api_item_count;
    api->item_at               = engine_api_item_at;
    api->diplomacy             = engine_api_diplomacy;
    api->duel_view             = engine_api_duel_view;
    api->debate_view           = engine_api_debate_view;
    api->command_court         = engine_api_command_court;
    api->command_captive       = engine_api_command_captive;
    api->command_move_officers = engine_api_command_move_officers;
    api->command_summon        = engine_api_command_summon;
    api->travel_days           = engine_api_travel_days;
    api->court_advice          = engine_api_court_advice;
    api->command_duel          = engine_api_command_duel;
    api->command_duel_officer  = engine_api_command_duel_officer;
    api->command_duel_action   = engine_api_command_duel_action;
    api->command_debate_action = engine_api_command_debate_action;
    api->start_debate          = engine_api_start_debate;
    api->set_officer           = engine_api_set_officer;
    api->set_item_owner        = engine_api_set_item_owner;
    api->set_diplomacy         = engine_api_set_diplomacy;
    api->set_stamina           = engine_api_set_stamina;
}
