#ifndef DECODE_PNG_H
#define DECODE_PNG_H

/*
The 2 methods declared in this file are the API for decoding
PNG files.
*/

// #define DECODE_PNG_SILENCE
// #define DECODE_PNG_IGNORE_CRC_CHECKS
// #define DECODE_PNG_IGNORE_ASSERTS

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

/*
This function must be run first, or you can't use anything else in this
header file.

Pass the malloc() function from <stdlib.h> or any other allocation function
with the same signature to give the PNG decoder memory to work with. Same for
the other required C library functions.

Finally, pass some working memory for the decoder to work with.

** Example:
** #include <stdlib.h>
** 
** init_PNG_decoder(
    malloc,
    free,
    memset,
    memcpy,
    120000000);
 );
*/
void
decode_png_init(
    void * (* malloc_funcptr)(size_t __size),
    void (* arg_free_funcptr)(void *),
    void * (* arg_memset_funcptr)(void *str, int c, size_t n),
    void * (* arg_memcpy_func)(void * dest, const void * src, size_t n),
    const uint32_t dpng_working_memory_size,
    const uint32_t thread_id);

void
decode_png_deinit(const uint32_t thread_id);

/*
You must run init_PNG_decode() first or this won't work.

Find out the width and height of a PNG by inspecting the first 26 bytes of a
file.

You can run this before running decode_PNG() to find out exactly how much
memory you need to store the whole PNG. decode_PNG() always returns 4 channels
(RGBA) per pixel, so the formula is width * height * 4, and you will have
exactly enough memory.

Make sure to check if good is set to 1 (success) or 0 (parsing error). If the
first 26 bytes fail, you can immediately abort. 
*/
void
decode_png_get_width_height(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint32_t * out_width,
    uint32_t * out_height,
    uint32_t * out_good);

/*
You must run init_PNG_decode() first or this won't work.

Decode the contents of a PNG file into uint8 RGBA values.

- compressed_input:
    the entire contents of your PNG file, including headers etc. this data
    will be treated as working memory and overwritten in the process!
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
void
decode_png(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    const uint8_t * out_rgba_values,
    const uint64_t rgba_values_size,
    uint32_t * out_good,
    const uint32_t thread_id);

#ifdef __cplusplus
}
#endif

#endif // DECODE_PNG_H
