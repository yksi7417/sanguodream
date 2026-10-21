/* Unit tests for code/render/portraits.h: the list of the pictures from
   the member names of the archive, and the picture of an officer. */

#define TEST_UTF8_LIU_BEI  "\xE5\x8A\x89\xE5\x82\x99"   /* U+5289 U+5099 */
#define TEST_UTF8_GUAN_YU  "\xE9\x97\x9C\xE7\xBE\xBD"   /* U+95DC U+7FBD */
#define TEST_UTF8_ZHANG_YI "\xE5\xBC\xB5\xE7\xBF\xBC"   /* U+5F35 U+7FFC */
#define TEST_UTF8_CAO_CAO  "\xE6\x9B\xB9\xE6\x93\x8D"   /* U+66F9 U+64CD */
#define TEST_UTF8_NO_NAME  "\xE7\x84\xA1\xE5\x90\x8D"   /* U+7121 U+540D */

internal b32
portrait_test_add(portrait_list *list, u32 member, const char *path)
{
    return portrait_list_add(list, member, path, (u32)strlen(path));
}

internal void
unit_portraits_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    portrait_list *list = PushStruct(&arena, portrait_list);

    TEST_CHECK(counts, portrait_pick(list, TEST_UTF8_LIU_BEI, 7) == PORTRAIT_NONE, "an empty list has no picture");
    TEST_CHECK(counts, portrait_test_add(list, 0, "Three Kingdoms Portraits/Shu/02 Guan Yu " TEST_UTF8_GUAN_YU ".jpg"), "a numbered picture is added");
    TEST_CHECK(counts, portrait_test_add(list, 1, "Three Kingdoms Portraits/Shu/01 Liu Bei " TEST_UTF8_LIU_BEI ".jpg"), "a second numbered picture is added");
    TEST_CHECK(counts, portrait_test_add(list, 2, "Three Kingdoms Portraits/Extras (not on the 120 list)/EXTRA - Zhang Yi " TEST_UTF8_ZHANG_YI ".jpg"), "an extra is added");
    TEST_CHECK(counts, !portrait_test_add(list, 3, "Three Kingdoms Portraits/Wei/"), "a folder is not a picture");
    TEST_CHECK(counts, !portrait_test_add(list, 4, "Three Kingdoms Portraits/readme.txt"), "a text file is not a picture");
    TEST_CHECK(counts, portrait_test_add(list, 5, "Wei/10 Cao Cao " TEST_UTF8_CAO_CAO ".JPG"), "the extension can be upper case");

    TEST_CHECK(counts, list->count == 4, "four pictures");
    TEST_CHECK(counts, list->pictures[0].member == 0 && list->pictures[0].number == 2 && strcmp(list->pictures[0].name, TEST_UTF8_GUAN_YU) == 0,
               "the number and the Chinese name come from the file name");
    TEST_CHECK(counts, list->pictures[2].number == 0 && strcmp(list->pictures[2].name, TEST_UTF8_ZHANG_YI) == 0, "an extra has no number");
    TEST_CHECK(counts, list->pictures[3].member == 5, "a picture keeps the index of its member");
    TEST_CHECK(counts, list->numbered_count == 3 && list->numbered[0] == 1 && list->numbered[1] == 0 && list->numbered[2] == 3,
               "the numbered pictures are in the order of their numbers, without the extra");

    TEST_CHECK(counts, portrait_pick(list, TEST_UTF8_LIU_BEI, 999) == 1, "the name of the officer picks his own picture");
    TEST_CHECK(counts, portrait_pick(list, TEST_UTF8_ZHANG_YI, 999) == 2, "an extra matches by name");
    TEST_CHECK(counts, portrait_pick(list, TEST_UTF8_NO_NAME, 0) == 1, "an officer without a picture: id 0 gets number 01");
    TEST_CHECK(counts, portrait_pick(list, TEST_UTF8_NO_NAME, 4) == 0, "id 4 modulo 3 numbered pictures gets number 02");
    TEST_CHECK(counts, portrait_pick(list, TEST_UTF8_NO_NAME, 5) == 3, "id 5 gets number 10");
    TEST_CHECK(counts, portrait_pick(list, TEST_UTF8_NO_NAME, 5) == portrait_pick(list, "", 5), "the choice depends only on the id");
    TEST_CHECK(counts, portrait_pick(list, 0, 3) == 1, "no name uses the id");
    test_arena_destroy(&arena);
}
