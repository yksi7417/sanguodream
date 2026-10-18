/* Unit tests for code/sim/save_format: each record decoder on a hand-built
   byte array, then save_load on a hand-built save file.

   The tests write values at the offsets of the spec tables as plain numbers
   (spec-src/spec/formats/save-file.md), not through the decoder's enums.
   A wrong enum value in the decoder then fails a test. Bytes that the
   decoder must not read are filled with FILLER_BYTE. */

#define FILLER_BYTE 0xEE

internal void
test_put_u16(u8 *bytes, u32 offset, u16 value)
{
    bytes[offset]     = (u8)(value & 0xFF);
    bytes[offset + 1] = (u8)(value >> 8);
}

internal void
test_put_u32(u8 *bytes, u32 offset, u32 value)
{
    for (u32 index = 0; index < 4; ++index) bytes[offset + index] = (u8)(value >> (8 * index));
}

/* Arbitrary non-ASCII bytes in Big5 range; the test claims no characters. */
global_variable const u8 test_family_name[5]   = { 0xB1, 0xE4, 0x00, 0x00, 0x00 };
global_variable const u8 test_given_name[5]    = { 0xBE, 0xDE, 0x00, 0x00, 0x00 };
global_variable const u8 test_courtesy_name[7] = { 0xA9, 0x73, 0xBC, 0x77, 0x00, 0x00, 0x00 };

/* The officer of the identity check in save-file.md: officer 343 with stats
   96/72/91/94/96, born 155, died 220, force 10, status 3. The other values
   are inside the observed ranges of their rows. */
internal void
test_build_officer_record(u8 *record)
{
    memset(record, FILLER_BYTE, 203);
    memcpy(record + 0,  test_family_name,   5);    /* +0  family name */
    memcpy(record + 5,  test_given_name,    5);    /* +5  given name */
    memcpy(record + 10, test_courtesy_name, 7);    /* +10 courtesy name */
    test_put_u16(record, 53, 833);                 /* +53 portrait number */
    test_put_u16(record, 56, 184);                 /* +56 appearance year */
    test_put_u16(record, 58, 155);                 /* +58 birth year */
    test_put_u16(record, 60, 220);                 /* +60 death year */
    test_put_u16(record, 63, 343);                 /* +63 family line head */
    test_put_u16(record, 65, 2028);                /* +65 father */
    test_put_u16(record, 67, 0xFFFF);              /* +67 related officer */
    test_put_u16(record, 70, 17);                  /* +70 spouse */
    test_put_u16(record, 72, 0xFFFF);              /* +72 sworn-brother group head */
    record[74] = 25;                               /* +74 compatibility */
    u16 liked[5]    = { 1, 2, 3, 4, 5 };
    u16 disliked[5] = { 0xFFFF, 6, 7, 8, 0xFFFF };
    for (u32 slot = 0; slot < 5; ++slot)
    {
        test_put_u16(record, 75 + 2 * slot, liked[slot]);      /* +75 liked officers */
        test_put_u16(record, 85 + 2 * slot, disliked[slot]);   /* +85 disliked officers */
    }
    record[95] = 10;                               /* +95 force id */
    test_put_u16(record, 96, 12);                  /* +96 base location */
    test_put_u16(record, 98, 92);                  /* +98 current position: unit 5 */
    record[100] = 3;                               /* +100 status */
    test_put_u16(record, 105, 27400);              /* +105 merit */
    u8 aptitudes[6] = { 3, 3, 1, 2, 0, 2 };
    memcpy(record + 107, aptitudes, 6);            /* +107 unit aptitudes */
    u8 stats[5] = { 96, 72, 91, 94, 96 };
    memcpy(record + 113, stats, 5);                /* +113 stats */
    record[124] = 99;                              /* +124 skill number */
    test_put_u16(record, 148, 0x0009);             /* +148 flags: bit 0 set */
    for (u32 slot = 0; slot < 5; ++slot)
    {
        test_put_u16(record, 153 + 2 * slot, (u16)(100 + slot));   /* +153 per-stat experience */
    }
    record[194] = 4;                               /* +194 order kind */
}

