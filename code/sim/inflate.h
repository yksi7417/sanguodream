#ifndef INFLATE_H
#define INFLATE_H
/* zlib inflate (RFC 1950 container, RFC 1951 deflate data). Self-contained:
   no CRT beyond base.h, no malloc, no STL. The decoder state comes from an
   arena and the caller owns the output buffer.

   The save file stores its game state as one zlib stream (see
   spec-src/spec/formats/save-file.md, Container). This decoder reads stored,
   fixed Huffman and dynamic Huffman blocks, then checks the adler32 trailer.

   The Huffman decoder is the canonical one: a count of codes per length plus
   the symbols in code order. It reads one bit at a time, which is simple and
   fast enough for a payload of approximately one megabyte. */

#include "base.h"

#define INFLATE_MAX_CODE_LENGTH     15
#define INFLATE_LITLEN_SYMBOLS      288   /* 0..255 literals, 256 end of block, 257..287 lengths */
#define INFLATE_DISTANCE_SYMBOLS    30
#define INFLATE_CODE_LENGTH_SYMBOLS 19
#define INFLATE_END_OF_BLOCK        256
#define INFLATE_FIRST_LENGTH_SYMBOL 257
#define INFLATE_LENGTH_SYMBOLS      29    /* 257..285 */

enum inflate_status
{
    INFLATE_OK,
    INFLATE_TRUNCATED_INPUT,     /* the stream ends before its final block or trailer */
    INFLATE_OUTPUT_FULL,         /* the data does not fit in the output buffer */
    INFLATE_BAD_ZLIB_HEADER,     /* method is not deflate, window too big, or check bits wrong */
    INFLATE_PRESET_DICTIONARY,   /* FDICT set: the save never uses one, so it is refused */
    INFLATE_BAD_BLOCK_TYPE,      /* block type 3 is reserved */
    INFLATE_BAD_STORED_LENGTH,   /* LEN and NLEN of a stored block do not agree */
    INFLATE_BAD_CODE_LENGTHS,    /* a dynamic block describes an impossible Huffman code */
    INFLATE_BAD_SYMBOL,          /* a code that the table does not hold, or symbol 286/287 */
    INFLATE_BAD_DISTANCE,        /* a back reference before the start of the output */
    INFLATE_ADLER32_MISMATCH,    /* the data decoded but its checksum is not the trailer */
    INFLATE_STATUS_COUNT
};

struct inflate_result
{
    inflate_status status;
    u64            bytes_written;
    u64            bytes_read;        /* of the source, trailer included when status is OK */
    u32            adler32_stored;    /* the trailer, big endian in the stream */
    u32            adler32_computed;  /* over the bytes written */
};

struct inflate_huffman
{
    u16 count_per_length[INFLATE_MAX_CODE_LENGTH + 1];
    u16 symbols[INFLATE_LITLEN_SYMBOLS];   /* in canonical code order */
};

struct inflate_state
{
    const u8 *source;
    u64       source_size;
    u64       source_position;
    u32       bit_buffer;
    u32       bit_count;
    b32       source_overrun;

    u8       *dest;
    u64       dest_capacity;
    u64       dest_position;

    inflate_huffman litlen;
    inflate_huffman distance;
    inflate_huffman code_length;
};

/* RFC 1951, 3.2.5: the base length and extra bits of length symbols 257..285,
   and the base distance and extra bits of distance symbols 0..29. */
global_variable const u16 inflate_length_base[INFLATE_LENGTH_SYMBOLS] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
global_variable const u8 inflate_length_extra_bits[INFLATE_LENGTH_SYMBOLS] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
global_variable const u16 inflate_distance_base[INFLATE_DISTANCE_SYMBOLS] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
global_variable const u8 inflate_distance_extra_bits[INFLATE_DISTANCE_SYMBOLS] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

/* RFC 1951, 3.2.7: the order in which a dynamic block sends the code lengths
   of the code-length alphabet. */
global_variable const u8 inflate_code_length_order[INFLATE_CODE_LENGTH_SYMBOLS] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

/* ---- adler32 (RFC 1950, 9) ---------------------------------------------- */

#define ADLER32_MODULUS 65521u
#define ADLER32_BLOCK   5552u    /* the most bytes before the sums can overflow a u32 */

