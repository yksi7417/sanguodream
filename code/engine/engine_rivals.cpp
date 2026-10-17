/* The engine_api of M8 (rivals): the ports and passes, the corps, the
   adviser, the council, the AI plan, the Han emperor, the relics and the
   events. Included by engine.cpp, before engine_make_api. */

/* ---- the bases ---------------------------------------------------------------------- */

internal u32
engine_api_base_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.base_count;
}

internal engine_base
engine_api_base_at(void *opaque, u32 index)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_base result = {};
    result.id    = ENGINE_NO_ROW;
    result.force = ENGINE_NO_FACTION;
    sim_base_state *base = sim_find_base(sim, index);
    if (!base) return result;
    base_kind *kind = sim->content.bases + index;
    result.id             = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.type           = kind->type;
    result.owner_city     = kind->owner_city;
    result.force          = base->force == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : base->force;
    result.durability     = base->durability;
    result.durability_max = kind->durability;
    result.troops         = base->troops;
    result.troop_cap      = base->force == SIM_NO_FORCE ? sim->content.rules.base_troop_cap : sim_base_troop_cap(sim, base->force);
    result.q              = base->hex.q;
    result.r              = base->hex.r;
    return result;
}

internal u32
engine_api_base_at_hex(void *opaque, s32 q, s32 r)
{
    hex_coord hex = {};
    hex.q = q;
    hex.r = r;
    u32 index = sim_base_at(((engine *)opaque)->sim, hex);
    return index == SIM_NO_BASE ? ENGINE_NO_BASE : index;
}

/* ---- the corps and the adviser -------------------------------------------------------- */

internal engine_corps
engine_api_corps_at(void *opaque, u8 force_id, u8 corps_index)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_corps result = {};
    result.viceroy        = ENGINE_NO_OFFICER;
    result.transport_city = ENGINE_NO_CITY;
    if (force_id >= SIM_FORCE_CAPACITY || corps_index >= SIM_CORPS_CAPACITY) return result;
    sim_corps *corps = sim->corps[force_id] + corps_index;
    if (!corps->alive) return result;
    result.alive          = true;
    result.viceroy        = corps->viceroy;
    result.policy         = corps->policy;
    result.target         = corps->target;
    result.flags          = corps->flags;
    result.transport_city = corps->transport_city;
    result.action_points  = sim->corps_ap[force_id][corps_index];
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == force_id && city->corps == corps_index) ++result.city_count;
    }
    return result;
}

internal u8
engine_api_city_corps(void *opaque, u8 city_id)
{
    city_component *city = sim_find_city(((engine *)opaque)->sim, city_id);
    return city ? city->corps : (u8)0;
}

internal u16
engine_api_adviser(void *opaque, u8 force_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    if (force_id >= SIM_FORCE_CAPACITY) return ENGINE_NO_OFFICER;
    officer_component *adviser = sim_find_officer(sim, sim->adviser[force_id]);
    return adviser && adviser->force_id == force_id && sim_officer_is_active(adviser) ? adviser->officer_id : (u16)ENGINE_NO_OFFICER;
}

/* ---- the plans: the AI, the council and the adviser ----------------------------------------- */

/* The city and the first officer of a proposal: a front end shows them. */
internal engine_proposal
engine_proposal_of(sim_state *sim, sim_proposal *proposal)
{
    engine_proposal result = {};
    sim_command *command = &proposal->command;
    result.kind    = (u8)command->kind;
    result.score   = proposal->score;
    result.city_id = ENGINE_NO_CITY;
    result.officer = ENGINE_NO_OFFICER;
    if (proposal->row < sim->content.priority_count) memcpy(result.key, sim->content.priorities[proposal->row].key, sizeof(result.key));
    switch (command->kind)
    {
        case SIM_COMMAND_PATROL:
        case SIM_COMMAND_CONSCRIPT:
        case SIM_COMMAND_TRAIN:
        case SIM_COMMAND_MERCHANT:
        case SIM_COMMAND_PRODUCE:  result.city_id = command->city.city_id;     result.officer = command->city.officers[0];     break;
        case SIM_COMMAND_DEVELOP:  result.city_id = command->develop.city_id;  result.officer = command->develop.officers[0];  break;
        case SIM_COMMAND_RESEARCH: result.city_id = command->research.city_id; result.officer = command->research.officers[0]; break;
        case SIM_COMMAND_SORTIE:
        case SIM_COMMAND_TRANSPORT: result.city_id = command->sortie.city_id;  result.officer = command->sortie.officers[0];   break;
        default:
        {
            if (command->kind >= SIM_COMMAND_SEARCH && command->kind <= SIM_COMMAND_FACTION_PLOT)
            {
                result.city_id = command->court.city_id;
                result.officer = command->court.officer;
            }
        } break;
    }
    if (result.officer == SIM_NO_OFFICER) result.officer = ENGINE_NO_OFFICER;
    return result;
}