internal void
unit_save_officer_tests(test_counts *counts)
{
    u8 record[203];
    test_build_officer_record(record);
    officer_record officer;
    save_decode_officer(record, 343, &officer);

    TEST_CHECK(counts, officer.id == 343, "officer id is the slot");
    TEST_CHECK(counts, memcmp(officer.family_name, test_family_name, 5) == 0 &&
                       memcmp(officer.given_name, test_given_name, 5) == 0 &&
                       memcmp(officer.courtesy_name, test_courtesy_name, 7) == 0,
               "officer names are the Big5 bytes verbatim");
    TEST_CHECK(counts, officer.portrait_number == 833, "officer +53 portrait number");
    TEST_CHECK(counts, officer.appearance_year == 184 && officer.birth_year == 155 && officer.death_year == 220,
               "officer +56 +58 +60 years");
    TEST_CHECK(counts, officer.family_line_head == 343 && officer.father == 2028 && officer.related_officer == 0xFFFF,
               "officer +63 +65 +67 family");
    TEST_CHECK(counts, officer.spouse == 17 && officer.sworn_brother_group_head == OFFICER_ID_NONE,
               "officer +70 spouse and +72 sworn-brother group head");
    TEST_CHECK(counts, officer.compatibility == 25, "officer +74 compatibility");
    TEST_CHECK(counts, officer.liked_officers[0] == 1 && officer.liked_officers[4] == 5, "officer +75 liked officers");
    TEST_CHECK(counts, officer.disliked_officers[0] == OFFICER_ID_NONE && officer.disliked_officers[1] == 6 &&
                       officer.disliked_officers[4] == OFFICER_ID_NONE, "officer +85 disliked officers keep empty slots in place");
    TEST_CHECK(counts, officer.force_id == 10, "officer +95 force id");
    TEST_CHECK(counts, officer.base_location == 12 && officer.current_position == 92, "officer +96 base location and +98 current position");
    TEST_CHECK(counts, officer.status == OFFICER_STATUS_REGULAR_OFFICER, "officer +100 status");
    TEST_CHECK(counts, officer.merit == 27400, "officer +105 merit");
    TEST_CHECK(counts, officer.unit_aptitudes[OFFICER_APTITUDE_SPEAR] == 3 && officer.unit_aptitudes[OFFICER_APTITUDE_CROSSBOW] == 1 &&
                       officer.unit_aptitudes[OFFICER_APTITUDE_NAVY] == 2, "officer +107 unit aptitudes");
    TEST_CHECK(counts, officer.leadership == 96 && officer.war == 72 && officer.intelligence == 91 &&
                       officer.politics == 94 && officer.charm == 96, "officer +113 stats in spec order");
    TEST_CHECK(counts, officer.skill_number == 99, "officer +124 skill number");
    TEST_CHECK(counts, officer.flags == 0x0009 && officer.has_acted_this_turn, "officer +148 flags, bit 0 is has acted");
    TEST_CHECK(counts, officer.has_save_fields, "a save record has the save-only fields");
    TEST_CHECK(counts, officer.per_stat_experience[0] == 100 && officer.per_stat_experience[4] == 104, "officer +153 per-stat experience");
    TEST_CHECK(counts, officer.order_kind == 4, "officer +194 order kind");

    officer_record shared;
    officer_decode_shared(record, 343, &shared);
    TEST_CHECK(counts, !shared.has_save_fields && shared.order_kind == 0 && shared.per_stat_experience[0] == 0,
               "the shared decoder leaves the save-only fields at zero");
    TEST_CHECK(counts, shared.birth_year == 155 && shared.charm == 96, "the shared decoder reads bytes +0..+151");

    test_put_u16(record, 148, 0x0008);
    save_decode_officer(record, 343, &officer);
    TEST_CHECK(counts, !officer.has_acted_this_turn, "flags without bit 0: has not acted");

    TEST_CHECK(counts, !officer_slot_is_empty(&officer), "a slot with a name is not empty");
    u8 empty_record[203];
    memset(empty_record, 0, sizeof(empty_record));
    empty_record[100] = 3;   /* status alone does not make a slot hold an officer */
    officer_record empty;
    save_decode_officer(empty_record, 900, &empty);
    TEST_CHECK(counts, officer_slot_is_empty(&empty), "a slot with all name bytes zero is empty");
    empty_record[10 + 6] = 0x41;   /* last byte of the courtesy name */
    save_decode_officer(empty_record, 900, &empty);
    TEST_CHECK(counts, !officer_slot_is_empty(&empty), "any name byte makes the slot hold an officer");

    TEST_CHECK(counts, strcmp(officer_status_name(OFFICER_STATUS_RULER), "ruler") == 0 &&
                       strcmp(officer_status_name(5), "?") == 0, "status labels; unlabelled values are ?");
}

