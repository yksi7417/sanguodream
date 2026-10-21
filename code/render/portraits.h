#ifndef PORTRAITS_H
#define PORTRAITS_H
/* The default portraits: the list of the pictures in the portrait archive,
   and the picture of an officer (docs/ASSETS.md). The platform layer adds
   one picture for each JPEG member of the archive. The UI picks a picture
   for an officer and asks the platform for its texture.

   The member names have the form "<folder>/<number> <English name>
   <Chinese name>.jpg", for example "Shu/01 Liu Bei 劉備.jpg". The extras
   have the form "EXTRA - Zhang Yi 張翼.jpg" and no number. The picture of
   an officer is:
   1. the picture with the Chinese name of the officer, if there is one;
   2. else the numbered picture at (officer id modulo the numbered count),
      in the order of the numbers.
   The second rule is stable: the same officer id always gets the same
   picture from the same archive. */

#include "base.h"

#define PORTRAIT_LIST_CAPACITY 512
#define PORTRAIT_NAME_SIZE     32     /* UTF-8 bytes of the Chinese name and its zero */
#define PORTRAIT_NONE          0xFFFFFFFFu

struct portrait_picture
{
    u32  member;                       /* the index of the member in the archive */
    u32  number;                       /* from the file name; 0 for an extra */
    char name[PORTRAIT_NAME_SIZE];     /* the Chinese name, UTF-8 */
};

struct portrait_list
{
    portrait_picture pictures[PORTRAIT_LIST_CAPACITY];
    u32              count;
    u32              numbered[PORTRAIT_LIST_CAPACITY];   /* picture indexes, in the order of their numbers */
    u32              numbered_count;
};

internal b32
portrait_has_jpeg_extension(const char *name, u32 size)
{
    const char *extensions[2] = { ".jpg", ".jpeg" };
    for (u32 kind = 0; kind < 2; ++kind)
    {
        u32 length = (u32)strlen(extensions[kind]);
        if (size < length) continue;
        b32 same = true;
        for (u32 at = 0; at < length; ++at)
        {
            char letter = name[size - length + at];
            if (letter >= 'A' && letter <= 'Z') letter = (char)(letter - 'A' + 'a');
            if (letter != extensions[kind][at]) same = false;
        }
        if (same) return true;
    }
    return false;
}

/* Adds the member if its name is a JPEG file with a Chinese name. The name
   is not zero-terminated. False if the member is not a picture or the list
   is full. */
internal b32
portrait_list_add(portrait_list *list, u32 member, const char *path, u32 path_size)
{
    if (list->count >= PORTRAIT_LIST_CAPACITY || !portrait_has_jpeg_extension(path, path_size)) return false;
    u32 base = 0;
    for (u32 at = 0; at < path_size; ++at) if (path[at] == '/' || path[at] == '\\') base = at + 1;
    u32 stem_end = path_size;
    while (stem_end > base && path[stem_end - 1] != '.') --stem_end;
    if (stem_end == base) return false;
    stem_end -= 1;   /* the dot */

    u32 name_start = stem_end;
    while (name_start > base && path[name_start - 1] != ' ') --name_start;
    u32 name_size = stem_end - name_start;
    if (name_size == 0 || name_size >= PORTRAIT_NAME_SIZE) return false;

    portrait_picture *picture = list->pictures + list->count;
    picture->member = member;
    picture->number = 0;
    for (u32 at = base; at < stem_end && path[at] >= '0' && path[at] <= '9'; ++at) picture->number = picture->number * 10 + (u32)(path[at] - '0');
    memcpy(picture->name, path + name_start, name_size);
    picture->name[name_size] = 0;

    /* Insert into the numbered order: by number, then by the order of addition. */
    if (picture->number != 0)
    {
        u32 slot = list->numbered_count;
        while (slot > 0 && list->pictures[list->numbered[slot - 1]].number > picture->number)
        {
            list->numbered[slot] = list->numbered[slot - 1];
            --slot;
        }
        list->numbered[slot] = list->count;
        list->numbered_count += 1;
    }
    list->count += 1;
    return true;
}

/* The picture index of an officer (the rules at the top of this file), or
   PORTRAIT_NONE if the list has no picture. name is the UTF-8 Chinese name
   of the officer and can be empty. */
internal u32
portrait_pick(const portrait_list *list, const char *name, u32 officer_id)
{
    if (!list || list->count == 0) return PORTRAIT_NONE;
    if (name && name[0])
    {
        for (u32 index = 0; index < list->count; ++index)
        {
            if (strcmp(list->pictures[index].name, name) == 0) return index;
        }
    }
    if (list->numbered_count) return list->numbered[officer_id % list->numbered_count];
    return officer_id % list->count;
}

#endif /* PORTRAITS_H */