internal u32
adler32_update(u32 adler, const u8 *data, u64 size)
{
    u32 low  = adler & 0xFFFF;
    u32 high = adler >> 16;
    while (size > 0)
    {
        u32 block = size < ADLER32_BLOCK ? (u32)size : ADLER32_BLOCK;
        size -= block;
        for (u32 index = 0; index < block; ++index)
        {
            low  += data[index];
            high += low;
        }
        data += block;
        low  %= ADLER32_MODULUS;
        high %= ADLER32_MODULUS;
    }
    return (high << 16) | low;
}

internal u32
adler32(const u8 *data, u64 size)
{
    return adler32_update(1, data, size);
}

/* ---- bits --------------------------------------------------------------- */

/* Deflate packs data elements from the least significant bit of each byte.
   Reads past the end return zero bits and set source_overrun. */
internal u32
inflate_read_bits(inflate_state *state, u32 bit_total)
{
    Assert(bit_total <= 24);
    while (state->bit_count < bit_total)
    {
        u32 next = 0;
        if (state->source_position < state->source_size)
        {
            next = state->source[state->source_position++];
        }
        else
        {
            state->source_overrun = true;
        }
        state->bit_buffer |= next << state->bit_count;
        state->bit_count  += 8;
    }
    u32 value = state->bit_buffer & ((1u << bit_total) - 1);
    state->bit_buffer >>= bit_total;
    state->bit_count   -= bit_total;
    return value;
}

/* The bits left in the buffer always belong to the last byte read, so to
   drop them is to move to the next byte boundary. */
internal void
inflate_align_to_byte(inflate_state *state)
{
    state->bit_buffer = 0;
    state->bit_count  = 0;
}

/* ---- Huffman tables ----------------------------------------------------- */

enum huffman_build_result
{
    HUFFMAN_COMPLETE,
    HUFFMAN_INCOMPLETE,       /* some codes are unused: legal only for a single code */
    HUFFMAN_OVERSUBSCRIBED    /* more codes than the lengths allow: always an error */
};

internal huffman_build_result
inflate_build_huffman(inflate_huffman *table, const u8 *code_lengths, u32 symbol_total)
{
    Assert(symbol_total <= INFLATE_LITLEN_SYMBOLS);
    for (u32 length = 0; length <= INFLATE_MAX_CODE_LENGTH; ++length) table->count_per_length[length] = 0;
    for (u32 symbol = 0; symbol < symbol_total; ++symbol) table->count_per_length[code_lengths[symbol]]++;

    s32 codes_left = 1;
    for (u32 length = 1; length <= INFLATE_MAX_CODE_LENGTH; ++length)
    {
        codes_left <<= 1;
        codes_left -= table->count_per_length[length];
        if (codes_left < 0) return HUFFMAN_OVERSUBSCRIBED;
    }

    u16 first_index_of_length[INFLATE_MAX_CODE_LENGTH + 1];
    first_index_of_length[1] = 0;
    for (u32 length = 1; length < INFLATE_MAX_CODE_LENGTH; ++length)
    {
        first_index_of_length[length + 1] = (u16)(first_index_of_length[length] + table->count_per_length[length]);
    }
    for (u32 symbol = 0; symbol < symbol_total; ++symbol)
    {
        u8 length = code_lengths[symbol];
        if (length != 0) table->symbols[first_index_of_length[length]++] = (u16)symbol;
    }
    return codes_left > 0 ? HUFFMAN_INCOMPLETE : HUFFMAN_COMPLETE;
}

/* A code with one symbol of length 1 is the only incomplete code allowed. */
internal b32
inflate_huffman_is_usable(inflate_huffman *table, huffman_build_result built, u32 used_symbols)
{
    if (built == HUFFMAN_COMPLETE) return true;
    if (built == HUFFMAN_OVERSUBSCRIBED) return false;
    return used_symbols == 1 && table->count_per_length[1] == 1;
}

/* Canonical decode: codes of one length are consecutive integers, and the
   first code of each length follows the last code of the length before.
   Returns the symbol, or -1 if no code matches. */
internal s32
inflate_decode_symbol(inflate_state *state, inflate_huffman *table)
{
    s32 code  = 0;
    s32 first = 0;
    s32 index = 0;
    for (u32 length = 1; length <= INFLATE_MAX_CODE_LENGTH; ++length)
    {
        code |= (s32)inflate_read_bits(state, 1);
        s32 count = table->count_per_length[length];
        if (code - first < count) return table->symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code  <<= 1;
    }
    return -1;
}

