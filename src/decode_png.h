/*
This API offers only the decode_PNG() function, which reads
& decompresses a PNG stream to the RGBA pixel values inside. 

DecodedImage * decode_PNG(
    uint8_t * compressed_bytes,
    uint32_t compressed_bytes_size)

You pass a stream of bytes to "compressed_bytes" that are
the full contents of the .png file

compressed_size_bytes is the size of the array you're passing
(so the size of the whole file in bytes).

[Q: How do I know if reading the PNG was succesful?]
-> The return value, a DecodedImage, has a field 'good' which will
be 1 if reading the PNG was succesful and 0 if it was a failure.

[Q: Do I have any options?]
-> you can use #define PNG_SILENCE, #define PNG_IGNORE_ASSERTS
in the .c file to avoid including "assert.h" (and you'll have no
debugging assert statements)  and "stdio.h" (and you'll have no
messages in the console).

All other code in the .c file is implementation detail.
*/

#ifndef DECODE_PNG_H
#define DECODE_PNG_H

#include "decodedimage.h"
#include "inflate.h"
#include "inttypes.h"

/*
You can find out the width and height of a PNG by inspecting
only the first 26 bytes of a file.

You can run this before running decode_PNG() to find out
exactly how much memory you need to decode the whole PNG. decode_PNG()
always returns 4 channels (RGBA) per pixel, so the formula is
width * height * 4, and you will have exactly enough memory.

Make sure to check if good is set to 1 (success) or 0 (parsing error). If
the first 26 bytes fail, you can immediately avoid all the extra work of
allocating memory for the image and parsing the entire file.
*/
void get_PNG_width_height(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good);

/*
You should use get_PNG_width_height first before using this!

DecodedImage->good will be set to 0 if the decoding was
unsuccesful, and to 1 if the decoding was succesful.
*/
void decode_PNG(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint8_t * out_rgba_values,
    uint64_t rgba_values_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good);

#endif
