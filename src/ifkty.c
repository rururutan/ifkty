/**
 * @file ifkty.c
 * @brief Susie I/F adapter for Kitty KTY and KT4 images.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "spibase.h"
#include "ktydecode.h"

LONG_PTR SpiGetFileSize(SPI_FILE *fp);

#define IFKTY_VERSION "0.10"

const int NumInfo = 4;
const LPCSTR PluginInfo[] = {
    "00IN",
    "Kitty KTY/KT4 to DIB filter ver." IFKTY_VERSION " (C) Ru^3",
    "*.kty;*.kt4",
    "Kitty KTY/KT4"
};

static int read_entire_file(SPI_FILE *file, uint8_t **data, size_t *size)
{
    LONG_PTR file_size = SpiGetFileSize(file);
    if (file_size <= 0 || file_size > 0x7fffffff)
        return SPI_ERROR_BROKEN_DATA;
    *size = (size_t)file_size;
    *data = (uint8_t *)malloc(*size);
    if (*data == NULL)
        return SPI_ERROR_ALLOCATE_MEMORY;
    SpiSeek(file, 0, FILE_BEGIN);
    if (SpiRead(*data, (DWORD)*size, file) != *size) {
        free(*data);
        *data = NULL;
        return SPI_ERROR_FILE_READ;
    }
    return 0;
}

static int has_kitty_extension(const char *filename)
{
    const char *extension;
    if (filename == NULL)
        return 0;
    extension = strrchr(filename, '.');
    if (extension == NULL)
        return 0;
    return _stricmp(extension, ".kty") == 0 ||
           _stricmp(extension, ".kt4") == 0;
}

/** KTY has no magic signature, so recognition is extension-based. */
int IsSupportedFormat(LPBYTE data, DWORD size, LPCSTR filename)
{
    (void)data;
    (void)size;
    return has_kitty_extension(filename);
}

/** Decode enough data to return the format-dependent image height. */
int GetImageInfo(SPI_FILE *file, PictureInfo *info)
{
    uint8_t *data;
    size_t size;
    KtyImage image;
    int result = read_entire_file(file, &data, &size);
    if (result != 0)
        return result;
    result = KtyDecode(data, size, &image);
    free(data);
    if (result != 0)
        return result == 3 ? SPI_ERROR_ALLOCATE_MEMORY : SPI_ERROR_BROKEN_DATA;
    SpiSetPictureInfo(info, image.width, image.height, 4, 0, 0, 0, 0, NULL);
    KtyFree(&image);
    return 0;
}

/** Decode Kitty pixels and return a bottom-up 4bpp DIB. */
int GetImage(SPI_FILE *file, HANDLE *info_handle, HANDLE *bitmap_handle,
             SPIPROC progress, LONG_PTR callback_data)
{
    static const uint8_t digital_palette[8][3] = {
        { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xf0 },
        { 0xf0, 0x00, 0x00 }, { 0xf0, 0x00, 0xf0 },
        { 0x00, 0xf0, 0x00 }, { 0x00, 0xf0, 0xf0 },
        { 0xf0, 0xf0, 0x00 }, { 0xf0, 0xf0, 0xf0 }
    };
    uint8_t *data;
    size_t size;
    KtyImage image;
    LPBITMAPINFO bitmap_info;
    LPBYTE bitmap_bits;
    DWORD bitmap_stride;
    uint32_t source_stride = 320;
    uint32_t y;
    int result;
    int i;

    result = read_entire_file(file, &data, &size);
    if (result != 0)
        return result;
    result = KtyDecode(data, size, &image);
    free(data);
    if (result != 0)
        return result == 3 ? SPI_ERROR_ALLOCATE_MEMORY : SPI_ERROR_BROKEN_DATA;

    result = SpiInitBitmap((HLOCAL *)info_handle, &bitmap_info,
                           (HLOCAL *)bitmap_handle, &bitmap_bits,
                           &bitmap_stride, image.width, image.height,
                           4, 16, 0, 0);
    if (result != 0) {
        KtyFree(&image);
        return result;
    }

    memset(bitmap_info->bmiColors, 0, sizeof(RGBQUAD) * 16);
    for (i = 0; i < 8; ++i) {
        bitmap_info->bmiColors[i].rgbRed = digital_palette[i][0];
        bitmap_info->bmiColors[i].rgbGreen = digital_palette[i][1];
        bitmap_info->bmiColors[i].rgbBlue = digital_palette[i][2];
    }
    for (y = 0; y < image.height; ++y) {
        memcpy(bitmap_bits + (size_t)(image.height - 1u - y) * bitmap_stride,
               image.pixels + (size_t)y * source_stride, source_stride);
    }

    KtyFree(&image);
    SpiUnlockBuffer(info_handle);
    SpiUnlockBuffer(bitmap_handle);
    if (progress != NULL)
        progress(100, 100, callback_data);
    return 0;
}
