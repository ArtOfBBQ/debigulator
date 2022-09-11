#include "decode_bmp.h"

#ifndef NULL
#define NULL 0
#endif

#pragma pack(push, 1)
typedef struct BitmapFileHeader {
    char character_header[2]; // BM, BA, CI, CP, IC, or PT
    uint32_t image_size;
    uint16_t reserved_1; // historical artifact, usually 0
    uint16_t reserved_2; // historical artifact, usually 0
    uint32_t image_offset;
} BitmapFileHeader;

typedef struct DIBHeader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;         // expecting 1
    uint16_t bits_per_pixel; // expecting 32
    uint32_t compression;    // expecting 0
    uint32_t image_size;     // 0 is valid if no compression
    uint32_t x_pixels_per_meter; // wtf
    uint32_t y_pixels_per_meter; // wtf
    uint32_t colors_used;
    uint32_t important_colors; // 0 = all
} DIBHeader;
#pragma pack(pop)

void get_BMP_width_height(
    const uint8_t * raw_input,
    const uint64_t raw_input_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good)
{
    uint8_t * raw_input_at = (uint8_t *)raw_input;
    uint64_t raw_input_left = raw_input_size;
    
    assert(raw_input_size >= sizeof(BitmapFileHeader));
    
    BitmapFileHeader header = *(BitmapFileHeader *)raw_input;
    raw_input_at += sizeof(BitmapFileHeader);
    raw_input_left -= sizeof(BitmapFileHeader);
    
    if (
        header.character_header[0] != 'B' ||
        header.character_header[1] != 'M')
    {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - Bitmap header character identifiers were [%c,%c], we "
            " only support [B,M]\n",
            header.character_header[0],
            header.character_header[1]);
        #endif
        *out_good = 0;
        return;
    }
    
    uint32_t DIB_size = *(uint32_t *)raw_input_at;
    raw_input_at += sizeof(uint32_t);
    raw_input_left -= sizeof(uint32_t);
    
    if (DIB_size != 40) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - currently supporting only 40-byte DIB headers."
            " Actual value was: %u\n",
            DIB_size);
        #endif
        *out_good = 0;
        return;
    }
    
    assert(raw_input_left >= 8);
    int32_t width = *(int32_t *)raw_input_at;
    raw_input_at += sizeof(int32_t);
    raw_input_left -= sizeof(int32_t);
    
    *out_width = (uint32_t)width;
    
    int32_t height = *(int32_t *)raw_input_at;
    raw_input_at += sizeof(int32_t);
    raw_input_left -= sizeof(int32_t);
    
    // height can be negative - it means the bitmap is stored from top to
    // bottom
    *out_height = (uint32_t)(
        ((height > 0) * height) +
        ((height < 0) * -height));
    
    *out_good = 1;
}

