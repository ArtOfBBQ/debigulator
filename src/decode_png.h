#define DECODE_PNG_SILENCE
// #define DECODE_PNG_IGNORE_CRC_CHECKS
// #define DECODE_PNG_IGNORE_ASSERTS

/*
This API offers only the decode_PNG() function, which reads & decompresses a
PNG stream to the RGBA pixel values inside. 

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

#ifndef DECODE_PNG_SILENCE
#include "stdio.h"
#endif

#ifndef DECODE_PNG_IGNORE_ASSERTS
#include "assert.h"
#endif

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

- compressed_input: the entire PNG file's data, including headers etc. Just
read in a PNG file and pass the whole thing directly. Note that this input will
be treated as working memory and overwritten inplace.
- compressed_input_size: the size in bytes of compressed_input. Getting this
wrong is very bad, as it can't be detected and will cause decode_PNG to write
to unrelated memory and corrupt your entire application.
- receiving_memory_store: A huge block of memory that will receive the final
output (the pixels of your PNG file), but will also be used as working memory
for storing hashmaps etc.
- receiving_memory_store: The size in bytes of receiving_memory_store. You can
pass far too much memory (e.g. a huge chunk reserved for storing 100's of
images) and it's fine, but at minimum you need 4 bytes per pixel in the final
image, PLUS 1 byte per row in the image (for scanline filters), and 5,000,000
bytes for storing decompression hashmaps. You don't need to keep the hashmaps
after the function returns, you can use them as memory for the next PNG to
decode.
- out_rgba_values_size: the amount of pixels * 4, aka the amount of RGBA
values. Your memory store pointer + this many bytes contains your PNG file,
so you should continue using the rest of your memory store in your application
or realloc() it, to avoid leaking lots of memory.
- out_good: will be set to 0 if the decoding was unsuccesful, and to 1 if the
decoding was succesful.
*/
void decode_PNG(
    const uint8_t * compressed_input,
    const uint64_t compressed_input_size,
    uint8_t * receiving_memory_store,
    const uint64_t receiving_memory_store_size,
    uint32_t * out_rgba_values_size,
    uint32_t * out_height,
    uint32_t * out_width,
    uint32_t * out_good);

#endif