internal void
unit_save_city_tests(test_counts *counts)
{
    u8 record[107];
    memset(record, FILLER_BYTE, sizeof(record));
    record[0] = 0x11;                          /* +0  flags */
    test_put_u16(record, 9, 442);              /* +9  commanding officer id */
    record[11] = 9;                            /* +11 owner force id */
    test_put_u32(record, 12, 150000);          /* +12 capacity constant */
    test_put_u32(record, 16, 82767);           /* +16 gold */
    test_put_u32(record, 20, 45395);           /* +20 troops */
    test_put_u32(record, 24, 233450);          /* +24 food */
    u32 stocks[10] = { 10288, 9884, 4294, 399, 1, 2, 3, 4, 0, 0 };
    for (u32 stock = 0; stock < 10; ++stock) test_put_u32(record, 32 + 4 * stock, stocks[stock]);   /* +32 equipment */
    record[77] = 1;                            /* +77 large-city flag */
    test_put_u16(record, 80, 8000);            /* +80 durability maximum */
    record[85] = 97;                           /* +85 public order */

    city_record city;
    save_decode_city(record, 36, &city);
    TEST_CHECK(counts, city.id == 36 && city.flags == 0x11, "city id and +0 flags");
    TEST_CHECK(counts, city.commanding_officer_id == 442 && city.owner_force_id == 9, "city +9 commander and +11 owner");
    TEST_CHECK(counts, city.capacity_constant == 150000, "city +12 capacity constant");
    TEST_CHECK(counts, city.gold == 82767 && city.troops == 45395 && city.food == 233450, "city +16 gold, +20 troops, +24 food");
    TEST_CHECK(counts, city.equipment_stocks[CITY_STOCK_SPEARS] == 10288 && city.equipment_stocks[CITY_STOCK_WAR_HORSES] == 399 &&
                       city.equipment_stocks[CITY_STOCK_CATAPULTS] == 3 && city.equipment_stocks[CITY_STOCK_SHIP_TYPE_3] == 0,
               "city +32 equipment stocks in spec order");
    TEST_CHECK(counts, city.large_city_flag == 1 && city.durability_maximum == 8000 && city.public_order == 97,
               "city +77 +80 +85 hypotheses");
}

internal void
unit_save_item_tests(test_counts *counts)
{
    u8 record[57];
    memset(record, 0, sizeof(record));
    record[0] = 0xA8; record[1] = 0xAA; record[2] = 0xA5; record[3] = 0x9F;   /* +0 name, Big5 bytes */
    record[51] = 30;                           /* +51 effect strength */
    record[52] = 4;                            /* +52 item template number */
    test_put_u16(record, 53, 660);             /* +53 owner officer id */
    record[55] = 255;                          /* +55 holding city */
    record[56] = 1;                            /* +56 owned */

    item_record item;
    item_decode(record, 0, &item);
    TEST_CHECK(counts, item.name[0] == 0xA8 && item.name[3] == 0x9F && item.name[4] == 0, "item +0 name verbatim");
    TEST_CHECK(counts, item.effect_strength == 30 && item.item_template_number == 4, "item +51 strength and +52 template");
    TEST_CHECK(counts, item.owner_officer_id == 660 && item.holding_city == 255 && item.owned == 1, "item +53 owner, +55 city, +56 owned");
}