/* ---- blocks ------------------------------------------------------------- */

internal inflate_status
inflate_stored_block(inflate_state *state)
{
    inflate_align_to_byte(state);
    if (state->source_position + 4 > state->source_size) return INFLATE_TRUNCATED_INPUT;
    const u8 *header = state->source + state->source_position;
    u32 length          = (u32)header[0] | ((u32)header[1] << 8);
    u32 length_inverted = (u32)header[2] | ((u32)header[3] << 8);
    state->source_position += 4;
    if (length != (~length_inverted & 0xFFFF)) return INFLATE_BAD_STORED_LENGTH;
    if (state->source_position + length > state->source_size) return INFLATE_TRUNCATED_INPUT;
    if (state->dest_position + length > state->dest_capacity) return INFLATE_OUTPUT_FULL;
    memcpy(state->dest + state->dest_position, state->source + state->source_position, length);
    state->dest_position   += length;
    state->source_position += length;
    return INFLATE_OK;
}

/* Decodes literals and back references until the end-of-block symbol. */
internal inflate_status
inflate_huffman_block_data(inflate_state *state)
{
    for (;;)
    {
        s32 symbol = inflate_decode_symbol(state, &state->litlen);
        if (state->source_overrun) return INFLATE_TRUNCATED_INPUT;
        if (symbol < 0) return INFLATE_BAD_SYMBOL;
        if (symbol < INFLATE_END_OF_BLOCK)
        {
            if (state->dest_position >= state->dest_capacity) return INFLATE_OUTPUT_FULL;
            state->dest[state->dest_position++] = (u8)symbol;
            continue;
        }
        if (symbol == INFLATE_END_OF_BLOCK) return INFLATE_OK;

        u32 length_index = (u32)symbol - INFLATE_FIRST_LENGTH_SYMBOL;
        if (length_index >= INFLATE_LENGTH_SYMBOLS) return INFLATE_BAD_SYMBOL;
        u32 length = inflate_length_base[length_index] +
                     inflate_read_bits(state, inflate_length_extra_bits[length_index]);

        s32 distance_symbol = inflate_decode_symbol(state, &state->distance);
        if (state->source_overrun) return INFLATE_TRUNCATED_INPUT;
        if (distance_symbol < 0 || distance_symbol >= INFLATE_DISTANCE_SYMBOLS) return INFLATE_BAD_SYMBOL;
        u32 distance = inflate_distance_base[distance_symbol] +
                       inflate_read_bits(state, inflate_distance_extra_bits[distance_symbol]);
        if (state->source_overrun) return INFLATE_TRUNCATED_INPUT;

        if (distance > state->dest_position) return INFLATE_BAD_DISTANCE;
        if (state->dest_position + length > state->dest_capacity) return INFLATE_OUTPUT_FULL;
        /* Byte by byte on purpose: a reference can overlap the bytes it writes. */
        u8 *to   = state->dest + state->dest_position;
        u8 *from = to - distance;
        for (u32 copied = 0; copied < length; ++copied) to[copied] = from[copied];
        state->dest_position += length;
    }
}

/* RFC 1951, 3.2.6: the fixed code. */
internal inflate_status
inflate_fixed_block(inflate_state *state)
{
    u8 lengths[INFLATE_LITLEN_SYMBOLS];
    u32 symbol = 0;
    for (; symbol < 144; ++symbol) lengths[symbol] = 8;
    for (; symbol < 256; ++symbol) lengths[symbol] = 9;
    for (; symbol < 280; ++symbol) lengths[symbol] = 7;
    for (; symbol < INFLATE_LITLEN_SYMBOLS; ++symbol) lengths[symbol] = 8;
    inflate_build_huffman(&state->litlen, lengths, INFLATE_LITLEN_SYMBOLS);

    u8 distance_lengths[INFLATE_DISTANCE_SYMBOLS];
    for (u32 index = 0; index < INFLATE_DISTANCE_SYMBOLS; ++index) distance_lengths[index] = 5;
    inflate_build_huffman(&state->distance, distance_lengths, INFLATE_DISTANCE_SYMBOLS);

    return inflate_huffman_block_data(state);
}

/* Reads the code lengths of the literal/length and distance alphabets, which
   a dynamic block sends compressed with the code-length alphabet. */
