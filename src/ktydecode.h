/**
 * @file ktydecode.h
 * @brief Decoder API for Kitty KTY and KT4 images.
 */
#ifndef IFKTY_KTYDECODE_H
#define IFKTY_KTYDECODE_H

#include <stddef.h>
#include <stdint.h>

/** Decoded Kitty image. Pixels are packed as two 4-bit indices per byte. */
typedef struct KtyImage {
    uint8_t *pixels; /**< Top-down packed 4bpp pixels. */
    uint32_t width;  /**< Image width, always 640. */
    uint32_t height; /**< Image height, 200 or 400. */
} KtyImage;

/**
 * Decode a complete KTY or KT4 file held in memory.
 *
 * @param data File contents.
 * @param size File size in bytes.
 * @param image Receives allocated pixels and dimensions.
 * @return 0 on success, 1 for malformed data, or 3 for allocation failure.
 */
int KtyDecode(const uint8_t *data, size_t size, KtyImage *image);

/** Release the pixel buffer owned by an image. */
void KtyFree(KtyImage *image);

#endif /* IFKTY_KTYDECODE_H */
