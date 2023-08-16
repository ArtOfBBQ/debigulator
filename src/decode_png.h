#ifndef DECODE_PNG_H
#define DECODE_PNG_H

/*
The 2 methods declared in this file are the API for decoding
PNG files.
*/

#define DECODE_PNG_SILENCE
// #define DECODE_PNG_IGNORE_CRC_CHECKS
#define DECODE_PNG_IGNORE_ASSERTS

#include "inflate.h"
#include <stddef.h>

#ifndef DECODE_PNG_SILENCE
#include "stdio.h"
#endif

#ifndef DECODE_PNG_IGNORE_ASSERTS
#include "assert.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void init_PNG_decoder(
    void * (* malloc_funcptr)(size_t __size)
    );

/*
Find out the width and height of a PNG by inspecting the first 26 bytes of a
file.

You can run this before running decode_PNG() to find out exactly how much
memory you need to store the whole PNG. decode_PNG() always returns 4 channels
(RGBA) per pixel, so the formula is width * height * 4, and you will have
exactly enough memory.

Make sure to check if good is set to 1 (success) or 0 (parsing error). If the
first 26 bytes fail, you can immediately avoid all the extra work of allocating
memory for the image and parsing the entire file.
*/
void get_PNG_width_height(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good);

/*
decode the contents of a PNG file into uint8 RGBA values.

- compressed_input:
    the entire contents of your file, including headers etc. this data will be
    treated as working memory and overwritten in the process!
- out_rgba_values:
    the buffer to copy into, that you already allocated to the correct size.
    Use get_PNG_width_height() above to get the size (width * height * 4).
- rgba_values_size:
    the size of rgba_values. Again this means you must know in advance how big
    the output will be because you've used get_PNG_width_height().
- out_good:
    will be set to 1 on success and 0 on failure. If your decoding fails but
    you don't know why, comment out #define DECODE_PNG_SILENCE and you should
    see printf statements guiding you.
*/
void decode_PNG(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    const uint8_t * out_rgba_values,
    const uint64_t rgba_values_size,
    uint32_t * out_good);

#ifdef __cplusplus
}
#endif

#endif // DECODE_PNG_H
