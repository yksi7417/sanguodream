#ifndef FACES_H
#define FACES_H
/* The generated face of an officer without a picture (docs/ASSETS.md,
   docs/GRAPHICS.md). A hash of the officer id picks each trait of the
   face. The traits are the shape of the head, the skin tone, the hair
   tone and style, the eyes, the beard, the background and the emotion.
   The sex and the
   age of the officer are inputs. A woman gets no beard and long hair. An
   old officer gets grey hair and lines under the eyes. The personality
   can tint the emotion.

   face_describe makes the description. face_key_pack folds it into one
   u32, so that the UI can pass it to the platform through one function.
   face_paint draws the face as an RGBA picture of FACE_WIDTH x
   FACE_HEIGHT pixels, with the 4:5 ratio of the default portraits. The
   platform paints a face one time for each officer and caches its texture
   by officer id (win32_faces.h). The same id always gives the same face.

   The hash is its own function. It never touches the PRNG of the sim. */

#include "base.h"

#define FACE_WIDTH     48
#define FACE_HEIGHT    60
#define FACE_SLOTS     1100          /* the officer ids of a save (SAVE_OFFICER_SLOTS): the size of the cache */
#define FACE_KEY_VALID 0x80000000u   /* the top bit of a packed key. A key of 0 is no face. */

enum face_emotion { FACE_CALM, FACE_HAPPY, FACE_ANGRY, FACE_SAD, FACE_SURPRISED, FACE_EMOTION_COUNT };
enum face_age     { FACE_YOUNG, FACE_ADULT, FACE_OLD, FACE_AGE_COUNT };
enum face_beard   { FACE_BEARD_NONE, FACE_BEARD_MOUSTACHE, FACE_BEARD_FULL, FACE_BEARD_COUNT };

#define FACE_SHAPE_COUNT       3    /* round, oval, square */
#define FACE_SKIN_COUNT        4
#define FACE_HAIR_COUNT        4    /* tones. The fourth is the grey of an old officer. */
#define FACE_HAIR_GREY         3
#define FACE_HAIR_STYLE_COUNT  4    /* fringe, side part, high forehead, topknot */
#define FACE_EYE_COUNT         3    /* almond, round, narrow */
#define FACE_BACKGROUND_COUNT  4
#define FACE_YOUNG_BELOW       25   /* years */
#define FACE_OLD_FROM          50
#define FACE_PERSONALITY_COUNT 4    /* engine_officer_detail: careful, calm, bold, reckless */

struct face_description
{
    u8 shape;        /* 0 .. FACE_SHAPE_COUNT - 1 */
    u8 skin;         /* 0 .. FACE_SKIN_COUNT - 1 */
    u8 hair;         /* 0 .. FACE_HAIR_COUNT - 1 */
    u8 hair_style;   /* 0 .. FACE_HAIR_STYLE_COUNT - 1 */
    u8 eyes;         /* 0 .. FACE_EYE_COUNT - 1 */
    u8 beard;        /* face_beard */
    u8 background;   /* 0 .. FACE_BACKGROUND_COUNT - 1 */
    u8 emotion;      /* face_emotion */
    u8 age;          /* face_age */
    u8 female;       /* 0 or 1 */
};

/* ---- the choice of the traits ------------------------------------------ */