internal inflate_status
inflate_read_dynamic_lengths(inflate_state *state, u8 *lengths, u32 litlen_total, u32 distance_total)
{
    u32 code_length_total = inflate_read_bits(state, 4) + 4;
    u8 code_length_lengths[INFLATE_CODE_LENGTH_SYMBOLS] = {};
    for (u32 index = 0; index < code_length_total; ++index)
    {
        code_length_lengths[inflate_code_length_order[index]] = (u8)inflate_read_bits(state, 3);
    }
    if (state->source_overrun) return INFLATE_TRUNCATED_INPUT;
    if (inflate_build_huffman(&state->code_length, code_length_lengths, INFLATE_CODE_LENGTH_SYMBOLS) != HUFFMAN_COMPLETE)
    {
        return INFLATE_BAD_CODE_LENGTHS;
    }

    u32 total = litlen_total + distance_total;
    u32 filled = 0;
    while (filled < total)
    {
        s32 symbol = inflate_decode_symbol(state, &state->code_length);
        if (state->source_overrun) return INFLATE_TRUNCATED_INPUT;
        if (symbol < 0) return INFLATE_BAD_CODE_LENGTHS;
        if (symbol < 16)
        {
            lengths[filled++] = (u8)symbol;
            continue;
        }
        u8  repeated = 0;
        u32 repeat   = 0;
        if (symbol == 16)
        {
            if (filled == 0) return INFLATE_BAD_CODE_LENGTHS;   /* nothing to repeat */
            repeated = lengths[filled - 1];
            repeat   = 3 + inflate_read_bits(state, 2);
        }
        else if (symbol == 17)
        {
            repeat = 3 + inflate_read_bits(state, 3);
        }
        else
        {
            repeat = 11 + inflate_read_bits(state, 7);
        }
        if (filled + repeat > total) return INFLATE_BAD_CODE_LENGTHS;
        while (repeat--) lengths[filled++] = repeated;
    }
    return INFLATE_OK;
}

internal inflate_status
inflate_dynamic_block(inflate_state *state)
{
    u32 litlen_total   = inflate_read_bits(state, 5) + 257;
    u32 distance_total = inflate_read_bits(state, 5) + 1;
    if (litlen_total > 286 || distance_total > INFLATE_DISTANCE_SYMBOLS) return INFLATE_BAD_CODE_LENGTHS;

    u8 lengths[INFLATE_LITLEN_SYMBOLS + INFLATE_DISTANCE_SYMBOLS];
    inflate_status status = inflate_read_dynamic_lengths(state, lengths, litlen_total, distance_total);
    if (status != INFLATE_OK) return status;
    if (lengths[INFLATE_END_OF_BLOCK] == 0) return INFLATE_BAD_CODE_LENGTHS;   /* a block must be able to end */

    u32 litlen_used = 0, distance_used = 0;
    for (u32 index = 0; index < litlen_total; ++index) if (lengths[index]) litlen_used++;
    for (u32 index = 0; index < distance_total; ++index) if (lengths[litlen_total + index]) distance_used++;

    huffman_build_result built = inflate_build_huffman(&state->litlen, lengths, litlen_total);
    if (!inflate_huffman_is_usable(&state->litlen, built, litlen_used)) return INFLATE_BAD_CODE_LENGTHS;
    /* RFC 1951, 3.2.7: a block of literals only can have no distance codes.
       The empty table then matches no code, so a length symbol in that
       block fails with INFLATE_BAD_SYMBOL. */
    built = inflate_build_huffman(&state->distance, lengths + litlen_total, distance_total);
    if (distance_used > 0 && !inflate_huffman_is_usable(&state->distance, built, distance_used)) return INFLATE_BAD_CODE_LENGTHS;

    return inflate_huffman_block_data(state);
}

/* ---- entry points ------------------------------------------------------- */

/* Raw deflate data, no zlib header or trailer. */
internal inflate_status
inflate_deflate_blocks(inflate_state *state)
{
    b32 final_block = false;
    while (!final_block)
    {
        final_block = (b32)inflate_read_bits(state, 1);
        u32 block_type = inflate_read_bits(state, 2);
        if (state->source_overrun) return INFLATE_TRUNCATED_INPUT;
        inflate_status status = INFLATE_OK;
        switch (block_type)
        {
            case 0:  status = inflate_stored_block(state);  break;
            case 1:  status = inflate_fixed_block(state);   break;
            case 2:  status = inflate_dynamic_block(state); break;
            default: status = INFLATE_BAD_BLOCK_TYPE;       break;
        }
        if (status != INFLATE_OK) return status;
    }
    return INFLATE_OK;
}

