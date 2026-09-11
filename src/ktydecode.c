/**
 * @file ktydecode.c
 * @brief Independent decoder for the Kitty KTY and KT4 formats.
 */
#include "ktydecode.h"

#include <stdlib.h>
#include <string.h>

enum { KTY_WIDTH = 640, KTY_COLUMNS = 160, KTY_ROWS = 100 };

typedef struct KtyDecoder {
    const uint8_t *data;
    size_t size;
    size_t position;
    uint8_t *pixels;
    uint8_t marks[KTY_ROWS][KTY_COLUMNS];
    int mode;
} KtyDecoder;

static int read_byte(KtyDecoder *decoder, uint8_t *value)
{
    if (decoder->position >= decoder->size)
        return 0;
    *value = decoder->data[decoder->position++];
    return 1;
}

static void put_pixel(uint8_t *row, int x, uint8_t color)
{
    int shift = (x & 1) ? 0 : 4;
    row[x / 2] |= (uint8_t)((color & 7) << shift);
}

static void put_tile(KtyDecoder *decoder, int tile_x, int tile_y,
                     const uint8_t *tile, int mode)
{
    int tile_rows = mode == 0 ? 2 : 4;
    int base_y = mode == 0 ? tile_y * 2 : tile_y * 4;
    int row;

    for (row = 0; row < tile_rows; ++row) {
        int source_row = mode < 2 ? row & 1 : row;
        int byte_group = source_row >= 2 ? 3 : 0;
        int nibble_shift = (source_row & 1) ? 0 : 4;
        uint8_t blue = (uint8_t)(tile[byte_group] >> nibble_shift);
        uint8_t red = (uint8_t)(tile[byte_group + 1] >> nibble_shift);
        uint8_t green = (uint8_t)(tile[byte_group + 2] >> nibble_shift);
        uint8_t *destination = decoder->pixels + (size_t)(base_y + row) * 320;
        int pixel;

        blue &= 0x0f;
        red &= 0x0f;
        green &= 0x0f;
        for (pixel = 0; pixel < 4; ++pixel) {
            uint8_t mask = (uint8_t)(8 >> pixel);
            uint8_t color = 0;
            if ((blue & mask) != 0) color |= 1;
            if ((red & mask) != 0) color |= 2;
            if ((green & mask) != 0) color |= 4;
            put_pixel(destination, tile_x * 4 + pixel, color);
        }
    }
}

static int mark_and_put(KtyDecoder *decoder, int x, int y,
                        const uint8_t *tile, int mode)
{
    if (x < 0 || x >= KTY_COLUMNS || y < 0 || y >= KTY_ROWS)
        return 0;
    decoder->marks[y][x] = 1;
    put_tile(decoder, x, y, tile, mode);
    return 1;
}

int KtyDecode(const uint8_t *data, size_t size, KtyImage *image)
{
    KtyDecoder decoder;
    uint8_t byte;
    int finished = 0;

    if (data == NULL || image == NULL || size == 0)
        return 1;
    memset(image, 0, sizeof(*image));
    memset(&decoder, 0, sizeof(decoder));
    decoder.data = data;
    decoder.size = size;
    decoder.pixels = (uint8_t *)calloc(320u * 400u, 1);
    if (decoder.pixels == NULL)
        return 3;

    while (read_byte(&decoder, &byte)) {
        const uint8_t *tile;
        size_t tile_size;

        if (byte == 255) {
            int x, y;
            int tail_mode = decoder.mode == 0 ? 0 : 2;
            tile_size = tail_mode == 0 ? 3 : 6;
            for (y = 0; y < KTY_ROWS; ++y) {
                for (x = 0; x < KTY_COLUMNS; ++x) {
                    if (decoder.marks[y][x] != 0)
                        continue;
                    if (decoder.position + tile_size > size)
                        goto malformed;
                    put_tile(&decoder, x, y, data + decoder.position, tail_mode);
                    decoder.position += tile_size;
                }
            }
            /* Some original KTY writers leave a short trailer after the raw
             * tile stream.  The contemporary loader stops as soon as every
             * cell has been filled, so the trailer is deliberately ignored. */
            finished = 1;
            break;
        }

        decoder.mode = byte;
        tile_size = decoder.mode < 2 ? 3 : 6;
        if (decoder.position + tile_size > size)
            goto malformed;
        tile = data + decoder.position;
        decoder.position += tile_size;

        for (;;) {
            uint8_t high, low, right, bottom;
            int offset, left, top, x, y;
            if (!read_byte(&decoder, &high)) goto malformed;
            if (high == 255) break;
            if (!read_byte(&decoder, &low)) goto malformed;
            offset = ((high & 63) << 8) | low;
            left = offset % KTY_COLUMNS;
            top = offset / KTY_COLUMNS;
            if (top >= KTY_ROWS) goto malformed;

            if ((high & 128) != 0) {
                right = (uint8_t)left;
                if (!read_byte(&decoder, &bottom)) goto malformed;
            } else {
                if (!read_byte(&decoder, &right)) goto malformed;
                if ((high & 64) != 0)
                    bottom = (uint8_t)top;
                else if (!read_byte(&decoder, &bottom))
                    goto malformed;
            }
            if (right < left || right >= KTY_COLUMNS ||
                bottom < top || bottom >= KTY_ROWS)
                goto malformed;
            for (y = top; y <= bottom; ++y)
                for (x = left; x <= right; ++x)
                    mark_and_put(&decoder, x, y, tile, decoder.mode);
        }

        for (;;) {
            uint8_t x, y;
            if (!read_byte(&decoder, &x)) goto malformed;
            if (x == 255) break;
            if (!read_byte(&decoder, &y) ||
                !mark_and_put(&decoder, x, y, tile, decoder.mode))
                goto malformed;
        }
    }

    if (!finished)
        goto malformed;
    image->pixels = decoder.pixels;
    image->width = KTY_WIDTH;
    image->height = decoder.mode == 0 ? 200 : 400;
    return 0;

malformed:
    free(decoder.pixels);
    return 1;
}

void KtyFree(KtyImage *image)
{
    if (image != NULL) {
        free(image->pixels);
        image->pixels = NULL;
    }
}