void decode_BMP(
    const uint8_t * raw_input,
    const uint64_t raw_input_size,
    uint8_t * out_rgba_values,
    const int64_t out_rgba_values_size,
    uint32_t * out_good)
{
    assert(raw_input_size >= sizeof(BitmapFileHeader));
    uint8_t * raw_input_at = (uint8_t *)raw_input;
    // uint64_t raw_input_left = raw_input_size;
    
    BitmapFileHeader header = *(BitmapFileHeader *)raw_input;
    raw_input_at += sizeof(BitmapFileHeader);
    // raw_input_left -= sizeof(BitmapFileHeader);
    
    if (header.character_header[0] != 'B' ||
        header.character_header[1] != 'M')
    {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - Bitmap header character identifiers were [%c,%c], we "
            " only support [B,M]\n",
            header.character_header[0],
            header.character_header[1]);
        #endif
        *out_good = 0;
        return;
    }
    
    if (header.image_offset + header.image_size + sizeof(BitmapFileHeader) <
            raw_input_size)
    {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - header says image starts at offset %u and is size %u"
            ", but raw file size was only %llu\n",
            header.image_offset,
            header.image_size,
            raw_input_size);
        #endif
        *out_good = 0;
        return;
    }
    
    DIBHeader dib_header = *(DIBHeader *)raw_input_at;
    raw_input_at += sizeof(DIBHeader);
    // raw_input_left -= sizeof(DIBHeader);
    if (dib_header.size != 40) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - currently supporting only 40-byte DIB headers."
            " Actual value was: %u\n",
            dib_header.size);
        #endif
        *out_good = 0;
        return;
    }
    
    // height can be negative - it means the bitmap is stored from top to
    // bottom
    dib_header.height = (
        ((dib_header.height > 0) * dib_header.height) +
        ((dib_header.height < 0) * -dib_header.height));
    
    if (
        (uint64_t)(dib_header.width * dib_header.height * 4) !=
            out_rgba_values_size)
    {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - bitmap has dib_header.height %i * dib_header.width %i"
            " * 4 pixels, but out_rgba_values_size is %llu\n",
            dib_header.height,
            dib_header.width,
            out_rgba_values_size);
        #endif
        *out_good = 0;
    }
    
    if (dib_header.planes != 1) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - # of dib_header.planes was: %u, expected 1\n",
            dib_header.planes);
        #endif
        *out_good = 0;
        return;
    }
    
    if (dib_header.bits_per_pixel != 32) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - # of dib_header.bits_per_pixel was: %u, expected 32\n",
            dib_header.bits_per_pixel);
        #endif
        *out_good = 0;
        return;
    }
    
    if (dib_header.compression != 0) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - dib_header.compression was: %u, expected 0\n",
            dib_header.compression);
        #endif
        *out_good = 0;
    }
    
    if (dib_header.image_size != out_rgba_values_size) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - dib_header.image_size was: %u, expected %llu\n",
            dib_header.image_size,
            out_rgba_values_size);
        #endif
        *out_good = 0;
    }

    if (dib_header.x_pixels_per_meter != 0) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - dib_header.x_pixels_per_meter was: %u, expected 0\n",
            dib_header.x_pixels_per_meter);
        #endif
        *out_good = 0;
    }
    if (dib_header.y_pixels_per_meter != 0) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - dib_header.y_pixels_per_meter was: %u, expected 0\n",
            dib_header.y_pixels_per_meter);
        #endif
        *out_good = 0;
    }
    if (dib_header.colors_used != 0) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - dib_header.colors_used was: %u, expected 0\n",
            dib_header.colors_used);
        #endif
        *out_good = 0;
    }
    if (dib_header.important_colors != 0) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - dib_header.important_colors was: %u, expected 0\n",
            dib_header.important_colors);
        #endif
        *out_good = 0;
    }
    
    uint32_t actual_image_size = dib_header.height * dib_header.width * 4;
    // copy pixel values
    // note: we want RGBA, but bitmaps are stored in BGRA
    for (
        uint32_t i = header.image_offset;
        i < (header.image_offset + actual_image_size);
        i += 4)
    {
        out_rgba_values[i - header.image_offset + 0] = raw_input[i + 2];
        out_rgba_values[i - header.image_offset + 1] = raw_input[i + 1];
        out_rgba_values[i - header.image_offset + 2] = raw_input[i + 0];
        out_rgba_values[i - header.image_offset + 3] = raw_input[i + 3];
    }
    
    *out_good = 1;
    return;
}

void encode_BMP(
    const uint8_t * rgba,
    const uint64_t rgba_size,
    const uint32_t width,
    const uint32_t height,
    unsigned char * recipient,
    const int64_t recipient_capacity)
{
    // reminder: the final +1 is for a potential null terminator
    assert(recipient_capacity == 14 + 40 + rgba_size + 1);
    unsigned char * recipient_at = (unsigned char*)recipient;
    
    BitmapFileHeader header;
    assert(sizeof(header) == 14);
    header.character_header[0] = 'B';
    header.character_header[1] = 'M';
    header.image_size = (width * height * 4) + 54;
    header.reserved_1 = 0;
    header.reserved_2 = 0;
    header.image_offset = 54;
    
    char * bitmap_header_at = (char *)&header;
    for (uint32_t _ = 0; _ < 14; _++)  {
        *recipient_at++ = *bitmap_header_at++;
    }
    
    DIBHeader dib_header;
    assert(sizeof(dib_header) == 40);
    dib_header.size = 40;
    dib_header.width = (int32_t)width;
    dib_header.height = -1 * (int32_t)height;
    dib_header.planes = 1;
    dib_header.bits_per_pixel = 32;
    dib_header.compression = 0;
    dib_header.image_size = width * height * 4;
    dib_header.x_pixels_per_meter = 0;
    dib_header.y_pixels_per_meter = 0;
    dib_header.colors_used = 0;
    dib_header.important_colors = 0;
    
    char * dib_header_at = (char *)&dib_header;
    for (uint32_t _ = 0; _ < 40; _++) {
        *recipient_at++ = *dib_header_at++;
    }
    
    for (
        uint32_t _ = 0;
        _ < rgba_size;
        _ += 4)
    {
        *recipient_at++ = rgba[_ + 2];
        *recipient_at++ = rgba[_ + 1];
        *recipient_at++ = rgba[_ + 0];
        *recipient_at++ = rgba[_ + 3];
    }
}