/* Inflates raw deflate data into dest, with no zlib header and no trailer.
   A member of a zip archive has this form. scratch holds the decoder state. */
internal inflate_result
raw_inflate(memory_arena *scratch, const u8 *source, u64 source_size, u8 *dest, u64 dest_capacity)
{
    inflate_result result = {};
    inflate_state *state = PushStruct(scratch, inflate_state);
    state->source        = source;
    state->source_size   = source_size;
    state->dest          = dest;
    state->dest_capacity = dest_capacity;

    result.status        = inflate_deflate_blocks(state);
    result.bytes_written = state->dest_position;
    result.bytes_read    = state->source_position;
    return result;
}

#define ZLIB_METHOD_DEFLATE   8
#define ZLIB_MAX_WINDOW_INFO  7      /* window of 2^(7+8) = 32 KB */
#define ZLIB_FLAG_DICTIONARY  0x20
#define ZLIB_HEADER_CHECK     31     /* (CMF * 256 + FLG) is a multiple of 31 */

/* Inflates a zlib stream into dest. scratch holds the decoder state and is
   not reset; the caller decides when to reset it. */
internal inflate_result
zlib_inflate(memory_arena *scratch, const u8 *source, u64 source_size, u8 *dest, u64 dest_capacity)
{
    inflate_result result = {};
    if (source_size < 2)
    {
        result.status = INFLATE_TRUNCATED_INPUT;
        return result;
    }
    u32 method_and_info = source[0];
    u32 flags           = source[1];
    if ((method_and_info & 0x0F) != ZLIB_METHOD_DEFLATE || (method_and_info >> 4) > ZLIB_MAX_WINDOW_INFO ||
        ((method_and_info << 8) | flags) % ZLIB_HEADER_CHECK != 0)
    {
        result.status = INFLATE_BAD_ZLIB_HEADER;
        return result;
    }
    if (flags & ZLIB_FLAG_DICTIONARY)
    {
        result.status = INFLATE_PRESET_DICTIONARY;
        return result;
    }

    inflate_state *state = PushStruct(scratch, inflate_state);
    state->source          = source;
    state->source_size     = source_size;
    state->source_position = 2;
    state->dest            = dest;
    state->dest_capacity   = dest_capacity;

    result.status        = inflate_deflate_blocks(state);
    result.bytes_written = state->dest_position;
    result.bytes_read    = state->source_position;
    if (result.status != INFLATE_OK) return result;

    inflate_align_to_byte(state);
    if (state->source_position + 4 > source_size)
    {
        result.status = INFLATE_TRUNCATED_INPUT;
        return result;
    }
    const u8 *trailer = source + state->source_position;
    result.adler32_stored   = ((u32)trailer[0] << 24) | ((u32)trailer[1] << 16) | ((u32)trailer[2] << 8) | (u32)trailer[3];
    result.adler32_computed = adler32(dest, state->dest_position);
    result.bytes_read       = state->source_position + 4;
    if (result.adler32_stored != result.adler32_computed) result.status = INFLATE_ADLER32_MISMATCH;
    return result;
}

internal const char *
inflate_status_name(inflate_status status)
{
    switch (status)
    {
        case INFLATE_OK:                 return "ok";
        case INFLATE_TRUNCATED_INPUT:    return "truncated input";
        case INFLATE_OUTPUT_FULL:        return "output full";
        case INFLATE_BAD_ZLIB_HEADER:    return "bad zlib header";
        case INFLATE_PRESET_DICTIONARY:  return "preset dictionary";
        case INFLATE_BAD_BLOCK_TYPE:     return "bad block type";
        case INFLATE_BAD_STORED_LENGTH:  return "bad stored length";
        case INFLATE_BAD_CODE_LENGTHS:   return "bad code lengths";
        case INFLATE_BAD_SYMBOL:         return "bad symbol";
        case INFLATE_BAD_DISTANCE:       return "bad distance";
        case INFLATE_ADLER32_MISMATCH:   return "adler32 mismatch";
        default:                         return "?";
    }
}

#endif /* INFLATE_H */
