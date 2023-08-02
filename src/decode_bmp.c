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

typedef struct BitmapV4DIBHeaderAddon {
    uint32_t red_channel_bit_mask;
    uint32_t green_channel_bit_mask;
    uint32_t blue_channel_bit_mask;
    uint32_t alpha_channel_bit_mask;
    uint32_t windows_color_space;
    char color_space_endpoints[36];
    uint32_t red_gamma;
    uint32_t green_gamma;
    uint32_t blue_gamma;
} BitmapV4DIBHeaderAddon;
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
    
    assert(raw_input_left >= sizeof(DIBHeader));
    DIBHeader dib_header = *(DIBHeader *)raw_input_at;
    
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
    
    /*
    height can be negative - it means the bitmap is stored from top to bottom
    */
    *out_height = (uint32_t)(
        ((dib_header.height > 0) *  dib_header.height) +
        ((dib_header.height < 0) * -dib_header.height));
    
    *out_width = (uint32_t)dib_header.width;
    
    *out_good = *out_width > 0 && *out_height > 0;
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
    
    assert(raw_input_size >= sizeof(DIBHeader));
    DIBHeader dib_header = *(DIBHeader *)raw_input_at;
    // raw_input_at += sizeof(DIBHeader);
    // raw_input_left -= sizeof(DIBHeader);
    switch (dib_header.size) {
        case 40:
            break;
        case 108:
            // BITMAPV4HEADER
            assert(sizeof(BitmapV4DIBHeaderAddon) == (108-40));
            break;
        default:
            #ifndef DECODE_BMP_SILENCE
            printf(
                "Error - currently supporting only 40-byte or 108-byte DIB "
                "headers. Actual value was: %u\n",
                dib_header.size);
            #endif
            *out_good = 0;
        return;
    }
    
    // height can be negative - it means the bitmap is stored from top to
    // bottom
    uint32_t read_bottom_first = 1;
    if (dib_header.height < 0)
    {
        read_bottom_first = 0;
        dib_header.height *= -1;
    }
    
    if (
        (uint64_t)(dib_header.width * dib_header.height * 4) !=
            (uint64_t)out_rgba_values_size)
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
    
    if (dib_header.compression != 0 && dib_header.compression != 3) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "%s\n",
            "Error - dib_header.compression was: %u, expected 0 or 3");
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
    
    // copy pixel values
    // note: we want RGBA, but bitmaps are stored in BGRA
    for (
        uint32_t x = 0;
        x < dib_header.width;
        x++)
    {
        for (
            uint32_t y = 0;
            y < dib_header.height;
            y++)
        {
            uint32_t target_i =
                (y * 4 * dib_header.width) +
                (x * 4);
            uint32_t source_i =
                (y * 4 * dib_header.width) + (x * 4);
            
            if (read_bottom_first) {
                target_i =
                    ((dib_header.height - y - 1) * 4 * dib_header.width) +
                    ((x) * 4);
            }
            
            out_rgba_values[target_i + 0] =
                raw_input[source_i + header.image_offset + 2];
            out_rgba_values[target_i + 1] =
                raw_input[source_i + header.image_offset + 1];
            out_rgba_values[target_i + 2] =
                raw_input[source_i + header.image_offset + 0];
            out_rgba_values[target_i + 3] =
                raw_input[source_i + header.image_offset + 3];
        }
    }
    
    *out_good = 1;
    return;
}

void encode_BMP(
    const uint8_t * rgba,
    const uint64_t rgba_size,
    const uint32_t width,
    const uint32_t height,
    char * recipient,
    const int64_t recipient_capacity)
{
    // reminder: the final +1 is for a potential null terminator
    assert((uint64_t)recipient_capacity == 14 + 40 + rgba_size + 1);
    char * recipient_at = (char *)recipient;
    
    BitmapFileHeader header;
    assert(sizeof(header) == 14);
    header.character_header[0] = 'B';
    header.character_header[1] = 'M';
    header.image_size = (width * height * 4) + 54;
    header.reserved_1 = 0;
    header.reserved_2 = 0;
    header.image_offset = 54;
    
    char * bitmap_header_at = (char *)(&header);
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
        *recipient_at++ = (char)rgba[_ + 2];
        *recipient_at++ = (char)rgba[_ + 1];
        *recipient_at++ = (char)rgba[_ + 0];
        *recipient_at++ = (char)rgba[_ + 3];
    }
}