/* A 32-bit integer hash with a good spread of the low bits. */
internal u32
face_hash(u32 value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

/* One trait of the officer: a number in 0 .. count - 1. Each trait has its
   own salt, so the traits of one officer are independent of each other. */
internal u32
face_trait(u32 officer_id, u32 salt, u32 count)
{
    return face_hash(officer_id * 0x9E3779B1u + salt * 0x85EBCA77u) % count;
}

internal u8
face_age_band(u32 age_years)
{
    if (age_years < FACE_YOUNG_BELOW) return FACE_YOUNG;
    if (age_years < FACE_OLD_FROM) return FACE_ADULT;
    return FACE_OLD;
}

/* The face of an officer. personality is the value of engine_officer_detail.
   A value outside 0 .. 3 gives no tint. */
internal face_description
face_describe(u32 officer_id, b32 female, u32 age_years, u32 personality)
{
    face_description face = {};
    face.shape      = (u8)face_trait(officer_id, 1, FACE_SHAPE_COUNT);
    face.skin       = (u8)face_trait(officer_id, 2, FACE_SKIN_COUNT);
    face.hair_style = (u8)face_trait(officer_id, 3, FACE_HAIR_STYLE_COUNT);
    face.eyes       = (u8)face_trait(officer_id, 4, FACE_EYE_COUNT);
    face.background = (u8)face_trait(officer_id, 5, FACE_BACKGROUND_COUNT);
    face.age        = face_age_band(age_years);
    face.female     = female ? 1 : 0;
    face.hair       = face.age == FACE_OLD ? (u8)FACE_HAIR_GREY : (u8)face_trait(officer_id, 6, FACE_HAIR_COUNT - 1);
    face.beard      = (face.female || face.age == FACE_YOUNG) ? (u8)FACE_BEARD_NONE : (u8)face_trait(officer_id, 7, FACE_BEARD_COUNT);
    face.emotion    = (u8)face_trait(officer_id, 8, FACE_EMOTION_COUNT);
    /* The personality tints the emotion of one officer in two. A bold
       officer smiles. A reckless one glares. A careful or calm one is calm. */
    if (personality < FACE_PERSONALITY_COUNT && face_trait(officer_id, 9, 2) == 1)
    {
        const u8 tint[FACE_PERSONALITY_COUNT] = { FACE_CALM, FACE_CALM, FACE_HAPPY, FACE_ANGRY };
        face.emotion = tint[personality];
    }
    return face;
}

/* The description as one u32 for the platform call. */
internal u32
face_key_pack(const face_description *face)
{
    u32 key = FACE_KEY_VALID;
    key |= (u32)(face->shape      & 3) << 0;
    key |= (u32)(face->skin       & 3) << 2;
    key |= (u32)(face->hair       & 3) << 4;
    key |= (u32)(face->hair_style & 3) << 6;
    key |= (u32)(face->eyes       & 3) << 8;
    key |= (u32)(face->beard      & 3) << 10;
    key |= (u32)(face->background & 3) << 12;
    key |= (u32)(face->emotion    & 7) << 14;
    key |= (u32)(face->age        & 3) << 17;
    key |= (u32)(face->female     & 1) << 19;
    return key;
}

internal face_description
face_key_unpack(u32 key)
{
    face_description face = {};
    face.shape      = (u8)((key >> 0)  & 3);
    face.skin       = (u8)((key >> 2)  & 3);
    face.hair       = (u8)((key >> 4)  & 3);
    face.hair_style = (u8)((key >> 6)  & 3);
    face.eyes       = (u8)((key >> 8)  & 3);
    face.beard      = (u8)((key >> 10) & 3);
    face.background = (u8)((key >> 12) & 3);
    face.emotion    = (u8)((key >> 14) & 7);
    face.age        = (u8)((key >> 17) & 3);
    face.female     = (u8)((key >> 19) & 1);
    return face;
}

/* ---- the picture --------------------------------------------------------

   The coordinates of the shapes are in units of the width of the picture.
   px runs from 0 to 1 and py from 0 to 1.25. The head is an ellipse with
   its centre at (0.5, 0.66). The eyes are at the height 0.66, the mouth at
   0.88. face_sample gives the colour of one point. It draws the layers
   from the back to the front:
   1. the background, the long hair, the robe and the neck;
   2. the head, the ears, the hair, the beard and the lines of age;
   3. the eyes, the brows, the nose and the mouth.
   face_paint averages four points in each pixel. */

struct face_rgb { u8 r, g, b; };

/* The colours and the measures of one face, computed one time for a paint. */
struct face_look
{
    face_rgb background, robe, skin, skin_dark, hair, eye_white, pupil, mouth, ornament;
    f32      head_rx, head_ry;   /* the radii of the head */
    f32      head_power;         /* 2 for an ellipse, 4 for a square head */
    f32      eye_rx, eye_ry;     /* the radii of one eye */
    f32      lid;                /* a lid covers the eye above centre - lid * eye_ry; 1 or more is no lid */
    f32      brow_inner, brow_outer;   /* the heights of the ends of a brow */
    f32      brow_thickness;
};

internal face_rgb
face_rgb_make(u32 r, u32 g, u32 b)
{
    face_rgb colour = { (u8)r, (u8)g, (u8)b };
    return colour;
}

internal face_rgb
face_rgb_scale(face_rgb colour, f32 factor)
{
    return face_rgb_make((u32)(colour.r * factor), (u32)(colour.g * factor), (u32)(colour.b * factor));
}

internal face_look
face_look_of(const face_description *face)
{
    const face_rgb skins[FACE_SKIN_COUNT]             = { { 0xF1, 0xD2, 0xB0 }, { 0xE0, 0xB8, 0x90 }, { 0xC9, 0x9A, 0x6E }, { 0xA3, 0x6F, 0x4A } };
    const face_rgb hairs[FACE_HAIR_COUNT]             = { { 0x1E, 0x1A, 0x1A }, { 0x4A, 0x30, 0x22 }, { 0x6B, 0x45, 0x2A }, { 0xB8, 0xB4, 0xAE } };
    const face_rgb backgrounds[FACE_BACKGROUND_COUNT] = { { 0x4F, 0x66, 0x80 }, { 0x4A, 0x6B, 0x50 }, { 0xB0, 0x8A, 0x48 }, { 0x6E, 0x4A, 0x62 } };
    const face_rgb robes[FACE_BACKGROUND_COUNT]       = { { 0x2E, 0x2A, 0x3A }, { 0x5A, 0x30, 0x28 }, { 0x3A, 0x4A, 0x5C }, { 0x4C, 0x3A, 0x24 } };
    const f32 head_rx[FACE_SHAPE_COUNT]    = { 0.30f, 0.27f, 0.29f };
    const f32 head_ry[FACE_SHAPE_COUNT]    = { 0.34f, 0.38f, 0.35f };
    const f32 head_power[FACE_SHAPE_COUNT] = { 2.0f, 2.0f, 4.0f };
    const f32 eye_rx[FACE_EYE_COUNT]       = { 0.065f, 0.055f, 0.070f };
    const f32 eye_ry[FACE_EYE_COUNT]       = { 0.035f, 0.045f, 0.028f };

    face_look look = {};
    look.background = backgrounds[face->background];
    look.robe       = robes[(face->background + 1) % FACE_BACKGROUND_COUNT];
    look.skin       = skins[face->skin];
    look.skin_dark  = face_rgb_scale(look.skin, 0.72f);
    look.hair       = hairs[face->hair];
    look.eye_white  = face_rgb_make(0xF4, 0xF0, 0xE8);
    look.pupil      = face_rgb_make(0x20, 0x18, 0x18);
    look.mouth      = face->female ? face_rgb_make(0x9A, 0x3A, 0x44) : face_rgb_make(0x6E, 0x30, 0x2C);
    look.ornament   = face_rgb_make(0xD8, 0xB0, 0x50);
    look.head_rx    = head_rx[face->shape];
    look.head_ry    = head_ry[face->shape];
    look.head_power = head_power[face->shape];
    if (face->age == FACE_YOUNG) { look.head_rx *= 0.94f; look.head_ry *= 0.94f; }
    look.eye_rx     = eye_rx[face->eyes];
    look.eye_ry     = eye_ry[face->eyes];
    look.lid        = 2.0f;
    look.brow_inner = look.brow_outer = 0.60f;
    look.brow_thickness = 0.020f;
    switch (face->emotion)
    {
        case FACE_HAPPY:     look.eye_ry *= 0.75f; look.brow_inner = look.brow_outer = 0.59f; break;
        case FACE_ANGRY:     look.lid = 0.3f; look.brow_inner = 0.615f; look.brow_outer = 0.585f; look.brow_thickness = 0.024f; break;
        case FACE_SAD:       look.lid = 0.5f; look.brow_inner = 0.585f; look.brow_outer = 0.612f; break;
        case FACE_SURPRISED: look.eye_ry *= 1.5f; look.eye_rx *= 1.1f; look.brow_inner = look.brow_outer = 0.565f; break;
        default:             look.lid = 0.6f; break;   /* calm: a half-closed eye */
    }
    return look;
}

internal f32
face_abs(f32 value)
{
    return value < 0.0f ? -value : value;
}

/* True inside the superellipse: (|dx| / rx) ^ power + (|dy| / ry) ^ power < 1. power is 2 or 4. */
internal b32
face_in_ellipse(f32 px, f32 py, f32 cx, f32 cy, f32 rx, f32 ry, f32 power)
{
    f32 nx = (px - cx) / rx, ny = (py - cy) / ry;
    f32 sx = nx * nx, sy = ny * ny;
    if (power > 2.0f) { sx *= sx; sy *= sy; }
    return sx + sy < 1.0f;
}

/* True within thickness / 2 of the segment from a to b. */
internal b32
face_near_segment(f32 px, f32 py, f32 ax, f32 ay, f32 bx, f32 by, f32 thickness)
{
    f32 dx = bx - ax, dy = by - ay;
    f32 length2 = dx * dx + dy * dy;
    f32 t = length2 > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / length2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    f32 ex = px - (ax + t * dx), ey = py - (ay + t * dy);
    return ex * ex + ey * ey < thickness * thickness * 0.25f;
}

/* True on the ring of radius r with its centre at (cx, cy), in its lower half if lower, else its upper half. */
internal b32
face_on_arc(f32 px, f32 py, f32 cx, f32 cy, f32 r, f32 thickness, b32 lower)
{
    if (lower ? py < cy : py > cy) return false;
    f32 dx = px - cx, dy = py - cy;
    f32 d2 = dx * dx + dy * dy;
    f32 inner = r - thickness * 0.5f, outer = r + thickness * 0.5f;
    return d2 > inner * inner && d2 < outer * outer;
}

/* The height of the hairline at px for a style. Hair covers the head above it. */
internal f32
face_hairline(const face_description *face, f32 px)
{
    f32 side = face_abs(px - 0.5f);
    switch (face->hair_style)
    {
        case 1:  return 0.40f + 0.16f * (px - 0.25f);          /* a side part: lower at the right */
        case 2:  return 0.46f - 0.5f * side;                   /* a high forehead: the hair stays at the temples */
        case 3:  return 0.43f + 0.3f * side * side;            /* a topknot: a tight cap */
        default: return 0.45f + 0.2f * side * side;            /* a fringe */
    }
}

internal b32
face_in_hair(const face_description *face, const face_look *look, f32 px, f32 py)
{
    f32 cap_rx = look->head_rx + 0.03f, cap_ry = 0.30f;
    if (face->hair_style == 3)
    {
        /* The bun above the head, and the band of the knot. */
        if (face_in_ellipse(px, py, 0.5f, 0.28f, 0.085f, 0.075f, 2.0f)) return true;
        if (face_near_segment(px, py, 0.40f, 0.345f, 0.60f, 0.345f, 0.03f)) return true;
    }
    return py < face_hairline(face, px) && face_in_ellipse(px, py, 0.5f, 0.58f, cap_rx, cap_ry, look->head_power);
}

internal b32
face_in_beard(const face_description *face, f32 px, f32 py, b32 in_head)
{
    if (face->beard == FACE_BEARD_NONE) return false;
    f32 side = face_abs(px - 0.5f);
    /* The moustache: two strokes above the mouth, lower at the outer ends. */
    if (side > 0.03f && side < 0.14f && py > 0.835f && py < 0.858f + 0.5f * (side - 0.03f)) return true;
    if (face->beard != FACE_BEARD_FULL || !in_head) return false;
    /* The full beard: the chin and the jaw line. */
    return py > 0.93f || (side > 0.19f && py > 0.80f);
}

internal face_rgb
face_sample(const face_description *face, const face_look *look, f32 px, f32 py)
{
    face_rgb colour = look->background;
    f32 side = face_abs(px - 0.5f);

    /* The long hair of a woman hangs behind the head and the shoulders. */
    if (face->female && face_in_ellipse(px, py, 0.5f, 0.80f, 0.37f, 0.48f, 2.0f)) colour = look->hair;
    /* The robe and the neck. */
    if (py > 1.03f && face_in_ellipse(px, py, 0.5f, 1.30f, 0.46f, 0.30f, 2.0f)) colour = look->robe;
    if (side < 0.075f && py > 0.92f && py < 1.06f) colour = look->skin_dark;

    b32 in_head = face_in_ellipse(px, py, 0.5f, 0.66f, look->head_rx, look->head_ry, look->head_power);
    if (in_head) colour = look->skin;
    if (face_in_ellipse(px, py, 0.5f - look->head_rx, 0.69f, 0.045f, 0.055f, 2.0f) ||
        face_in_ellipse(px, py, 0.5f + look->head_rx, 0.69f, 0.045f, 0.055f, 2.0f)) colour = look->skin;
    if (face_in_hair(face, look, px, py)) colour = look->hair;
    if (face_in_beard(face, px, py, in_head)) colour = look->hair;
    if (face->female && face->hair_style == 3 && face_in_ellipse(px, py, 0.5f, 0.28f, 0.03f, 0.03f, 2.0f)) colour = look->ornament;

    /* The lines of age under the eyes and at the mouth. */
    if (face->age == FACE_OLD)
    {
        if (face_near_segment(px, py, 0.30f, 0.735f, 0.38f, 0.745f, 0.012f) ||
            face_near_segment(px, py, 0.62f, 0.745f, 0.70f, 0.735f, 0.012f) ||
            face_near_segment(px, py, 0.40f, 0.84f, 0.42f, 0.92f, 0.012f) ||
            face_near_segment(px, py, 0.60f, 0.84f, 0.58f, 0.92f, 0.012f)) colour = look->skin_dark;
    }

    /* The eyes: the white, the pupil, then the lid. */
    for (u32 eye = 0; eye < 2; ++eye)
    {
        f32 cx = eye ? 0.62f : 0.38f, cy = 0.66f;
        if (!face_in_ellipse(px, py, cx, cy, look->eye_rx, look->eye_ry, 2.0f)) continue;
        colour = look->eye_white;
        f32 pupil = face->emotion == FACE_SURPRISED ? 0.018f : 0.022f;
        if (face_in_ellipse(px, py, cx, cy + 0.004f, pupil, pupil, 2.0f)) colour = look->pupil;
        if (py < cy - look->lid * look->eye_ry) colour = look->skin;
    }

    /* The brows, from the inner end to the outer end. */
    if (face_near_segment(px, py, 0.31f, look->brow_outer, 0.44f, look->brow_inner, look->brow_thickness) ||
        face_near_segment(px, py, 0.56f, look->brow_inner, 0.69f, look->brow_outer, look->brow_thickness)) colour = look->hair;

    /* The nose: a line down and a short base. */
    if (face_near_segment(px, py, 0.50f, 0.70f, 0.50f, 0.79f, 0.012f) ||
        face_near_segment(px, py, 0.47f, 0.795f, 0.53f, 0.795f, 0.012f)) colour = look->skin_dark;

    /* The mouth of each emotion. */
    switch (face->emotion)
    {
        case FACE_HAPPY:     if (face_on_arc(px, py, 0.5f, 0.84f, 0.085f, 0.020f, true)) colour = look->mouth; break;
        case FACE_SAD:       if (face_on_arc(px, py, 0.5f, 0.945f, 0.075f, 0.020f, false)) colour = look->mouth; break;
        case FACE_SURPRISED: if (face_in_ellipse(px, py, 0.5f, 0.885f, 0.045f, 0.055f, 2.0f)) colour = face_rgb_scale(look->mouth, 0.55f); break;
        case FACE_ANGRY:
            if (face_near_segment(px, py, 0.43f, 0.885f, 0.57f, 0.885f, 0.022f) ||
                face_near_segment(px, py, 0.43f, 0.885f, 0.42f, 0.905f, 0.016f) ||
                face_near_segment(px, py, 0.57f, 0.885f, 0.58f, 0.905f, 0.016f)) colour = look->mouth;
            break;
        default:             if (face_near_segment(px, py, 0.44f, 0.88f, 0.56f, 0.88f, 0.018f)) colour = look->mouth; break;
    }
    return colour;
}

/* Paints the face into rgba: FACE_WIDTH * FACE_HEIGHT * 4 bytes, R G B A, the
   top row first. Each pixel averages four points, so the edges are smooth. */
internal void
face_paint(const face_description *face, u8 *rgba)
{
    face_look look = face_look_of(face);
    const f32 step = 1.0f / (f32)FACE_WIDTH;
    const f32 offsets[2] = { 0.25f, 0.75f };
    for (u32 y = 0; y < FACE_HEIGHT; ++y)
    {
        for (u32 x = 0; x < FACE_WIDTH; ++x)
        {
            u32 r = 0, g = 0, b = 0;
            for (u32 sy = 0; sy < 2; ++sy)
            {
                for (u32 sx = 0; sx < 2; ++sx)
                {
                    face_rgb colour = face_sample(face, &look, ((f32)x + offsets[sx]) * step, ((f32)y + offsets[sy]) * step);
                    r += colour.r; g += colour.g; b += colour.b;
                }
            }
            u8 *pixel = rgba + (y * FACE_WIDTH + x) * 4;
            pixel[0] = (u8)(r / 4);
            pixel[1] = (u8)(g / 4);
            pixel[2] = (u8)(b / 4);
            pixel[3] = 255;
        }
    }
}

#endif /* FACES_H */
