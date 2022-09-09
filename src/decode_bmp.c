#include "decode_bmp.h"

#ifndef NULL
#define NULL 0
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

/*
Header	14 bytes	 	
Windows Structure: BITMAPFILEHEADER
Signature	2 bytes	0000h	'BM'
FileSize	4 bytes	0002h	File size in bytes
reserved	4 bytes	0006h	unused (=0)
DataOffset	4 bytes	000Ah	Offset from beginning of file to bitmap data
*/

#pragma pack(push, 1)
typedef struct BitmapFileHeader {
    char character_header[2]; // BM, BA, CI, CP, IC, or PT
    uint32_t image_size;
    uint16_t reserved_1; // historical artifact, usually 0
    uint16_t reserved_2; // historical artifact, usually 0
    uint32_t image_offset;
} BitmapFileHeader;
#pragma pack(pop)

void get_BMP_size(
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
        *out_good = false;
        return;
    }
    
    // read DIB header
    printf("reading DIB header from 4 bytes: %u, %u, %u, %u\n",
        raw_input_at[0],
        raw_input_at[1],
        raw_input_at[2],
        raw_input_at[3]);
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
        *out_good = false;
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
    
    printf("out_height: %u, out_width: %u\n", *out_height, *out_width);
    
    *out_good = true;
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
    uint64_t raw_input_left = raw_input_size;
    
    BitmapFileHeader header = *(BitmapFileHeader *)raw_input;
    raw_input_at += sizeof(BitmapFileHeader);
    raw_input_left -= sizeof(BitmapFileHeader);
    
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
        *out_good = false;
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
        *out_good = false;
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
        *out_good = false;
        return;
    }
    
    int32_t width = *(int32_t *)raw_input_at;
    raw_input_at += sizeof(int32_t);
    raw_input_left -= sizeof(int32_t);
    
    int32_t height = *(int32_t *)raw_input_at;
    raw_input_at += sizeof(int32_t);
    raw_input_left -= sizeof(int32_t);
    // height can be negative - it means the bitmap is stored from top to
    // bottom
    height = (
        ((height > 0) * height) +
        ((height < 0) * -height));
    
    if (
        (uint64_t)(width * height * 4) != out_rgba_values_size)
    {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - bitmap has height %i * width %i * 4 pixels, but "
            "out_rgba_values_size is %llu\n",
            height,
            width,
            out_rgba_values_size);
        #endif
        *out_good = false;
    }

    uint16_t planes =  *(int16_t *)raw_input_at;
    raw_input_at += sizeof(int16_t);
    raw_input_left -= sizeof(int16_t);
    if (planes != 1) {
        #ifndef DECODE_BMP_SILENCE
        printf("Error - # of planes was: %u, expected 1\n", planes);
        #endif
        *out_good = false;
        return;
    }
    
    uint16_t bits_per_pixel =  *(int16_t *)raw_input_at;
    raw_input_at += sizeof(int16_t);
    raw_input_left -= sizeof(int16_t);
    if (bits_per_pixel != 32) {
        #ifndef DECODE_BMP_SILENCE
        printf(
            "Error - # of bits_per_pixel was: %u, expected 32\n",
            bits_per_pixel);
        #endif
        *out_good = false;
        return;
    }

    uint32_t actual_image_size = height * width * 4;
    
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
    
    #ifndef DECODE_BMP_SILENCE
    printf("No errors detected, assuming success\n");
    #endif
    *out_good = true;
    return;
}