internal engine_plan_view
engine_plan_view_of(sim_state *sim, sim_plan *plan, sim_refusal refusal)
{
    engine_plan_view view = {};
    view.refusal = refusal;
    for (u32 index = 0; index < plan->count && view.count < ENGINE_PLAN_CAPACITY; ++index)
    {
        view.items[view.count++] = engine_proposal_of(sim, plan->items + index);
    }
    return view;
}

internal engine_plan_view
engine_api_advice(void *opaque, u8 force_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim_plan plan_memory;
    sim_plan *plan = &plan_memory;
    sim_advice(sim, force_id, plan);
    engine_plan_view view = engine_plan_view_of(sim, plan, plan->count ? SIM_REFUSAL_NONE : SIM_REFUSAL_NO_OFFICER);
    return view;
}

internal engine_plan_view
engine_api_council(void *opaque, u8 force_id, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim_plan plan_memory;
    sim_plan *plan = &plan_memory;
    sim_refusal refusal = sim_council_plan(sim, force_id, city_id, plan);
    engine_plan_view view = engine_plan_view_of(sim, plan, refusal);
    return view;
}

/* What the AI of the faction proposes now, in the order it acts: the
   feature tests read the first command of a state. */
internal engine_plan_view
engine_api_ai_plan(void *opaque, u8 force_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim_plan plan_memory;
    sim_plan *plan = &plan_memory;
    plan->count = 0;
    if (force_id < SIM_FORCE_CAPACITY) sim_plan_faction(sim, force_id, SIM_NO_CITY, 0xFF, CORPS_FLAG_ALL, plan);
    engine_plan_view view = engine_plan_view_of(sim, plan, SIM_REFUSAL_NONE);
    return view;
}

/* ---- the emperor and the relics --------------------------------------------------------- */

internal engine_emperor
engine_api_emperor(void *opaque)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_emperor result = {};
    result.who       = sim->emperor.who;
    result.city      = sim->emperor.city;
    u8 protector     = sim_emperor_protector(sim);
    result.protector = protector == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : protector;
    return result;
}

internal u32
engine_api_relic_count(void *opaque)
{
    /* M10: the real map has 6 + 6 event sites; the placeholder map has 7 relics. */
    return ((engine *)opaque)->sim->map_layers.loaded ? SIM_RELIC_COUNT : SIM_RELIC_PLACEHOLDER_COUNT;
}

internal engine_relic
engine_api_relic_at(void *opaque, u32 index)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_relic result = {};
    if (index >= SIM_RELIC_COUNT || !sim->relics[index].alive) return result;
    result.alive = true;
    result.kind  = sim->relics[index].kind;
    result.q     = sim->relics[index].hex.q;
    result.r     = sim->relics[index].hex.r;
    return result;
}

/* ---- the events ------------------------------------------------------------------------- */

internal u32
engine_api_event_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.event_count;
}

internal engine_event_kind
engine_api_event_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_event_kind result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->event_count) return result;
    event_kind *kind = content->events + index;
    result.id       = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.check    = kind->check;
    result.min_days = kind->min_days;
    result.chance   = kind->chance;
    return result;
}

internal engine_pending_event
engine_api_pending_event(void *opaque)
{
    sim_pending_event *pending = &((engine *)opaque)->sim->pending;
    engine_pending_event result = {};
    result.officer_a = result.officer_b = ENGINE_NO_OFFICER;
    result.city  = ENGINE_NO_CITY;
    result.force = ENGINE_NO_FACTION;
    if (!pending->active) return result;
    result.active    = true;
    result.event     = pending->event;
    result.force     = pending->force;
    result.officer_a = pending->officer_a == SIM_NO_OFFICER ? (u16)ENGINE_NO_OFFICER : pending->officer_a;
    result.officer_b = pending->officer_b == SIM_NO_OFFICER ? (u16)ENGINE_NO_OFFICER : pending->officer_b;
    result.city      = pending->city == SIM_NO_CITY ? (u8)ENGINE_NO_CITY : pending->city;
    result.choices   = pending->choices;
    return result;
}

/* ---- the commands ----------------------------------------------------------------------- */

internal sim_m8_order
engine_m8_order(u8 force_id, u8 city_id, u16 officer, u8 value)
{
    sim_m8_order order = {};
    order.force   = force_id;
    order.city_id = city_id;
    order.officer = officer;
    order.value   = value;
    return order;
}