/* A header of 0x1BF bytes with the values of the Container table. */
internal void
test_build_save_header(u8 *file, u32 uncompressed_length, u32 compressed_length)
{
    memset(file, 0, 0x1BF);
    file[0x002] = 0xFE; file[0x003] = 0xFF;                     /* constant marker */
    test_put_u32(file, 0x004, 0x1A);
    memcpy(file + 0x008, "KOEI%SAN11", 10);                     /* magic text */
    test_put_u32(file, 0x018, 1);
    test_put_u32(file, 0x01C, 2);
    test_put_u32(file, 0x020, 1);
    file[0x05A] = 0xB1; file[0x05B] = 0x69;                     /* player's ruler name */
    test_put_u16(file, 0x063, 66);                              /* unknown */
    file[0x065] = 255;
    file[0x066] = 2;                                            /* scenario number */
    test_put_u16(file, 0x067, 194);                             /* scenario start year */
    file[0x069] = 6;                                            /* scenario start month */
    file[0x06A] = 1;                                            /* scenario start ten-day period */
    file[0x06B] = 0xB8; file[0x06C] = 0x73;                     /* scenario name */
    test_put_u32(file, 0x07C, 77);                              /* unknown */
    memcpy(file + 0x1B3, "MWC\x1A", 4);                         /* tag */
    test_put_u32(file, 0x1B7, uncompressed_length);
    test_put_u32(file, 0x1BB, compressed_length);
}

internal void
unit_save_header_tests(test_counts *counts)
{
    u8 file[0x1BF];
    test_build_save_header(file, 1154785, 1234);
    save_header header = save_decode_header(file);
    TEST_CHECK(counts, header.constant_at_0x000 == 0 && header.constant_marker[0] == 0xFE && header.constant_marker[1] == 0xFF &&
                       header.file_kind == 0x1A, "header constants and file kind");
    TEST_CHECK(counts, memcmp(header.magic_text, "KOEI%SAN11", 10) == 0, "header magic text");
    TEST_CHECK(counts, header.constant_at_0x018 == 1 && header.constant_at_0x01c == 2 && header.constant_at_0x020 == 1 &&
                       header.constant_at_0x065 == 255, "header constants at 0x018, 0x01C, 0x020, 0x065");
    TEST_CHECK(counts, header.player_ruler_name[0] == 0xB1 && header.player_ruler_name[1] == 0x69 && header.player_ruler_name[2] == 0,
               "header player's ruler name verbatim");
    TEST_CHECK(counts, header.unknown_at_0x063 == 66 && header.unknown_at_0x07c == 77, "header unknowns at 0x063 and 0x07C");
    TEST_CHECK(counts, header.scenario_number == 2 && header.scenario_start_year == 194 && header.scenario_start_month == 6 &&
                       header.scenario_start_ten_day_period == 1, "header scenario number and start date");
    TEST_CHECK(counts, header.scenario_name[0] == 0xB8 && header.scenario_name[1] == 0x73, "header scenario name verbatim");
    TEST_CHECK(counts, memcmp(header.tag, "MWC\x1A", 4) == 0, "header tag");
    TEST_CHECK(counts, header.uncompressed_payload_length == 1154785 && header.compressed_stream_length == 1234, "header lengths");
}

