/* Unit tests for code/render/faces.h: the generated face of an officer
   without a picture. The same id gives the same face. Different ids give
   different faces. Each trait value is reachable. The sex and the age
   change the face. The key packs and unpacks without loss. The paint
   gives an opaque picture that differs between two faces. */

internal b32
face_test_same(const face_description *a, const face_description *b)
{
    return face_key_pack(a) == face_key_pack(b);
}

internal void
unit_faces_tests(test_counts *counts)
{
    /* The same officer always gets the same face. */
    face_description first  = face_describe(17, false, 30, 1);
    face_description second = face_describe(17, false, 30, 1);
    TEST_CHECK(counts, face_test_same(&first, &second), "the same id gives the same face");
    TEST_CHECK(counts, face_hash(17) == face_hash(17), "the hash is a pure function");
    TEST_CHECK(counts, face_hash(17) != face_hash(18), "the hash spreads close ids");

    /* Different ids give different faces, over the officer slots of a save. */
    u32 distinct = 0;
    for (u32 id = 1; id < FACE_SLOTS; ++id)
    {
        face_description before = face_describe(id - 1, false, 30, 1);
        face_description now    = face_describe(id, false, 30, 1);
        if (!face_test_same(&before, &now)) ++distinct;
    }
    TEST_CHECK(counts, distinct > (FACE_SLOTS - 1) * 9 / 10, "nine ids in ten differ from the id before");

    /* Every emotion, skin, hair tone, style, eye, beard and background is reachable. */
    u32 emotions = 0, skins = 0, hairs = 0, styles = 0, eyes = 0, beards = 0, backgrounds = 0, shapes = 0;
    for (u32 id = 0; id < FACE_SLOTS; ++id)
    {
        face_description face = face_describe(id, false, 30, 255);
        emotions    |= 1u << face.emotion;
        skins       |= 1u << face.skin;
        hairs       |= 1u << face.hair;
        styles      |= 1u << face.hair_style;
        eyes        |= 1u << face.eyes;
        beards      |= 1u << face.beard;
        backgrounds |= 1u << face.background;
        shapes      |= 1u << face.shape;
    }
    TEST_CHECK(counts, emotions == (1u << FACE_EMOTION_COUNT) - 1, "every emotion is reachable");
    TEST_CHECK(counts, skins == (1u << FACE_SKIN_COUNT) - 1, "every skin tone is reachable");
    TEST_CHECK(counts, hairs == (1u << (FACE_HAIR_COUNT - 1)) - 1, "every hair tone but grey is reachable for an adult");
    TEST_CHECK(counts, styles == (1u << FACE_HAIR_STYLE_COUNT) - 1, "every hair style is reachable");
    TEST_CHECK(counts, eyes == (1u << FACE_EYE_COUNT) - 1, "every eye shape is reachable");
    TEST_CHECK(counts, beards == (1u << FACE_BEARD_COUNT) - 1, "every beard is reachable for a man");
    TEST_CHECK(counts, backgrounds == (1u << FACE_BACKGROUND_COUNT) - 1, "every background is reachable");
    TEST_CHECK(counts, shapes == (1u << FACE_SHAPE_COUNT) - 1, "every head shape is reachable");

    /* The sex and the age of the officer. */
    b32 woman_has_no_beard = true, youth_has_no_beard = true, old_is_grey = true;
    for (u32 id = 0; id < FACE_SLOTS; ++id)
    {
        if (face_describe(id, true, 30, 1).beard != FACE_BEARD_NONE) woman_has_no_beard = false;
        if (face_describe(id, false, 18, 1).beard != FACE_BEARD_NONE) youth_has_no_beard = false;
        if (face_describe(id, false, 61, 1).hair != FACE_HAIR_GREY) old_is_grey = false;
    }
    TEST_CHECK(counts, woman_has_no_beard, "a woman has no beard");
    TEST_CHECK(counts, youth_has_no_beard, "a young officer has no beard");
    TEST_CHECK(counts, old_is_grey, "an old officer has grey hair");
    TEST_CHECK(counts, face_describe(5, false, 24, 1).age == FACE_YOUNG, "24 years is young");
    TEST_CHECK(counts, face_describe(5, false, 25, 1).age == FACE_ADULT, "25 years is adult");
    TEST_CHECK(counts, face_describe(5, false, 50, 1).age == FACE_OLD, "50 years is old");
    face_description man = face_describe(5, false, 30, 1), woman = face_describe(5, true, 30, 1);
    TEST_CHECK(counts, man.skin == woman.skin && man.background == woman.background, "the sex changes no other trait");

    /* The personality tints the emotion of some officers, and never past the five emotions. */
    u32 tinted = 0;
    b32 in_range = true;
    for (u32 id = 0; id < FACE_SLOTS; ++id)
    {
        face_description plain = face_describe(id, false, 30, 255), reckless = face_describe(id, false, 30, 3);
        if (plain.emotion != reckless.emotion) ++tinted;
        if (reckless.emotion >= FACE_EMOTION_COUNT) in_range = false;
    }
    TEST_CHECK(counts, tinted > 0 && tinted < FACE_SLOTS, "the personality tints some faces and not all");
    TEST_CHECK(counts, in_range, "a tinted emotion is one of the five");

    /* The key round trip. */
    face_description sample = face_describe(321, true, 55, 2);
    face_description back   = face_key_unpack(face_key_pack(&sample));
    TEST_CHECK(counts, face_key_pack(&sample) & FACE_KEY_VALID, "a packed key has the valid bit");
    TEST_CHECK(counts, back.shape == sample.shape && back.skin == sample.skin && back.hair == sample.hair &&
                       back.hair_style == sample.hair_style && back.eyes == sample.eyes && back.beard == sample.beard &&
                       back.background == sample.background && back.emotion == sample.emotion &&
                       back.age == sample.age && back.female == sample.female, "the key unpacks to the same face");

    /* The paint. */
    local_persist u8 pixels_a[FACE_WIDTH * FACE_HEIGHT * 4], pixels_b[FACE_WIDTH * FACE_HEIGHT * 4];
    face_description face_a = face_describe(7, false, 30, 1), face_b = face_describe(8, true, 60, 3);
    face_paint(&face_a, pixels_a);
    face_paint(&face_b, pixels_b);
    b32 opaque = true, differs = false, has_skin = false;
    face_look look = face_look_of(&face_a);
    for (u32 pixel = 0; pixel < FACE_WIDTH * FACE_HEIGHT; ++pixel)
    {
        if (pixels_a[pixel * 4 + 3] != 255) opaque = false;
        if (memcmp(pixels_a + pixel * 4, pixels_b + pixel * 4, 4) != 0) differs = true;
        if (pixels_a[pixel * 4] == look.skin.r && pixels_a[pixel * 4 + 1] == look.skin.g && pixels_a[pixel * 4 + 2] == look.skin.b) has_skin = true;
    }
    TEST_CHECK(counts, opaque, "every pixel is opaque");
    TEST_CHECK(counts, differs, "two faces paint different pictures");
    TEST_CHECK(counts, has_skin, "the picture has the skin tone of the face");
    u8 *corner = pixels_a;
    TEST_CHECK(counts, corner[0] == look.background.r && corner[1] == look.background.g && corner[2] == look.background.b, "the top left pixel is the background");
}