internal engine_command_result
engine_run_m8(engine *e, sim_command_kind kind, sim_m8_order order)
{
    engine_command_result result = {};
    switch (kind)
    {
        case SIM_COMMAND_ADVISER:       result.refusal = sim_check_adviser(e->sim, &order).refusal;       break;
        case SIM_COMMAND_COUNCIL:       result.refusal = sim_check_council(e->sim, &order).refusal;       break;
        case SIM_COMMAND_EVENT_CHOICE:  result.refusal = sim_check_event_choice(e->sim, &order).refusal;  break;
        case SIM_COMMAND_TRIGGER_EVENT: result.refusal = sim_check_trigger_event(e->sim, &order).refusal; break;
        default:                        result.refusal = SIM_REFUSAL_NO_EVENT;                            break;
    }
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, kind);
    command.m8 = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_corps(void *opaque, const sim_corps_order *order)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    if (!order) { result.refusal = SIM_REFUSAL_NO_CORPS; return result; }
    sim_corps_order copy = *order;
    result.refusal = sim_check_corps(e->sim, &copy).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_CORPS);
    command.corps = copy;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_adviser(void *opaque, u8 force_id, u16 officer)
{
    return engine_run_m8((engine *)opaque, SIM_COMMAND_ADVISER, engine_m8_order(force_id, SIM_NO_CITY, officer, 0));
}

internal engine_command_result
engine_api_command_council(void *opaque, u8 force_id, u8 city_id, u8 proposal)
{
    return engine_run_m8((engine *)opaque, SIM_COMMAND_COUNCIL, engine_m8_order(force_id, city_id, SIM_NO_OFFICER, proposal));
}

internal engine_command_result
engine_api_command_event_choice(void *opaque, u8 choice)
{
    engine *e = (engine *)opaque;
    return engine_run_m8(e, SIM_COMMAND_EVENT_CHOICE, engine_m8_order(e->sim->pending.force, SIM_NO_CITY, SIM_NO_OFFICER, choice));
}

/* ---- test and editor commands ------------------------------------------------------------ */

internal engine_command_result
engine_api_trigger_event(void *opaque, u8 event, u8 force_id, u8 city_id, u16 officer_or_unit)
{
    u8  force   = force_id == ENGINE_NO_FACTION ? (u8)SIM_NO_FORCE : force_id;
    u8  city    = city_id == ENGINE_NO_CITY ? (u8)SIM_NO_CITY : city_id;
    u16 officer = officer_or_unit == ENGINE_NO_OFFICER ? (u16)SIM_NO_OFFICER : officer_or_unit;
    return engine_run_m8((engine *)opaque, SIM_COMMAND_TRIGGER_EVENT, engine_m8_order(force, city, officer, event));
}

internal void
engine_api_set_emperor(void *opaque, u8 who, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim->emperor.who  = who < EMPEROR_KIND_COUNT ? who : (u8)EMPEROR_NONE;
    sim->emperor.city = sim_find_city(sim, city_id) ? city_id : (u8)SIM_NO_CITY;
}

internal void
engine_api_set_base(void *opaque, u32 index, u8 force_id, u32 troops, u32 durability)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim_base_state *base = sim_find_base(sim, index);
    if (!base) return;
    base->force      = force_id == ENGINE_NO_FACTION || force_id >= SIM_FORCE_CAPACITY ? (u8)SIM_NO_FORCE : force_id;
    base->troops     = Minimum(troops, base->force == SIM_NO_FORCE ? sim->content.rules.base_troop_cap : sim_base_troop_cap(sim, base->force));
    base->durability = Minimum(durability, sim->content.bases[index].durability);
}

internal void
engine_bind_rivals_api(engine_api *api)
{
    api->base_count           = engine_api_base_count;
    api->base_at              = engine_api_base_at;
    api->base_at_hex          = engine_api_base_at_hex;
    api->corps_at             = engine_api_corps_at;
    api->city_corps           = engine_api_city_corps;
    api->adviser              = engine_api_adviser;
    api->advice               = engine_api_advice;
    api->council              = engine_api_council;
    api->ai_plan              = engine_api_ai_plan;
    api->emperor              = engine_api_emperor;
    api->relic_count          = engine_api_relic_count;
    api->relic_at             = engine_api_relic_at;
    api->event_kind_count     = engine_api_event_kind_count;
    api->event_kind_at        = engine_api_event_kind_at;
    api->pending_event        = engine_api_pending_event;
    api->command_corps        = engine_api_command_corps;
    api->command_adviser      = engine_api_command_adviser;
    api->command_council      = engine_api_command_council;
    api->command_event_choice = engine_api_command_event_choice;
    api->trigger_event        = engine_api_trigger_event;
    api->set_emperor          = engine_api_set_emperor;
    api->set_base             = engine_api_set_base;
}