/* Wraps data in a zlib stream of stored blocks (at most 65535 bytes each). */
internal u64
test_zlib_store(const u8 *data, u64 size, u8 *out)
{
    u64 written = 0;
    out[written++] = 0x78;
    out[written++] = 0x01;
    u64 done = 0;
    do
    {
        u32 block = (u32)Minimum(size - done, (u64)0xFFFF);
        b32 last  = done + block == size;
        out[written++] = last ? 1 : 0;
        test_put_u16(out, (u32)written, (u16)block);
        test_put_u16(out, (u32)written + 2, (u16)~block);
        written += 4;
        memcpy(out + written, data + done, block);
        written += block;
        done    += block;
    } while (done < size);
    u32 check = adler32(data, size);
    out[written++] = (u8)(check >> 24);
    out[written++] = (u8)(check >> 16);
    out[written++] = (u8)(check >> 8);
    out[written++] = (u8)check;
    return written;
}

/* A save of the spec's shape: header, then a payload of 1,154,785 bytes with
   the scenario date at payload 0..4, officer 343 at 0x0488A4 + 203 * 343,
   city 36 at 0x0830FF + 107 * 36 and item 0 at 0x07F0E8. Every other officer
   slot has status 255 at +100. Returns the file size. */
internal u64
test_build_save_file(u8 *file, memory_arena *arena)
{
    u32 payload_length = 1154785;
    u8 *payload = (u8 *)arena_push_size(arena, payload_length, 1);
    payload[0] = 2;
    test_put_u16(payload, 1, 194);
    payload[3] = 6;
    payload[4] = 1;
    for (u32 slot = 0; slot < 1100; ++slot) payload[0x0488A4 + 203 * slot + 100] = 255;
    test_build_officer_record(payload + 0x0488A4 + 203 * 343);
    u8 *city = payload + 0x0830FF + 107 * 36;
    test_put_u16(city, 9, 343);
    city[11] = 10;
    test_put_u32(city, 16, 5000);
    payload[0x07F0E8 + 53] = 0x57;   /* item 0: owner 343 = 0x0157 */
    payload[0x07F0E8 + 54] = 0x01;
    /* M11: building record 87 is a mint (type 38) with 1000 durability on (122, 142). */
    u8 *building = payload + 0x0008A3 + 18 * 87;
    building[1] = 38;  building[2] = 255;
    building[3] = 0xE8; building[4] = 0x03;
    building[14] = 122; building[16] = 142;
    /* M11: unit 5 has the leader 343, no deputies and 3000 troops. */
    u8 *unit = payload + 0x086B71 + 98 * 5;
    unit[0] = 0x57; unit[1] = 0x01;
    unit[2] = 0xFF; unit[3] = 0xFF; unit[4] = 0xFF; unit[5] = 0xFF;
    unit[6] = 0xB8; unit[7] = 0x0B;
    unit[23] = 135; unit[24] = 0; unit[25] = 129; unit[26] = 0;   /* the hex 135,129 (spec 7fff546) */
    /* Spec b3ed57d: morale 100, gold 168, grain 6135, 混亂, 軍馬 (row 4) and 走舸 (row 9). */
    unit[8] = 100;
    unit[9] = 0xA8;
    unit[13] = 0xF7; unit[14] = 0x17;
    unit[17] = 1;
    unit[36] = 4; unit[41] = 9; unit[42] = 1;
    /* The per-hex block, the last 160000 bytes: 49,67 burns 3 turns; 10,20 is an event site (16). */
    u8 *hex_block = payload + SAVE_PAYLOAD_LENGTH - 160000;
    hex_block[4 * (49 * 200 + 67)] = 3;
    hex_block[4 * (10 * 200 + 20)] = 16;
    /* Record 55 lies past 0x08803B: the array goes on (sample.s11 has units 54 to 56). */
    u8 *late = payload + 0x086B71 + 98 * 55;
    late[0] = 0xB5; late[1] = 0x01;   /* the leader 437 */
    late[23] = 91; late[25] = 46;
    /* The gate and port records at 0x08428D (spec 9632e15): record 30 is base 72 (皖口港). */
    u8 *port = payload + 0x08428D + 70 * 30;
    port[9] = 6; port[10] = 0x7A; port[11] = 0x18;   /* owner 6, 6266 soldiers */
    u8 *free_port = payload + 0x08428D + 70 * 33;
    free_port[9] = 255;                              /* base 75: no owner */

    u64 stream_size = test_zlib_store(payload, payload_length, file + 0x1BF);
    test_build_save_header(file, payload_length, (u32)stream_size);
    return 0x1BF + stream_size;
}

internal void
unit_save_load_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(16));
    u8 *file = (u8 *)arena_push_size(&arena, Megabytes(2), 1);
    u64 file_size = test_build_save_file(file, &arena);

    save_file save;
    save_load_result result = save_load(&arena, file, file_size, &save);
    TEST_CHECK(counts, result.status == SAVE_LOAD_OK, "a hand-built save loads");
    TEST_CHECK(counts, result.inflate.bytes_written == 1154785, "the payload inflates to the declared length");
    TEST_CHECK(counts, save.header.scenario_start_year == 194, "the header is decoded");
    TEST_CHECK(counts, save.payload_scenario_number == 2 && save.payload_scenario_start_year == 194 &&
                       save.payload_scenario_start_month == 6 && save.payload_scenario_start_ten_day_period == 1,
               "payload bytes 0..4 hold the scenario date");
    TEST_CHECK(counts, save.officer_slot_count == 1100 && save.city_count == 42 && save.item_count == 50, "array sizes of the payload map");
    TEST_CHECK(counts, save.officers[343].id == 343 && save.officers[343].birth_year == 155 &&
                       save.officers[343].leadership == 96 && save.officers[343].force_id == 10,
               "officer 343 is read from its slot");
    TEST_CHECK(counts, officer_slot_is_empty(save.officers + 342) && officer_slot_is_empty(save.officers + 1099) &&
                       save.officers[342].status == OFFICER_STATUS_EMPTY_SLOT, "other slots are empty");
    TEST_CHECK(counts, save.cities[36].id == 36 && save.cities[36].commanding_officer_id == 343 &&
                       save.cities[36].owner_force_id == 10 && save.cities[36].gold == 5000, "city 36 is read from its slot");
    TEST_CHECK(counts, save.items[0].owner_officer_id == 343, "item 0 is read from its slot");
    TEST_CHECK(counts, save.building_count == 16384 && save.buildings[87].type == 38 && save.buildings[87].owner_force_id == 255 &&
                       save.buildings[87].durability == 1000 && save.buildings[87].x == 122 && save.buildings[87].y == 142,
               "building record 87 is read from its slot (M11)");
    TEST_CHECK(counts, save.unit_count == 388 && save.units[5].officers[0] == 343 && save.units[5].officers[1] == OFFICER_ID_NONE &&
                       save.units[5].officers[2] == OFFICER_ID_NONE && save.units[5].troops == 3000,
               "unit 5 is read from its slot; the array has 388 records up to the end of the table at 0x08803B (Q-216)");
    TEST_CHECK(counts, save.units[5].x == 135 && save.units[5].y == 129, "the hex of unit 5 is read from +23 x and +25 y");
    TEST_CHECK(counts, save.units[5].morale == 100 && save.units[5].gold == 168 && save.units[5].grain == 6135 &&
                       save.units[5].state == SAVE_UNIT_STATE_CONFUSED,
               "unit 5: +8 morale, +9 gold, +13 grain and +17 the state (1 = 混亂)");
    TEST_CHECK(counts, save.units[5].armament == 4 && save.units[5].ship == 9 && save.units[5].on_water == 1,
               "unit 5: +36 the land armament, +41 the ship class, +42 the water flag");
    TEST_CHECK(counts, save.hex_state_count == 40000 && (save.hex_states[49 * 200 + 67] & SAVE_HEX_FIRE_MASK) == 3 &&
                       save.hex_states[10 * 200 + 20] == SAVE_HEX_SITE_LOW && save.hex_states[0] == 0,
               "the per-hex block: index x * 200 + y; the low bits are the fire turns, 16 an event site");
    TEST_CHECK(counts, save.units[55].officers[0] == 437 && save.units[55].x == 91 && save.units[55].y == 46,
               "record 55 past 0x08803B is a unit record of the same stride");
    TEST_CHECK(counts, save.base_count == 45 && save.bases[30].id == 72 && save.bases[30].owner_force_id == 6 && save.bases[30].soldiers == 6266,
               "the base record of 皖口港 (base 72) gives the owner at +9 and the soldiers at +10");
    TEST_CHECK(counts, save.bases[0].id == 42 && save.bases[33].id == 75 && save.bases[33].owner_force_id == SAVE_BASE_NO_OWNER,
               "record k of the base array is base 42 + k; 255 at +9 is no owner");

    u64 used_after_load = arena.used;
    TEST_CHECK(counts, save_load(&arena, file, 0x100, &save).status == SAVE_LOAD_FILE_TOO_SMALL, "a file shorter than the header is refused");
    TEST_CHECK(counts, arena.used == used_after_load, "a refused file takes no memory");

    file[0x008] = 'X';
    TEST_CHECK(counts, save_load(&arena, file, file_size, &save).status == SAVE_LOAD_NOT_A_SAVE, "a wrong magic text is refused");
    file[0x008] = 'K';
    file[0x1B5] = 'X';
    TEST_CHECK(counts, save_load(&arena, file, file_size, &save).status == SAVE_LOAD_NOT_A_SAVE, "a wrong tag is refused");
    file[0x1B5] = 'C';

    test_put_u32(file, 0x1B7, 1000);
    TEST_CHECK(counts, save_load(&arena, file, file_size, &save).status == SAVE_LOAD_UNEXPECTED_PAYLOAD_LENGTH,
               "a payload length other than 1,154,785 is refused");
    test_put_u32(file, 0x1B7, 1154785);

    save_load_result truncated = save_load(&arena, file, file_size - 100, &save);
    TEST_CHECK(counts, truncated.status == SAVE_LOAD_INFLATE_FAILED && truncated.inflate.status == INFLATE_TRUNCATED_INPUT,
               "a cut stream fails to inflate and says why");
    TEST_CHECK(counts, arena.used == used_after_load && save.officers == 0, "a failed inflate gives its memory back");

    u64 before_second_load = arena.used;
    save_load(&arena, file, file_size, &save);
    u64 record_bytes = 1100 * sizeof(officer_record) + 42 * sizeof(city_record) + 50 * sizeof(item_record) +
                       SAVE_BUILDING_COUNT * sizeof(building_record) + SAVE_UNIT_CAPACITY * sizeof(unit_record) +   /* M11 */
                       SAVE_BASE_RECORD_COUNT * sizeof(base_record) + SAVE_HEX_COUNT * sizeof(u32);
    TEST_CHECK(counts, arena.used - before_second_load < record_bytes + 64, "a load keeps the records, not the payload");

    memory_arena small = arena_sub(&arena, Megabytes(1));
    TEST_CHECK(counts, save_load(&small, file, file_size, &save).status == SAVE_LOAD_OUT_OF_MEMORY && small.used == 0,
               "an arena without room for the payload is refused before any push");

    TEST_CHECK(counts, strcmp(save_load_status_name(SAVE_LOAD_NOT_A_SAVE), "not a save") == 0, "load status name");
    test_arena_destroy(&arena);
}

internal void
unit_save_format_tests(test_counts *counts)
{
    unit_save_officer_tests(counts);
    unit_save_city_tests(counts);
    unit_save_item_tests(counts);
    unit_save_header_tests(counts);
    unit_save_load_tests(counts);
}
